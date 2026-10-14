#include "ui/main_window.h"

#include "ui/replica_row.h"
#include "ui/replica_window.h"

void MainWindow::updateFlashAggregate()
{
    QHash<QString, QList<ReplicaWindow*>> groups;
    for (auto* window : windows_) {
        QString key;
        if (auto* row = rows_.value(window->replicaId()); row != nullptr) {
            key = row->currentRegionPresetName();
        }
        if (key.isEmpty()) {
            const auto& region = window->capturedRegion();
            if (region.has_value()) {
                key = QStringLiteral("region:%1,%2,%3,%4")
                          .arg(region->x())
                          .arg(region->y())
                          .arg(region->width())
                          .arg(region->height());
            } else if (window->source().has_value()) {
                key = QStringLiteral("source:%1")
                          .arg(static_cast<qulonglong>(window->source()->hwnd));
            } else {
                key = QStringLiteral("window:%1").arg(window->replicaId());
            }
        }
        groups[key].append(window);
    }
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        bool anyFlashing = false;
        for (auto* window : it.value()) {
            if (window->isFlashActive()) {
                anyFlashing = true;
                break;
            }
        }
        for (auto* window : it.value()) {
            window->setGlobalFlashState(anyFlashing);
        }
    }
}
