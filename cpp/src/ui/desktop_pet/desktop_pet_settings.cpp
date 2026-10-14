#include "ui/desktop_pet_page.h"

#include <algorithm>
#include <QGuiApplication>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QSignalBlocker>
#include <QSlider>
#include "ui/desktop_pet/avatar_crop_widget.h"
#include "ui/desktop_pet/floating_pet_window.h"

#include "ui/desktop_pet/desktop_pet_constants.h"

using namespace DesktopPetUi;

void DesktopPetPage::loadSettings()
{
    const auto loaded = settingsStore_.load();
    settingsWritable_ = loaded.writable;
    if (!loaded.loaded) {
        if (loaded.parseFailed) {
            statusLabel_->setText(QStringLiteral("桌宠配置无法解析，已暂停保存；修复后请重新启动。"));
        }
        return;
    }
    const QJsonObject& settings = loaded.settings;
    petEnabled_ = settings.value(QStringLiteral("enabled")).toBool(false);
    const QJsonObject messageSettings = settings.value(QStringLiteral("triggerMessages")).toObject();
    for (auto it = messageSettings.constBegin(); it != messageSettings.constEnd(); ++it) {
        bool numberIsValid = false;
        const int ruleNumber = it.key().toInt(&numberIsValid);
        const QString message = it.value().toString();
        if (numberIsValid && ruleNumber > 0 && !message.trimmed().isEmpty()) {
            ruleMessages_.insert(ruleNumber, message);
        }
    }
    const QImage image = settingsStore_.loadSourceImage();
    if (!image.isNull()) {
        sourceImage_ = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        cropWidget_->setImage(sourceImage_);
        imageNameLabel_->setText(QStringLiteral("上次使用的头像"));
        petButton_->setEnabled(true);
        statusLabel_->setText(QStringLiteral("已恢复上次的头像设置。"));
    }

    const QSignalBlocker zoomBlocker(zoomSlider_);
    const QSignalBlocker sizeBlocker(sizeSlider_);
    const int zoom = std::clamp(settings.value(QStringLiteral("zoomPercent")).toInt(100), 100, 300);
    zoomSlider_->setValue(zoom);
    cropWidget_->setZoomPercent(zoom);
    cropWidget_->setNormalizedOffset(QPointF(
        std::clamp(settings.value(QStringLiteral("offsetX")).toDouble(0.0), -1.0, 1.0),
        std::clamp(settings.value(QStringLiteral("offsetY")).toDouble(0.0), -1.0, 1.0)));
    sizeSlider_->setValue(std::clamp(
        settings.value(QStringLiteral("size")).toInt(128), kPetSizeMinimum, kPetSizeMaximum));

    const QPoint storedPosition(
        settings.value(QStringLiteral("x")).toInt(),
        settings.value(QStringLiteral("y")).toInt());
    if (settings.contains(QStringLiteral("x")) && settings.contains(QStringLiteral("y"))) {
        bool onAvailableScreen = false;
        for (QScreen* screen : QGuiApplication::screens()) {
            if (screen->availableGeometry().adjusted(-petWindow_->width(), -petWindow_->height(),
                                                      petWindow_->width(), petWindow_->height())
                    .contains(storedPosition)) {
                onAvailableScreen = true;
                break;
            }
        }
        if (onAvailableScreen) {
            petWindow_->move(storedPosition);
            petPositionInitialized_ = true;
        }
    }
}

void DesktopPetPage::saveSettings() const
{
    if (!settingsWritable_) {
        return;
    }
    const QPointF offset = cropWidget_->normalizedOffset();
    QJsonObject messages;
    for (auto it = ruleMessages_.cbegin(); it != ruleMessages_.cend(); ++it) {
        if (!it.value().trimmed().isEmpty()) {
            messages.insert(QString::number(it.key()), it.value());
        }
    }
    QJsonObject settings{
        {QStringLiteral("circular"), true},
        {QStringLiteral("zoomPercent"), zoomSlider_->value()},
        {QStringLiteral("offsetX"), offset.x()},
        {QStringLiteral("offsetY"), offset.y()},
        {QStringLiteral("size"), sizeSlider_->value()},
        {QStringLiteral("enabled"), petEnabled_},
        {QStringLiteral("x"), petWindow_->pos().x()},
        {QStringLiteral("y"), petWindow_->pos().y()},
        {QStringLiteral("triggerMessages"), messages},
    };
    static_cast<void>(settingsStore_.save(settings));
}
