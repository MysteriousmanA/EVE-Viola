#include "ui/main_window.h"

#include "ui/desktop_pet_page.h"
#include "ui/radial_switch_overlay.h"
#include "ui/replica_row.h"
#include "ui/replica_window.h"

void MainWindow::connectReplicaSignals(ReplicaWindow* window, ReplicaRow* row)
{
    connect(
        row,
        &ReplicaRow::replicaVisibilityChanged,
        this,
        [this, window, id = window->replicaId()](bool visible) {
            const auto config = replicaController_.setUserVisible(id, visible);
            if (!visible) {
                if (auto* row = rows_.value(id)) {
                    row->setColorLogicSlot(2, config.colorLogicSlotRule[1]);
                    row->setActiveColorLogicSlot(config.activeColorLogicSlot);
                    row->setColorLogicEnabled(config.colorLogicEnabled);
                }
            }
            if (runtimePolicy_.replicaVisible(visible)) { window->show(); }
            else { window->hide(); }
        });
    connect(
        window,
        &ReplicaWindow::visibilityChanged,
        row,
        &ReplicaRow::setVisibilityChecked);
    connect(
        window,
        &ReplicaWindow::colorLogicTriggered,
        desktopPetPage_,
        [this, window](int ruleNumber) {
            const QString sourceTitle = window->source().has_value()
                ? window->source()->title
                : QString{};
            desktopPetPage_->showRuleMessage(ruleNumber, sourceTitle);
        });
    connect(window, &ReplicaWindow::colorLogicNotificationRequested,
            desktopPetPage_, [this, window](int ruleNumber) {
                desktopPetPage_->showRuleMessage(ruleNumber,
                    window->source() ? window->source()->title : QString{});
            });
    connect(
        window,
        &ReplicaWindow::sourceTitleChanged,
        row,
        [window, row](const QString& title) {
            if (window->source().has_value()) {
                row->setSourceWindow(*window->source());
            } else {
                row->setSourceTitle(title);
            }
        });
    connect(
        window,
        &ReplicaWindow::sourceTitleChanged,
        this,
        [this](const QString&) {
            refreshReplicaFilters();
            refreshReplicaGroups();
        });
    connect(
        window,
        &ReplicaWindow::sourceUnavailable,
        this,
        [this, id = window->replicaId()](const QString& title) {
            auto desired = replicaController_.desiredConfig(id);
            if (!title.isEmpty()) {
                desired.sourceTitle = title;
                replicaController_.setDesiredConfig(id, desired);
            }
            if (config_.restoreWindowsWithGame) {
                replicaController_.appendDormant(replicaConfigFor(id, desired));
            }
        });
    connect(
        window,
        &ReplicaWindow::regionCaptureChanged,
        row,
        &ReplicaRow::setCaptureRegion);
    connect(
        row,
        &ReplicaRow::regionEditRequested,
        window,
        &ReplicaWindow::setCapturedRegion);
    connect(
        row,
        &ReplicaRow::regionPresetSelected,
        this,
        [this, window, row](const RegionPreset& preset) {
            window->applyRegionPreset(preset);
            row->setCurrentRegionPresetName(preset.name);
            refreshReplicaFilters();
            refreshReplicaGroups();
        });
    connect(
        window,
        &ReplicaWindow::regionCaptured,
        row,
        &ReplicaRow::setCurrentRegion);
    connect(
        window,
        &ReplicaWindow::regionCaptureChanged,
        this,
        [this](const QString&) {
            refreshReplicaFilters();
            refreshReplicaGroups();
        });
    connect(
        window,
        &ReplicaWindow::positionChanged,
        row,
        &ReplicaRow::setWindowPosition);
    connect(
        window,
        &ReplicaWindow::positionChanged,
        this,
        [this](const QPoint&) { scheduleReplicaOverlapOrder(); });
    connect(
        window,
        &ReplicaWindow::sizeChanged,
        row,
        &ReplicaRow::setWindowSize);
    connect(
        window,
        &ReplicaWindow::sizeChanged,
        this,
        [this](const QSize&) { scheduleReplicaOverlapOrder(); });
    connect(
        row,
        &ReplicaRow::replicaVisibilityChanged,
        this,
        [this](bool) { scheduleReplicaOverlapOrder(); });
    connect(
        row,
        &ReplicaRow::positionEditRequested,
        window,
        &ReplicaWindow::moveTo);
    connect(
        row,
        &ReplicaRow::scalePercentageEdited,
        window,
        &ReplicaWindow::resizeToScalePercentage);
    connect(
        row,
        &ReplicaRow::statisticsIntervalChanged,
        window,
        &ReplicaWindow::setStatisticsInterval);
    connect(
        window,
        &ReplicaWindow::statisticsIntervalChanged,
        row,
        &ReplicaRow::setStatisticsInterval);
    connect(
        window,
        &ReplicaWindow::statisticsIntervalChanged,
        this,
        [this](int) { refreshReplicaGroups(); });
    connect(
        row,
        &ReplicaRow::sourceNameVisibilityChanged,
        window,
        &ReplicaWindow::setSourceNameVisible);
    connect(
        window,
        &ReplicaWindow::colorStatisticsChanged,
        row,
        &ReplicaRow::setColorStatistics);
    connect(row, &ReplicaRow::statisticsColorsChanged,
            window, &ReplicaWindow::setStatisticsColors);
    connect(
        window,
        &ReplicaWindow::pixelStatisticsUnavailable,
        row,
        &ReplicaRow::clearPixelStatistics);
    connect(
        row,
        &ReplicaRow::colorLogicSlotChanged,
        window,
        &ReplicaWindow::setColorLogicSlot);
    connect(
        row,
        &ReplicaRow::colorLogicEnabledChanged,
        window,
        &ReplicaWindow::setColorLogicEnabled);
    connect(
        window,
        &ReplicaWindow::colorLogicEnabledChanged,
        row,
        &ReplicaRow::setColorLogicEnabled);
    connect(
        row,
        &ReplicaRow::activeColorLogicSlotChanged,
        window,
        &ReplicaWindow::setActiveColorLogicSlot);
    connect(
        window,
        &ReplicaWindow::activeColorLogicSlotChanged,
        row,
        &ReplicaRow::setActiveColorLogicSlot);
    connect(
        row,
        &ReplicaRow::colorLogicSlotColorChanged,
        window,
        &ReplicaWindow::setColorLogicSlotColor);
    connect(
        window,
        &ReplicaWindow::flashStateChanged,
        this,
        &MainWindow::updateFlashAggregate);
    connect(
        window,
        &ReplicaWindow::positionChanged,
        this,
        &MainWindow::schedulePersistReplicas);
    connect(
        window,
        &ReplicaWindow::sizeChanged,
        this,
        &MainWindow::schedulePersistReplicas);
    connect(
        window,
        &ReplicaWindow::sourceTitleChanged,
        this,
        &MainWindow::schedulePersistReplicas);
    connect(
        window,
        &ReplicaWindow::regionCaptureChanged,
        this,
        &MainWindow::schedulePersistReplicas);
    connect(
        window,
        &ReplicaWindow::regionCaptureChanged,
        this,
        &MainWindow::updateFlashAggregate);
    connect(
        window,
        &ReplicaWindow::visibilityChanged,
        this,
        &MainWindow::schedulePersistReplicas);
    connect(
        row,
        &ReplicaRow::colorLogicSlotChanged,
        this,
        &MainWindow::schedulePersistReplicas);
    connect(
        row,
        &ReplicaRow::colorLogicEnabledChanged,
        this,
        &MainWindow::schedulePersistReplicas);
    connect(
        row,
        &ReplicaRow::activeColorLogicSlotChanged,
        this,
        &MainWindow::schedulePersistReplicas);
    connect(
        row,
        &ReplicaRow::colorLogicSlotColorChanged,
        this,
        &MainWindow::schedulePersistReplicas);
    connect(
        row,
        &ReplicaRow::sourceNameVisibilityChanged,
        this,
        &MainWindow::schedulePersistReplicas);
    connect(
        row,
        &ReplicaRow::replicaVisibilityChanged,
        this,
        &MainWindow::schedulePersistReplicas);
    connect(row, &ReplicaRow::positionEditRequested, this, &MainWindow::schedulePersistReplicas);
    connect(row, &ReplicaRow::scalePercentageEdited, this, &MainWindow::schedulePersistReplicas);
    connect(window, &ReplicaWindow::settingsChanged, this, &MainWindow::schedulePersistReplicas);
    connect(
        row,
        &ReplicaRow::collapseChanged,
        this,
        &MainWindow::schedulePersistReplicas);
    connect(
        row,
        &ReplicaRow::collapseChanged,
        this,
        [this, id = window->replicaId()](bool collapsed) {
            synchronizeReplicaGroupCollapse(id, collapsed);
        });
}
