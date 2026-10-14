#include "ui/main_window.h"

#include "ui/page_partition_page.h"
#include "platform/window_catalog.h"

QWidget* MainWindow::buildPagePartitionPage()
{
    pagePartitionPage_ = new PagePartitionPage(config_.pagePartition);
    connect(
        pagePartitionPage_, &PagePartitionPage::configurationChanged,
        this, [this] {
            const bool wasMonitoringEve = config_.pagePartition.onlyShowWhenEveDetected;
            config_.pagePartition = pagePartitionPage_->partitionConfig();
            if (wasMonitoringEve != config_.pagePartition.onlyShowWhenEveDetected) {
                setupPagePartitionEveMonitoring();
            }
            persistConfiguration();
        });
    return pagePartitionPage_;
}

void MainWindow::setupPagePartitionEveMonitoring()
{
    windowCatalog_->setMonitoring(config_.restoreWindowsWithGame
        || config_.pagePartition.onlyShowWhenEveDetected);
    if (config_.pagePartition.onlyShowWhenEveDetected && pagePartitionPage_) {
        pagePartitionPage_->setEveClientPresent(false);
        updatePagePartitionEvePresence();
    }
}

void MainWindow::updatePagePartitionEvePresence()
{
    if (!config_.pagePartition.onlyShowWhenEveDetected || pagePartitionPage_ == nullptr) {
        return;
    }
    try {
        const QList<WindowInfo> available = windowCatalog_->windows();
        pagePartitionPage_->setEveClientPresent(!available.isEmpty());
    } catch (const NativeException&) {
        // An enumeration error does not mean that all EVE clients have exited.
    }
}
