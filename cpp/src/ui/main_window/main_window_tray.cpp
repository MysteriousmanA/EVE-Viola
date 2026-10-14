#include "ui/main_window.h"

#include <QAction>
#include <QIcon>
#include <QMenu>
#include <QSystemTrayIcon>

void MainWindow::setupTray()
{
    const bool enable = config_.keepInTray && QSystemTrayIcon::isSystemTrayAvailable();
    if (enable && trayIcon_ == nullptr) {
        trayIcon_ = new QSystemTrayIcon(
            QIcon(QStringLiteral(":/icons/viola.png")), this);
        trayMenu_ = new QMenu(this);
        auto* showAction = trayMenu_->addAction(QStringLiteral("显示主窗口"));
        auto* quitAction = trayMenu_->addAction(QStringLiteral("退出"));
        connect(showAction, &QAction::triggered, this, &MainWindow::showAndActivate);
        connect(quitAction, &QAction::triggered, this, [this] {
            forceQuit();
        });
        connect(trayIcon_, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger
                    || reason == QSystemTrayIcon::DoubleClick) {
                    showAndActivate();
                }
            });
        trayIcon_->setContextMenu(trayMenu_);
        trayIcon_->show();
    } else if (!enable && trayIcon_ != nullptr) {
        trayIcon_->hide();
        trayIcon_->deleteLater();
        trayIcon_ = nullptr;
        trayMenu_->deleteLater();
        trayMenu_ = nullptr;
    }
}

void MainWindow::showAndActivate()
{
    showNormal();
    raise();
    activateWindow();
}
