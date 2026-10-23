#include "ui/replica_window.h"
#include "features/advanced_logic/advanced_logic_service.h"
#include <QScopedValueRollback>

void ReplicaWindow::setAdvancedLogicService(AdvancedLogicService* service)
{
    if (advancedLogicService_ == service) { return; }
    cancelAdvancedLogic();
    advancedLogicService_ = service;
}

void ReplicaWindow::cancelAdvancedLogic()
{
    colorLogicEngine_.resetNotificationObservation();
    const auto feedbackIds = pendingAdvancedFeedbacks_.keys();
    pendingAdvancedFeedbacks_.clear();
    approvedAdvancedFeedbacks_.clear();
    for (const auto feedbackId : feedbackIds) {
        static_cast<void>(colorLogicEngine_.feedbackSuppressed(feedbackId));
    }
    if (advancedLogicService_) { advancedLogicService_->cancelOwner(replicaId_); }
}

void ReplicaWindow::drainAdvancedLogic()
{
    if (drainingAdvancedLogic_) { return; }
    const QScopedValueRollback guard(drainingAdvancedLogic_, true);
    while (!approvedAdvancedFeedbacks_.isEmpty() && !colorLogicEngine_.hasActiveFeedback()) {
        const auto id = approvedAdvancedFeedbacks_.takeFirst();
        if (!pendingAdvancedFeedbacks_.contains(id)) { continue; }
        const auto pending = pendingAdvancedFeedbacks_.take(id);
        if (!colorLogicEngine_.activateDeferredFeedback(id)) { continue; }
        const auto currentStatistics = lastStatistics_;
        const auto currentRed = lastRedCount_;
        const auto currentWhite = lastWhiteCount_;
        lastStatistics_ = pending.statistics;
        lastRedCount_ = pending.statistics.redCount;
        lastWhiteCount_ = pending.statistics.whiteCount;
        applyColorLogicEffects({pending.effect});
        if (pending.effect.feedback == TriggerFeedback::None) {
            applyColorLogicEffects(colorLogicEngine_.feedbackCompleted(id));
        }
        lastStatistics_ = currentStatistics;
        lastRedCount_ = currentRed;
        lastWhiteCount_ = currentWhite;
        ColorLogicEffect flash;
        flash.kind = ColorLogicEffectKind::SetFlashActive;
        flash.flashActive = true;
        applyColorLogicEffects({flash});
    }
}

void ReplicaWindow::setColorRules(const QList<ColorRule>& rules)
{
    if (appliedColorRules_ == rules) { return; }
    cancelAdvancedLogic();
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
    cancelAdvancedLogic();
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
    cancelAdvancedLogic();
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
    cancelAdvancedLogic();
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

void ReplicaWindow::evaluateColorLogic(const PixelStatistics& statistics)
{
    lastRedCount_ = statistics.redCount;
    lastWhiteCount_ = statistics.whiteCount;
    lastStatistics_ = statistics;
    if (desktopPetMode_ == DesktopPetMode::DoNotDisturb) { return; }
    if (desktopPetMode_ == DesktopPetMode::Fish) {
        if (const auto number = colorLogicEngine_.observeNotification(statistics)) {
            emit colorLogicNotificationRequested(*number);
        }
        return;
    }
    const auto effects = colorLogicEngine_.observe(statistics, advancedLogicService_ != nullptr);
    for (const auto& effect : effects) {
        if (advancedLogicService_ && effect.kind == ColorLogicEffectKind::Triggered) {
            ColorLogicEffect clearFlash;
            clearFlash.kind = ColorLogicEffectKind::SetFlashActive;
            applyColorLogicEffects({clearFlash});
            pendingAdvancedFeedbacks_.insert(effect.feedbackId, {effect, statistics});
            const auto sourceKey = source_ ? QString::number(source_->hwnd)
                                          : QStringLiteral("replica:%1").arg(replicaId_);
            advancedLogicService_->submit(this, replicaId_, sourceKey,
                source_ ? source_->title : QString{}, colorLogicEngine_.ruleNumberForSlot(effect.slotIndex),
                [this, id = effect.feedbackId](bool execute) {
                    if (!pendingAdvancedFeedbacks_.contains(id)) { return; }
                    if (!execute) {
                        pendingAdvancedFeedbacks_.remove(id);
                        static_cast<void>(colorLogicEngine_.feedbackSuppressed(id));
                        return;
                    }
                    approvedAdvancedFeedbacks_.append(id);
                    drainAdvancedLogic();
                });
        } else if (pendingAdvancedFeedbacks_.isEmpty() || colorLogicEngine_.hasActiveFeedback()) {
            applyColorLogicEffects({effect});
        }
    }
}
