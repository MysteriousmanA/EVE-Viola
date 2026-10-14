#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTextStream>
#include <QTimer>

#include "config/app_config.h"
#include "platform/native_gateway.h"
#include "runtime/performance_recorder.h"
#include "features/replica/statistics_coordinator.h"

namespace {
quint64 processCpuNs()
{
    FILETIME creation{}, exit{}, kernel{}, user{};
    if (!GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user)) { return 0; }
    ULARGE_INTEGER k{}, u{};
    k.LowPart = kernel.dwLowDateTime;
    k.HighPart = kernel.dwHighDateTime;
    u.LowPart = user.dwLowDateTime;
    u.HighPart = user.dwHighDateTime;
    return (k.QuadPart + u.QuadPart) * 100;
}
struct Plan {
    ReplicaId id;
    std::uintptr_t hwnd;
    std::optional<QRect> region;
    int interval;
};
}

int main(int argc, char* argv[])
{
    // Match QApplication's physical pixel coordinates on scaled desktops.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    QCoreApplication application(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({QStringLiteral("list-sources"), QStringLiteral("List accessible EVE client windows without capturing.")});
    parser.addOption({QStringLiteral("legacy-capture"), QStringLiteral("Compare the original bitmap/copy capture path; use the same scheduler and regions.")});
    parser.addOption({QStringLiteral("config"), QStringLiteral("Read existing replica configuration from this directory."), QStringLiteral("directory")});
    parser.addOption({QStringLiteral("output"), QStringLiteral("Write report JSON and phased JSONL (path + .jsonl)."), QStringLiteral("path")});
    parser.addOption({QStringLiteral("duration-ms"), QStringLiteral("Measured scheduling duration."), QStringLiteral("milliseconds"), QStringLiteral("30000")});
    parser.addOption({QStringLiteral("warmup-ms"), QStringLiteral("Unmeasured warmup duration."), QStringLiteral("milliseconds"), QStringLiteral("2000")});
    parser.process(application);
    QTextStream output(stdout);
    QTextStream errors(stderr);
    try {
        auto functions = NativeFunctions::linked();
        if (parser.isSet(QStringLiteral("legacy-capture"))) {
            functions.captureFrame = nullptr;
            functions.releaseFrame = nullptr;
        }
        NativeGateway gateway(functions);
        const auto sources = gateway.listEveWindows();
        QJsonArray sourceInfo;
        for (const auto& source : sources) {
            RECT rect{};
            const auto hwnd = reinterpret_cast<HWND>(source.hwnd);
            GetClientRect(hwnd, &rect);
            sourceInfo.append(QJsonObject{
                {QStringLiteral("hwnd"), QStringLiteral("0x%1").arg(static_cast<qulonglong>(source.hwnd), 0, 16)},
                {QStringLiteral("title"), source.title},
                {QStringLiteral("width"), static_cast<int>(rect.right - rect.left)},
                {QStringLiteral("height"), static_cast<int>(rect.bottom - rect.top)},
                {QStringLiteral("minimized"), IsIconic(hwnd) != FALSE},
            });
        }
        if (parser.isSet(QStringLiteral("list-sources"))) {
            output << QJsonDocument(sourceInfo).toJson(QJsonDocument::Indented);
            return sources.isEmpty() ? 3 : 0;
        }
        if (sources.isEmpty()) {
            errors << "No accessible EVE windows. Run the probe in the desktop session.\n";
            return 3;
        }
        bool validDuration = false;
        bool validWarmup = false;
        const int duration = parser.value(QStringLiteral("duration-ms")).toInt(&validDuration);
        const int warmup = parser.value(QStringLiteral("warmup-ms")).toInt(&validWarmup);
        const QString reportPath = parser.value(QStringLiteral("output"));
        if (!validDuration || duration < 1000 || duration > 3600000
            || !validWarmup || warmup < 0 || warmup > 60000 || reportPath.isEmpty()) {
            errors << "Specify --output, duration 1000..3600000, and warmup 0..60000.\n";
            return 2;
        }
        AppConfig config;
        if (parser.isSet(QStringLiteral("config"))) {
            QString error;
            if (!JsonConfigStore(parser.value(QStringLiteral("config"))).load(config, &error)) {
                errors << error << '\n';
                return 2;
            }
        }
        StatisticsCoordinator coordinator(gateway);
        ReplicaId nextId = 1;
        QList<Plan> plans;
        const auto addRegistration = [&](std::uintptr_t hwnd, std::optional<QRect> region, int interval) {
            plans.append(Plan{nextId, hwnd, region, interval});
            coordinator.upsert(nextId++, hwnd, region, interval);
        };
        QJsonArray registrations;
        for (const auto& source : sources) {
            bool matched = false;
            for (const auto& replica : config.replicas) {
                if (replica.sourceTitle.compare(source.title, Qt::CaseInsensitive) != 0) {
                    continue;
                }
                matched = true;
                if (replica.statisticsIntervalMs <= 0) {
                    continue;
                }
                const std::optional<QRect> region = replica.capturedRegion.isValid()
                    ? std::optional<QRect>(replica.capturedRegion) : std::nullopt;
                addRegistration(source.hwnd, region, replica.statisticsIntervalMs);
                registrations.append(QJsonObject{
                    {QStringLiteral("source"), source.title},
                    {QStringLiteral("interval_ms"), replica.statisticsIntervalMs},
                    {QStringLiteral("x"), region.has_value() ? region->x() : 0},
                    {QStringLiteral("y"), region.has_value() ? region->y() : 0},
                    {QStringLiteral("width"), region.has_value() ? region->width() : 0},
                    {QStringLiteral("height"), region.has_value() ? region->height() : 0},
                });
            }
            // Without config, measure one full-client scan per source per second.
            if (!matched && !parser.isSet(QStringLiteral("config"))) {
                addRegistration(source.hwnd, std::nullopt, 1000);
                registrations.append(QJsonObject{
                    {QStringLiteral("source"), source.title},
                    {QStringLiteral("interval_ms"), 1000},
                    {QStringLiteral("full_client"), true},
                });
            }
        }
        if (registrations.isEmpty()) {
            errors << "No enabled configured replicas match the running sources.\n";
            return 3;
        }
        int unavailable = 0;
        quint64 deliveries = 0;
        QObject::connect(&coordinator, &StatisticsCoordinator::statisticsUnavailable,
                         &application, [&unavailable] { ++unavailable; });
        QObject::connect(&coordinator, &StatisticsCoordinator::statisticsReady,
                         &application, [&deliveries] { ++deliveries; });
        QEventLoop loop;
        QString logError;
        bool logReady = false;
        quint64 cpuStart = 0;
        StatisticsDiagnostics warmupDiagnostics;
        QTimer::singleShot(warmup, &loop, [&] {
            // Drain warmup tasks before starting the measured scheduling window.
            coordinator.stop();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
            warmupDiagnostics = coordinator.diagnostics();
            logReady = PerformanceRecorder::initialize(reportPath + QStringLiteral(".jsonl"), &logError);
            deliveries = 0;
            unavailable = 0;
            if (!logReady) {
                loop.quit();
                return;
            }
            cpuStart = processCpuNs();
            coordinator.start();
            for (const auto& plan : plans) {
                coordinator.upsert(plan.id, plan.hwnd, plan.region, plan.interval);
            }
            QTimer::singleShot(duration, Qt::PreciseTimer, &loop, &QEventLoop::quit);
        });
        output << "Sampling " << sources.size() << " EVE sources, " << registrations.size()
               << " enabled regions; warmup " << warmup << " ms, measurement " << duration << " ms.\n";
        output.flush();
        loop.exec();
        coordinator.stop();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
        if (!logReady) {
            errors << logError << '\n';
            return 2;
        }
        const auto diagnostics = coordinator.diagnostics();
        const quint64 cpuEnd = processCpuNs();
        const auto performance = PerformanceRecorder::snapshot();
        const double elapsedMs = performance.value(QStringLiteral("interval_ms")).toDouble();
        const QJsonObject report{
            {QStringLiteral("build"), QStringLiteral("Release recommended")},
            {QStringLiteral("source_kind"), QStringLiteral("live_eve_clients")},
            {QStringLiteral("capture_backend"), parser.isSet(QStringLiteral("legacy-capture"))
                 ? QStringLiteral("legacy_bitmap_copy") : QStringLiteral("cached_dib_view")},
            {QStringLiteral("duration_ms"), duration},
            {QStringLiteral("warmup_ms"), warmup},
            {QStringLiteral("sources"), sourceInfo},
            {QStringLiteral("registrations"), registrations},
            {QStringLiteral("delivered_results"), static_cast<qint64>(deliveries)},
            {QStringLiteral("unavailable_results"), unavailable},
            {QStringLiteral("process_cpu_ms"), cpuStart != 0 && cpuEnd >= cpuStart
                 ? static_cast<double>(cpuEnd - cpuStart) / 1e6 : -1.0},
            {QStringLiteral("process_cpu_percent_of_one_core"), cpuStart != 0 && elapsedMs > 0 && cpuEnd >= cpuStart
                 ? static_cast<double>(cpuEnd - cpuStart) / 1e6 / elapsedMs * 100.0 : -1.0},
            {QStringLiteral("completed_batches"), static_cast<qint64>(diagnostics.completedBatches - warmupDiagnostics.completedBatches)},
            {QStringLiteral("failed_batches"), static_cast<qint64>(diagnostics.failedBatches - warmupDiagnostics.failedBatches)},
            {QStringLiteral("merged_captures"), static_cast<qint64>(diagnostics.mergedCaptures - warmupDiagnostics.mergedCaptures)},
            {QStringLiteral("skipped_intervals"), static_cast<qint64>(diagnostics.skippedIntervals - warmupDiagnostics.skippedIntervals)},
            {QStringLiteral("completed_batches_including_warmup"), static_cast<qint64>(diagnostics.completedBatches)},
            {QStringLiteral("failed_batches_including_warmup"), static_cast<qint64>(diagnostics.failedBatches)},
            {QStringLiteral("merged_captures_including_warmup"), static_cast<qint64>(diagnostics.mergedCaptures)},
            {QStringLiteral("skipped_intervals_including_warmup"), static_cast<qint64>(diagnostics.skippedIntervals)},
            {QStringLiteral("performance"), performance},
        };
        if (!PerformanceRecorder::flush(&logError)) {
            errors << logError << '\n';
            return 2;
        }
        QSaveFile file(reportPath);
        const auto bytes = QJsonDocument(report).toJson(QJsonDocument::Indented);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
            errors << file.errorString() << '\n';
            return 2;
        }
        output << "Report: " << reportPath << "; delivered " << deliveries
               << "; unavailable " << unavailable << '\n';
        return diagnostics.failedBatches == 0 && unavailable == 0 && deliveries != 0 ? 0 : 1;
    } catch (const std::exception& error) {
        errors << error.what() << '\n';
        return 1;
    }
}
