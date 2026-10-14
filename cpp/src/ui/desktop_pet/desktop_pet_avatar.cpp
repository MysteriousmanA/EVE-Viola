#include "ui/desktop_pet_page.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QImageReader>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include "ui/desktop_pet/avatar_crop_widget.h"
#include "ui/desktop_pet/avatar_preview_widget.h"
#include "ui/desktop_pet/floating_pet_window.h"

#include "ui/desktop_pet/desktop_pet_constants.h"

using namespace DesktopPetUi;

void DesktopPetPage::connectEditorSignals(QLabel* zoomValue, QLabel* sizeValue)
{
    connect(uploadButton_, &QPushButton::clicked, this, &DesktopPetPage::chooseImage);
    connect(petButton_, &QPushButton::clicked, this, &DesktopPetPage::togglePet);
    connect(cropWidget_, &AvatarCropWidget::cropChanged,
            this, &DesktopPetPage::refreshPreview);
    connect(zoomSlider_, &QSlider::valueChanged, this, [this, zoomValue](int value) {
        cropWidget_->setZoomPercent(value);
        zoomValue->setText(QStringLiteral("%1%").arg(value));
    });
    connect(zoomSlider_, &QSlider::sliderReleased, this, [this] { saveSettings(); });
    connect(cropWidget_, &AvatarCropWidget::cropInteractionFinished,
            this, [this] { saveSettings(); });
    connect(sizeSlider_, &QSlider::valueChanged, this, [this, sizeValue](int value) {
        sizeValue->setText(QStringLiteral("%1 px").arg(value));
        refreshPet();
    });
    connect(sizeSlider_, &QSlider::sliderReleased, this, [this] { saveSettings(); });
}

void DesktopPetPage::chooseImage()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("选择桌宠头像"),
        QString{},
        QStringLiteral("图片文件 (*.png *.jpg *.jpeg *.bmp *.webp);;所有文件 (*.*)"));
    if (path.isEmpty()) {
        return;
    }

    QImageReader reader(path);
    reader.setAutoTransform(true);
    QImage image = reader.read();
    if (image.isNull()) {
        statusLabel_->setText(QStringLiteral("图片无法读取：%1").arg(reader.errorString()));
        return;
    }

    sourceImage_ = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    cropWidget_->setImage(sourceImage_);
    zoomSlider_->setValue(100);
    imageNameLabel_->setText(QFileInfo(path).fileName());
    imageNameLabel_->setToolTip(path);
    petButton_->setEnabled(true);
    statusLabel_->setText(QStringLiteral("头像已准备好，可以放到桌面上。"));
    if (!settingsStore_.saveSourceImage(sourceImage_)) {
        statusLabel_->setText(QStringLiteral("图片已载入；缓存写入失败，关闭软件后需要重新选择。"));
    }
    refreshPreview();
    if (petEnabled_) {
        if (!petPositionInitialized_) {
            setDefaultPetPosition();
        }
        petWindow_->show();
        statusLabel_->setText(QStringLiteral("桌宠已启用并显示在桌面上。"));
        updatePetButton();
    }
    saveSettings();
}

void DesktopPetPage::refreshPreview()
{
    const QImage avatar = cropWidget_->croppedImage();
    previewWidget_->setAvatar(avatar);
    petButton_->setEnabled(!avatar.isNull() || petEnabled_);
    refreshPet();
}
