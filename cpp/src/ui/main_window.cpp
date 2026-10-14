#include "ui/main_window.h"

#include <utility>

#include <QCoreApplication>

#include "features/quick_switch/quick_switch_service.h"
#include "platform/window_catalog.h"
#include "ui/pages/home/home_page.h"
#include "ui/pages/replica_list/replica_list_page.h"

namespace {

QString defaultConfigDirectory()
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/cache");
}

} // namespace

MainWindow::MainWindow(NativeGateway& gateway, QWidget* parent)
    : MainWindow(gateway, defaultConfigDirectory(), parent)
{
}

MainWindow::MainWindow(NativeGateway& gateway, QString configDirectory, QWidget* parent)
    : QMainWindow(parent)
    , gateway_(gateway)
    , configStore_(std::move(configDirectory))
{
    statisticsCoordinator_ = new StatisticsCoordinator(gateway_, this);
    audioFeedbackManager_ = new AudioFeedbackManager(this);

    QString loadError;
    const bool loaded = loadConfiguration(loadError);
    quickSwitchService_ = new QuickSwitchService(gateway_, this);
    windowCatalog_ = new WindowCatalog(gateway_, this);
    initializeAccountSwitchController();
    initializeRegionCaptureController();
    connectWindowCatalogSignals();
    connectStatisticsShutdown();
    connectQuickSwitchSignals();
    buildWindowLayout();
    refreshConfigurationPages();
    refreshPresetBars();
    homePage_->setConfig(config_);
    replicaListPage_->setSizeLocked(config_.lockReplicaSize);
    setupTray();

    initializePersistenceTimer();
    initializeReplicaOverlapOrder();
    setupGameRestoreMonitoring();
    setupPagePartitionEveMonitoring();

    restoreConfiguredReplicas();
    startQuickSwitch();
    if (!loaded) {
        showConfigurationLoadWarning(loadError);
    }
}
