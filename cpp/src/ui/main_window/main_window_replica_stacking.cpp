#include "ui/main_window.h"

#include <algorithm>
#include <QTimer>
#include "ui/replica_window.h"

void MainWindow::initializeReplicaOverlapOrder()
{
    replicaOverlapOrderTimer_ = new QTimer(this);
    replicaOverlapOrderTimer_->setSingleShot(true);
    replicaOverlapOrderTimer_->setInterval(100);
    connect(
        replicaOverlapOrderTimer_, &QTimer::timeout,
        this, &MainWindow::synchronizeReplicaOverlapOrder);
}

void MainWindow::scheduleReplicaOverlapOrder()
{
    if (!closing_ && replicaOverlapOrderTimer_ != nullptr) {
        replicaOverlapOrderTimer_->start();
    }
}

void MainWindow::synchronizeReplicaOverlapOrder()
{
    struct PositionedReplica {
        ReplicaWindow* window{};
        QRect geometry;
        ReplicaId id{};
    };

    QList<PositionedReplica> visibleReplicas;
    visibleReplicas.reserve(windows_.size());
    for (auto* window : windows_) {
        if (window == nullptr || !window->isVisible()) {
            continue;
        }
        const QRect geometry = window->geometry();
        if (geometry.width() <= 0 || geometry.height() <= 0) {
            continue;
        }
        visibleReplicas.append({window, geometry, window->replicaId()});
    }

    QList<bool> visited(visibleReplicas.size(), false);
    for (qsizetype start = 0; start < visibleReplicas.size(); ++start) {
        if (visited[start]) {
            continue;
        }

        QList<qsizetype> overlapGroup{start};
        visited[start] = true;
        for (qsizetype cursor = 0; cursor < overlapGroup.size(); ++cursor) {
            const auto current = overlapGroup[cursor];
            for (qsizetype candidate = 0; candidate < visibleReplicas.size(); ++candidate) {
                if (visited[candidate]
                    || !visibleReplicas[current].geometry.intersects(
                        visibleReplicas[candidate].geometry)) {
                    continue;
                }
                visited[candidate] = true;
                overlapGroup.append(candidate);
            }
        }
        if (overlapGroup.size() < 2) {
            continue;
        }

        std::sort(
            overlapGroup.begin(),
            overlapGroup.end(),
            [&visibleReplicas](qsizetype left, qsizetype right) {
                const auto& first = visibleReplicas[left];
                const auto& second = visibleReplicas[right];
                if (first.geometry.top() != second.geometry.top()) {
                    return first.geometry.top() < second.geometry.top();
                }
                if (first.geometry.left() != second.geometry.left()) {
                    return first.geometry.left() < second.geometry.left();
                }
                return first.id < second.id;
            });

        // Raising from top to bottom in screen coordinates leaves the lowest
        // replica at the front, so every vertically offset title edge stays exposed.
        for (const qsizetype index : overlapGroup) {
            visibleReplicas[index].window->raise();
        }
    }
}
