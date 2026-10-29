#include "ui/replica_window.h"

#include "config/config_validation.h"

void ReplicaWindow::initializeStatistics()
{
    statisticsCoordinator_.setResultHandler(replicaId_, this,
        [this](const std::optional<PixelStatistics>& result) {
            if (!result) {
                emit pixelStatisticsUnavailable();
                return;
            }
            const auto& statistics = *result;
            emit pixelStatisticsChanged(statistics.redCount, statistics.whiteCount);
            emit colorStatisticsChanged(statistics);
            evaluateColorLogic(statistics);
        });
}

int ReplicaWindow::statisticsIntervalMs() const noexcept
{
    return statisticsIntervalMs_;
}

void ReplicaWindow::setStatisticsInterval(int milliseconds)
{
    if (!isValidStatisticsIntervalMs(milliseconds)) {
        return;
    }
    const bool changed = statisticsIntervalMs_ != milliseconds;
    statisticsIntervalMs_ = milliseconds;
    const bool enabled = milliseconds != kStatisticsIntervalDisabledMs;
    if (!enabled) { cancelAdvancedLogic(); }
    if (enabled != statisticsEnabled_) {
        statisticsEnabled_ = enabled;
        if (!enabled) {
            emit pixelStatisticsUnavailable();
        }
    }
    syncStatisticsRegistration();
    if (changed) {
        emit statisticsIntervalChanged(milliseconds);
    }
}

void ReplicaWindow::syncStatisticsRegistration()
{
    if (!statisticsEnabled_ || !source_.has_value()) {
        statisticsCoordinator_.remove(replicaId_);
        return;
    }
    auto targets = statisticsColors_;
    for (const auto& rule : appliedColorRules_) {
        if (rule.number != colorLogicSlotRule(1) && rule.number != colorLogicSlotRule(2)) { continue; }
        for (const auto& condition : {rule.triggerCondition1, rule.triggerCondition2}) {
            if (!condition.isEmpty() && pixelColorValue(condition.pixel).isValid()) {
                const PixelColorTarget target{condition.pixel, condition.tolerance};
                if (!targets.contains(target)) { targets.append(target); }
            }
        }
    }
    statisticsCoordinator_.upsert(
        replicaId_,
        source_->hwnd,
        capturedRegion_,
        statisticsIntervalMs_, targets);
}

void ReplicaWindow::setStatisticsColors(const QList<PixelColorTarget>& colors)
{
    const auto normalized = normalizedPixelColorTargets(colors);
    if (normalized == statisticsColors_) { return; }
    statisticsColors_ = normalized;
    emit pixelStatisticsUnavailable();
    syncStatisticsRegistration();
    emit settingsChanged();
}
