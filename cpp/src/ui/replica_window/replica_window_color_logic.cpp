#include "ui/replica_window.h"

void ReplicaWindow::setColorRules(const QList<ColorRule>& rules)
{
    if (appliedColorRules_ == rules) { return; }
    colorLogicEngine_.resetNotificationObservation();
    appliedColorRules_ = rules;
    gateway_.cancelAutomaticActivation(replicaId_);
    applyColorLogicEffects(colorLogicEngine_.setRules(rules));
    syncStatisticsRegistration();
    emit settingsChanged();
}

void ReplicaWindow::setColorLogicSlot(int slotIndex, int ruleNumber)
{
    if (slotIndex < 1 || slotIndex > 2) {
        return;
    }
    if (colorLogicEngine_.ruleNumberForSlot(slotIndex) == ruleNumber) { return; }
    colorLogicEngine_.resetNotificationObservation();
    gateway_.cancelAutomaticActivation(replicaId_);
    applyColorLogicEffects(colorLogicEngine_.setSlotRule(slotIndex, ruleNumber));
    syncStatisticsRegistration();
    emit settingsChanged();
}

void ReplicaWindow::setColorLogicEnabled(bool enabled)
{
    if (colorLogicEngine_.enabled() == enabled) {
        return;
    }
    colorLogicEngine_.resetNotificationObservation();
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
    colorLogicEngine_.resetNotificationObservation();
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
    if (globalColorLogicManaged_) { return; }
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

void ReplicaWindow::evaluateColorLogic(const PixelStatistics& statistics)
{
    lastRedCount_ = statistics.redCount;
    lastWhiteCount_ = statistics.whiteCount;
    lastStatistics_ = statistics;
    if (globalColorLogicManaged_ || desktopPetMode_ == DesktopPetMode::DoNotDisturb) { return; }
    if (desktopPetMode_ == DesktopPetMode::Fish) {
        if (const auto number = colorLogicEngine_.observeNotification(statistics)) {
            emit colorLogicNotificationRequested(*number);
        }
        return;
    }
    applyColorLogicEffects(colorLogicEngine_.observe(statistics));
}

void ReplicaWindow::setLogicRequiredColors(const QList<PixelColorTarget>& colors)
{
    if (logicRequiredColors_ == colors) { return; }
    logicRequiredColors_ = colors;
    emit pixelStatisticsUnavailable();
    syncStatisticsRegistration();
}
void ReplicaWindow::executeSharedColorLogic(int ruleNumber, quint64 token, bool popup,
    std::function<void()> completed)
{
    ColorLogicEffect effect;
    effect.kind = ColorLogicEffectKind::Triggered;
    handleColorLogicTrigger(effect, ruleNumber);
    if (!popup) { return; }
    if (isVisible() || desktopPetMode_ != DesktopPetMode::Resident || presentationSuppressed_) {
        completed(); return;
    }
    sharedPopupCallbacks_.insert(token, std::move(completed));
    setVisible(true);
    raise();
    activateWindow();
    emit visibilityChanged(true);
}
void ReplicaWindow::cancelSharedColorLogic(quint64 token) { sharedPopupCallbacks_.remove(token); }
void ReplicaWindow::setSharedFlashActive(bool active)
{
    if (flashActive_ == active) { return; }
    gateway_.setAutomaticTriggerActive(replicaId_, active);
    if (!active) { gateway_.cancelAutomaticActivation(replicaId_); }
    setFlashActive(active);
}
