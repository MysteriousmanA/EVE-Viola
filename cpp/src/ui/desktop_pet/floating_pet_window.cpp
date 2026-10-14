#include "ui/desktop_pet/floating_pet_window.h"

#include <QLabel>
#include <QPainter>
#include "ui/desktop_pet/avatar_painting.h"

using namespace DesktopPetUi;

FloatingPetWindow::FloatingPetWindow()
    : QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint
                           | Qt::WindowStaysOnTopHint | Qt::NoDropShadowWindowHint)
{
    setObjectName(QStringLiteral("desktopPetFloatingWindow"));
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setMouseTracking(true);
    initializeMessageBubble();
    updateWindowRegion();
}

void FloatingPetWindow::setAvatar(const QImage& image, int size)
{
    image_ = image;
    avatarSize_ = size;
    updateWindowRegion();
    update();
}

void FloatingPetWindow::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    if (image_.isNull()) {
        return;
    }
    const QRectF bounds = QRectF(avatarRect()).adjusted(2, 2, -2, -2);
    painter.setClipPath(cropPath(bounds, true));
    painter.drawImage(bounds, image_);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(116, 200, 229, 230), 2));
    painter.drawPath(cropPath(bounds.adjusted(1, 1, -1, -1), true));
}
