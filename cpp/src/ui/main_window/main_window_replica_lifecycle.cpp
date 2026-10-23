#include "ui/main_window.h"

#include <memory>
#include <utility>
#include <QTimer>
#include "platform/window_catalog.h"
#include "ui/pages/replica_list/replica_list_page.h"
#include "ui/region_capture_controller.h"
#include "ui/replica_binding.h"
#include "ui/replica_row.h"
#include "ui/replica_window.h"

namespace {

class ReplicaOwner final : public CloseableReplica {
public:
    explicit ReplicaOwner(std::unique_ptr<ReplicaWindow> window)
        : window_(std::move(window))
    {
    }

    void closeReplica() noexcept override
    {
        window_->close();
    }

private:
    std::unique_ptr<ReplicaWindow> window_;
};

} // namespace

ReplicaWindow* MainWindow::createReplica()
{
    QList<WindowInfo> sourceOptions;
    try {
        sourceOptions = windowCatalog_->windows();
    } catch (const NativeException&) {
    }
    return createReplica(sourceOptions);
}

ReplicaWindow* MainWindow::createReplica(const QList<WindowInfo>& sourceOptions)
{
    ReplicaWindow* createdWindow = nullptr;
    const auto id = replicaController_.create(
        [this, &createdWindow](
            ReplicaId replicaId,
            ReplicaManager::CloseCallback removeFromManager) {
            auto window = std::make_unique<ReplicaWindow>(
                replicaId,
                gateway_,
                *statisticsCoordinator_,
                *audioFeedbackManager_,
                [this, removeFromManager = std::move(removeFromManager)](
                    ReplicaId closedId) mutable {
                    QTimer::singleShot(
                        0,
                        this,
                        [this, closedId, removeFromManager = std::move(removeFromManager)]() mutable {
                            removeFromManager();
                            onReplicaClosed(closedId);
                        });
                });
            createdWindow = window.get();
            return std::make_unique<ReplicaOwner>(std::move(window));
        });

    windows_.insert(id, createdWindow);
    createdWindow->setDesktopPetMode(runtimePolicy_.desktopPetMode);
    createdWindow->setPresentationSuppressed(
        runtimePolicy_.presentationSuppressed() || runtimePolicy_.regionCapture);
    createdWindow->setAdvancedLogicService(advancedLogicService_);
    createdWindow->setRegionPresets(config_.regionPresets);
    createdWindow->setColorRules(config_.colorRules);
    createdWindow->setAutomaticActivationEnabled(
        runtimePolicy_.automaticActivation(config_.activateSourceOnColorLogicTrigger));
    createdWindow->setAutomaticActivationCooldownMs(
        config_.automaticActivationCooldownMs);
    createdWindow->setRaiseOnClickEnabled(config_.raiseReplicaOnClick);
    createdWindow->setWheelZoomLocked(config_.lockReplicaSize);
    auto* row = new ReplicaRow(id);
    rows_.insert(id, row);
    connect(
        row,
        &ReplicaRow::reorderRequested,
        this,
        &MainWindow::reorderReplicaRow);
    row->setColorRules(config_.colorRules);
    row->setRegionPresets(config_.regionPresets);
    createdWindow->setColorLogicSlotColor(1, row->colorLogicSlotColor(1));
    createdWindow->setColorLogicSlotColor(2, row->colorLogicSlotColor(2));
    connect(row, &ReplicaRow::closeRequested, createdWindow, [window = QPointer(createdWindow)] {
        if (window) {
            window->close();
        }
    });
    connect(row, &ReplicaRow::copyRequested, this, [this, id] {
        static_cast<void>(duplicateReplica(id));
    });
    connect(row, &ReplicaRow::syncToRequested, this, [this, id] {
        showSyncReplicasDialog(id);
    });
    connect(row, &ReplicaRow::regionRecaptureRequested, this, [this, id] {
        regionCaptureController_->beginScreenRegionCapture(id);
    });
    connect(row, &ReplicaRow::sourceSelectionRequested, createdWindow, &ReplicaWindow::selectSource);
    connect(row, &ReplicaRow::sourceOptionsRequested, this, [this, id] {
        if (auto* target = rows_.value(id)) {
            try {
                target->setSourceOptions(windowCatalog_->windows(true));
            } catch (const NativeException&) {
            }
        }
    });
    row->setSourceOptions(sourceOptions);
    connectReplicaSignals(createdWindow, row);
    ReplicaBinding::attach(replicaController_, *createdWindow, *row, this,
        [this] { return restoringReplicas_; });
    row->setWindowSize(createdWindow->size());
    connect(row, &ReplicaRow::saveRegionPresetRequested, this, [this, id] {
        saveRegionPresetFromReplica(id);
    });
    replicaListPage_->addRow(row);
    refreshReplicaFilters();
    refreshReplicaGroups();

    const auto offset = static_cast<int>(((id - 1) % 10) * 24);
    createdWindow->move(x() + width() + 18 + offset, y() + 40 + offset);
    row->setWindowPosition(createdWindow->pos());
    createdWindow->show();
    applyRuntimePolicy();
    scheduleReplicaOverlapOrder();
    refreshSummary();
    updateFlashAggregate();
    schedulePersistReplicas();
    refreshReplicaListSize();
    return createdWindow;
}

ReplicaWindow* MainWindow::duplicateReplica(ReplicaId id)
{
    const auto* original = windows_.value(id);
    if (original == nullptr || !replicaController_.contains(id)) { return nullptr; }
    const auto config = replicaController_.desiredConfig(id);
    const auto source = original->source();
    QList<WindowInfo> available;
    try { available = windowCatalog_->windows(); }
    catch (const NativeException&) {}
    auto* copy = restoreReplica(config, available, source ? &*source : nullptr, true);
    if (copy != nullptr && replicaListPage_ != nullptr) {
        const QPointer<ReplicaRow> row(rows_.value(copy->replicaId()));
        QTimer::singleShot(0, this, [this, row] {
            if (row != nullptr) { replicaListPage_->ensureRowVisible(row); }
        });
    }
    schedulePersistReplicas();
    return copy;
}

std::size_t MainWindow::activeReplicaCount() const noexcept
{
    return replicaController_.count();
}

QList<ReplicaWindow*> MainWindow::replicaWindows() const
{
    QList<ReplicaWindow*> ordered;
    for (const ReplicaId id : replicaController_.ids()) {
        if (auto* window = windows_.value(id); window != nullptr) {
            ordered.append(window);
        }
    }
    return ordered;
}

QList<ReplicaRow*> MainWindow::replicaRows() const
{
    QList<ReplicaRow*> ordered;
    for (const ReplicaId id : replicaController_.ids()) {
        if (auto* row = rows_.value(id); row != nullptr) {
            ordered.append(row);
        }
    }
    return ordered;
}

void MainWindow::onReplicaClosed(ReplicaId id)
{
    suppressedWindowStates_.remove(id);
    regionCaptureController_->replicaClosed(id);
    windows_.remove(id);
    static_cast<void>(replicaController_.remove(id));
    if (auto* row = rows_.take(id)) {
        row->hide();
        row->deleteLater();
    }
    refreshReplicaFilters();
    refreshReplicaGroups();
    refreshSummary();
    updateFlashAggregate();
    scheduleReplicaOverlapOrder();
    schedulePersistReplicas();
    refreshReplicaListSize();
}

void MainWindow::closeAllReplicas()
{
    const QList<ReplicaWindow*> windows = windows_.values();
    for (auto* window : windows) {
        window->close();
    }
}
