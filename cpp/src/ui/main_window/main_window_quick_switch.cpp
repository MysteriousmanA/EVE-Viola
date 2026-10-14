#include "ui/main_window.h"

#include <QJsonObject>
#include "runtime/runtime_logger.h"
#include "features/quick_switch/quick_switch_service.h"
#include "platform/window_catalog.h"
#include "ui/account_switch_controller.h"
#include "ui/pages/quick_switch/quick_switch_page.h"
#include "ui/pages/quick_switch/quick_switch_controller.h"

void MainWindow::initializeAccountSwitchController()
{
    accountSwitchController_ = new AccountSwitchController(
        gateway_, *windowCatalog_, config_, *quickSwitchService_, this);
    radialSwitchOverlay_ = accountSwitchController_->overlay();
}

void MainWindow::connectQuickSwitchSignals()
{
    connect(accountSwitchController_, &AccountSwitchController::accountPositionSelected,
        this, [this](const QString& title, int position) {
            if (quickSwitchPage_) { quickSwitchPage_->setRadialAccountPosition(title, position); }
        });
    connect(
        quickSwitchService_, &QuickSwitchService::switchRequested,
        accountSwitchController_, &AccountSwitchController::switchEveClient);
    connect(
        quickSwitchService_, &QuickSwitchService::radialSwitchHoldStarted,
        accountSwitchController_, &AccountSwitchController::beginRadialSwitchHold);
    connect(
        quickSwitchService_, &QuickSwitchService::radialSwitchHoldEnded,
        accountSwitchController_, &AccountSwitchController::endRadialSwitchHold);
    connect(
        quickSwitchService_, &QuickSwitchService::hookError,
        this, [this](const QString& message) {
            RuntimeLogger::writeEvent(
                QStringLiteral("quick_switch_hook_failed"),
                QJsonObject{{QStringLiteral("message"), message}});
        });
}

void MainWindow::startQuickSwitch()
{
    quickSwitchService_->start(
        config_.previousClientKeys,
        config_.nextClientKeys,
        config_.radialSwitchKey);
}

QWidget* MainWindow::buildQuickSwitchPage()
{
    quickSwitchPage_ = new QuickSwitchPage;
    quickSwitchController_ = new QuickSwitchController(*quickSwitchPage_, config_, *quickSwitchService_, *windowCatalog_, *accountSwitchController_);
    connect(quickSwitchController_, &QuickSwitchController::persistRequested, this, &MainWindow::persistConfiguration);
    return quickSwitchPage_;
}
