#include <QtTest>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QJsonObject>
#include <QTimeZone>
#include "features/page_partition/page_partition_model.h"
#include "features/expedition_timing/expedition_timing_service.h"
#include "features/replica_list/statistics_workload_estimator.h"
#include "features/desktop_pet/desktop_pet_settings_store.h"

class PageFeatureServicesTest final : public QObject
{
    Q_OBJECT
  private slots:
    void identicalPetSettingsDoNotRewriteFile()
    {
        QTemporaryDir directory;
        DesktopPetSettingsStore store(directory.path());
        const QJsonObject settings{{QStringLiteral("enabled"), true}};
        QVERIFY(store.save(settings));
        QFile file(directory.filePath(QStringLiteral("desktopPet.json")));
        QVERIFY(file.open(QIODevice::ReadWrite));
        QVERIFY(file.setFileTime(QDateTime(QDate(2000, 1, 1), QTime(0, 0)),
                                 QFileDevice::FileModificationTime));
        const auto before = QFileInfo(file).lastModified();
        file.close();
        QVERIFY(store.save(settings));
        QCOMPARE(QFileInfo(file).lastModified(), before);
    }
    void undoLinesPreservesSettings()
    {
        PagePartitionModel model;
        PagePartitionLine first;
        first.id = QStringLiteral("first");
        PagePartitionLine second;
        second.id = QStringLiteral("second");
        model.addLine(first);
        model.addLine(second);
        model.moveLine(1, 0);
        model.setDefaultWidth(7);
        QVERIFY(model.undo());
        QCOMPARE(model.config().lines.front().id, first.id);
        QCOMPARE(model.config().defaultWidth, 7);
        model.removeLine(0);
        QVERIFY(model.undo());
        QCOMPARE(model.config().lines.size(), 2);
        model.clearLines();
        QVERIFY(model.config().lines.isEmpty());
        QVERIFY(model.undo());
        QCOMPARE(model.config().lines.size(), 2);
    }
    void invalidMovesDoNotCreateUndoHistory()
    {
        PagePartitionModel model;
        model.moveLine(-1, 0);
        model.removeLine(0);
        model.updateLine(0, {});
        QVERIFY(!model.canUndo());
        QVERIFY(!model.undo());
    }
    void undoHistoryIsBounded()
    {
        PagePartitionModel model;
        for (int i = 0; i < 25; ++i) {
            PagePartitionLine line;
            line.id = QString::number(i);
            model.addLine(line);
        }
        for (int i = 0; i < 20; ++i) {
            QVERIFY(model.undo());
        }
        QVERIFY(!model.undo());
        QCOMPARE(model.config().lines.size(), 5);
    }
    void expiryCrossesYearAndSupportsNegativeOffsets()
    {
        const QDateTime start(QDate(2026, 12, 31), QTime(23, 50), QTimeZone::UTC);
        QCOMPARE(ExpeditionTimingService::expiryText(start, {0, 0, 20}, {}, {}),
                 QStringLiteral("兔子1-10 1月1日0时10分过期"));
        QCOMPARE(ExpeditionTimingService::expiryText(start, {-1, 0, -10}, QStringLiteral("天蛇"),
                                                     QStringLiteral("三无畏")),
                 QStringLiteral("天蛇三无畏 12月30日23时40分过期"));
        QCOMPARE(ExpeditionTimingService::defaultDuration(QStringLiteral("三无畏")).days, 2);
        QCOMPARE(ExpeditionTimingService::defaultDuration(QStringLiteral("1-10")).days, 0);
    }
    void sameSourceCapturesAndRegionsAreMerged()
    {
        const QRect region(0, 0, 10, 10);
        const auto merged = estimateStatisticsWorkload({{1, QSize(100, 100), region, 100},
                                                        {1, QSize(100, 100), region, 200},
                                                        {1, QSize(100, 100), region, 100}});
        QCOMPARE(merged.capturePixelsPerSecond, 100000.0);
        QCOMPARE(merged.scanPixelsPerSecond, 1000.0);
        const auto distinct = estimateStatisticsWorkload(
            {{1, QSize(100, 100), region, 100}, {1, QSize(100, 100), QRect(20, 20, 10, 10), 100}});
        QCOMPARE(distinct.capturePixelsPerSecond, 100000.0);
        QCOMPARE(distinct.scanPixelsPerSecond, 2000.0);
        const auto unknown = estimateStatisticsWorkload({{1, {}, std::nullopt, 100}});
        QCOMPARE(unknown.capturePixelsPerSecond, -1.0);
        QCOMPARE(unknown.scanPixelsPerSecond, -1.0);
    }
    void corruptPetSettingsAreNeverOverwritten()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("desktopPet.json"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray corrupt("{broken");
        QCOMPARE(file.write(corrupt), corrupt.size());
        file.close();
        DesktopPetSettingsStore store(directory.path());
        const auto loaded = store.load();
        QVERIFY(loaded.parseFailed);
        QVERIFY(!loaded.writable);
        QVERIFY(!store.save({{QStringLiteral("enabled"), true}}));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), corrupt);
    }
    void petSettingsAndAvatarRoundTrip()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        DesktopPetSettingsStore store(directory.path());
        const QJsonObject settings{{QStringLiteral("enabled"), true},
                                   {QStringLiteral("triggerMessages"),
                                    QJsonObject{{QStringLiteral("3"), QStringLiteral("提醒")}}}};
        QVERIFY(store.save(settings));
        QCOMPARE(store.load().settings, settings);
        QImage image(10, 12, QImage::Format_ARGB32);
        image.fill(Qt::red);
        QVERIFY(store.saveSourceImage(image));
        const auto restored = store.loadSourceImage();
        QCOMPARE(restored.size(), image.size());
        QCOMPARE(restored.pixelColor(0, 0), QColor(Qt::red));
    }
};
QTEST_GUILESS_MAIN(PageFeatureServicesTest)
#include "page_feature_services_test.moc"
