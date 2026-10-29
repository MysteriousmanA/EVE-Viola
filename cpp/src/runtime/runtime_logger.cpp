#include "runtime/runtime_logger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QMutex>
#include <QMutexLocker>
#include <QUuid>

namespace {

struct LoggerState {
    QMutex mutex;
    QString directory;
    QFile file;
    QDate date;
    QByteArray pending;
};

constexpr qsizetype kBatchBytes = 64 * 1024;
constexpr qsizetype kMaxPendingBytes = 256 * 1024;

bool flushPending(LoggerState& state)
{
    if (state.pending.isEmpty()) {
        return true;
    }
    if (!state.file.isOpen()) {
        if (state.directory.isEmpty() || !QDir().mkpath(state.directory)) {
            return false;
        }
        state.file.setFileName(QDir(state.directory).filePath(
            QStringLiteral("eve-viola-%1.jsonl")
                .arg(state.date.toString(QStringLiteral("yyyy-MM-dd")))));
        if (!state.file.open(QIODevice::WriteOnly | QIODevice::Append)) {
            return false;
        }
    }
    const auto written = state.file.write(state.pending);
    if (written > 0) {
        state.pending.remove(0, written);
    }
    return state.file.flush() && state.pending.isEmpty();
}

LoggerState& loggerState()
{
    static LoggerState state;
    return state;
}

}  // namespace

void RuntimeLogger::initialize(const QString& logDirectory)
{
    auto& state = loggerState();
    const QMutexLocker lock(&state.mutex);
    static_cast<void>(flushPending(state));
    state.file.close();
    state.pending.clear();
    state.date = {};
    state.directory = logDirectory.isEmpty() ? QString{} : QDir(logDirectory).absolutePath();
}

bool RuntimeLogger::flush()
{
    auto& state = loggerState();
    const QMutexLocker lock(&state.mutex);
    return flushPending(state);
}

QString RuntimeLogger::writeEvent(
    const QString& eventType,
    QJsonObject fields)
{
    auto& state = loggerState();
    const QMutexLocker lock(&state.mutex);

    const QString eventId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    fields.insert(QStringLiteral("event"), eventType);
    fields.insert(QStringLiteral("event_id"), eventId);
    const auto now = QDateTime::currentDateTime();
    fields.insert(
        QStringLiteral("timestamp"),
        now.toString(Qt::ISODateWithMs));

    if (state.directory.isEmpty()) {
        return eventId;
    }
    if (state.date != now.date()) {
        if (!flushPending(state)) {
            return eventId;
        }
        state.file.close();
        state.date = now.date();
    }
    const auto line = QJsonDocument(fields).toJson(QJsonDocument::Compact) + '\n';
    // Logging is best effort. Bound memory if the destination is unavailable.
    if (state.pending.size() + line.size() > kMaxPendingBytes) {
        if (!flushPending(state)) {
            return eventId;
        }
        if (line.size() > kMaxPendingBytes) {
            return eventId;
        }
    }
    state.pending.append(line);
    if (state.pending.size() >= kBatchBytes) {
        static_cast<void>(flushPending(state));
    }
    return eventId;
}

QString RuntimeLogger::formatWindowHandle(std::uintptr_t hwnd)
{
    return QStringLiteral("0x%1")
        .arg(static_cast<qulonglong>(hwnd), 0, 16);
}
