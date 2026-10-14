#include "ui/replica_window.h"

#include "config/config_validation.h"

void ReplicaWindow::initializeStatistics()
{
    connect(
        &statisticsCoordinator_,
        &StatisticsCoordinator::statisticsReady,
        this,
        [this](ReplicaId id, qint64 redCount, qint64 whiteCount) {
            if (id != replicaId_) {
                return;
            }
            emit pixelStatisticsChanged(redCount, whiteCount);
            evaluateColorLogic(redCount, whiteCount);
        });
    connect(
        &statisticsCoordinator_,
        &StatisticsCoordinator::statisticsUnavailable,
        this,
        [this](ReplicaId id) {
            if (id == replicaId_) {
                emit pixelStatisticsUnavailable();
            }
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
    statisticsCoordinator_.upsert(
        replicaId_,
        source_->hwnd,
        capturedRegion_,
        statisticsIntervalMs_);
}
