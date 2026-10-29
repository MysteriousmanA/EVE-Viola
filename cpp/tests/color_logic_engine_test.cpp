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
    void conditionRelations_data()
    {
        QTest::addColumn<bool>("useOr");
        QTest::addColumn<int>("conditions");
        QTest::addColumn<int>("red");
        QTest::addColumn<int>("white");
        QTest::addColumn<bool>("expected");
        QTest::newRow("and-neither") << false << 3 << 0 << 0 << false;
        QTest::newRow("and-first") << false << 3 << 2 << 0 << false;
        QTest::newRow("and-second") << false << 3 << 0 << 1 << false;
        QTest::newRow("and-both") << false << 3 << 2 << 1 << true;
        QTest::newRow("or-neither") << true << 3 << 0 << 0 << false;
        QTest::newRow("or-first") << true << 3 << 2 << 0 << true;
        QTest::newRow("or-second") << true << 3 << 0 << 1 << true;
        QTest::newRow("or-both") << true << 3 << 2 << 1 << true;
        QTest::newRow("and-only-first-false") << false << 1 << 0 << 1 << false;
        QTest::newRow("and-only-first-true") << false << 1 << 2 << 0 << true;
        QTest::newRow("or-only-first-false") << true << 1 << 0 << 1 << false;
        QTest::newRow("or-only-first-true") << true << 1 << 2 << 0 << true;
        QTest::newRow("and-only-second-false") << false << 2 << 2 << 0 << false;
        QTest::newRow("and-only-second-true") << false << 2 << 0 << 1 << true;
        QTest::newRow("or-only-second-false") << true << 2 << 2 << 0 << false;
        QTest::newRow("or-only-second-true") << true << 2 << 0 << 1 << true;
        QTest::newRow("and-empty") << false << 0 << 2 << 1 << false;
        QTest::newRow("or-empty") << true << 0 << 2 << 1 << false;
    }

    void conditionRelations()
    {
        QFETCH(bool, useOr);
        QFETCH(int, conditions);
        QFETCH(int, red);
        QFETCH(int, white);
        QFETCH(bool, expected);
        auto rule = redRule();
        rule.triggerCondition1 = conditions & 1 ? rule.triggerCondition1 : ColorRuleCondition{};
        if (conditions & 2) {
            rule.triggerCondition2 = {QStringLiteral("白色"), QStringLiteral(">=1")};
        }
        rule.triggerConditionRelation = useOr ? TriggerConditionRelation::Or : TriggerConditionRelation::And;
        ColorLogicEngine engine({rule});
        engine.setSlotRule(1, 1);
        engine.setEnabled(true);
        QCOMPARE(triggerCount(engine.observe(red, white)), expected ? 1 : 0);
        QCOMPARE(engine.observeNotification({0, red, white}).has_value(), expected);
    }

    void orRelationKeepsOneEdgeWhenMatchingConditionChanges()
    {
        auto rule = redRule();
        rule.triggerConditionRelation = TriggerConditionRelation::Or;
        rule.triggerCondition2 = {QStringLiteral("白色"), QStringLiteral(">=1")};
        ColorLogicEngine engine({rule});
        engine.setSlotRule(1, 1);
        engine.setEnabled(true);
        QCOMPARE(triggerCount(engine.observe(2, 0)), 1);
        QCOMPARE(triggerCount(engine.observe(2, 1)), 0);
        QCOMPARE(triggerCount(engine.observe(0, 1)), 0);
        QCOMPARE(triggerCount(engine.observe(0, 0)), 0);
        QCOMPARE(triggerCount(engine.observe(0, 1)), 1);
    }

    void orRelationTreatsInvalidOrUnavailableConditionAsUnmatched()
    {
        for (const auto condition : {
                 ColorRuleCondition{QStringLiteral("白色"), QStringLiteral("bad")},
                 ColorRuleCondition{QStringLiteral("#123456"), QStringLiteral("<=0"), 0}}) {
            auto rule = redRule();
            rule.triggerConditionRelation = TriggerConditionRelation::Or;
            rule.triggerCondition1 = condition;
            rule.triggerCondition2 = {QStringLiteral("红色"), QStringLiteral(">=2")};
            ColorLogicEngine engine({rule});
            engine.setSlotRule(1, 1);
            engine.setEnabled(true);
            QCOMPARE(triggerCount(engine.observe(0, 0)), 0);
            QCOMPARE(triggerCount(engine.observe(2, 0)), 1);
        }
    }

    void notificationObservationDoesNotExecuteOrAdvanceRules()
    {
        ColorLogicEngine engine({audioRule(PostTriggerAction::Jump)});
        engine.setSlotRule(1, 1);
        engine.setEnabled(true);
        QCOMPARE(engine.observeNotification({0, 2, 0}), std::optional<int>(1));
        QVERIFY(!engine.observeNotification({0, 2, 0}));
        QCOMPARE(engine.triggerCount(1), 0);
        QCOMPARE(engine.activeSlot(), 1);
        QVERIFY(!engine.hasActiveFeedback());
        QVERIFY(!engine.observeNotification({0, 0, 0}));
        QCOMPARE(engine.observeNotification({0, 2, 0}), std::optional<int>(1));
        engine.setEnabled(false);
        QVERIFY(!engine.observeNotification({0, 2, 0}));
    }

    void resettingPendingFeedbackCancelsAudioWithoutPostTriggerJump()
    {
        ColorLogicEngine engine({audioRule(PostTriggerAction::Jump)});
        engine.setSlotRule(1, 1);
        engine.setEnabled(true);
        const auto trigger = engine.observe(2, 0).first();
        QVERIFY(engine.hasActiveFeedback());
        const auto cancelled = engine.resetPendingFeedback();
        QVERIFY(hasEffect(cancelled, ColorLogicEffectKind::CancelAudio));
        QVERIFY(!hasEffect(cancelled, ColorLogicEffectKind::SwitchActiveSlot));
        QVERIFY(!engine.hasActiveFeedback());
        QCOMPARE(engine.activeSlot(), 1);
        QCOMPARE(engine.triggerCount(1), 1);
        QVERIFY(engine.feedbackCompleted(trigger.feedbackId).isEmpty());
        QCOMPARE(triggerCount(engine.observe(2, 0)), 1);
    }

    void completingEarlierAudioDoesNotRearmALaterSuppressedEdge()
    {
        ColorLogicEngine engine({audioRule()});
        engine.setSlotRule(1, 1);
        engine.setEnabled(true);
        const auto first = engine.observe(PixelStatistics{0, 2, 0}, true).first();
        static_cast<void>(engine.observe(PixelStatistics{0, 0, 0}, true));
        const auto second = engine.observe(PixelStatistics{0, 2, 0}, true).first();
        QVERIFY(engine.activateDeferredFeedback(first.feedbackId));
        static_cast<void>(engine.feedbackSuppressed(second.feedbackId));
        static_cast<void>(engine.feedbackCompleted(first.feedbackId));
        QCOMPARE(triggerCount(engine.observe(2, 0)), 0);
        static_cast<void>(engine.observe(0, 0));
        QCOMPARE(triggerCount(engine.observe(2, 0)), 1);
    }

    void deferredNoneFeedbackDoesNotJumpUntilApproved()
    {
        auto rule = redRule();
        rule.postTriggerAction = PostTriggerAction::Jump;
        ColorLogicEngine engine({rule, redRule(2)});
        engine.setSlotRule(1, 1);
        engine.setSlotRule(2, 2);
        engine.setEnabled(true);
        const auto pending = engine.observe(PixelStatistics{0, 2, 0}, true);
        QCOMPARE(pending.size(), 1);
        QCOMPARE(engine.activeSlot(), 1);
        QVERIFY(engine.activateDeferredFeedback(pending.first().feedbackId));
        QVERIFY(hasEffect(engine.feedbackCompleted(pending.first().feedbackId), ColorLogicEffectKind::SwitchActiveSlot));
        QCOMPARE(engine.activeSlot(), 2);
    }

    void suppressionSkipsPostActionAndDoesNotLoopOnTheSameCondition()
    {
        auto rule = redRule();
        rule.postTriggerAction = PostTriggerAction::Loop;
        ColorLogicEngine engine({rule});
        engine.setSlotRule(1, 1);
        engine.setEnabled(true);
        const auto pending = engine.observe(PixelStatistics{0, 2, 0}, true);
        QVERIFY(engine.feedbackSuppressed(pending.first().feedbackId).isEmpty());
        QCOMPARE(triggerCount(engine.observe(2, 0)), 0);
        static_cast<void>(engine.observe(0, 0));
        QCOMPARE(triggerCount(engine.observe(2, 0)), 1);
    }

    void triggersForBlueAndCustomColorWithItsOwnTolerance()
    {
        ColorRule rule = redRule();
        rule.triggerCondition1 = {QStringLiteral("蓝色"), QStringLiteral(">=1")};
        rule.triggerCondition2 = {QStringLiteral("#00ff00"), QStringLiteral(">=2"), 16};
        ColorLogicEngine engine({rule});
        engine.setSlotRule(1, 1);
        engine.setEnabled(true);
        QImage image(3, 1, QImage::Format_RGB32);
        image.fill(Qt::green);
        image.setPixel(0, 0, qRgb(48, 79, 91));
        image.setPixel(1, 0, qRgb(16, 240, 16));
        const auto statistics = calculatePixelStatistics(image, std::nullopt,
            {{QStringLiteral("#00ff00"), 16}, {QStringLiteral("#00ff00"), 0}});
        QCOMPARE(triggerCount(engine.observe(statistics)), 1);
        rule.triggerCondition2.tolerance = 0;
        engine.setRules({rule});
        engine.setSlotRule(1, 1);
        QCOMPARE(triggerCount(engine.observe(statistics)), 0);
        // A missing requested color must never satisfy a zero-count rule.
        rule.triggerCondition2 = {QStringLiteral("#123456"), QStringLiteral("<=0"), 0};
        engine.setRules({rule});
        engine.setSlotRule(1, 1);
        QCOMPARE(triggerCount(engine.observe(statistics)), 0);
    }

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
