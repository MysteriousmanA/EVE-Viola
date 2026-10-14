#include "ui/main_window.h"

#include <QStatusBar>
#include "platform/window_catalog.h"
#include "ui/region_capture_controller.h"
#include "ui/replica_window.h"

void MainWindow::initializeRegionCaptureController()
{
    regionCaptureController_ = new RegionCaptureController(
        gateway_, *windowCatalog_, runtimePolicy_, this,
        [this](ReplicaId id) { return windows_.value(id); },
        [this] { return windows_.values(); },
        [this](const QString& message, int timeout) { statusBar()->showMessage(message, timeout); },
        [this](const QRect& region, int interval, bool replace) { saveRegionPreset(region, interval, replace); },
        [this] { applyRuntimePolicy(); });
}
