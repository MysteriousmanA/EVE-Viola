#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include "config/json_config_store.h"
#include "features/color_rule/logic_binding_plan.h"
#include "features/region_preset/region_preset_config.h"

class LogicBindingPlanTest final : public QObject {
    Q_OBJECT
    static ColorRule rule() {
        ColorRule rule; rule.number = 7; rule.targetReplicaId = "A";
        rule.windowConditions = {{QStringLiteral("红色"), QStringLiteral(">=2"), -1, "A"},
            {QStringLiteral("白色"), QStringLiteral("<2"), -1, "B"}};
        return rule;
    }
    static QList<ReplicaConfig> templates() {
        ReplicaConfig a; a.id = "A"; a.capturedRegion = QRect(0, 0, 20, 10);
        ReplicaConfig b; b.id = "B"; b.capturedRegion = QRect(20, 0, 20, 10);
        return {a, b};
    }
private slots:
    void toleranceSettingsAreSharedByRegionAndPersist()
    {
        AppConfig config;
        config.colorTolerances = {{QStringLiteral("红色"), 12}, {QStringLiteral("白色"), 8}};
        config.regionPresets = {{QStringLiteral("护盾"), QRect(0, 0, 20, 10)},
                                {QStringLiteral("本地"), QRect(20, 0, 20, 10)}};
        config.regionPresets[1].colorTolerances = {{QStringLiteral("红色"), 0}};
        QCOMPARE(regionColorTolerances(config, QStringLiteral("护盾")), config.colorTolerances);
        QCOMPARE(colorToleranceFor(regionColorTolerances(config, QStringLiteral("本地")), QStringLiteral("红色")), 0);
        auto presets = config.regionPresets;
        for (auto& preset : presets) { preset.colorTolerances = regionColorTolerances(config, preset.name); }
        auto definition = rule();
        definition.windowConditions[0].replicaId = logicRegionPresetId(QStringLiteral("护盾"));
        definition.windowConditions[0].tolerance = 200; // Old per-condition overrides must not win.
        definition.windowConditions[1] = {QStringLiteral("红色"), QStringLiteral(">0"), 200,
            logicRegionPresetId(QStringLiteral("本地"))};
        LogicBindingConfig binding; binding.ruleNumber = 7; binding.sourceTitle = "EVE - A";
        const auto templates = logicRegionTemplates(presets);
        auto plan = planLogicBinding(binding, {definition}, templates, 1);
        QVERIFY(plan.error.isEmpty());
        QCOMPARE(plan.rules.first().windowConditions[0].tolerance, 12);
        QCOMPARE(plan.rules.first().windowConditions[1].tolerance, 0);
        binding.sourceTitle = "EVE - B";
        QCOMPARE(planLogicBinding(binding, {definition}, templates, 1).rules.first().windowConditions[0].tolerance, 12);
        QTemporaryDir directory; JsonConfigStore store(directory.path()); QString error;
        QVERIFY2(store.save(config, &error), qPrintable(error));
        AppConfig loaded; QVERIFY2(store.load(loaded, &error), qPrintable(error));
        QCOMPARE(loaded.colorTolerances, config.colorTolerances);
        QCOMPARE(loaded.regionPresets, config.regionPresets);
        loaded.colorTolerances[0].tolerance = 30;
        QCOMPARE(colorToleranceFor(regionColorTolerances(loaded, QStringLiteral("护盾")), QStringLiteral("红色")), 30);
        QCOMPARE(colorToleranceFor(regionColorTolerances(loaded, QStringLiteral("本地")), QStringLiteral("红色")), 0);
        QVERIFY(renameRegionPreset(loaded, 1, QStringLiteral("新本地")));
        QCOMPARE(colorToleranceFor(regionColorTolerances(loaded, QStringLiteral("新本地")), QStringLiteral("红色")), 0);
    }
    void regionPresetReferencesSurviveMigrationRenameAndSave()
    {
        AppConfig config; config.replicas = templates(); config.colorRules = {rule()};
        config.regionPresets = {{QStringLiteral("护盾"), config.replicas[0].capturedRegion, 750},
            {QStringLiteral("本地"), config.replicas[1].capturedRegion, 1500}};
        migrateLogicRegionReferences(config);
        QCOMPARE(config.colorRules[0].targetReplicaId, logicRegionPresetId(QStringLiteral("护盾")));
        LogicBindingConfig savedBinding; savedBinding.ruleNumber = 7;
        savedBinding.windowIds.insert(logicRegionPresetId(QStringLiteral("护盾")), "existing-shield");
        config.logicBindings = {savedBinding};
        QVERIFY(renameRegionPreset(config, 0, QStringLiteral("新护盾")));
        QCOMPARE(config.colorRules[0].windowConditions[0].replicaId, logicRegionPresetId(QStringLiteral("新护盾")));
        QCOMPARE(config.logicBindings.first().windowIds.value(logicRegionPresetId(QStringLiteral("新护盾"))),
            QStringLiteral("existing-shield"));
        const auto definitions = config.colorRules;
        config.replicas.clear(); // Presets alone are enough; no ownerless small windows are required.
        migrateLogicRegionReferences(config);
        QCOMPARE(config.colorRules, definitions);
        LogicBindingConfig binding; binding.ruleNumber = 7; binding.sourceTitle = "EVE - A";
        auto plan = planLogicBinding(binding, definitions, logicRegionTemplates(config.regionPresets), 1);
        QVERIFY2(plan.error.isEmpty(), qPrintable(plan.error)); QCOMPARE(plan.windows.size(), 2);
        for (const auto& window : plan.windows) {
            QCOMPARE(window.sourceTitle, binding.sourceTitle);
            if (window.capturedRegion == config.regionPresets[0].region) { QCOMPARE(window.statisticsIntervalMs, 750); }
        }
        QTemporaryDir directory; JsonConfigStore store(directory.path()); QString error;
        QVERIFY2(store.save(config, &error), qPrintable(error));
        AppConfig actual; QVERIFY2(store.load(actual, &error), qPrintable(error));
        QCOMPARE(actual.colorRules, config.colorRules);
        config.regionPresets.removeFirst();
        QVERIFY(!planLogicBinding(binding, definitions, logicRegionTemplates(config.regionPresets), 1).error.isEmpty());
    }
    void allWindowsUseOneBindingSourceAndInstancesHaveIndependentKeys() {
        LogicBindingConfig binding; binding.id = "one"; binding.ruleNumber = 7; binding.sourceTitle = "EVE - A";
        const auto first = planLogicBinding(binding, {rule()}, templates(), 100);
        QVERIFY(first.error.isEmpty()); QCOMPARE(first.windows.size(), 2);
        for (const auto& window : first.windows) { QCOMPARE(window.sourceTitle, binding.sourceTitle); }
        QCOMPARE(first.rules.first().number, 100); QCOMPARE(first.rules.first().definitionNumber, 7);
        binding.id = "two"; binding.sourceTitle = "EVE - B";
        const auto second = planLogicBinding(binding, {rule()}, templates(), 200);
        QVERIFY(second.error.isEmpty());
        QVERIFY(first.rules.first().windowConditions.first().replicaId != second.rules.first().windowConditions.first().replicaId);
        QCOMPARE(second.rules.first().number, 200);
        QVERIFY(rule().windowConditions.first().replicaId == "A");
    }
    void jumpTargetsRemainInsideTheSameInstance() {
        auto a = rule(); auto b = a; b.number = 9;
        a.postTriggerAction = PostTriggerAction::Jump; a.jumpToRule = 9;
        b.postTriggerAction = PostTriggerAction::Jump; b.jumpToRule = 7;
        LogicBindingConfig binding; binding.ruleNumber = 7;
        const auto plan = planLogicBinding(binding, {a, b}, templates(), 10);
        QVERIFY(plan.error.isEmpty()); QCOMPARE(plan.rules.size(), 2);
        QCOMPARE(plan.rules[0].jumpToRule, 11); QCOMPARE(plan.rules[1].jumpToRule, 10);
        QVERIFY(plan.rules[0].enabled); QVERIFY(!plan.rules[1].enabled);
    }
    void switchTargetsStayWithTheirBindingSource() {
        auto warning = rule();
        warning.windowSwitches = {{QStringLiteral("C"), QColor(255, 80, 80), 9}};
        auto reverse = rule(); reverse.number = 9;
        auto candidates = templates();
        ReplicaConfig c; c.id = QStringLiteral("C"); c.capturedRegion = QRect(40, 0, 20, 10);
        candidates.append(c);
        LogicBindingConfig binding; binding.id = QStringLiteral("warning");
        binding.ruleNumber = 7; binding.sourceTitle = QStringLiteral("EVE - A");
        const auto plan = planLogicBinding(binding, {warning, reverse}, candidates, 20);
        QVERIFY2(plan.error.isEmpty(), qPrintable(plan.error));
        QCOMPARE(plan.windows.size(), 3);
        for (const auto& window : plan.windows) { QCOMPARE(window.sourceTitle, binding.sourceTitle); }
        QCOMPARE(plan.rules.size(), 2);
        QCOMPARE(plan.rules.first().targetReplicaId,
            plan.rules.first().windowConditions.first().replicaId);
        QCOMPARE(plan.rules.first().windowSwitches.first().middleClickRule, 21);
        QVERIFY(plan.rules.first().windowSwitches.first().replicaId.startsWith(
            QStringLiteral("logic:warning:")));
        QVERIFY(plan.rules.first().enabled);
        QVERIFY(!plan.rules.last().enabled);
    }
    void ownedOrDeletedTemplatesAreRejected() {
        LogicBindingConfig binding; binding.ruleNumber = 7;
        auto candidates = templates(); candidates[0].sourceTitle = "Already owned";
        QVERIFY(!planLogicBinding(binding, {rule()}, candidates, 1).error.isEmpty());
        QVERIFY(!planLogicBinding(binding, {rule()}, {}, 1).error.isEmpty());
        QVERIFY(!planLogicBinding(binding, {}, templates(), 1).error.isEmpty());
    }
    void bindingSourcesPersistWithoutChangingDefinitions() {
        QTemporaryDir directory;
        JsonConfigStore store(directory.path()); AppConfig config;
        config.colorRules = {rule()}; config.replicas = templates();
        LogicBindingConfig binding; binding.ruleNumber = 7; binding.sourceTitle = QStringLiteral("EVE - 玩家");
        binding.sourceProcessName = QStringLiteral("exefile.exe");
        binding.windowIds = {{"A", "existing-a"}, {"B", "existing-b"}};
        const auto plan = planLogicBinding(binding, config.colorRules, templates(), 10);
        QCOMPARE(plan.rules.first().windowConditions[0].replicaId, QStringLiteral("existing-a"));
        QCOMPARE(plan.rules.first().windowConditions[1].replicaId, QStringLiteral("existing-b"));
        config.logicBindings = {binding}; QString error;
        QVERIFY2(store.save(config, &error), qPrintable(error));
        QFile savedFile(directory.filePath(QStringLiteral("colorRules.json")));
        QVERIFY(savedFile.open(QIODevice::ReadOnly));
        auto document = QJsonDocument::fromJson(savedFile.readAll()).object();
        savedFile.close();
        auto instances = document.value(QStringLiteral("logicBindings")).toArray();
        auto instance = instances.first().toObject();
        QVERIFY(!instance.contains(QStringLiteral("sourceHwnd")));
        QCOMPARE(instance.value(QStringLiteral("sourceProcessName")).toString(), QStringLiteral("exefile.exe"));
        // An old cache may carry an obsolete HWND; loading migrates to process + title.
        instance.remove(QStringLiteral("sourceProcessName"));
        instance.insert(QStringLiteral("sourceHwnd"), QStringLiteral("4886718345"));
        instances[0] = instance;
        document.insert(QStringLiteral("logicBindings"), instances);
        QVERIFY(savedFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QVERIFY(savedFile.write(QJsonDocument(document).toJson()) > 0);
        savedFile.close();
        AppConfig actual; QVERIFY2(store.load(actual, &error), qPrintable(error));
        QCOMPARE(actual.logicBindings, config.logicBindings);
        QCOMPARE(actual.colorRules, config.colorRules);
    }
    void legacyBoundRulesBecomeUnownedTemplatesAndOneBindingOnce() {
        AppConfig config; config.colorRules = {rule()}; config.replicas = templates();
        for (auto& window : config.replicas) { window.sourceTitle = "EVE - A"; }
        migrateLogicBindings(config);
        QCOMPARE(config.logicBindings.size(), 1);
        QCOMPARE(config.logicBindings.first().sourceTitle, QStringLiteral("EVE - A"));
        QVERIFY(config.logicBindings.first().enabled);
        QCOMPARE(config.regionPresets.size(), 2);
        const auto rules = config.colorRules; const auto replicas = config.replicas;
        migrateLogicBindings(config);
        QCOMPARE(config.logicBindings.size(), 1);
        QCOMPARE(config.colorRules, rules); QCOMPARE(config.replicas, replicas);
        QCOMPARE(config.replicas.size(), 2); // Migration must not add ordinary small windows.
        QVERIFY(planLogicBinding(config.logicBindings.first(), config.colorRules,
            logicRegionTemplates(config.regionPresets), 1).error.isEmpty());
    }
    void oldCrossSourceRuleWaitsForExplicitNewBinding() {
        AppConfig config; config.colorRules = {rule()}; config.replicas = templates();
        config.replicas[0].sourceTitle = "EVE - A"; config.replicas[1].sourceTitle = "EVE - B";
        migrateLogicBindings(config);
        QCOMPARE(config.logicBindings.size(), 1);
        QVERIFY(config.logicBindings.first().sourceTitle.isEmpty());
        QVERIFY(!config.logicBindings.first().enabled);
    }
};
QTEST_GUILESS_MAIN(LogicBindingPlanTest)
#include "logic_binding_plan_test.moc"
