#include "ui/main_window.h"

#include <QLabel>
#include "ui/pages/home/home_page.h"
#include "ui/pages/home/home_controller.h"
#include "ui/replica_window.h"

QLabel* MainWindow::countLabel() const noexcept
{
    return homePage_->countLabel();
}

void MainWindow::refreshSummary()
{
    const auto count = replicaController_.count();
    homePage_->setReplicaCount(count);
    homeController_->setReplicaCount(count);
}

void MainWindow::refreshPresetBars() { homePage_->setPresets(config_); }

QWidget* MainWindow::buildHomePage()
{
    homePage_ = new HomePage;
    homeController_ = new HomeController(*homePage_, config_, this);
    connect(homePage_, &HomePage::createRequested, this, [this] { static_cast<void>(createReplica()); });
    connect(homeController_, &HomeController::closeAllRequested, this, &MainWindow::closeAllReplicas);
    connect(homeController_, &HomeController::snapshotRequested, this, &MainWindow::collectReplicaConfigs);
    connect(homeController_, &HomeController::rebuildRequested, this, &MainWindow::rebuildReplicas);
    connect(homeController_, &HomeController::persistRequested, this, &MainWindow::persistConfiguration);
    connect(homeController_, &HomeController::keepInTrayChanged, this, &MainWindow::setupTray);
    connect(homeController_, &HomeController::restoreWithGameChanged, this, &MainWindow::setupGameRestoreMonitoring);
    connect(homeController_, &HomeController::activateSourceChanged, this, [this](bool enabled) {
        for (auto* window : windows_) { window->setAutomaticActivationEnabled(runtimePolicy_.automaticActivation(enabled)); }
    });
    connect(homeController_, &HomeController::raiseOnClickChanged, this, [this](bool enabled) {
        for (auto* window : windows_) { window->setRaiseOnClickEnabled(enabled); }
    });
    connect(homeController_, &HomeController::cooldownChanged, this, [this](int milliseconds) {
        for (auto* window : windows_) { window->setAutomaticActivationCooldownMs(milliseconds); }
    });
    connect(homeController_, &HomeController::mouseQuietPeriodChanged, this, [this](int milliseconds) {
        gateway_.setAutomaticActivationMouseQuietPeriodMs(milliseconds);
    });
    return homePage_;
}
