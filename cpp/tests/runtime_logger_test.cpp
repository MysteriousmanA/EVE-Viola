#include <QtTest>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <thread>
#include <vector>

#include "runtime/runtime_logger.h"

class RuntimeLoggerTest final : public QObject {
    Q_OBJECT

    static QList<QByteArray> lines(const QString& directory)
    {
        QList<QByteArray> result;
        for (const auto& name : QDir(directory).entryList({QStringLiteral("*.jsonl")}, QDir::Files)) {
            QFile file(QDir(directory).filePath(name));
            if (file.open(QIODevice::ReadOnly)) {
                for (const auto& line : file.readAll().split('\n')) {
                    if (!line.isEmpty()) { result.append(line); }
                }
            }
        }
        return result;
    }

private slots:
    void cleanup()
    {
        RuntimeLogger::initialize({});
    }

    void smallBatchStaysInMemoryUntilFlush()
    {
        QTemporaryDir directory;
        const auto close = qScopeGuard([] { RuntimeLogger::initialize({}); });
        RuntimeLogger::initialize(directory.path());
        const auto id = RuntimeLogger::writeEvent(QStringLiteral("test"), {{QStringLiteral("value"), 42}});
        QVERIFY(lines(directory.path()).isEmpty());
        QVERIFY(RuntimeLogger::flush());
        const auto saved = lines(directory.path());
        QCOMPARE(saved.size(), 1);
        const auto event = QJsonDocument::fromJson(saved.first()).object();
        QCOMPARE(event.value(QStringLiteral("event_id")).toString(), id);
        QCOMPARE(event.value(QStringLiteral("value")).toInt(), 42);
        QVERIFY(RuntimeLogger::flush());
        QCOMPARE(lines(directory.path()).size(), 1);
    }

    void thresholdAndConcurrentEventsPreserveEveryLine()
    {
        QTemporaryDir directory;
        const auto close = qScopeGuard([] { RuntimeLogger::initialize({}); });
        RuntimeLogger::initialize(directory.path());
        std::vector<std::thread> writers;
        for (int thread = 0; thread < 4; ++thread) {
            writers.emplace_back([thread] {
                for (int sequence = 0; sequence < 200; ++sequence) {
                    RuntimeLogger::writeEvent(QStringLiteral("concurrent"), {
                        {QStringLiteral("writer"), thread},
                        {QStringLiteral("sequence"), sequence},
                        {QStringLiteral("payload"), QString(200, QLatin1Char('x'))}});
                }
            });
        }
        for (auto& writer : writers) { writer.join(); }
        QVERIFY(!lines(directory.path()).isEmpty());
        QVERIFY(RuntimeLogger::flush());
        const auto saved = lines(directory.path());
        QCOMPARE(saved.size(), 800);
        QSet<QString> ids;
        QSet<QString> events;
        for (const auto& line : saved) {
            QJsonParseError error;
            const auto document = QJsonDocument::fromJson(line, &error);
            QCOMPARE(error.error, QJsonParseError::NoError);
            const auto event = document.object();
            ids.insert(event.value(QStringLiteral("event_id")).toString());
            events.insert(QStringLiteral("%1:%2")
                .arg(event.value(QStringLiteral("writer")).toInt())
                .arg(event.value(QStringLiteral("sequence")).toInt()));
        }
        QCOMPARE(ids.size(), 800);
        QCOMPARE(events.size(), 800);
    }

    void reinitializeFlushesPreviousDirectory()
    {
        QTemporaryDir first;
        QTemporaryDir second;
        const auto close = qScopeGuard([] { RuntimeLogger::initialize({}); });
        RuntimeLogger::initialize(first.path());
        RuntimeLogger::writeEvent(QStringLiteral("first"));
        RuntimeLogger::initialize(second.path());
        QCOMPARE(lines(first.path()).size(), 1);
        RuntimeLogger::writeEvent(QStringLiteral("second"));
        QVERIFY(RuntimeLogger::flush());
        QCOMPARE(lines(second.path()).size(), 1);
    }

    void failedDestinationCanRecoverWithoutLosingPendingEvents()
    {
        QTemporaryDir directory;
        const auto close = qScopeGuard([] { RuntimeLogger::initialize({}); });
        const auto destination = directory.filePath(QStringLiteral("blocked"));
        QFile blocker(destination);
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        RuntimeLogger::initialize(destination);
        RuntimeLogger::writeEvent(QStringLiteral("retry"));
        QVERIFY(!RuntimeLogger::flush());
        QVERIFY(blocker.remove());
        QVERIFY(RuntimeLogger::flush());
        QCOMPARE(lines(destination).size(), 1);
    }
};

QTEST_GUILESS_MAIN(RuntimeLoggerTest)
#include "runtime_logger_test.moc"
