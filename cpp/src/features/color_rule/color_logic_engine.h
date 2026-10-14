#pragma once

#include <array>
#include <optional>

#include <QList>
#include <QString>
#include <QtTypes>

#include "config/config_types.h"

enum class ColorLogicEffectKind {
    Triggered,
    CancelAudio,
    SwitchActiveSlot,
    SetFlashActive,
};

struct ColorLogicEffect {
    ColorLogicEffectKind kind{};
    int slotIndex{};
    quint64 feedbackId{};
    TriggerFeedback feedback{TriggerFeedback::None};
    QString audioPath;
    int activeSlot{};
    bool flashActive{};
};

class ColorLogicEngine final {
public:
    explicit ColorLogicEngine(QList<ColorRule> rules = {});

    QList<ColorLogicEffect> setRules(QList<ColorRule> rules);
    QList<ColorLogicEffect> setSlotRule(int slotIndex, int ruleNumber);
    QList<ColorLogicEffect> setEnabled(bool enabled);
    QList<ColorLogicEffect> setActiveSlot(int slotIndex);

    [[nodiscard]] QList<ColorLogicEffect> observe(qint64 redCount, qint64 whiteCount);
    [[nodiscard]] QList<ColorLogicEffect> feedbackCompleted(quint64 feedbackId);
    [[nodiscard]] QList<ColorLogicEffect> feedbackFailed(quint64 feedbackId);
    [[nodiscard]] QList<ColorLogicEffect> feedbackCancelled(quint64 feedbackId);

    [[nodiscard]] bool enabled() const noexcept;
    [[nodiscard]] int activeSlot() const noexcept;
    [[nodiscard]] int ruleNumberForSlot(int slotIndex) const noexcept;
    [[nodiscard]] int triggerCount(int slotIndex) const noexcept;

private:
    struct SlotState {
        int ruleNumber{};
        bool busy{};
        bool condition{};
        bool replayRequested{};
        int triggerCount{};
    };

    struct ActiveFeedback {
        quint64 id{};
        int slotIndex{};
        TriggerFeedback feedback{TriggerFeedback::None};
    };

    [[nodiscard]] const ColorRule* findRule(int number) const;
    [[nodiscard]] bool conditionMet(
        const ColorRule& rule,
        qint64 redCount,
        qint64 whiteCount) const;
    [[nodiscard]] qint64 relevantCount(
        const ColorRuleCondition& condition,
        qint64 redCount,
        qint64 whiteCount) const;
    [[nodiscard]] QList<ColorLogicEffect> cancelSlotFeedback(int slotIndex);
    [[nodiscard]] QList<ColorLogicEffect> completeSlot(int slotIndex);
    void failSlot(int slotIndex);

    QList<ColorRule> rules_;
    std::array<SlotState, 2> slots_{};
    std::optional<ActiveFeedback> activeFeedback_;
    quint64 nextFeedbackId_{};
    bool enabled_{false};
    int activeSlot_{1};
};
