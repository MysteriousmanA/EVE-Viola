#include "ui/replica_window.h"

#include <QTimer>
#include <QJsonArray>
#include "config/config_validation.h"
#include "runtime/runtime_logger.h"

void ReplicaWindow::handleColorLogicTrigger(const ColorLogicEffect& effect)
{
    const int ruleNumber = colorLogicEngine_.ruleNumberForSlot(effect.slotIndex);
    emit colorLogicTriggered(ruleNumber);
    const QString smallWindowName = QStringLiteral("小窗 #%1").arg(replicaId_);
    const bool hasSourceWindow = source_.has_value() && source_->hwnd != 0;
    const bool activationRequested = automaticActivationEnabled_ && hasSourceWindow;
    QJsonArray colorCounts;
    for (const auto& target : defaultPixelColorTargets()) {
        colorCounts.append(QJsonObject{{QStringLiteral("color"), target.color},
            {QStringLiteral("tolerance"), target.tolerance},
            {QStringLiteral("count"), static_cast<double>(lastStatistics_.count(target))}});
    }
    for (const auto& entry : lastStatistics_.colors) {
        colorCounts.append(QJsonObject{{QStringLiteral("color"), entry.target.color},
            {QStringLiteral("tolerance"), entry.target.tolerance},
            {QStringLiteral("count"), static_cast<double>(entry.count)}});
    }
    const QString triggerEventId = RuntimeLogger::writeEvent(
        QStringLiteral("color_logic_triggered"),
        QJsonObject{
            {QStringLiteral("replica_id"), static_cast<double>(replicaId_)},
            {QStringLiteral("small_window"), smallWindowName},
            {QStringLiteral("slot"), effect.slotIndex},
            {QStringLiteral("rule_number"), ruleNumber},
            {QStringLiteral("condition_satisfied"), true},
            {QStringLiteral("red_pixel_count"), static_cast<double>(lastRedCount_)},
            {QStringLiteral("white_pixel_count"), static_cast<double>(lastWhiteCount_)},
            {QStringLiteral("blue_pixel_count"), static_cast<double>(lastStatistics_.blueCount)},
            {QStringLiteral("color_pixel_counts"), colorCounts},
            {QStringLiteral("topmost_activation_requested"), activationRequested},
            {QStringLiteral("target_window_title"),
             source_.has_value() ? source_->title : QString{}},
            {QStringLiteral("target_hwnd"),
             RuntimeLogger::formatWindowHandle(
                 hasSourceWindow ? source_->hwnd : 0)},
        });

    if (activationRequested) {
        gateway_.queueAutomaticActivation(
            replicaId_,
            source_->hwnd,
            automaticActivationCooldownMs_,
            triggerEventId,
            smallWindowName,
            source_->title);
        QTimer::singleShot(0, this, [this] {
            try {
                gateway_.flushAutomaticActivations();
            } catch (const NativeException& error) {
                showFailure(QStringLiteral("无法置顶源窗口"), error);
            }
        });
    } else {
        RuntimeLogger::writeEvent(
            QStringLiteral("window_activation"),
            QJsonObject{
                {QStringLiteral("origin"), QStringLiteral("color_logic")},
                {QStringLiteral("trigger_event_id"), triggerEventId},
                {QStringLiteral("replica_id"), static_cast<double>(replicaId_)},
                {QStringLiteral("small_window"), smallWindowName},
                {QStringLiteral("target_window_title"),
                 hasSourceWindow ? source_->title : QString{}},
                {QStringLiteral("target_hwnd"),
                 RuntimeLogger::formatWindowHandle(
                     hasSourceWindow ? source_->hwnd : 0)},
                {QStringLiteral("result"), QStringLiteral("skipped")},
                {QStringLiteral("reason"),
                 automaticActivationEnabled_
                     ? QStringLiteral("no_selected_window")
                     : QStringLiteral("disabled_by_user")},
            });
    }
}

void ReplicaWindow::handleSourceClick()
{
    if (source_.has_value()) {
        QString activationResult = QStringLiteral("success");
        QString activationReason;
        try {
            gateway_.activateWindow(source_->hwnd);
        } catch (const NativeException& error) {
            activationResult = QStringLiteral("failed");
            activationReason = QStringLiteral("%1 (status %2)")
                                   .arg(error.operation())
                                   .arg(error.status());
            showFailure(QStringLiteral("无法激活窗口"), error);
        }
        RuntimeLogger::writeEvent(
            QStringLiteral("window_activation"),
            QJsonObject{
                {QStringLiteral("origin"), QStringLiteral("small_window_click")},
                {QStringLiteral("replica_id"), static_cast<double>(replicaId_)},
                {QStringLiteral("small_window"),
                 QStringLiteral("小窗 #%1").arg(replicaId_)},
                {QStringLiteral("target_window_title"), source_->title},
                {QStringLiteral("target_hwnd"),
                 RuntimeLogger::formatWindowHandle(source_->hwnd)},
                {QStringLiteral("result"), activationResult},
                {QStringLiteral("reason"), activationReason},
            });
        if (popupRevealed_) {
            const quint64 revealedFeedbackId = popupRevealedFeedbackId_;
            setVisible(false);
            emit visibilityChanged(false);
            popupRevealedFeedbackId_ = 0;
            applyColorLogicEffects(
                colorLogicEngine_.feedbackCompleted(revealedFeedbackId));
        }
    }
    if (raiseOnClickEnabled_ && isVisible()) {
        raise();
    }
}

bool ReplicaWindow::automaticActivationEnabled() const noexcept
{
    return automaticActivationEnabled_;
}

void ReplicaWindow::setAutomaticActivationEnabled(bool enabled)
{
    if (automaticActivationEnabled_ == enabled) {
        return;
    }
    automaticActivationEnabled_ = enabled;
    if (!enabled) {
        gateway_.cancelAutomaticActivation(replicaId_);
    }
}

int ReplicaWindow::automaticActivationCooldownMs() const noexcept
{
    return automaticActivationCooldownMs_;
}

void ReplicaWindow::setAutomaticActivationCooldownMs(int milliseconds)
{
    if (isValidAutomaticActivationCooldownMs(milliseconds)) {
        automaticActivationCooldownMs_ = milliseconds;
    }
}
