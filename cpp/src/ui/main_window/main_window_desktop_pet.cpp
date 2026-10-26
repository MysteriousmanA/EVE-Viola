#include "ui/main_window.h"

#include "ui/desktop_pet_page.h"

QWidget* MainWindow::buildDesktopPetPage()
{
    desktopPetPage_ = new DesktopPetPage(configStore_.directoryPath());
    connect(desktopPetPage_, &DesktopPetPage::modeChanged,
            this, &MainWindow::setDesktopPetMode);
    connect(desktopPetPage_, &DesktopPetPage::stopSoftwareRequested,
            this, &MainWindow::forceQuit, Qt::QueuedConnection);
    runtimePolicy_.desktopPetMode = desktopPetPage_->mode();
    return desktopPetPage_;
}
