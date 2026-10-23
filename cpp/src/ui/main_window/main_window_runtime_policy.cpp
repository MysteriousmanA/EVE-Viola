#include "ui/main_window.h"

#include <QStatusBar>
#include "ui/page_partition_page.h"
#include "ui/replica_window.h"

void MainWindow::setDesktopPetMode(DesktopPetMode mode)
{
    if (!isValidDesktopPetMode(mode) || runtimePolicy_.desktopPetMode == mode) { return; }
    runtimePolicy_.desktopPetMode = mode;
    applyRuntimePolicy();
    statusBar()->showMessage(QStringLiteral("已切换到%1。").arg(desktopPetModeName(mode)), 4000);
}

void MainWindow::applyRuntimePolicy()
{
    if (pagePartitionPage_) {
        pagePartitionPage_->setFishModeSuppressed(runtimePolicy_.presentationSuppressed());
        pagePartitionPage_->setCaptureOverlaySuppressed(runtimePolicy_.regionCapture);
    }
    const bool suppressed = runtimePolicy_.presentationSuppressed() || runtimePolicy_.regionCapture;
    for (auto it = windows_.cbegin(); it != windows_.cend(); ++it) {
        auto* window = it.value();
        if (suppressed && !suppressedWindowStates_.contains(it.key())) {
            suppressedWindowStates_.insert(it.key(), window->windowState());
        }
        window->setPresentationSuppressed(suppressed);
        window->setDesktopPetMode(runtimePolicy_.desktopPetMode);
        window->setAutomaticActivationEnabled(runtimePolicy_.automaticActivation(
            config_.activateSourceOnColorLogicTrigger));
        if (!suppressed && suppressedWindowStates_.contains(it.key())) {
            window->setWindowState(suppressedWindowStates_.take(it.key()));
        }
        if (runtimePolicy_.replicaVisible(replicaController_.desiredConfig(it.key()).visible)) {
            window->show();
        } else {
            window->hide();
        }
    }
    scheduleReplicaOverlapOrder();
}
