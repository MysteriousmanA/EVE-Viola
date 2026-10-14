#include "ui/replica_window.h"

#include <utility>

void ReplicaWindow::initializeAudioFeedback()
{
    connect(
        &audioFeedbackManager_,
        &AudioFeedbackManager::feedbackCompleted,
        this,
        [this](ReplicaId id, quint64 token) {
            if (id != replicaId_
                || token != activeAudioRequestToken_
                || activeAudioFeedbackId_ == 0) {
                return;
            }
            activeAudioRequestToken_ = 0;
            const quint64 feedbackId = activeAudioFeedbackId_;
            activeAudioFeedbackId_ = 0;
            applyColorLogicEffects(colorLogicEngine_.feedbackCompleted(feedbackId));
        });
    connect(
        &audioFeedbackManager_,
        &AudioFeedbackManager::feedbackFailed,
        this,
        [this](ReplicaId id, quint64 token) {
            if (id != replicaId_
                || token != activeAudioRequestToken_
                || activeAudioFeedbackId_ == 0) {
                return;
            }
            activeAudioRequestToken_ = 0;
            const quint64 feedbackId = activeAudioFeedbackId_;
            activeAudioFeedbackId_ = 0;
            applyColorLogicEffects(colorLogicEngine_.feedbackFailed(feedbackId));
        });
}

void ReplicaWindow::setFishModeEnabled(bool enabled)
{
    if (fishModeEnabled_ == enabled) {
        return;
    }
    fishModeEnabled_ = enabled;
    audioFeedbackEnabled_ = !enabled;
    if (enabled) {
        gateway_.cancelAutomaticActivation(replicaId_);
        setVisible(false);
        if (activeAudioFeedbackId_ != 0) {
            const quint64 feedbackId = activeAudioFeedbackId_;
            const quint64 token = activeAudioRequestToken_;
            activeAudioFeedbackId_ = 0;
            activeAudioRequestToken_ = 0;
            audioFeedbackManager_.cancel(replicaId_, token);
            applyColorLogicEffects(colorLogicEngine_.feedbackCompleted(feedbackId));
        }
    }
}

void ReplicaWindow::applyColorLogicEffects(const QList<ColorLogicEffect>& effects)
{
    QList<ColorLogicEffect> pending = effects;
    while (!pending.isEmpty()) {
        const ColorLogicEffect effect = pending.takeFirst();
        switch (effect.kind) {
        case ColorLogicEffectKind::Triggered: {
            handleColorLogicTrigger(effect);
            if (effect.feedback == TriggerFeedback::Popup) {
                if (isVisible() || fishModeEnabled_ || presentationSuppressed_) {
                    auto followUp = colorLogicEngine_.feedbackCompleted(effect.feedbackId);
                    followUp.append(std::move(pending));
                    pending = std::move(followUp);
                } else {
                    popupRevealed_ = true;
                    popupRevealedFeedbackId_ = effect.feedbackId;
                    setVisible(true);
                    raise();
                    activateWindow();
                    emit visibilityChanged(true);
                }
            } else if (effect.feedback == TriggerFeedback::Audio) {
                if (!audioFeedbackEnabled_ || effect.audioPath.isEmpty()) {
                    auto followUp = colorLogicEngine_.feedbackCompleted(effect.feedbackId);
                    followUp.append(std::move(pending));
                    pending = std::move(followUp);
                } else {
                    activeAudioFeedbackId_ = effect.feedbackId;
                    activeAudioRequestToken_ = ++nextAudioRequestToken_;
                    if (audioFeedbackManager_.request(
                            replicaId_, activeAudioRequestToken_, effect.audioPath)
                        != AudioRequestDisposition::Joined) {
                        const quint64 failedFeedbackId = activeAudioFeedbackId_;
                        activeAudioFeedbackId_ = 0;
                        activeAudioRequestToken_ = 0;
                        auto followUp = colorLogicEngine_.feedbackFailed(failedFeedbackId);
                        followUp.append(std::move(pending));
                        pending = std::move(followUp);
                    }
                }
            }
            break;
        }
        case ColorLogicEffectKind::CancelAudio:
            if (effect.feedback == TriggerFeedback::Audio
                && activeAudioFeedbackId_ == effect.feedbackId) {
                const quint64 token = activeAudioRequestToken_;
                activeAudioFeedbackId_ = 0;
                activeAudioRequestToken_ = 0;
                audioFeedbackManager_.cancel(replicaId_, token);
            }
            break;
        case ColorLogicEffectKind::SwitchActiveSlot:
            applyColorLogicBorder();
            emit activeColorLogicSlotChanged(colorLogicEngine_.activeSlot());
            break;
        case ColorLogicEffectKind::SetFlashActive:
            gateway_.setAutomaticTriggerActive(replicaId_, effect.flashActive);
            setFlashActive(effect.flashActive);
            break;
        }
    }
}
