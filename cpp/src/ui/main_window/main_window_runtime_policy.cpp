#include "ui/main_window.h"

#include <QStatusBar>
#include "ui/page_partition_page.h"
#include "ui/replica_window.h"

void MainWindow::setFishModeEnabled(bool enabled)
{
    if (runtimePolicy_.fishMode == enabled) { return; }
    runtimePolicy_.fishMode = enabled;
    applyRuntimePolicy();
    statusBar()->showMessage(enabled
        ? QStringLiteral("摸鱼模式已启用：小窗、分区线、音频和图色自动跳转已暂停。")
        : QStringLiteral("摸鱼模式已关闭，已恢复小窗和分区线。"), 4000);
}

void MainWindow::applyRuntimePolicy()
{
    if (pagePartitionPage_) {
        pagePartitionPage_->setFishModeSuppressed(runtimePolicy_.fishMode);
        pagePartitionPage_->setCaptureOverlaySuppressed(runtimePolicy_.regionCapture);
    }
    const bool suppressed = runtimePolicy_.fishMode || runtimePolicy_.regionCapture;
    for (auto it = windows_.cbegin(); it != windows_.cend(); ++it) {
        auto* window = it.value();
        if (suppressed && !suppressedWindowStates_.contains(it.key())) {
            suppressedWindowStates_.insert(it.key(), window->windowState());
        }
        window->setPresentationSuppressed(suppressed);
        window->setFishModeEnabled(runtimePolicy_.fishMode);
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
