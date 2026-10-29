#include <QtTest>

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QTemporaryDir>

#include "config/app_config.h"

class AppConfigTest final : public QObject {
    Q_OBJECT

private slots:
    void statisticsColorsAndRuleToleranceRoundTripInReplicasAndPresets()
    {
        QTemporaryDir directory;
        JsonConfigStore store(directory.path());
        AppConfig expected;
        ReplicaConfig replica;
        replica.statisticsColors = {{QStringLiteral("蓝色")}, {QStringLiteral("#abcdef"), 15}};
        expected.replicas.append(replica);
        expected.presets[0].hasConfig = true;
        expected.presets[0].replicas = expected.replicas;
        ColorRule rule;
        rule.number = 1;
        rule.triggerCondition1 = {QStringLiteral("#abcdef"), QStringLiteral(">=2"), 15};
        rule.triggerCondition2 = {QStringLiteral("蓝色"), QStringLiteral("<10"), 0};
        rule.triggerConditionRelation = TriggerConditionRelation::Or;
        expected.colorRules.append(rule);
        QString error;
        QVERIFY2(store.save(expected, &error), qPrintable(error));
        AppConfig actual;
        QVERIFY2(store.load(actual, &error), qPrintable(error));
        QCOMPARE(actual.replicas, expected.replicas);
        QCOMPARE(actual.presets[0], expected.presets[0]);
        QCOMPARE(actual.colorRules, expected.colorRules);
    }

    void legacyAndInvalidStatisticsColorsUseRedWhiteBlueDefaults()
    {
        QTemporaryDir directory;
        QFile file(directory.filePath(QStringLiteral("replicas.json")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"({"replicas":[{}, {"statisticsColors":[]},
            {"statisticsColors":[{"color":"bad"}]}]})");
        file.close();
        AppConfig config;
        QString error;
        QVERIFY2(JsonConfigStore(directory.path()).load(config, &error), qPrintable(error));
        QCOMPARE(config.replicas.size(), 3);
        for (const auto& replica : config.replicas) {
            QVERIFY(replica.statisticsColors == defaultPixelColorTargets());
        }
    }

    void saveRejectsCorruptLaterSectionBeforeWritingAnyOtherFile()
    {
        QTemporaryDir directory;
        JsonConfigStore store(directory.path());
        AppConfig config;
        QString error;
        QVERIFY2(store.save(config, &error), qPrintable(error));
        QFile intact(directory.filePath(QStringLiteral("regionPresets.json")));
        QVERIFY(intact.open(QIODevice::ReadOnly));
        const auto before = intact.readAll();
        intact.close();
        QFile corrupt(directory.filePath(QStringLiteral("preset3.json")));
        QVERIFY(corrupt.open(QIODevice::WriteOnly | QIODevice::Truncate));
        const QByteArray broken("{broken");
        QCOMPARE(corrupt.write(broken), broken.size());
        corrupt.close();
        config.regionPresets.append({QStringLiteral("new"), QRect(1, 1, 20, 20), 1000});
        QVERIFY(!store.save(config, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(intact.open(QIODevice::ReadOnly));
        QCOMPARE(intact.readAll(), before);
        QVERIFY(corrupt.open(QIODevice::ReadOnly));
        QCOMPARE(corrupt.readAll(), broken);
    }
    void savingGeometryDoesNotRewriteUnchangedSections()
    {
        QTemporaryDir directory;
        JsonConfigStore store(directory.path());
        AppConfig config;
        config.replicas.append(ReplicaConfig{});
        QString error;
        QVERIFY2(store.save(config, &error), qPrintable(error));
        QFile rules(directory.filePath(QStringLiteral("colorRules.json")));
        QVERIFY(rules.open(QIODevice::ReadWrite));
        QVERIFY(rules.setFileTime(QDateTime(QDate(2000, 1, 1), QTime(0, 0)), QFileDevice::FileModificationTime));
        const auto originalTime = QFileInfo(rules).lastModified();
        rules.close();
        config.replicas.first().position = QPoint(123, 456);
        QVERIFY2(store.save(config, &error), qPrintable(error));
        QCOMPARE(QFileInfo(rules).lastModified(), originalTime);
        AppConfig actual;
        QVERIFY(store.load(actual, &error));
        QCOMPARE(actual.replicas.first().position, QPoint(123, 456));
    }
    void pagePartitionConfigRoundTripsWithNegativeCoordinates()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        JsonConfigStore store(directory.path());
        AppConfig expected;
        expected.pagePartition.defaultColor = QColor(QStringLiteral("#123456"));
        expected.pagePartition.selectedColor = QColor(QStringLiteral("#ff00aa"));
        expected.pagePartition.defaultWidth = 5;
        expected.pagePartition.visible = false;
        expected.pagePartition.lines.append({
            QStringLiteral("partition-1"), QPoint(-1920, 120), QPoint(-200, 120),
            QColor(QStringLiteral("#11bbdd")), 4, QStringLiteral("左屏顶部")});

        QString error;
        QVERIFY2(store.save(expected, &error), qPrintable(error));
        QVERIFY(QFileInfo::exists(
            directory.filePath(QStringLiteral("pagePartition.json"))));

        AppConfig actual;
        QVERIFY2(store.load(actual, &error), qPrintable(error));
        QCOMPARE(actual.pagePartition, expected.pagePartition);
    }

    void savesAndLoadsReplicaConfigs()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.path();
        JsonConfigStore store(path);

        AppConfig expected;
        expected.replicas.append({
            QStringLiteral("EVE Online - Alpha"),
            QRect(120, 80, 640, 480),
            1500,
            {1, 2},
            true,
            2,
            true,
            false,
            false,
            QPoint(300, 200),
            QSize(720, 480),
            QColor(255, 0, 0),
            QColor(0, 0, 255),
        });

        QString error;
        QVERIFY2(store.save(expected, &error), qPrintable(error));

        AppConfig actual;
        QVERIFY2(store.load(actual, &error), qPrintable(error));
        QCOMPARE(actual.replicas, expected.replicas);
    }

    void savesAndLoadsPresetSlots()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.path();
        JsonConfigStore store(path);

        AppConfig expected;
        expected.presets[0].name = QStringLiteral("PVE");
        expected.presets[0].hasConfig = true;
        expected.presets[0].replicas.append({
            QStringLiteral("EVE - Alpha"),
            QRect{},
            1500,
            {0, 0},
            false,
            1,
            false,
            true,
            false,
            QPoint(10, 10),
            QSize(360, 240),
            QColor(46, 204, 64),
            QColor(41, 121, 255),
        });
        expected.presets[1].name = QStringLiteral("PVP");
        expected.presets[1].hasConfig = false;
        expected.defaultPresetSlot = 1;

        QString error;
        QVERIFY2(store.save(expected, &error), qPrintable(error));

        AppConfig actual;
        QVERIFY2(store.load(actual, &error), qPrintable(error));
        QCOMPARE(actual.presets, expected.presets);
        QCOMPARE(actual.defaultPresetSlot, 1);
    }

    void presetFilesOnlyStoreReplicaConfiguration()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        JsonConfigStore store(directory.path());

        AppConfig config;
        config.regionPresets.append({QStringLiteral("全局区域"), QRect(1, 2, 30, 40)});
        config.colorRules.append({
            1, QStringLiteral("全局规则"), QStringLiteral("红色"), QStringLiteral(">=20"),
            TriggerFeedback::None, QString{}, PostTriggerAction::Loop});
        config.presets[0].name = QStringLiteral("首页预设");
        config.presets[0].hasConfig = true;

        QString error;
        QVERIFY2(store.save(config, &error), qPrintable(error));

        QFile file(directory.filePath(QStringLiteral("preset1.json")));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray json = file.readAll();
        QVERIFY(!json.contains("regionPresets"));
        QVERIFY(!json.contains("colorRules"));
        QVERIFY(!json.contains("nextColorRuleNumber"));
        QVERIFY(json.contains("replicas"));
    }

    void savesSplitSectionFilesInCacheDirectory()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.path();
        JsonConfigStore store(path);
        AppConfig config;
        config.regionPresets.append({QStringLiteral("总览"), QRect(1, 2, 30, 40)});
        QString error;
        QVERIFY2(store.save(config, &error), qPrintable(error));

        QVERIFY(QFileInfo::exists(path + QStringLiteral("/regionPresets.json")));
        QVERIFY(QFileInfo::exists(path + QStringLiteral("/colorRules.json")));
        QVERIFY(QFileInfo::exists(path + QStringLiteral("/replicas.json")));
        QVERIFY(QFileInfo::exists(path + QStringLiteral("/preset1.json")));
        QVERIFY(QFileInfo::exists(path + QStringLiteral("/preset2.json")));
        QVERIFY(QFileInfo::exists(path + QStringLiteral("/preset3.json")));
    }

    void loadsLegacyGlobalSectionsEmbeddedInPresetFile()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.path();
        QFile file(path + QStringLiteral("/preset1.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write(R"json({
            "colorRules": [{
                "number": 7,
                "note": "旧规则",
                "triggerPixel": "红色",
                "triggerCondition": ">=20",
                "triggerFeedback": "弹窗",
                "conditionLimit": "跳转至状态"
            }],
            "hasConfig": true,
            "name": "旧预设",
            "nextColorRuleNumber": 8,
            "regionPresets": [{
                "name": "旧区域",
                "x": 1,
                "y": 2,
                "width": 30,
                "height": 40
            }],
            "replicas": []
        })json") > 0);
        file.close();

        JsonConfigStore store(path);
        AppConfig config;
        QString error;
        QVERIFY2(store.load(config, &error), qPrintable(error));
        QCOMPARE(config.regionPresets.size(), 1);
        QCOMPARE(config.regionPresets.first().name, QStringLiteral("旧区域"));
        QCOMPARE(config.colorRules.size(), 1);
        QCOMPARE(config.colorRules.first().number, 7);
        QCOMPARE(config.nextColorRuleNumber, 8);
        QCOMPARE(config.presets[0].name, QStringLiteral("旧预设"));
    }

    void keepInTraySettingSurvivesJsonRoundTrip()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.path();
        JsonConfigStore store(path);
        AppConfig config;
        config.keepInTray = true;
        QString error;
        QVERIFY2(store.save(config, &error), qPrintable(error));

        AppConfig actual;
        QVERIFY2(store.load(actual, &error), qPrintable(error));
        QVERIFY(actual.keepInTray);
    }

    void raiseReplicaOnClickDefaultsEnabledAndSurvivesJsonRoundTrip()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        JsonConfigStore store(directory.path());
        QString error;

        AppConfig defaults;
        QVERIFY(defaults.raiseReplicaOnClick);
        defaults.raiseReplicaOnClick = false;
        QVERIFY2(store.save(defaults, &error), qPrintable(error));

        AppConfig actual;
        QVERIFY2(store.load(actual, &error), qPrintable(error));
        QVERIFY(!actual.raiseReplicaOnClick);

        QFile file(directory.filePath(QStringLiteral("replicas.json")));
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QVERIFY(file.write(R"json({})json") > 0);
        file.close();

        QVERIFY2(store.load(actual, &error), qPrintable(error));
        QVERIFY(actual.raiseReplicaOnClick);
    }

    void colorTriggerSourceActivationDefaultsEnabledAndSurvivesJsonRoundTrip()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        JsonConfigStore store(directory.path());
        QString error;

        AppConfig defaults;
        QVERIFY(defaults.activateSourceOnColorLogicTrigger);
        defaults.activateSourceOnColorLogicTrigger = false;
        QVERIFY2(store.save(defaults, &error), qPrintable(error));

        AppConfig actual;
        QVERIFY2(store.load(actual, &error), qPrintable(error));
        QVERIFY(!actual.activateSourceOnColorLogicTrigger);

        QFile file(directory.filePath(QStringLiteral("replicas.json")));
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QVERIFY(file.write(R"json({})json") > 0);
        file.close();

        QVERIFY2(store.load(actual, &error), qPrintable(error));
        QVERIFY(actual.activateSourceOnColorLogicTrigger);
    }

    void automaticActivationCooldownDefaultsToFiveSecondsInJson()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        JsonConfigStore store(directory.path());
        QString error;

        QVERIFY2(store.save(AppConfig{}, &error), qPrintable(error));

        QFile file(directory.filePath(QStringLiteral("replicas.json")));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto root = QJsonDocument::fromJson(file.readAll()).object();
        QCOMPARE(root.value(QStringLiteral("automaticActivationCooldownMs")).toInt(), 5000);
    }

    void automaticActivationCooldownRoundTripsAndRejectsInvalidValues()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        JsonConfigStore store(directory.path());
        AppConfig expected;
        expected.automaticActivationCooldownMs = 7500;
        QString error;

        QVERIFY2(store.save(expected, &error), qPrintable(error));
        AppConfig actual;
        QVERIFY2(store.load(actual, &error), qPrintable(error));
        QCOMPARE(actual.automaticActivationCooldownMs, 7500);

        QFile file(directory.filePath(QStringLiteral("replicas.json")));
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QVERIFY(file.write(R"json({"automaticActivationCooldownMs":1234})json") > 0);
        file.close();

        QVERIFY2(store.load(actual, &error), qPrintable(error));
        QCOMPARE(actual.automaticActivationCooldownMs, 5000);
    }

    void restoreWindowsWithGameSettingSurvivesJsonRoundTrip()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        JsonConfigStore store(directory.path());
        AppConfig config;
        config.restoreWindowsWithGame = true;
        QString error;
        QVERIFY2(store.save(config, &error), qPrintable(error));

        AppConfig actual;
        QVERIFY2(store.load(actual, &error), qPrintable(error));
        QVERIFY(actual.restoreWindowsWithGame);
    }

    void savesAndLoadsRegionPresetsAndColorRules()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.path();
        JsonConfigStore store(path);

        AppConfig expected;
        expected.regionPresets.append({QStringLiteral("扫描窗口"), QRect(10, 20, 300, 180)});
        expected.regionPresets.append({QStringLiteral("阈值窗口"), QRect(5, 8, 100, 60), 1500});
        expected.colorRules.append({
            7,
            QStringLiteral("护盾告警"),
            QStringLiteral("红色"),
            QStringLiteral(">=20&<=80"),
            TriggerFeedback::Popup,
            QString{},
            PostTriggerAction::Jump,
        });

        QString error;
        QVERIFY2(store.save(expected, &error), qPrintable(error));

        AppConfig actual;
        QVERIFY2(store.load(actual, &error), qPrintable(error));
        QCOMPARE(actual.regionPresets, expected.regionPresets);
        QCOMPARE(actual.colorRules, expected.colorRules);
    }

    void savesAndLoadsTwoOptionalColorRuleConditions()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        JsonConfigStore store(directory.path());

        AppConfig expected;
        ColorRule rule;
        rule.number = 8;
        rule.note = QStringLiteral("双条件");
        rule.triggerCondition1 = {
            QStringLiteral("红色"), QStringLiteral(">=20")};
        rule.triggerCondition2 = {
            QStringLiteral("白色"), QStringLiteral("<=5")};
        expected.colorRules.append(rule);

        QString error;
        QVERIFY2(store.save(expected, &error), qPrintable(error));

        AppConfig actual;
        QVERIFY2(store.load(actual, &error), qPrintable(error));
        QCOMPARE(actual.colorRules, expected.colorRules);
    }

    void missingFileLoadsAsEmptyConfiguration()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        JsonConfigStore store(directory.filePath(QStringLiteral("missing-cache")));

        AppConfig config;
        config.regionPresets.append({QStringLiteral("旧数据"), QRect(1, 2, 3, 4)});
        QString error;

        QVERIFY2(store.load(config, &error), qPrintable(error));
        QVERIFY(config.regionPresets.isEmpty());
        QVERIFY(config.colorRules.isEmpty());
    }

    void legacyRegionPresetWithoutIntervalDefaultsToOneAndAHalfSeconds()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.path();
        QFile file(path + QStringLiteral("/regionPresets.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{\"regionPresets\":[{\"name\":\"旧预设\",\"x\":1,\"y\":2,\"width\":3,\"height\":4}]}");
        file.close();

        JsonConfigStore store(path);
        AppConfig config;
        QString error;
        QVERIFY2(store.load(config, &error), qPrintable(error));
        QCOMPARE(config.regionPresets.size(), 1);
        QCOMPARE(config.regionPresets.first().statisticsIntervalMs, kDefaultStatisticsIntervalMs);
    }

    void invalidRegionPresetIntervalFallsBackToOneAndAHalfSeconds()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.path();
        QFile file(path + QStringLiteral("/regionPresets.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{\"regionPresets\":[{\"name\":\"坏间隔\",\"x\":1,\"y\":2,\"width\":3,\"height\":4,"
                   "\"statisticsIntervalMs\":1234}]}");
        file.close();

        JsonConfigStore store(path);
        AppConfig config;
        QString error;
        QVERIFY2(store.load(config, &error), qPrintable(error));
        QCOMPARE(config.regionPresets.size(), 1);
        QCOMPARE(config.regionPresets.first().statisticsIntervalMs, kDefaultStatisticsIntervalMs);
    }

    void malformedJsonIsRejectedWithoutReplacingCurrentConfiguration()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.path();
        QFile file(path + QStringLiteral("/regionPresets.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("{ definitely not json"), qint64{21});
        file.close();

        JsonConfigStore store(path);
        AppConfig config;
        config.regionPresets.append({QStringLiteral("保留"), QRect(4, 5, 6, 7)});
        QString error;

        QVERIFY(!store.load(config, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(config.regionPresets.size(), 1);
        QCOMPARE(config.regionPresets.first().name, QStringLiteral("保留"));
    }

    void regionPresetCrudRejectsDuplicateNames()
    {
        AppConfig config;
        QVERIFY(addRegionPreset(config, {QStringLiteral("采矿"), QRect(1, 2, 30, 40)}));
        QVERIFY(!addRegionPreset(config, {QStringLiteral("采矿"), QRect(5, 6, 70, 80)}));
        QVERIFY(addRegionPreset(config, {QStringLiteral("战斗"), QRect(9, 10, 110, 120)}));

        QVERIFY(!renameRegionPreset(config, 1, QStringLiteral("采矿")));
        QVERIFY(renameRegionPreset(config, 1, QStringLiteral("PVP")));
        QCOMPARE(config.regionPresets[1].name, QStringLiteral("PVP"));

        QVERIFY(removeRegionPreset(config, 0));
        QCOMPARE(config.regionPresets.size(), 1);
        QCOMPARE(config.regionPresets.first().name, QStringLiteral("PVP"));
    }

    void colorRuleCrudAssignsStableIncreasingNumbers()
    {
        AppConfig config;
        ColorRule first{
            0, QStringLiteral("A"), QStringLiteral("红色"), QStringLiteral(">=10"),
            TriggerFeedback::Popup, QString{}, PostTriggerAction::Loop};
        ColorRule second{
            0, QStringLiteral("B"), QStringLiteral("白色"), QStringLiteral("<5"),
            TriggerFeedback::None, QString{}, PostTriggerAction::Jump};

        QCOMPARE(addColorRule(config, first), 1);
        QCOMPARE(addColorRule(config, second), 2);
        QVERIFY(removeColorRule(config, 1));
        QCOMPARE(addColorRule(config, first), 3);

        second.note = QStringLiteral("已编辑");
        QVERIFY(updateColorRule(config, 2, second));
        QCOMPARE(config.colorRules.first().number, 2);
        QCOMPARE(config.colorRules.first().note, QStringLiteral("已编辑"));
        QVERIFY(!updateColorRule(config, 99, second));

        QVERIFY(removeColorRule(config, 3));
        QCOMPARE(addColorRule(config, first), 4);
    }

    void nextColorRuleNumberSurvivesJsonRoundTrip()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        JsonConfigStore store(directory.path());
        AppConfig config;
        ColorRule rule{0, QStringLiteral("A"), {}, {}, TriggerFeedback::None, {}, PostTriggerAction::Loop};
        QCOMPARE(addColorRule(config, rule), 1);
        QVERIFY(removeColorRule(config, 1));
        QString error;
        QVERIFY2(store.save(config, &error), qPrintable(error));

        AppConfig loaded;
        QVERIFY2(store.load(loaded, &error), qPrintable(error));
        QCOMPARE(addColorRule(loaded, rule), 2);
    }

    void deletingMaximumColorRuleNumberIsNotReusedAfterJsonRoundTrip()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        JsonConfigStore store(directory.path());
        AppConfig config;
        ColorRule rule{0, QStringLiteral("A"), {}, {}, TriggerFeedback::None, {}, PostTriggerAction::Loop};
        QCOMPARE(addColorRule(config, rule), 1);
        QCOMPARE(addColorRule(config, rule), 2);
        QCOMPARE(addColorRule(config, rule), 3);
        QVERIFY(removeColorRule(config, 3));
        QString error;
        QVERIFY2(store.save(config, &error), qPrintable(error));

        QFile file(directory.path() + QStringLiteral("/colorRules.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(file.readAll().contains("\"nextColorRuleNumber\": 4"));

        AppConfig loaded;
        QVERIFY2(store.load(loaded, &error), qPrintable(error));
        QCOMPARE(addColorRule(loaded, rule), 4);
    }

    void triggerConditionParsesSingleAndRangeBounds()
    {
        const auto none = parseTriggerCondition(QStringLiteral(""));
        QVERIFY(none.has_value());
        QVERIFY(none->isEmpty());

        const auto lower = parseTriggerCondition(QStringLiteral(">=20"));
        QVERIFY(lower.has_value());
        QCOMPARE(lower->lowerOperator, QStringLiteral(">="));
        QCOMPARE(lower->lowerValue, 20.0);
        QCOMPARE(lower->toString(), QStringLiteral(">=20"));

        const auto range = parseTriggerCondition(QStringLiteral(">15&<60"));
        QVERIFY(range.has_value());
        QCOMPARE(range->lowerOperator, QStringLiteral(">"));
        QCOMPARE(range->lowerValue, 15.0);
        QCOMPARE(range->upperOperator, QStringLiteral("<"));
        QCOMPARE(range->upperValue, 60.0);
        QCOMPARE(range->toString(), QStringLiteral(">15&<60"));

        const auto upperOnly = parseTriggerCondition(QStringLiteral("<=50"));
        QVERIFY(upperOnly.has_value());
        QVERIFY(upperOnly->lowerOperator.isEmpty());
        QCOMPARE(upperOnly->upperOperator, QStringLiteral("<="));
        QCOMPARE(upperOnly->upperValue, 50.0);
        QCOMPARE(upperOnly->toString(), QStringLiteral("<=50"));

        const auto chinese = parseTriggerCondition(QStringLiteral("≥20 且 ≤80"));
        QVERIFY(chinese.has_value());
        QCOMPARE(chinese->toString(), QStringLiteral(">=20&<=80"));

        const auto maxBound = parseTriggerCondition(
            QStringLiteral(">=%1").arg(kColorRulePixelValueMaximum));
        QVERIFY(maxBound.has_value());
        QCOMPARE(maxBound->lowerValue, static_cast<double>(kColorRulePixelValueMaximum));

        QVERIFY(!parseTriggerCondition(QStringLiteral("占比 ≥ 20%")).has_value());
        QVERIFY(!parseTriggerCondition(QStringLiteral(">10&<30&<50")).has_value());
        QVERIFY(!parseTriggerCondition(QStringLiteral(">=5001")).has_value());
        QVERIFY(!parseTriggerCondition(QStringLiteral(">5001&<6000")).has_value());
    }

    void feedbackAndActionNamesRoundTrip()
    {
        QCOMPARE(triggerFeedbackName(TriggerFeedback::None), QStringLiteral("无"));
        QCOMPARE(triggerFeedbackName(TriggerFeedback::Popup), QStringLiteral("弹窗"));
        QCOMPARE(triggerFeedbackName(TriggerFeedback::Audio), QStringLiteral("播放音频"));
        QCOMPARE(
            triggerFeedbackFromName(QStringLiteral("播放音频")).value_or(TriggerFeedback::None),
            TriggerFeedback::Audio);
        QVERIFY(!triggerFeedbackFromName(QStringLiteral("声音")).has_value());
        QCOMPARE(postTriggerActionName(PostTriggerAction::Loop), QStringLiteral("正向循环"));
        QCOMPARE(postTriggerActionName(PostTriggerAction::Jump), QStringLiteral("跳转至另一状态"));
        QCOMPARE(postTriggerActionName(PostTriggerAction::ReverseWait), QStringLiteral("反向等待"));
        QCOMPARE(
            postTriggerActionFromName(QStringLiteral("跳转至另一状态")).value_or(PostTriggerAction::Loop),
            PostTriggerAction::Jump);
        QCOMPARE(
            postTriggerActionFromName(QStringLiteral("跳转至状态")).value_or(PostTriggerAction::Loop),
            PostTriggerAction::Jump);
        QCOMPARE(
            postTriggerActionFromName(QStringLiteral("正向循环")).value_or(PostTriggerAction::Jump),
            PostTriggerAction::Loop);
        QCOMPARE(
            postTriggerActionFromName(QStringLiteral("恢复原状态")).value_or(PostTriggerAction::Jump),
            PostTriggerAction::Loop);
        QCOMPARE(
            postTriggerActionFromName(QStringLiteral("反向等待")).value_or(PostTriggerAction::Jump),
            PostTriggerAction::ReverseWait);
    }

    void evaluateTriggerConditionAppliesBounds()
    {
        QVERIFY(evaluateTriggerCondition(*parseTriggerCondition(QStringLiteral(">=50")), 60.0));
        QVERIFY(!evaluateTriggerCondition(*parseTriggerCondition(QStringLiteral(">=50")), 40.0));
        QVERIFY(evaluateTriggerCondition(*parseTriggerCondition(QStringLiteral(">15&<60")), 30.0));
        QVERIFY(!evaluateTriggerCondition(*parseTriggerCondition(QStringLiteral(">15&<60")), 70.0));
        QVERIFY(!evaluateTriggerCondition(*parseTriggerCondition(QStringLiteral(">15&<60")), 15.0));
        QVERIFY(evaluateTriggerCondition(*parseTriggerCondition(QStringLiteral("<=20")), 20.0));
        QVERIFY(!evaluateTriggerCondition(*parseTriggerCondition(QStringLiteral("<=20")), 21.0));
        QVERIFY(!evaluateTriggerCondition(TriggerCondition{}, 50.0));
    }

    void legacyColorRuleFieldsAreMigratedOnLoad()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.path();
        QFile file(path + QStringLiteral("/colorRules.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{\"colorRules\":[{\"number\":5,\"note\":\"旧规则\",\"triggerPixel\":\"红色\","
                   "\"triggerCondition\":\">=20\",\"triggerFeedback\":\"弹窗\","
                   "\"conditionLimit\":\"跳转至状态\"}]}");
        file.close();

        JsonConfigStore store(path);
        AppConfig config;
        QString error;
        QVERIFY2(store.load(config, &error), qPrintable(error));
        QCOMPARE(config.colorRules.size(), 1);
        const auto& rule = config.colorRules.first();
        QCOMPARE(rule.number, 5);
        QCOMPARE(rule.triggerCondition1.pixel, QStringLiteral("红色"));
        QCOMPARE(rule.triggerCondition1.expression, QStringLiteral(">=20"));
        QVERIFY(rule.triggerCondition2.isEmpty());
        QCOMPARE(rule.triggerFeedback, TriggerFeedback::Popup);
        QCOMPARE(rule.postTriggerAction, PostTriggerAction::Jump);
    }
};

QTEST_MAIN(AppConfigTest)

#include "app_config_test.moc"
