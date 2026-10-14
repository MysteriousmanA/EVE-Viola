#include <algorithm>

#include <QtTest>

#include "features/color_rule/color_logic_engine.h"

namespace {

ColorRule redRule(int number = 1)
{
    ColorRule rule;
    rule.number = number;
    rule.triggerCondition1 = {
        QStringLiteral("红色"), QStringLiteral(">=2")};
    rule.triggerFeedback = TriggerFeedback::None;
    rule.postTriggerAction = PostTriggerAction::ReverseWait;
    return rule;
}

ColorRule audioRule(PostTriggerAction action = PostTriggerAction::Loop)
{
    ColorRule rule = redRule();
    rule.triggerFeedback = TriggerFeedback::Audio;
    rule.triggerFeedbackAudio = QStringLiteral("alarm.wav");
    rule.postTriggerAction = action;
    return rule;
}

int triggerCount(const QList<ColorLogicEffect>& effects)
{
    return static_cast<int>(std::count_if(
        effects.cbegin(), effects.cend(), [](const ColorLogicEffect& effect) {
            return effect.kind == ColorLogicEffectKind::Triggered;
        }));
}

bool hasEffect(const QList<ColorLogicEffect>& effects, ColorLogicEffectKind kind)
{
    return std::any_of(
        effects.cbegin(), effects.cend(), [kind](const ColorLogicEffect& effect) {
            return effect.kind == kind;
        });
}

} // namespace

class ColorLogicEngineTest final : public QObject {
    Q_OBJECT

private slots:
    void triggersOnlyOnFalseToTrueEdge()
    {
        ColorLogicEngine engine({redRule()});
        engine.setSlotRule(1, 1);
        engine.setEnabled(true);

        QCOMPARE(triggerCount(engine.observe(1, 0)), 0);
        QCOMPARE(triggerCount(engine.observe(2, 0)), 1);
        QCOMPARE(triggerCount(engine.observe(2, 0)), 0);
        QCOMPARE(triggerCount(engine.observe(1, 0)), 0);
        QCOMPARE(triggerCount(engine.observe(2, 0)), 1);
    }

    void requiresAllNonEmptyConditionsAndIgnoresOneEmptyCondition()
    {
        ColorRule rule = redRule();
        rule.triggerCondition2 = {QStringLiteral("白色"), QStringLiteral(">=1")};
        ColorLogicEngine engine({rule});
        engine.setSlotRule(1, 1);
        engine.setEnabled(true);

        QCOMPARE(triggerCount(engine.observe(2, 0)), 0);
        QCOMPARE(triggerCount(engine.observe(2, 1)), 1);

        ColorRule oneCondition = redRule(2);
        oneCondition.triggerCondition2 = {};
        engine.setRules({oneCondition});
        engine.setSlotRule(1, 2);
        QCOMPARE(triggerCount(engine.observe(1, 0)), 0);
        QCOMPARE(triggerCount(engine.observe(2, 0)), 1);
    }

    void disabledOrEmptyRuleDoesNotTrigger()
    {
        ColorRule emptyRule;
        emptyRule.number = 1;
        ColorLogicEngine engine({emptyRule});
        engine.setSlotRule(1, 1);

        QCOMPARE(triggerCount(engine.observe(10, 10)), 0);
        engine.setEnabled(true);
        QCOMPARE(triggerCount(engine.observe(10, 10)), 0);
        engine.setEnabled(false);
        QCOMPARE(triggerCount(engine.observe(10, 10)), 0);
    }

    void audioFeedbackCompletesOnceAndLoopRearms()
    {
        ColorLogicEngine engine({audioRule()});
        engine.setSlotRule(1, 1);
        engine.setEnabled(true);

        const auto effects = engine.observe(2, 0);
        QCOMPARE(triggerCount(effects), 1);
        QVERIFY(effects.first().feedbackId != 0);
        const quint64 feedbackId = effects.first().feedbackId;
        QCOMPARE(triggerCount(engine.observe(2, 0)), 0);

        QCOMPARE(triggerCount(engine.feedbackCompleted(feedbackId)), 0);
        QCOMPARE(triggerCount(engine.feedbackCompleted(feedbackId)), 0);
        QCOMPARE(triggerCount(engine.observe(2, 0)), 1);
    }

    void conditionDropDoesNotCancelAcceptedAudio()
    {
        ColorLogicEngine engine({audioRule()});
        engine.setSlotRule(1, 1);
        engine.setEnabled(true);

        const auto started = engine.observe(2, 0);
        const quint64 feedbackId = started.first().feedbackId;
        const auto dropped = engine.observe(0, 0);
        QVERIFY(!hasEffect(dropped, ColorLogicEffectKind::CancelAudio));
        QVERIFY(hasEffect(dropped, ColorLogicEffectKind::SetFlashActive));
        QCOMPARE(engine.feedbackCompleted(feedbackId).size(), 0);

        QCOMPARE(triggerCount(engine.observe(2, 0)), 1);
    }

    void disablingAudioRejectsLateCompletion()
    {
        ColorLogicEngine engine({audioRule()});
        engine.setSlotRule(1, 1);
        engine.setEnabled(true);

        const auto started = engine.observe(2, 0);
        const quint64 feedbackId = started.first().feedbackId;
        const auto cancelled = engine.setEnabled(false);
        QVERIFY(hasEffect(cancelled, ColorLogicEffectKind::CancelAudio));
        QCOMPARE(cancelled.first().kind, ColorLogicEffectKind::CancelAudio);
        QCOMPARE(cancelled.first().feedbackId, feedbackId);
        QCOMPARE(engine.feedbackCompleted(feedbackId).size(), 0);
    }

    void failureKeepsTheCurrentConditionLatched()
    {
        ColorLogicEngine engine({audioRule()});
        engine.setSlotRule(1, 1);
        engine.setEnabled(true);

        const quint64 feedbackId = engine.observe(2, 0).first().feedbackId;
        QCOMPARE(engine.feedbackFailed(feedbackId).size(), 0);
        QCOMPARE(triggerCount(engine.observe(2, 0)), 0);
        QCOMPARE(triggerCount(engine.observe(0, 0)), 0);
        QCOMPARE(triggerCount(engine.observe(2, 0)), 1);
    }

    void reverseWaitRequiresAFalseObservationBeforeRetriggering()
    {
        ColorLogicEngine engine({redRule()});
        engine.setSlotRule(1, 1);
        engine.setEnabled(true);

        QCOMPARE(triggerCount(engine.observe(2, 0)), 1);
        QCOMPARE(triggerCount(engine.observe(2, 0)), 0);
        QCOMPARE(triggerCount(engine.observe(0, 0)), 0);
        QCOMPARE(triggerCount(engine.observe(2, 0)), 1);
    }

    void jumpSwitchesSlotBeforeTheNextObservation()
    {
        ColorRule jump = redRule();
        jump.postTriggerAction = PostTriggerAction::Jump;
        ColorRule target;
        target.number = 2;
        target.triggerCondition1 = {
            QStringLiteral("白色"), QStringLiteral(">=2")};
        target.postTriggerAction = PostTriggerAction::ReverseWait;
        ColorLogicEngine engine({jump, target});
        engine.setSlotRule(1, 1);
        engine.setSlotRule(2, 2);
        engine.setEnabled(true);

        const auto first = engine.observe(2, 0);
        QCOMPARE(triggerCount(first), 1);
        QVERIFY(hasEffect(first, ColorLogicEffectKind::SwitchActiveSlot));
        QCOMPARE(engine.activeSlot(), 2);
        QCOMPARE(triggerCount(engine.observe(2, 2)), 1);
    }
};

QTEST_GUILESS_MAIN(ColorLogicEngineTest)
#include "color_logic_engine_test.moc"
