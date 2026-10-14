#include "ui/main_window.h"

#include "ui/pages/replica_list/replica_list_page.h"
#include "ui/replica_window.h"

void MainWindow::reorderReplicaRow(ReplicaId sourceId, ReplicaId targetId, bool insertAfter)
{
    if (!replicaController_.reorder(sourceId, targetId, insertAfter)) { return; }
    refreshReplicaGroups();
    schedulePersistReplicas();
}

QWidget* MainWindow::buildReplicaListPage()
{
    replicaListPage_ = new ReplicaListPage;
    connect(replicaListPage_, &ReplicaListPage::persistRequested, this, &MainWindow::schedulePersistReplicas);
    connect(replicaListPage_, &ReplicaListPage::sizeLockChanged, this, [this](bool locked) {
        config_.lockReplicaSize = locked;
        for (auto* window : windows_) { window->setWheelZoomLocked(locked); }
        persistConfiguration();
    });
    return replicaListPage_;
}

void MainWindow::refreshReplicaGroups()
{
    replicaListPage_->setEntries(replicaController_.ids(), rows_, windows_);
    replicaListPage_->refreshReplicaGroups();
}

void MainWindow::refreshReplicaFilters()
{
    replicaListPage_->setEntries(replicaController_.ids(), rows_, windows_);
    replicaListPage_->refreshReplicaFilters();
}

void MainWindow::refreshReplicaListSize() { replicaListPage_->refreshReplicaListSize(); }

void MainWindow::synchronizeReplicaGroupCollapse(ReplicaId id, bool collapsed)
{
    replicaListPage_->setEntries(replicaController_.ids(), rows_, windows_);
    replicaListPage_->synchronizeReplicaGroupCollapse(id, collapsed);
}
