#include "ui/desktop_pet_page.h"

#include <utility>

#include <QTimer>

#include "ui/desktop_pet/floating_pet_window.h"

DesktopPetPage::DesktopPetPage(QString settingsDirectory, QWidget* parent)
    : QWidget(parent)
    , settingsStore_(std::move(settingsDirectory))
{
    const auto [zoomValue, sizeValue] = buildLayout();
    petWindow_ = new FloatingPetWindow;
    initializeMessagePersistence();
    connectEditorSignals(zoomValue, sizeValue);
    connectFloatingPetSignals();

    loadSettings();
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
