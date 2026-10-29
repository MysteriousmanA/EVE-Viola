#include <QtTest>
#include <QJsonArray>
#include <QJsonObject>
#include "config/serialization/application_options_json.h"
#include "config/serialization/quick_switch_json.h"
#include "config/serialization/replicas_json.h"
#include "config/serialization/presets_json.h"
#include "config/serialization/color_rules_json.h"
#include "config/serialization/region_presets_json.h"
#include "config/serialization/page_partition_json.h"
#include "features/quick_switch/quick_switch_keys.h"
#include "features/color_rule/trigger_condition.h"

using namespace ConfigSerialization;

class ConfigModulesTest final : public QObject
{
    Q_OBJECT
private slots:
    void independentSerializersPreserveSectionLayout()
    {
        AppConfig config;
        config.keepInTray = true;
        config.defaultPresetSlot = 2;
        config.automaticActivationCooldownMs = 6500;
        ReplicaConfig replica;
        replica.sourceTitle = QStringLiteral("EVE - account");
        replica.position = QPoint(-1920, 100);
        replica.capturedRegion = QRect(10, 20, 80, 90);
        config.replicas.append(replica);
        config.previousClientKeys = {QStringLiteral("Mouse4")};
        config.nextClientKeys = {QStringLiteral("KeyboardVK:41:0")};
        const auto section = replicasSectionToJson(config);
        QVERIFY(section.value(QStringLiteral("replicas")).isArray());
        QVERIFY(section.value(QStringLiteral("keepInTray")).toBool());
        QCOMPARE(section.value(QStringLiteral("defaultPresetSlot")).toInt(), 2);
        QVERIFY(!section.contains(QStringLiteral("options")));
        QVERIFY(!section.contains(QStringLiteral("quickSwitch")));
        AppConfig decoded;
        decoded.replicas = parseReplicas(section.value(QStringLiteral("replicas")).toArray());
        parseApplicationOptions(section, decoded);
        parseQuickSwitchConfig(section, decoded);
        QCOMPARE(decoded.replicas, config.replicas);
        QCOMPARE(decoded.keepInTray, config.keepInTray);
        QCOMPARE(decoded.automaticActivationCooldownMs, config.automaticActivationCooldownMs);
        QCOMPARE(decoded.previousClientKeys, config.previousClientKeys);
        QCOMPARE(decoded.nextClientKeys, config.nextClientKeys);
        PresetConfig preset;
        preset.name = QStringLiteral("test");
        preset.hasConfig = true;
        preset.replicas = config.replicas;
        const auto presetJson = presetToJson(preset);
        QCOMPARE(presetJson.size(), 3);
        QVERIFY(!presetJson.contains(QStringLiteral("previousClientKeys")));
        QVERIFY(parsePresetObject(presetJson) == preset);
    }

    void hotkeySerializationRejectsConflictsAndNormalizesAccounts()
    {
        const QJsonObject object{
            {QStringLiteral("previousClientKeys"),
             QJsonArray{QStringLiteral("keyboardvk:21:1"), QStringLiteral("PageUp"), 123,
                        QStringLiteral("bad")}},
            {QStringLiteral("nextClientKeys"),
             QJsonArray{QStringLiteral("PageUp"), QStringLiteral("Mouse5")}},
            {QStringLiteral("radialSwitchKey"), QStringLiteral("Mouse4")},
            {QStringLiteral("radialAccounts"),
             QJsonArray{QJsonObject{{QStringLiteral("windowTitle"), QStringLiteral(" B ")},
                                    {QStringLiteral("position"), 8}},
                        QJsonObject{{QStringLiteral("windowTitle"), QStringLiteral("A")},
                                    {QStringLiteral("position"), 2}},
                        QJsonObject{{QStringLiteral("windowTitle"), QStringLiteral("B")},
                                    {QStringLiteral("position"), 1}}}}};
        AppConfig config;
        parseQuickSwitchConfig(object, config);
        QCOMPARE(config.previousClientKeys, QStringList{QStringLiteral("PageUp")});
        QCOMPARE(config.nextClientKeys, QStringList{QStringLiteral("Mouse5")});
        QCOMPARE(config.radialSwitchKey, kDefaultRadialSwitchKey);
        QCOMPARE(config.radialAccounts.size(), 2);
        QCOMPARE(config.radialAccounts.first().windowTitle, QStringLiteral("A"));
        QCOMPARE(config.radialAccounts.last().position, 2);
        AppConfig roundTrip;
        parseQuickSwitchConfig(quickSwitchConfigToJson(config), roundTrip);
        QCOMPARE(roundTrip.radialAccounts, config.radialAccounts);
        QCOMPARE(roundTrip.previousClientKeys, config.previousClientKeys);
        QCOMPARE(normalizeQuickSwitchKeys(
                     {QStringLiteral("Mouse4"), QStringLiteral("Mouse4"), QStringLiteral("bad")}),
                 QStringList{QStringLiteral("Mouse4")});
    }

    void legacyKeysAndColorRulesStillMigrateWithoutStorage()
    {
        AppConfig config;
        parseQuickSwitchConfig({{QStringLiteral("previousClientKey"), QStringLiteral("Mouse4")},
                                {QStringLiteral("nextClientKey"), QStringLiteral("Mouse5")}},
                               config);
        QCOMPARE(config.previousClientKeys, QStringList{QStringLiteral("Mouse4")});
        QCOMPARE(config.nextClientKeys, QStringList{QStringLiteral("Mouse5")});
        int nextNumber = 1;
        const auto rules =
            parseColorRules(QJsonArray{QJsonObject{
                                {QStringLiteral("number"), 9},
                                {QStringLiteral("triggerPixel"), QStringLiteral("红色")},
                                {QStringLiteral("triggerCondition"), QStringLiteral(">=20")},
                                {QStringLiteral("triggerFeedback"), QStringLiteral("弹窗")},
                                {QStringLiteral("conditionLimit"), QStringLiteral("跳转至状态")}}},
                            &nextNumber);
        QCOMPARE(rules.size(), 1);
        QCOMPARE(nextNumber, 10);
        QCOMPARE(rules.first().triggerCondition1.expression, QStringLiteral(">=20"));
        QCOMPARE(rules.first().triggerConditionRelation, TriggerConditionRelation::And);
        QCOMPARE(rules.first().triggerFeedback, TriggerFeedback::Popup);
        QCOMPARE(rules.first().postTriggerAction, PostTriggerAction::Jump);
        const auto condition = parseTriggerCondition(rules.first().triggerCondition1.expression);
        QVERIFY(condition.has_value());
        QVERIFY(evaluateTriggerCondition(*condition, 20));
        QVERIFY(!evaluateTriggerCondition(*condition, 19));
    }

    void colorRuleRelationRoundTripsAndDefaultsToAnd()
    {
        ColorRule rule;
        rule.number = 1;
        for (const auto relation : {TriggerConditionRelation::And, TriggerConditionRelation::Or}) {
            rule.triggerConditionRelation = relation;
            const auto json = colorRuleToJson(rule);
            QCOMPARE(json.value(QStringLiteral("triggerConditionRelation")).toString(),
                     relation == TriggerConditionRelation::Or ? QStringLiteral("or") : QStringLiteral("and"));
            QCOMPARE(parseColorRules(QJsonArray{json}, nullptr).first(), rule);
        }
        auto json = colorRuleToJson(rule);
        json.remove(QStringLiteral("triggerConditionRelation"));
        QCOMPARE(parseColorRules(QJsonArray{json}, nullptr).first().triggerConditionRelation,
                 TriggerConditionRelation::And);
        for (const auto value : {QJsonValue(QStringLiteral("invalid")), QJsonValue(1), QJsonValue()}) {
            json.insert(QStringLiteral("triggerConditionRelation"), value);
            QCOMPARE(parseColorRules(QJsonArray{json}, nullptr).first().triggerConditionRelation,
                     TriggerConditionRelation::And);
        }
    }

    void regionAndPartitionSchemasRoundTripIndependently()
    {
        const QList<RegionPreset> presets{
            {QStringLiteral("region"), QRect(-5, 10, 100, 200), 1500}};
        const auto json = regionPresetsSectionToJson(presets);
        QCOMPARE(parseRegionPresets(json.value(QStringLiteral("regionPresets")).toArray()),
                 presets);
        PagePartitionConfig partition;
        partition.visible = false;
        partition.lines.append({QStringLiteral("line"), QPoint(-100, 5), QPoint(200, 5),
                                QColor(Qt::red), 4, QStringLiteral("note")});
        const auto restored = parsePagePartitionConfig(pagePartitionConfigToJson(partition));
        QVERIFY(restored == partition);
    }
};
QTEST_GUILESS_MAIN(ConfigModulesTest)
#include "config_modules_test.moc"
