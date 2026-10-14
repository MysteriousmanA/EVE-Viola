#include "ui/main_window.h"

#include <QStatusBar>
#include "ui/pages/region_preset/region_preset_page.h"
#include "ui/pages/region_preset/region_preset_controller.h"
#include "ui/region_capture_controller.h"
#include "ui/replica_row.h"
#include "ui/replica_window.h"

QWidget* MainWindow::buildRegionPresetPage()
{
    regionPresetPage_ = new RegionPresetPage;
    regionPresetController_ = new RegionPresetController(config_, this);
    connect(regionPresetController_, &RegionPresetController::presetsChanged, this, &MainWindow::refreshRegionPresets);
    connect(regionPresetController_, &RegionPresetController::persistRequested, this, &MainWindow::persistConfiguration);
    connect(regionPresetController_, &RegionPresetController::replicaPersistRequested, this, &MainWindow::schedulePersistReplicas);
    connect(regionPresetController_, &RegionPresetController::statusMessage, this, [this](const QString& message, int timeout) { statusBar()->showMessage(message, timeout); });
    connect(regionPresetController_, &RegionPresetController::presetReplaced, this,
        [this](const QString& previousName, const RegionPreset& preset) {
            for (auto row = rows_.cbegin(); row != rows_.cend(); ++row) {
                if (row.value() && row.value()->currentRegionPresetName().compare(previousName, Qt::CaseInsensitive) == 0) {
                    if (auto* replica = windows_.value(row.key())) {
                        replica->applyRegionPreset(preset);
                        row.value()->setCurrentRegionPresetName(preset.name);
                    }
                }
            }
        });
    connect(regionPresetController_, &RegionPresetController::intervalChanged, this,
        [this](const QString& name, int interval) {
            for (auto row = rows_.cbegin(); row != rows_.cend(); ++row) {
                if (row.value() && row.value()->currentRegionPresetName().compare(name, Qt::CaseInsensitive) == 0) {
                    if (auto* replica = windows_.value(row.key())) { replica->setStatisticsInterval(interval); }
                }
            }
        });
    connect(
        regionPresetPage_, &RegionPresetPage::captureRequested,
        regionCaptureController_, &RegionCaptureController::beginRegionPresetCapture);
    connect(
        regionPresetPage_, &RegionPresetPage::saveRequested,
        this, &MainWindow::persistConfiguration);
    connect(
        regionPresetPage_, &RegionPresetPage::renameRequested,
        regionPresetController_, &RegionPresetController::renameRegionPresetAt);
    connect(
        regionPresetPage_, &RegionPresetPage::deleteRequested,
        regionPresetController_, &RegionPresetController::deleteRegionPresetAt);
    connect(
        regionPresetPage_, &RegionPresetPage::statisticsIntervalChanged,
        regionPresetController_, &RegionPresetController::updateRegionPresetInterval);
    return regionPresetPage_;
}

void MainWindow::saveRegionPresetFromReplica(ReplicaId id)
{
    const auto window = windows_.value(id);
    if (window == nullptr || !window->capturedRegion().has_value()) {
        return;
    }
    saveRegionPreset(*window->capturedRegion(), window->statisticsIntervalMs(), false);
}

void MainWindow::refreshRegionPresets()
{
    regionPresetPage_->setPresets(config_.regionPresets);
    for (auto* window : windows_) {
        window->setRegionPresets(config_.regionPresets);
    }
    for (auto* row : rows_) {
        row->setRegionPresets(config_.regionPresets);
    }
    refreshReplicaFilters();
    refreshReplicaGroups();
}

void MainWindow::saveRegionPreset(const QRect& region, int interval, bool replace)
{
    regionPresetController_->saveRegionPreset(region, interval, replace);
}
