#include "ui/main_window.h"

#include <QJsonObject>
#include "runtime/runtime_logger.h"
#include "platform/window_catalog.h"

void MainWindow::connectWindowCatalogSignals()
{
    connect(windowCatalog_, &WindowCatalog::refreshed, this, [this] {
        updatePagePartitionEvePresence();
        restorePendingGameWindows();
    });
    connect(windowCatalog_, &WindowCatalog::refreshFailed, this, [](const QString& message) {
        RuntimeLogger::writeEvent(QStringLiteral("window_catalog_failed"),
            QJsonObject{{QStringLiteral("reason"), message}});
    });
}
