#include <QtTest>
#include "features/color_rule/window_color_logic_engine.h"
#include "features/color_rule/color_rule_config.h"
#include "config/serialization/color_rules_json.h"
#include "config/serialization/replicas_json.h"

class WindowColorLogicEngineTest final : public QObject {
    Q_OBJECT
    static ColorRule jointRule() {
        ColorRule r;
        r.number = 1;
        r.windowConditions = {{QStringLiteral("红色"), QStringLiteral(">=2"), -1, QStringLiteral("A")},
            {QStringLiteral("白色"), QStringLiteral("<2"), -1, QStringLiteral("B")}};
        r.targetReplicaId = QStringLiteral("A");
        r.postTriggerAction = PostTriggerAction::ReverseWait;
        return r;
    }
private slots:
    void incrementalConfigurationPreservesPendingAndJumpState() {
        auto first = jointRule(); first.postTriggerAction = PostTriggerAction::Jump; first.jumpToRule = 2;
        auto second = first; second.number = 2; second.enabled = false;
        second.postTriggerAction = PostTriggerAction::ReverseWait;
        auto unrelated = jointRule(); unrelated.number = 3;
        unrelated.windowConditions[0].replicaId = "C";
        unrelated.windowConditions[1].replicaId = "D";
        WindowColorLogicEngine engine; engine.configure({first, second});
        engine.observe("A", {10, 2, 0}, 0, 3000);
        const auto trigger = engine.observe("B", {10, 0, 0}, 1, 3000).first();
        engine.configure({unrelated, first, second}, true);
        QVERIFY(engine.pending(trigger.token));
        QVERIFY(engine.conditionMet(1, 2));
        QCOMPARE(engine.triggerCount(1), 1);
        QVERIFY(engine.observe("A", {10, 2, 0}, 2, 3000).isEmpty());
        engine.complete(trigger.token, true);
        engine.configure({second, first}, true);
        QVERIFY(!engine.ruleActive(1)); QVERIFY(engine.ruleActive(2));
        const auto next = engine.observe("B", {10, 0, 0}, 3, 3000);
        QCOMPARE(next.size(), 1); QCOMPARE(next.first().rule.number, 2);
        engine.complete(next.first().token, true);
        engine.configure({first, unrelated, second}, true);
        QVERIFY(engine.observe("B", {10, 0, 0}, 4, 3000).isEmpty());
        QCOMPARE(engine.triggerCount(2), 1);
        engine.observe("A", {10, 0, 0}, 5, 3000);
        QCOMPARE(engine.observe("A", {10, 2, 0}, 6, 3000).size(), 1);
    }
    void incrementalConfigurationResetsEditedRulesOnly() {
        auto first = jointRule(); auto second = first; second.number = 2;
        WindowColorLogicEngine engine; engine.configure({first, second});
        engine.observe("A", {10, 2, 0}, 0, 3000);
        const auto triggers = engine.observe("B", {10, 0, 0}, 1, 3000);
        QCOMPARE(triggers.size(), 2);
        second.windowConditions[0].expression = ">=3";
        engine.configure({first, second}, true);
        QVERIFY(engine.pending(triggers[0].token));
        QVERIFY(!engine.pending(triggers[1].token));
        QCOMPARE(engine.triggerCount(1), 1); QCOMPARE(engine.triggerCount(2), 0);
    }
    void requiresBothWindowsAndRearmsAfterFalse() {
        WindowColorLogicEngine e;
        e.configure({jointRule()});
        QVERIFY(e.observe("A", {10, 2, 0}, 0, 3000).isEmpty());
        QVERIFY(e.observe("B", {10, 0, 3}, 1, 3000).isEmpty());
        auto triggers = e.observe("B", {10, 0, 1}, 2, 3000);
        QCOMPARE(triggers.size(), 1);
        QVERIFY(e.observe("A", {10, 3, 0}, 3, 3000).isEmpty());
        e.complete(triggers.first().token, true);
        QVERIFY(e.observe("B", {10, 0, 1}, 4, 3000).isEmpty());
        QVERIFY(e.observe("A", {10, 0, 0}, 5, 3000).isEmpty());
        QCOMPARE(e.observe("A", {10, 2, 0}, 6, 3000).size(), 1);
        QCOMPARE(e.triggerCount(1), 2);
    }
    void staleMissingAndInvalidatedSamplesDoNotMatch() {
        WindowColorLogicEngine e;
        e.configure({jointRule()});
        e.observe("A", {10, 2, 0}, 0, 100);
        QVERIFY(e.observe("B", {10, 0, 0}, 100, 100).isEmpty());
        auto t = e.observe("A", {10, 2, 0}, 101, 100);
        QCOMPARE(t.size(), 1);
        e.complete(t.first().token, true);
        e.invalidate("B", 102);
        QVERIFY(!e.conditionMet(1, 102));
        QVERIFY(e.observe("A", {10, 2, 0}, 103, 100).isEmpty());
        QCOMPARE(e.observe("B", {10, 0, 0}, 104, 100).size(), 1);
        e.expire(204);
        QVERIFY(!e.conditionMet(1, 204));
    }
    void boundsOrTriggersOutsideTheRange() {
        auto rule = jointRule();
        rule.windowConditions = {{QStringLiteral("红色"), QStringLiteral(">100|<10"), -1, "A"}};
        WindowColorLogicEngine engine; engine.configure({rule});
        QVERIFY(engine.observe("A", {200, 50, 0}, 0, 100).isEmpty());
        auto triggers = engine.observe("A", {200, 9, 0}, 1, 100);
        QCOMPARE(triggers.size(), 1);
        engine.complete(triggers.first().token, true);
        QVERIFY(engine.observe("A", {200, 10, 0}, 2, 100).isEmpty());
        QVERIFY(engine.observe("A", {200, 100, 0}, 3, 100).isEmpty());
        QCOMPARE(engine.observe("A", {200, 101, 0}, 4, 100).size(), 1);
    }
    void orCanMatchOneValidWindow() {
        auto r = jointRule(); r.triggerConditionRelation = TriggerConditionRelation::Or;
        WindowColorLogicEngine e; e.configure({r});
        QCOMPARE(e.observe("A", {10, 2, 0}, 0, 100).size(), 1);
        e.invalidate("A", 1);
        QVERIFY(!e.conditionMet(1, 1));
    }
    void loopWaitsForCompletionAndNextSample() {
        auto r = jointRule(); r.postTriggerAction = PostTriggerAction::Loop;
        WindowColorLogicEngine e; e.configure({r});
        e.observe("A", {10, 2, 0}, 0, 100);
        auto t = e.observe("B", {10, 0, 0}, 1, 100);
        QVERIFY(e.observe("A", {10, 2, 0}, 2, 100).isEmpty());
        e.complete(t.first().token, true);
        QCOMPARE(e.observe("A", {10, 2, 0}, 3, 100).size(), 1);
    }
    void suppressedAndFailedFeedbackDoNotReplay() {
        auto r = jointRule(); r.postTriggerAction = PostTriggerAction::Loop;
        WindowColorLogicEngine e; e.configure({r});
        e.observe("A", {10, 2, 0}, 0, 100);
        const auto t = e.observe("B", {10, 0, 0}, 1, 100).first();
        e.suppress(t.token);
        QVERIFY(e.observe("A", {10, 2, 0}, 2, 100).isEmpty());
        e.configure({r});
        e.observe("A", {10, 2, 0}, 3, 100);
        const auto next = e.observe("B", {10, 0, 0}, 4, 100).first();
        QVERIFY(next.token != t.token);
        e.complete(t.token, true);
        QVERIFY(e.pending(next.token));
        e.complete(next.token, false);
        QVERIFY(e.observe("A", {10, 2, 0}, 5, 100).isEmpty());
    }
    void jumpActivatesDestinationAfterFeedback() {
        auto r = jointRule(); r.postTriggerAction = PostTriggerAction::Jump; r.jumpToRule = 2;
        auto next = r; next.number = 2; next.enabled = false;
        next.postTriggerAction = PostTriggerAction::ReverseWait;
        WindowColorLogicEngine e; e.configure({r, next});
        QVERIFY(e.ruleActive(1)); QVERIFY(!e.ruleActive(2));
        e.observe("A", {10, 2, 0}, 0, 100);
        auto t = e.observe("B", {10, 0, 0}, 1, 100);
        QCOMPARE(t.size(), 1); QCOMPARE(t.first().rule.number, 1);
        e.complete(t.first().token, true);
        QVERIFY(!e.ruleActive(1)); QVERIFY(e.ruleActive(2));
        t = e.observe("A", {10, 2, 0}, 2, 100);
        QCOMPARE(t.size(), 1); QCOMPARE(t.first().rule.number, 2);
    }
    void middleClickSwitchAlternatesActiveRules() {
        auto warning = jointRule();
        auto reverse = warning; reverse.number = 2; reverse.enabled = false;
        WindowColorLogicEngine engine; engine.configure({warning, reverse});
        QVERIFY(engine.ruleActive(1)); QVERIFY(!engine.ruleActive(2));
        engine.observe("A", {10, 2, 0}, 0, 100);
        auto triggers = engine.observe("B", {10, 0, 0}, 1, 100);
        QCOMPARE(triggers.size(), 1);
        QCOMPARE(triggers.first().rule.number, 1);
        engine.suppress(triggers.first().token);
        QVERIFY(engine.toggleRules(1, 2));
        QVERIFY(!engine.ruleActive(1)); QVERIFY(engine.ruleActive(2));
        engine.observe("A", {10, 0, 0}, 2, 100);
        triggers = engine.observe("A", {10, 2, 0}, 3, 100);
        QCOMPARE(triggers.size(), 1);
        QCOMPARE(triggers.first().rule.number, 2);
        engine.suppress(triggers.first().token);
        QVERIFY(engine.toggleRules(1, 2));
        engine.observe("A", {10, 0, 0}, 4, 100);
        triggers = engine.observe("A", {10, 2, 0}, 5, 100);
        QCOMPARE(triggers.size(), 1);
        QCOMPARE(triggers.first().rule.number, 1);
    }
    void expiredFeedbackCannotJumpToAnotherRule() {
        auto r = jointRule(); r.postTriggerAction = PostTriggerAction::Jump; r.jumpToRule = 2;
        auto next = r; next.number = 2; next.enabled = false;
        WindowColorLogicEngine e; e.configure({r, next});
        e.observe("A", {10, 2, 0}, 0, 100);
        auto t = e.observe("B", {10, 0, 0}, 1, 100);
        e.expire(101);
        e.complete(t.first().token, true);
        e.observe("A", {10, 2, 0}, 102, 100);
        t = e.observe("B", {10, 0, 0}, 103, 100);
        QCOMPARE(t.size(), 1);
        QCOMPARE(t.first().rule.number, 1);
    }
    void independentRulesCanShareAWindow() {
        auto r = jointRule(); auto next = r; next.number = 2;
        next.windowConditions = {{QStringLiteral("白色"), QStringLiteral(">=2"), -1, QStringLiteral("A")}};
        WindowColorLogicEngine e; e.configure({r, next});
        QCOMPARE(e.requiredColors("A").size(), 2);
        auto t = e.observe("A", {10, 2, 2}, 0, 100);
        QCOMPARE(t.size(), 1); QCOMPARE(t.first().rule.number, 2);
        t = e.observe("B", {10, 0, 0}, 1, 100);
        QCOMPARE(t.size(), 1); QCOMPARE(t.first().rule.number, 1);
    }
    void notificationsAndModeChangesPreserveExecutionCounts() {
        WindowColorLogicEngine e; e.configure({jointRule()});
        e.observe("A", {10, 2, 0}, 0, 100);
        auto t = e.observe("B", {10, 0, 0}, 1, 100);
        e.complete(t.first().token, true);
        QCOMPARE(e.triggerCount(1), 1);
        e.resetObservations();
        e.observe("A", {10, 2, 0}, 2, 100, true);
        t = e.observe("B", {10, 0, 0}, 3, 100, true);
        QCOMPARE(t.size(), 1);
        e.suppress(t.first().token);
        QCOMPARE(e.triggerCount(1), 1);
        QVERIFY(e.observe("B", {10, 0, 0}, 4, 100, true).isEmpty());
    }
    void persistsBindingsAndRepairsDuplicateIds() {
        auto r = jointRule(); r.jumpToRule = 7; r.enabled = false;
        r.windowSwitches = {{QStringLiteral("B"), QColor(12, 34, 56), 7}};
        const auto parsed = ConfigSerialization::parseColorRules(
            QJsonArray{ConfigSerialization::colorRuleToJson(r)}, nullptr);
        QCOMPARE(parsed.size(), 1); QVERIFY(parsed.first() == r);
        ReplicaConfig replica; replica.id = "stable";
        const auto json = ConfigSerialization::replicaToJson(replica);
        auto replicas = ConfigSerialization::parseReplicas(QJsonArray{json, json, QJsonObject{}});
        QCOMPARE(replicas.first().id, QStringLiteral("stable"));
        QVERIFY(replicas[1].id != replicas[0].id);
        QVERIFY(!replicas[2].id.isEmpty());
    }
    void legacySlotsMigrateOnceWithJumpPairAndDistinctWindows() {
        AppConfig config;
        ColorRule r; r.number = 1; r.triggerCondition1 = {QStringLiteral("红色"), QStringLiteral(">=2")};
        r.postTriggerAction = PostTriggerAction::Jump;
        config.colorRules = {r};
        ReplicaConfig a; a.colorLogicSlotRule[0] = 1; a.colorLogicSlotRule[1] = 1;
        a.colorLogicEnabled = true; a.activeColorLogicSlot = 2;
        config.replicas = {a, a};
        migrateWindowColorLogic(config);
        QVERIFY(config.replicas[0].id != config.replicas[1].id);
        const auto rules = config.colorRules;
        int bound = 0; int enabled = 0;
        for (const auto& rule : rules) {
            if (rule.windowConditions.isEmpty()) { continue; }
            ++bound; enabled += rule.enabled;
            QVERIFY(rule.jumpToRule > 0);
            QCOMPARE(rule.targetReplicaId, rule.windowConditions.first().replicaId);
        }
        QCOMPARE(bound, 4); QCOMPARE(enabled, 2);
        migrateWindowColorLogic(config);
        QVERIFY(config.colorRules == rules);
        QCOMPARE(config.replicas.first().colorLogicSlotRule[0], 0);
    }
};
QTEST_GUILESS_MAIN(WindowColorLogicEngineTest)
#include "window_color_logic_engine_test.moc"
