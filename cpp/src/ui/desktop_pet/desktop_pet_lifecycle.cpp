#include "ui/desktop_pet_page.h"

#include <QCursor>
#include <QGuiApplication>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QSlider>
#include <QTimer>
#include "features/desktop_pet/astrbot_notifier.h"
#include "ui/desktop_pet/avatar_crop_widget.h"
#include "ui/desktop_pet/floating_pet_window.h"

void DesktopPetPage::connectFloatingPetSignals()
{
    connect(petWindow_, &FloatingPetWindow::sizeChanged, this, [this](int size) {
        sizeSlider_->setValue(size);
        saveSettings();
    });
    connect(petWindow_, &FloatingPetWindow::leftClicked,
            petWindow_, &FloatingPetWindow::toggleTriggerHistory);
    connect(petWindow_, &FloatingPetWindow::modeChanged, this, &DesktopPetPage::setMode);
    connect(petWindow_, &FloatingPetWindow::stopSoftwareRequested, this, [this] {
        messagePersistTimer_->stop();
        saveSettings();
        astrBotNotifier_->cancelPending();
        emit stopSoftwareRequested();
    });
    connect(petWindow_, &FloatingPetWindow::hiddenByUser, this, [this] {
        petEnabled_ = false;
        activeTriggerMessages_.clear();
        activeTriggerAccounts_.clear();
        statusLabel_->setText(QStringLiteral("桌宠已禁用。"));
        updatePetButton();
        saveSettings();
    });
    connect(petWindow_, &FloatingPetWindow::messageExpired, this, [this] {
        activeTriggerMessages_.clear();
        activeTriggerAccounts_.clear();
    });
    connect(petWindow_, &FloatingPetWindow::autoHiddenByMessage, this, [this] {
        statusLabel_->setText(QStringLiteral("桌宠提示已结束。"));
        updatePetButton();
        saveSettings();
    });
    connect(petWindow_, &FloatingPetWindow::positionChanged,
            this, [this] {
                petPositionInitialized_ = true;
                saveSettings();
            });
}

void DesktopPetPage::restoreEnabledPet()
{
    if (petEnabled_ && !sourceImage_.isNull()) {
        if (!petPositionInitialized_) {
            setDefaultPetPosition();
        }
        petWindow_->show();
        statusLabel_->setText(QStringLiteral("已启用桌宠，并恢复到桌面。"));
        updatePetButton();
        saveSettings();
    }
}

void DesktopPetPage::togglePet()
{
    if (petWindow_ == nullptr) {
        return;
    }
    if (petEnabled_) {
        petEnabled_ = false;
        petWindow_->hideByUser();
        statusLabel_->setText(QStringLiteral("桌宠已禁用。"));
    } else {
        if (sourceImage_.isNull()) {
            return;
        }
        petEnabled_ = true;
        refreshPet();
        if (!petPositionInitialized_) {
            setDefaultPetPosition();
        }
        petWindow_->show();
        statusLabel_->setText(QStringLiteral("桌宠正在桌面上；右键可切换常驻、摸鱼、勿扰模式，禁用桌宠或停止软件。"));
    }
    updatePetButton();
    saveSettings();
}

void DesktopPetPage::setDefaultPetPosition()
{
    QScreen* screen = QGuiApplication::screenAt(QCursor::pos());
    if (screen == nullptr) {
        screen = QGuiApplication::primaryScreen();
    }
    if (screen != nullptr) {
        const QRect available = screen->availableGeometry();
        const int petSize = sizeSlider_ == nullptr ? 128 : sizeSlider_->value();
        const int wheelRadius = qMin(168, petSize / 2 + 84);
        petWindow_->move(
            available.right() - wheelRadius - petWindow_->width() / 2 - 10,
            available.top() + wheelRadius - petSize / 2 + 10);
    }
    petPositionInitialized_ = true;
}

void DesktopPetPage::refreshPet()
{
    if (petWindow_ == nullptr) {
        return;
    }
    petWindow_->setAvatar(
        cropWidget_->croppedImage(),
        sizeSlider_ == nullptr ? 128 : sizeSlider_->value());
}

void DesktopPetPage::updatePetButton()
{
    if (petButton_ != nullptr) {
        petButton_->setText(petEnabled_
                                ? QStringLiteral("禁用桌宠")
                                : QStringLiteral("启用桌宠"));
        petButton_->setEnabled(petEnabled_ || !sourceImage_.isNull());
        petButton_->setToolTip(petEnabled_
                ? QStringLiteral("禁用桌宠，并取消下次启动时显示")
                : QStringLiteral("启用桌宠；下次启动程序时会自动显示"));
    }
}
