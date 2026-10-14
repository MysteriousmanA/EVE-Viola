#include "features/color_rule/color_logic_engine.h"
#include "features/color_rule/trigger_condition.h"

#include <algorithm>
#include <utility>

#include "features/replica/pixel_statistics.h"

ColorLogicEngine::ColorLogicEngine(QList<ColorRule> rules)
    : rules_(std::move(rules))
{
}

QList<ColorLogicEffect> ColorLogicEngine::setRules(QList<ColorRule> rules)
{
    QList<ColorLogicEffect> effects;
    for (int slot = 0; slot < 2; ++slot) {
        if (slots_[slot].ruleNumber != 0
            && std::none_of(rules.cbegin(), rules.cend(), [this, slot](const ColorRule& rule) {
                   return rule.number == slots_[slot].ruleNumber;
               })) {
            effects.append(cancelSlotFeedback(slot));
            slots_[slot].ruleNumber = 0;
            slots_[slot].condition = false;
            slots_[slot].replayRequested = false;
        }
    }
    rules_ = std::move(rules);
    return effects;
}

QList<ColorLogicEffect> ColorLogicEngine::setSlotRule(int slotIndex, int ruleNumber)
{
    if (slotIndex < 1 || slotIndex > 2) {
        return {};
    }
    const int slot = slotIndex - 1;
    auto effects = cancelSlotFeedback(slot);
    slots_[slot].ruleNumber = ruleNumber;
    slots_[slot].condition = false;
    slots_[slot].replayRequested = false;
    return effects;
}

QList<ColorLogicEffect> ColorLogicEngine::setEnabled(bool enabled)
{
    if (enabled_ == enabled) {
        return {};
    }
    enabled_ = enabled;
    QList<ColorLogicEffect> effects;
    for (int slot = 0; slot < 2; ++slot) {
        if (!enabled_) {
            effects.append(cancelSlotFeedback(slot));
        }
        slots_[slot].condition = false;
        slots_[slot].replayRequested = false;
    }
    return effects;
}

QList<ColorLogicEffect> ColorLogicEngine::setActiveSlot(int slotIndex)
{
    if (slotIndex < 1 || slotIndex > 2 || activeSlot_ == slotIndex) {
        return {};
    }
    auto effects = cancelSlotFeedback(activeSlot_ - 1);
    activeSlot_ = slotIndex;
    slots_[0].condition = false;
    slots_[1].condition = false;
    slots_[0].replayRequested = false;
    slots_[1].replayRequested = false;
    return effects;
}

QList<ColorLogicEffect> ColorLogicEngine::observe(qint64 redCount, qint64 whiteCount)
{
    if (!enabled_) {
        return {};
    }
    const int slot = activeSlot_ - 1;
    if (slot < 0 || slot >= 2) {
        return {};
    }

    const auto& state = slots_[slot];
    const ColorRule* rule = findRule(state.ruleNumber);
    const bool met = rule != nullptr && conditionMet(*rule, redCount, whiteCount);
    if (slots_[slot].busy) {
        slots_[slot].condition = met;
        return {ColorLogicEffect{
            ColorLogicEffectKind::SetFlashActive,
            slot + 1,
            0,
            TriggerFeedback::None,
            {},
            0,
            met}};
    }

    const bool triggered = met
        && (!slots_[slot].condition || slots_[slot].replayRequested);
    slots_[slot].condition = met;
    if (!met) {
        slots_[slot].replayRequested = false;
    }
    if (!triggered || rule == nullptr) {
        return {ColorLogicEffect{
            ColorLogicEffectKind::SetFlashActive,
            slot + 1,
            0,
            TriggerFeedback::None,
            {},
            0,
            false}};
    }

    ++slots_[slot].triggerCount;
    slots_[slot].busy = true;
    slots_[slot].replayRequested = false;
    ColorLogicEffect effect{
        ColorLogicEffectKind::Triggered,
        slot + 1,
        ++nextFeedbackId_,
        rule->triggerFeedback,
        rule->triggerFeedbackAudio,
        0,
        true};
    activeFeedback_ = ActiveFeedback{effect.feedbackId, slot, rule->triggerFeedback};
    if (rule->triggerFeedback == TriggerFeedback::None) {
        activeFeedback_.reset();
        auto effects = QList<ColorLogicEffect>{effect};
        effects.append(completeSlot(slot));
        effects.append(ColorLogicEffect{
            ColorLogicEffectKind::SetFlashActive,
            slot + 1,
            0,
            TriggerFeedback::None,
            {},
            0,
            true});
        return effects;
    }
    auto effects = QList<ColorLogicEffect>{effect};
    effects.append(ColorLogicEffect{
        ColorLogicEffectKind::SetFlashActive,
        slot + 1,
        0,
        TriggerFeedback::None,
        {},
        0,
        true});
    return effects;
}

QList<ColorLogicEffect> ColorLogicEngine::feedbackCompleted(quint64 feedbackId)
{
    if (!activeFeedback_.has_value() || activeFeedback_->id != feedbackId) {
        return {};
    }
    const int slot = activeFeedback_->slotIndex;
    activeFeedback_.reset();
    return completeSlot(slot);
}

QList<ColorLogicEffect> ColorLogicEngine::feedbackFailed(quint64 feedbackId)
{
    if (!activeFeedback_.has_value() || activeFeedback_->id != feedbackId) {
        return {};
    }
    const int slot = activeFeedback_->slotIndex;
    activeFeedback_.reset();
    failSlot(slot);
    return {};
}

QList<ColorLogicEffect> ColorLogicEngine::feedbackCancelled(quint64 feedbackId)
{
    if (!activeFeedback_.has_value() || activeFeedback_->id != feedbackId) {
        return {};
    }
    const int slot = activeFeedback_->slotIndex;
    activeFeedback_.reset();
    slots_[slot].busy = false;
    return {};
}

bool ColorLogicEngine::enabled() const noexcept
{
    return enabled_;
}

int ColorLogicEngine::activeSlot() const noexcept
{
    return activeSlot_;
}

int ColorLogicEngine::ruleNumberForSlot(int slotIndex) const noexcept
{
    return slotIndex >= 1 && slotIndex <= 2
        ? slots_[slotIndex - 1].ruleNumber
        : 0;
}

int ColorLogicEngine::triggerCount(int slotIndex) const noexcept
{
    return slotIndex >= 1 && slotIndex <= 2 ? slots_[slotIndex - 1].triggerCount : 0;
}

const ColorRule* ColorLogicEngine::findRule(int number) const
{
    for (const auto& rule : rules_) {
        if (rule.number == number) {
            return &rule;
        }
    }
    return nullptr;
}

bool ColorLogicEngine::conditionMet(
    const ColorRule& rule,
    qint64 redCount,
    qint64 whiteCount) const
{
    const ColorRuleCondition conditions[]{rule.triggerCondition1, rule.triggerCondition2};
    int activeConditionCount = 0;
    for (const auto& ruleCondition : conditions) {
        if (ruleCondition.isEmpty()) {
            continue;
        }
        ++activeConditionCount;
        const auto parsed = parseTriggerCondition(ruleCondition.expression);
        const qint64 count = relevantCount(ruleCondition, redCount, whiteCount);
        if (count < 0 || !parsed.has_value() || parsed->isEmpty()
            || !evaluateTriggerCondition(*parsed, static_cast<double>(count))) {
            return false;
        }
    }
    return activeConditionCount > 0;
}

qint64 ColorLogicEngine::relevantCount(
    const ColorRuleCondition& condition,
    qint64 redCount,
    qint64 whiteCount) const
{
    if (condition.pixel.compare(
            pixelColorCategoryName(PixelColorCategory::Red), Qt::CaseInsensitive) == 0) {
        return redCount;
    }
    if (condition.pixel.compare(
            pixelColorCategoryName(PixelColorCategory::White), Qt::CaseInsensitive) == 0) {
        return whiteCount;
    }
    return -1;
}

QList<ColorLogicEffect> ColorLogicEngine::cancelSlotFeedback(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= 2) {
        return {};
    }
    if (!activeFeedback_.has_value() || activeFeedback_->slotIndex != slotIndex) {
        slots_[slotIndex].busy = false;
        return {};
    }
    const quint64 feedbackId = activeFeedback_->id;
    const auto feedback = activeFeedback_->feedback;
    activeFeedback_.reset();
    slots_[slotIndex].busy = false;
    return {ColorLogicEffect{
        ColorLogicEffectKind::CancelAudio,
        slotIndex + 1,
        feedbackId,
        feedback,
        {},
        0,
        false}};
}

QList<ColorLogicEffect> ColorLogicEngine::completeSlot(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= 2) {
        return {};
    }
    slots_[slotIndex].busy = false;
    const ColorRule* rule = findRule(slots_[slotIndex].ruleNumber);
    if (rule == nullptr) {
        return {};
    }
    switch (rule->postTriggerAction) {
    case PostTriggerAction::Loop:
        slots_[slotIndex].replayRequested = true;
        return {};
    case PostTriggerAction::Jump: {
        const int other = activeSlot_ == 1 ? 2 : 1;
        if (enabled_ && other != activeSlot_) {
            activeSlot_ = other;
            slots_[0].condition = false;
            slots_[1].condition = false;
            slots_[0].replayRequested = false;
            slots_[1].replayRequested = false;
            return {ColorLogicEffect{
                ColorLogicEffectKind::SwitchActiveSlot,
                slotIndex + 1,
                0,
                TriggerFeedback::None,
                {},
                activeSlot_}};
        }
        return {};
    }
    case PostTriggerAction::ReverseWait:
        return {};
    }
    return {};
}

void ColorLogicEngine::failSlot(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= 2) {
        return;
    }
    slots_[slotIndex].busy = false;
    slots_[slotIndex].condition = true;
    slots_[slotIndex].replayRequested = false;
}
