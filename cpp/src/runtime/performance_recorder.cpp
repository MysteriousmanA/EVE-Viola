#include "runtime/performance_recorder.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QMutex>
#include <QMutexLocker>
#include <QUuid>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace {
constexpr qsizetype kSampleLimit = 256;
constexpr qsizetype kSeriesLimit = 1024;

qint64 processCpuNs()
{
#ifdef Q_OS_WIN
    FILETIME creation{}, exit{}, kernel{}, user{};
    if (!GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user)) { return -1; }
    ULARGE_INTEGER k{}, u{};
    k.LowPart = kernel.dwLowDateTime;
    k.HighPart = kernel.dwHighDateTime;
    u.LowPart = user.dwLowDateTime;
    u.HighPart = user.dwHighDateTime;
    return static_cast<qint64>((k.QuadPart + u.QuadPart) * 100);
#else
    return -1;
#endif
}

struct Series {
    QString stage;
    std::uintptr_t source{};
    quint64 count{};
    quint64 failures{};
    quint64 units{};
    qint64 totalNs{};
    qint64 maximumNs{};
    QList<qint64> samples;
};

struct State {
    std::atomic<bool> enabled{false};
    QMutex mutex;
    QString path;
    QString session;
    qint64 intervalStart{};
    qint64 cpuStart{-1};
    QMap<QString, Series> series;
};

State& state()
{
    static State instance;
    return instance;
}

QJsonObject snapshotLocked(const State& data)
{
    const double elapsedMs = static_cast<double>(PerformanceRecorder::nowNs() - data.intervalStart) / 1e6;
    const qint64 cpuEnd = processCpuNs();
    const double cpuMs = data.cpuStart >= 0 && cpuEnd >= data.cpuStart
        ? static_cast<double>(cpuEnd - data.cpuStart) / 1e6 : -1.0;
    QJsonArray stages;
    for (const auto& entry : data.series) {
        auto sorted = entry.samples;
        std::sort(sorted.begin(), sorted.end());
        const auto percentile = [&sorted](double fraction) {
            if (sorted.isEmpty()) {
                return 0.0;
            }
            const auto index = static_cast<qsizetype>(
                std::ceil(fraction * static_cast<double>(sorted.size()))) - 1;
            return static_cast<double>(sorted.at(std::max<qsizetype>(0, index))) / 1e6;
        };
        stages.append(QJsonObject{
            {QStringLiteral("stage"), entry.stage},
            {QStringLiteral("source_hwnd"), QStringLiteral("0x%1").arg(
                 static_cast<qulonglong>(entry.source), 0, 16)},
            {QStringLiteral("count"), static_cast<qint64>(entry.count)},
            {QStringLiteral("failures"), static_cast<qint64>(entry.failures)},
            {QStringLiteral("units"), static_cast<qint64>(entry.units)},
            {QStringLiteral("total_ms"), static_cast<double>(entry.totalNs) / 1e6},
            {QStringLiteral("mean_ms"), entry.count == 0 ? 0.0
                 : static_cast<double>(entry.totalNs) / static_cast<double>(entry.count) / 1e6},
            {QStringLiteral("max_ms"), static_cast<double>(entry.maximumNs) / 1e6},
            {QStringLiteral("p50_ms"), percentile(0.50)},
            {QStringLiteral("p95_ms"), percentile(0.95)},
            {QStringLiteral("percentile_samples"), sorted.size()},
        });
    }
    return QJsonObject{
        {QStringLiteral("event"), QStringLiteral("performance_summary")},
        {QStringLiteral("schema_version"), 1},
        {QStringLiteral("session_id"), data.session},
        {QStringLiteral("pid"), QCoreApplication::applicationPid()},
        {QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {QStringLiteral("interval_ms"), elapsedMs},
        {QStringLiteral("process_cpu_ms"), cpuMs},
        {QStringLiteral("process_cpu_percent_of_one_core"), cpuMs >= 0 && elapsedMs > 0 ? cpuMs / elapsedMs * 100.0 : -1.0},
        {QStringLiteral("percentile_policy"), QStringLiteral("last_256_per_stage_and_source")},
        {QStringLiteral("stages"), stages},
    };
}
} // namespace

bool PerformanceRecorder::initialize(const QString& filePath, QString* error)
{
    auto& data = state();
    const QMutexLocker lock(&data.mutex);
    data.enabled.store(false, std::memory_order_relaxed);
    const auto absolute = QFileInfo(filePath).absoluteFilePath();
    QFile file(absolute);
    if (filePath.isEmpty() || !QDir().mkpath(QFileInfo(absolute).absolutePath())
        || !file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        if (error != nullptr) {
            *error = QStringLiteral("Cannot open performance log: %1 (%2)")
                         .arg(absolute, file.errorString());
        }
        return false;
    }
    data.path = absolute;
    data.session = QUuid::createUuid().toString(QUuid::WithoutBraces);
    data.intervalStart = nowNs();
    data.cpuStart = processCpuNs();
    data.series.clear();
    data.enabled.store(true, std::memory_order_relaxed);
    return true;
}

bool PerformanceRecorder::enabled() noexcept
{
    return state().enabled.load(std::memory_order_relaxed);
}

qint64 PerformanceRecorder::nowNs() noexcept
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

void PerformanceRecorder::record(const char* stage, qint64 nanoseconds,
                                 std::uintptr_t source, quint64 units, bool failed)
{
    if (!enabled()) {
        return;
    }
    auto& data = state();
    const QMutexLocker lock(&data.mutex);
    if (!data.enabled.load(std::memory_order_relaxed)) {
        return;
    }
    QString stageName = QString::fromLatin1(stage);
    QString key = stageName + QChar(u':') + QString::number(static_cast<qulonglong>(source), 16);
    if (!data.series.contains(key) && data.series.size() >= kSeriesLimit) {
        stageName = QStringLiteral("overflow");
        source = 0;
        key = stageName;
    }
    auto& entry = data.series[key];
    entry.stage = stageName;
    entry.source = source;
    const qint64 duration = std::max<qint64>(0, nanoseconds);
    entry.totalNs += duration;
    entry.maximumNs = std::max(entry.maximumNs, duration);
    entry.failures += failed ? 1 : 0;
    entry.units += units;
    if (entry.samples.size() < kSampleLimit) {
        entry.samples.append(duration);
    } else {
        entry.samples[static_cast<qsizetype>(entry.count % kSampleLimit)] = duration;
    }
    ++entry.count;
}

QJsonObject PerformanceRecorder::snapshot()
{
    auto& data = state();
    const QMutexLocker lock(&data.mutex);
    return snapshotLocked(data);
}

bool PerformanceRecorder::flush(QString* error)
{
    auto& data = state();
    const QMutexLocker lock(&data.mutex);
    if (!data.enabled.load(std::memory_order_relaxed) || data.series.isEmpty()) {
        return true;
    }
    QFile file(data.path);
    const QByteArray line = QJsonDocument(snapshotLocked(data)).toJson(QJsonDocument::Compact) + '\n';
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append)
        || file.write(line) != line.size() || !file.flush()) {
        if (error != nullptr) {
            *error = file.errorString();
        }
        return false;
    }
    data.series.clear();
    data.intervalStart = nowNs();
    data.cpuStart = processCpuNs();
    return true;
}

void PerformanceRecorder::disable()
{
    auto& data = state();
    const QMutexLocker lock(&data.mutex);
    data.enabled.store(false, std::memory_order_relaxed);
    data.series.clear();
}

PerformanceScope::PerformanceScope(const char* stage, std::uintptr_t source) noexcept
    : stage_(stage), source_(source), start_(PerformanceRecorder::enabled() ? PerformanceRecorder::nowNs() : 0)
{
}

PerformanceScope::~PerformanceScope()
{
    if (start_ != 0) {
        PerformanceRecorder::record(stage_, PerformanceRecorder::nowNs() - start_, source_);
    }
}
