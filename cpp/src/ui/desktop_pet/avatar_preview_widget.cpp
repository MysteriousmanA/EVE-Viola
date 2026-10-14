#include "ui/desktop_pet/avatar_preview_widget.h"

#include <QPainter>
#include "ui/desktop_pet/avatar_painting.h"

using namespace DesktopPetUi;

AvatarPreviewWidget::AvatarPreviewWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("desktopPetAvatarPreview"));
    setFixedSize(184, 184);
}

void AvatarPreviewWidget::setAvatar(const QImage& image)
{
    image_ = image;
    update();
}

void AvatarPreviewWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(QStringLiteral("#0B171F")));
    painter.drawRoundedRect(rect(), 10, 10);
    const QRectF imageRect(19, 19, 146, 146);
    drawCheckerboard(painter, imageRect, true);
    if (!image_.isNull()) {
        painter.save();
        painter.setClipPath(cropPath(imageRect, true));
        painter.drawImage(imageRect, image_);
        painter.restore();
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(QStringLiteral("#74C8E5")), 1.5));
        painter.drawPath(cropPath(imageRect.adjusted(0.75, 0.75, -0.75, -0.75), true));
    } else {
        painter.setPen(QColor(QStringLiteral("#728995")));
        painter.drawText(imageRect, Qt::AlignCenter, QStringLiteral("头像预览"));
    }
}
