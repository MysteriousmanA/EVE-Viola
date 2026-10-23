#include "ui/desktop_pet_page.h"

#include <utility>

#include <QTimer>

#include "features/desktop_pet/astrbot_notifier.h"
#include "ui/desktop_pet/floating_pet_window.h"

DesktopPetPage::DesktopPetPage(QString settingsDirectory, QWidget* parent)
    : QWidget(parent)
    , settingsStore_(std::move(settingsDirectory))
{
    const auto [zoomValue, sizeValue] = buildLayout();
    petWindow_ = new FloatingPetWindow;
    initializeMessagePersistence();
    astrBotNotifier_ = new AstrBotNotifier(this);
    connectEditorSignals(zoomValue, sizeValue);
    connectFloatingPetSignals();

    loadSettings();
    connectNotificationSignals();
    refreshPreview();
    updatePetButton();
    restoreEnabledPet();
}

DesktopPetPage::~DesktopPetPage()
{
    if (messagePersistTimer_ != nullptr) {
        messagePersistTimer_->stop();
    }
    saveSettings();
    delete petWindow_;
}
