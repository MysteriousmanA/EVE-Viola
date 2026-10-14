#include "ui/main_window.h"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QStatusBar>
#include <QSystemTrayIcon>
#include <QTimer>
#include "features/quick_switch/quick_switch_service.h"
#include "platform/window_catalog.h"
#include "ui/account_switch_controller.h"
#include "ui/region_capture_controller.h"

void MainWindow::connectStatisticsShutdown()
{
    connect(statisticsCoordinator_, &StatisticsCoordinator::idle, this, [this] {
        if (closing_) { QTimer::singleShot(0, this, &QWidget::close); }
    });
}

MainWindow::~MainWindow()
{
    closing_ = true;
    regionCaptureController_->finishScreenRegionCapture({}, false);
    accountSwitchController_->stop();
    if (quickSwitchService_ != nullptr) {
        quickSwitchService_->stop();
    }
    replicaController_.closeAll();
    windows_.clear();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    regionCaptureController_->finishScreenRegionCapture({}, false);
    if (config_.keepInTray && trayIcon_ != nullptr && trayIcon_->isVisible()
        && !quitRequested_) {
        hide();
        event->ignore();
        return;
    }
    if (!closing_) {
        closing_ = true;
        setEnabled(false);
        persistConfiguration();
        accountSwitchController_->stop();
        quickSwitchService_->stop();
        windowCatalog_->setMonitoring(false);
        statisticsCoordinator_->stop();
    }
    if (!statisticsCoordinator_->isIdle()) {
        statusBar()->showMessage(QStringLiteral("正在结束后台截图，完成后退出……"));
        event->ignore();
        return;
    }
    replicaController_.closeAll();
    windows_.clear();
    replicaController_.clear();
    refreshSummary();
    event->accept();
    if (quitRequested_) { QTimer::singleShot(0, QCoreApplication::instance(), &QCoreApplication::quit); }
}

void MainWindow::forceQuit()
{
    quitRequested_ = true;
    if (trayIcon_ != nullptr) {
        trayIcon_->hide();
    }
    close();
}
