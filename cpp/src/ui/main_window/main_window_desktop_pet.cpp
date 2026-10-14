#include "ui/main_window.h"

#include "ui/desktop_pet_page.h"
#include "ui/account_switch_controller.h"

QWidget* MainWindow::buildDesktopPetPage()
{
    desktopPetPage_ = new DesktopPetPage(configStore_.directoryPath());
    connect(desktopPetPage_, &DesktopPetPage::accountWheelRequested,
            accountSwitchController_, &AccountSwitchController::showDesktopPetAccountWheel);
    connect(desktopPetPage_, &DesktopPetPage::accountWheelGeometryChanged,
            accountSwitchController_, &AccountSwitchController::moveDesktopPetAccountWheel);
    connect(desktopPetPage_, &DesktopPetPage::accountWheelDismissRequested,
            accountSwitchController_, &AccountSwitchController::dismissDesktopPetAccountWheel);
    connect(desktopPetPage_, &DesktopPetPage::fishModeToggled,
            this, &MainWindow::setFishModeEnabled);
    return desktopPetPage_;
}
