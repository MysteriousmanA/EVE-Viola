#include "ui/main_window.h"

#include <optional>
#include <QCoreApplication>
#include <QScopedValueRollback>
#include <QTimer>
#include "features/region_preset/preset_service.h"
#include "platform/window_catalog.h"
#include "ui/replica_row.h"
#include "ui/replica_window.h"

namespace {

inline constexpr int kRegionRestoreRetryIntervalMs = 1000;
inline constexpr int kRegionRestoreRetryCount = 5;

} // namespace

void MainWindow::rebuildReplicas()
{
    replicaController_.clearDormant();
    const auto windows = windows_.values();
    for (auto* window : windows) {
        window->close();
    }
    QCoreApplication::processEvents();
    restoreConfiguredReplicas();
    refreshReplicaFilters();
    refreshReplicaGroups();
}

void MainWindow::setupGameRestoreMonitoring()
{
    windowCatalog_->setMonitoring(config_.restoreWindowsWithGame
        || config_.pagePartition.onlyShowWhenEveDetected);
    if (config_.restoreWindowsWithGame) {
        QTimer::singleShot(0, this, &MainWindow::restorePendingGameWindows);
    }
}

void MainWindow::restorePendingGameWindows()
{
    if (!config_.restoreWindowsWithGame || closing_) {
        return;
    }
    QList<WindowInfo> available;
    try {
        available = windowCatalog_->windows();
    } catch (const NativeException&) {
        return;
    }
    for (const auto& config : PresetService::takeRestorable(replicaController_, available)) {
        restoreReplica(config, available);
    }
    for (auto it = windows_.cbegin(); it != windows_.cend(); ++it) {
        ReplicaWindow* window = it.value();
        if (window == nullptr || window->source().has_value()) {
            continue;
        }
        const ReplicaConfig desired = replicaController_.desiredConfig(it.key());
        if (desired.sourceTitle.isEmpty()) {
            continue;
        }
        const auto* candidate = PresetService::findSource(
            available, desired.sourceTitle);
        if (candidate == nullptr) {
            continue;
        }
        const QScopedValueRollback restoring(restoringReplicas_, true);
        window->selectSource(*candidate);
        if (!window->source().has_value()) {
            continue;
        }
        if (desired.capturedRegion.isValid() && !desired.capturedRegion.isEmpty()) {
            window->setCapturedRegion(desired.capturedRegion);
        }
        if (desired.size.isValid() && !desired.size.isEmpty()) {
            window->resize(desired.size);
        }
        window->moveTo(desired.position);
        if (auto* row = rows_.value(it.key()); row != nullptr) {
            row->setVisibilityChecked(desired.visible);
        }
        if (desired.capturedRegion.isValid() && !desired.capturedRegion.isEmpty()
            && window->capturedRegion() != std::optional<QRect>(desired.capturedRegion)) {
            QTimer::singleShot(
                kRegionRestoreRetryIntervalMs,
                this,
                [this, id = it.key()] {
                    retryReplicaRegionRestore(id, kRegionRestoreRetryCount);
                });
        }
    }
}

void MainWindow::retryReplicaRegionRestore(ReplicaId id, int remainingAttempts)
{
    const QScopedValueRollback restoring(restoringReplicas_, true);
    if (remainingAttempts <= 0 || closing_) {
        return;
    }
    ReplicaWindow* window = windows_.value(id);
    const ReplicaConfig desired = replicaController_.desiredConfig(id);
    if (window == nullptr || !window->source().has_value()
        || !desired.capturedRegion.isValid() || desired.capturedRegion.isEmpty()) {
        return;
    }
    window->setCapturedRegion(desired.capturedRegion);
    if (window->capturedRegion() == std::optional<QRect>(desired.capturedRegion)) {
        window->moveTo(desired.position);
        return;
    }
    QTimer::singleShot(
        kRegionRestoreRetryIntervalMs,
        this,
        [this, id, remainingAttempts] {
            retryReplicaRegionRestore(id, remainingAttempts - 1);
        });
}

ReplicaWindow* MainWindow::restoreReplica(
    const ReplicaConfig& config,
    const QList<WindowInfo>& availableWindows,
    const WindowInfo* sourceOverride,
    bool useDefaultPosition)
{
    const QScopedValueRollback restoring(restoringReplicas_, true);
    auto* window = createReplica(availableWindows);
    const QPoint defaultPosition = window->pos();
    replicaController_.setDesiredConfig(window->replicaId(), config);
    auto* row = rows_.value(window->replicaId());
    if (row == nullptr) {
        return nullptr;
    }
    row->setColorLogicSlotColor(1, config.slotColor1);
    row->setColorLogicSlotColor(2, config.slotColor2);
    row->setColorLogicSlot(1, config.colorLogicSlotRule[0]);
    row->setColorLogicSlot(2, config.colorLogicSlotRule[1]);
    row->setColorLogicEnabled(config.colorLogicEnabled);
    row->setActiveColorLogicSlot(config.activeColorLogicSlot);
    // Do not let a half-restored window enter the capture worker.
    window->setStatisticsInterval(kStatisticsIntervalDisabledMs);
    if (sourceOverride != nullptr) {
        window->selectSource(*sourceOverride);
    } else if (!config.sourceTitle.isEmpty()) {
        if (const auto* candidate = PresetService::findSource(
                availableWindows, config.sourceTitle)) {
            window->selectSource(*candidate);
        }
    }
    if (config.capturedRegion.isValid() && !config.capturedRegion.isEmpty()) {
        window->setCapturedRegion(config.capturedRegion);
    }
    row->setSourceNameVisible(config.sourceNameVisible);
    row->setVisibilityChecked(config.visible);
    row->setCollapsed(config.collapsed);
    if (config.size.isValid() && !config.size.isEmpty()) {
        window->resize(config.size);
    }
    window->moveTo(useDefaultPosition ? defaultPosition : config.position);
    window->setStatisticsInterval(config.statisticsIntervalMs);
    auto restored = replicaController_.desiredConfig(window->replicaId());
    if (window->source()) { restored.sourceTitle = window->source()->title; }
    restored.position = window->pos();
    restored.size = window->size();
    restored.colorLogicSlotRule[0] = window->colorLogicSlotRule(1);
    restored.colorLogicSlotRule[1] = window->colorLogicSlotRule(2);
    restored.colorLogicEnabled = window->colorLogicEnabled();
    restored.activeColorLogicSlot = window->activeColorLogicSlot();
    replicaController_.setDesiredConfig(window->replicaId(), restored);
    if (window->source().has_value()
        && config.capturedRegion.isValid() && !config.capturedRegion.isEmpty()
        && window->capturedRegion() != std::optional<QRect>(config.capturedRegion)) {
        QTimer::singleShot(
            kRegionRestoreRetryIntervalMs,
            this,
            [this, id = window->replicaId()] {
                retryReplicaRegionRestore(id, kRegionRestoreRetryCount);
            });
    }
    return window;
}

void MainWindow::restoreConfiguredReplicas()
{
    restoringReplicas_ = true;
    if (persistTimer_ != nullptr) {
        persistTimer_->stop();
    }
    if (statisticsCoordinator_ != nullptr) {
        statisticsCoordinator_->stop();
    }
    const QList<ReplicaConfig> configured = config_.replicas;
    QList<WindowInfo> available;
    if (!configured.isEmpty()) {
        try {
            available = windowCatalog_->windows();
        } catch (const NativeException&) {
        }
    }
    const auto plan = PresetService::planRestore(configured, available, config_.restoreWindowsWithGame);
    for (const auto& replicaConfig : plan.dormant) {
        replicaController_.appendDormant(replicaConfig);
    }
    for (const auto& replicaConfig : plan.active) {
        restoreReplica(replicaConfig, available);
    }
    if (statisticsCoordinator_ != nullptr) {
        statisticsCoordinator_->start();
    }
    restoringReplicas_ = false;
    refreshSummary();
}
