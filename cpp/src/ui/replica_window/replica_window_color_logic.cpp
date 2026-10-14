#include "ui/replica_window.h"

void ReplicaWindow::setColorRules(const QList<ColorRule>& rules)
{
    if (appliedColorRules_ == rules) { return; }
    appliedColorRules_ = rules;
    gateway_.cancelAutomaticActivation(replicaId_);
    applyColorLogicEffects(colorLogicEngine_.setRules(rules));
    emit settingsChanged();
}

void ReplicaWindow::setColorLogicSlot(int slotIndex, int ruleNumber)
{
    if (slotIndex < 1 || slotIndex > 2) {
        return;
    }
    if (colorLogicEngine_.ruleNumberForSlot(slotIndex) == ruleNumber) { return; }
    gateway_.cancelAutomaticActivation(replicaId_);
    applyColorLogicEffects(colorLogicEngine_.setSlotRule(slotIndex, ruleNumber));
    emit settingsChanged();
}

void ReplicaWindow::setColorLogicEnabled(bool enabled)
{
    if (colorLogicEngine_.enabled() == enabled) {
        return;
    }
    applyColorLogicEffects(colorLogicEngine_.setEnabled(enabled));
    if (!enabled) {
        gateway_.cancelAutomaticActivation(replicaId_);
    }
    applyColorLogicBorder();
    emit settingsChanged();
}

void ReplicaWindow::setActiveColorLogicSlot(int slotIndex)
{
    if (slotIndex < 1 || slotIndex > 2
        || colorLogicEngine_.activeSlot() == slotIndex) {
        return;
    }
    gateway_.cancelAutomaticActivation(replicaId_);
    applyColorLogicEffects(colorLogicEngine_.setActiveSlot(slotIndex));
    applyColorLogicBorder();
    emit settingsChanged();
}

void ReplicaWindow::setColorLogicSlotColor(int slotIndex, const QColor& color)
{
    if (slotIndex < 1 || slotIndex > 2 || !color.isValid()) {
        return;
    }
    if (colorLogicSlotColor_[slotIndex - 1] == color) { return; }
    colorLogicSlotColor_[slotIndex - 1] = color;
    if (colorLogicEngine_.enabled() && colorLogicEngine_.activeSlot() == slotIndex) {
        applyColorLogicBorder();
    }
    emit settingsChanged();
}

int ReplicaWindow::colorLogicSlotRule(int slotIndex) const noexcept
{
    return colorLogicEngine_.ruleNumberForSlot(slotIndex);
}

QColor ReplicaWindow::colorLogicSlotColor(int slotIndex) const noexcept
{
    return slotIndex >= 1 && slotIndex <= 2 ? colorLogicSlotColor_[slotIndex - 1] : QColor{};
}

void ReplicaWindow::toggleActiveColorLogicSlot()
{
    const int next = colorLogicEngine_.activeSlot() == 1 ? 2 : 1;
    if (next != colorLogicEngine_.activeSlot()) {
        setActiveColorLogicSlot(next);
        emit activeColorLogicSlotChanged(colorLogicEngine_.activeSlot());
    }
}

bool ReplicaWindow::colorLogicEnabled() const noexcept
{
    return colorLogicEngine_.enabled();
}

int ReplicaWindow::activeColorLogicSlot() const noexcept
{
    return colorLogicEngine_.activeSlot();
}

int ReplicaWindow::colorLogicTriggerCount(int slotIndex) const noexcept
{
    return colorLogicEngine_.triggerCount(slotIndex);
}

void ReplicaWindow::evaluateColorLogic(qint64 redCount, qint64 whiteCount)
{
    lastRedCount_ = redCount;
    lastWhiteCount_ = whiteCount;
    applyColorLogicEffects(colorLogicEngine_.observe(redCount, whiteCount));
}
