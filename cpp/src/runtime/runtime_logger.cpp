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
};

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
    state.directory = QDir(logDirectory).absolutePath();
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
    fields.insert(
        QStringLiteral("timestamp"),
        QDateTime::currentDateTime().toString(Qt::ISODateWithMs));

    QString directory = state.directory;
    if (directory.isEmpty() || !QDir().mkpath(directory)) {
        return eventId;
    }

    const QString fileName = QStringLiteral("eve-viola-%1.jsonl")
                                 .arg(QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd")));
    QFile file(QDir(directory).filePath(fileName));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        return eventId;
    }

    file.write(QJsonDocument(fields).toJson(QJsonDocument::Compact));
    file.write("\n");
    return eventId;
}

QString RuntimeLogger::formatWindowHandle(std::uintptr_t hwnd)
{
    return QStringLiteral("0x%1")
        .arg(static_cast<qulonglong>(hwnd), 0, 16);
}
