#include "ui/desktop_pet/floating_pet_window.h"

#include <algorithm>
#include <QGuiApplication>
#include <QLabel>
#include <QRegion>
#include <QScreen>

using namespace DesktopPetUi;

void FloatingPetWindow::keepWindowOnScreen()
{
    const QPoint avatarCenter = mapToGlobal(QPoint(width() / 2, avatarSize_ / 2));
    if (QScreen* screen = QGuiApplication::screenAt(avatarCenter); screen != nullptr) {
        const QRect available = screen->availableGeometry();
        move(
            std::clamp(x(), available.left(),
                       qMax(available.left(), available.right() - width() + 1)),
            std::clamp(y(), available.top(),
                       qMax(available.top(), available.bottom() - height() + 1)));
    }
}

QRect FloatingPetWindow::avatarRect() const
{
    return QRect(
        (width() - avatarSize_) / 2,
        0,
        avatarSize_,
        avatarSize_);
}

void FloatingPetWindow::updateWindowRegion()
{
    setFixedSize(qMax(kPetSizeMaximum + 52, bubbleWidth_),
                 avatarSize_ + bubbleRegionHeight_);
    if (bubble_ != nullptr) {
        bubble_->setGeometry(
            (width() - bubbleWidth_) / 2,
            avatarSize_ + 8,
            bubbleWidth_,
            bubbleHeight_);
    }
    const QRect imageRect = avatarRect();
    QRegion region(imageRect, QRegion::Ellipse);
    if (messageVisible_) {
        region |= QRegion(bubble_->geometry());
    }
    setMask(region);
}
