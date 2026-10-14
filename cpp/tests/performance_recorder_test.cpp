#include <thread>
#include <vector>

#include <QFile>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>

#include "runtime/performance_recorder.h"

class PerformanceRecorderTest final : public QObject {
    Q_OBJECT
private slots:
    void cleanup() { PerformanceRecorder::disable(); }

    void aggregatesConcurrentSamplesAndPreservesFailures()
    {
        QTemporaryDir directory(QDir::currentPath() + QStringLiteral("/performance-test-XXXXXX"));
        const QString path = directory.filePath(QStringLiteral("perf.jsonl"));
        QVERIFY(PerformanceRecorder::initialize(path));
        std::vector<std::thread> workers;
        for (int thread = 0; thread < 4; ++thread) {
            workers.emplace_back([] {
                for (int sample = 0; sample < 100; ++sample) {
                    PerformanceRecorder::record("scan", 1000000, 77, 10, sample == 0);
                }
            });
        }
        for (auto& worker : workers) { worker.join(); }
        const auto stages = PerformanceRecorder::snapshot().value(QStringLiteral("stages")).toArray();
        QCOMPARE(stages.size(), 1);
        const auto entry = stages[0].toObject();
        QCOMPARE(entry.value(QStringLiteral("count")).toInt(), 400);
        QCOMPARE(entry.value(QStringLiteral("failures")).toInt(), 4);
        QCOMPARE(entry.value(QStringLiteral("units")).toInt(), 4000);
        QCOMPARE(entry.value(QStringLiteral("total_ms")).toDouble(), 400.0);
        QCOMPARE(entry.value(QStringLiteral("p95_ms")).toDouble(), 1.0);
        QCOMPARE(entry.value(QStringLiteral("percentile_samples")).toInt(), 256);
        QVERIFY(PerformanceRecorder::flush());
        QVERIFY(PerformanceRecorder::snapshot().value(QStringLiteral("stages")).toArray().isEmpty());
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto report = QJsonDocument::fromJson(file.readLine()).object();
        QCOMPARE(report.value(QStringLiteral("stages")).toArray()[0].toObject(), entry);
    }

    void disabledRecordingDoesNotCreateSamples()
    {
        QTemporaryDir directory(QDir::currentPath() + QStringLiteral("/performance-test-XXXXXX"));
        QVERIFY(PerformanceRecorder::initialize(directory.filePath(QStringLiteral("perf.jsonl"))));
        PerformanceRecorder::disable();
        PerformanceRecorder::record("scan", 1000000, 77);
        QVERIFY(PerformanceRecorder::snapshot().value(QStringLiteral("stages")).toArray().isEmpty());
    }

    void percentilesDescribeTheMostRecentBoundedSamples()
    {
        QTemporaryDir directory(QDir::currentPath() + QStringLiteral("/performance-test-XXXXXX"));
        QVERIFY(PerformanceRecorder::initialize(directory.filePath(QStringLiteral("perf.jsonl"))));
        for (qint64 sample = 1; sample <= 400; ++sample) {
            PerformanceRecorder::record("scan", sample * 1000000, 77);
        }
        const auto entry = PerformanceRecorder::snapshot().value(QStringLiteral("stages")).toArray()[0].toObject();
        QCOMPARE(entry.value(QStringLiteral("p50_ms")).toDouble(), 272.0);
        QCOMPARE(entry.value(QStringLiteral("p95_ms")).toDouble(), 388.0);
        QCOMPARE(entry.value(QStringLiteral("total_ms")).toDouble(), 80200.0);
        QCOMPARE(entry.value(QStringLiteral("max_ms")).toDouble(), 400.0);
    }

    void rejectsUnwritableDestination()
    {
        QTemporaryDir directory(QDir::currentPath() + QStringLiteral("/performance-test-XXXXXX"));
        QString error;
        QVERIFY(!PerformanceRecorder::initialize(directory.path(), &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!PerformanceRecorder::enabled());
    }

    void keepsSamplesWhenFlushingFails()
    {
        QTemporaryDir directory(QDir::currentPath() + QStringLiteral("/performance-test-XXXXXX"));
        const QString path = directory.filePath(QStringLiteral("perf.jsonl"));
        QVERIFY(PerformanceRecorder::initialize(path));
        PerformanceRecorder::record("scan", 1000000, 77);
        QVERIFY(QFile::remove(path));
        QVERIFY(QDir().mkdir(path));
        QString error;
        QVERIFY(!PerformanceRecorder::flush(&error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(PerformanceRecorder::snapshot().value(QStringLiteral("stages")).toArray()[0]
                     .toObject().value(QStringLiteral("count")).toInt(), 1);
    }
};

QTEST_GUILESS_MAIN(PerformanceRecorderTest)
#include "performance_recorder_test.moc"
