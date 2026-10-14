#include "ui/desktop_pet/avatar_crop_widget.h"

#include <algorithm>
#include <QMouseEvent>
#include <QPainter>
#include "ui/desktop_pet/avatar_painting.h"

using namespace DesktopPetUi;

AvatarCropWidget::AvatarCropWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("desktopPetCropCanvas"));
    setFixedSize(344, 344);
    setCursor(Qt::OpenHandCursor);
    setToolTip(QStringLiteral("按住头像拖动，调整裁剪位置"));
}

void AvatarCropWidget::setImage(const QImage& image)
{
    image_ = image;
    offset_ = {};
    normalizedOffset_ = {};
    clampOffset();
    update();
    emit cropChanged();
}

void AvatarCropWidget::setZoomPercent(int percent)
{
    zoom_ = std::clamp(percent, 100, 300) / 100.0;
    clampOffset();
    update();
    emit cropChanged();
}

void AvatarCropWidget::setNormalizedOffset(const QPointF& offset)
{
    normalizedOffset_ = offset;
    const QRectF crop = cropRect();
    const QSizeF rendered = renderedImageSize();
    const qreal maxX = qMax<qreal>(0.0, (rendered.width() - crop.width()) / 2.0);
    const qreal maxY = qMax<qreal>(0.0, (rendered.height() - crop.height()) / 2.0);
    offset_ = QPointF(maxX * normalizedOffset_.x(), maxY * normalizedOffset_.y());
    clampOffset();
    update();
}

int AvatarCropWidget::zoomPercent() const
{
    return qRound(zoom_ * 100.0);
}

QPointF AvatarCropWidget::normalizedOffset() const
{
    const QRectF crop = cropRect();
    const QSizeF rendered = renderedImageSize();
    const qreal maxX = qMax<qreal>(0.0, (rendered.width() - crop.width()) / 2.0);
    const qreal maxY = qMax<qreal>(0.0, (rendered.height() - crop.height()) / 2.0);
    return QPointF(
        maxX > 0.0 ? offset_.x() / maxX : 0.0,
        maxY > 0.0 ? offset_.y() / maxY : 0.0);
}

QImage AvatarCropWidget::croppedImage(int pixels) const
{
    if (image_.isNull()) {
        return {};
    }

    const QRectF crop = cropRect();
    const QRectF imageRect = renderedImageRect();
    const qreal scale = imageRect.width() / image_.width();
    if (scale <= 0.0) {
        return {};
    }

    const QRectF sourceRect(
        (crop.left() - imageRect.left()) / scale,
        (crop.top() - imageRect.top()) / scale,
        crop.width() / scale,
        crop.height() / scale);
    QImage result(pixels, pixels, QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setClipPath(cropPath(QRectF(0, 0, pixels, pixels), true));
    painter.drawImage(QRectF(0, 0, pixels, pixels), image_, sourceRect);
    return result;
}

void AvatarCropWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setPen(QPen(QColor(QStringLiteral("#2B4654")), 1));
    painter.setBrush(QColor(QStringLiteral("#0B171F")));
    painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 9, 9);

    const QRectF crop = cropRect();
    if (!image_.isNull()) {
        drawCheckerboard(painter, crop, true);
        painter.save();
        painter.setClipPath(cropPath(crop, true));
        painter.drawImage(renderedImageRect(), image_);
        painter.restore();

        QPainterPath shade;
        shade.setFillRule(Qt::OddEvenFill);
        shade.addRect(rect());
        shade.addPath(cropPath(crop, true));
        painter.fillPath(shade, QColor(4, 10, 15, 174));
    } else {
        drawCheckerboard(painter, crop, true);
        painter.setPen(QColor(QStringLiteral("#7E97A4")));
        painter.drawText(crop, Qt::AlignCenter | Qt::TextWordWrap,
                         QStringLiteral("选择一张图片\n开始制作头像"));
    }

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(QStringLiteral("#74C8E5")), 2));
    painter.drawPath(cropPath(crop.adjusted(1, 1, -1, -1), true));
}

void AvatarCropWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && !image_.isNull()) {
        dragging_ = true;
        lastPointer_ = event->position();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void AvatarCropWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (dragging_) {
        offset_ += event->position() - lastPointer_;
        lastPointer_ = event->position();
        clampOffset();
        update();
        emit cropChanged();
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void AvatarCropWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (dragging_ && event->button() == Qt::LeftButton) {
        dragging_ = false;
        setCursor(Qt::OpenHandCursor);
        emit cropInteractionFinished();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

QRectF AvatarCropWidget::cropRect() const
{
    constexpr qreal margin = 23.0;
    const qreal side = qMin(width(), height()) - margin * 2.0;
    return QRectF((width() - side) / 2.0, (height() - side) / 2.0, side, side);
}

QSizeF AvatarCropWidget::renderedImageSize() const
{
    if (image_.isNull()) {
        return {};
    }
    const QRectF crop = cropRect();
    const qreal scale = qMax(crop.width() / image_.width(), crop.height() / image_.height())
        * zoom_;
    return QSizeF(image_.width() * scale, image_.height() * scale);
}

QRectF AvatarCropWidget::renderedImageRect() const
{
    const QRectF crop = cropRect();
    const QSizeF size = renderedImageSize();
    return QRectF(
        crop.center().x() - size.width() / 2.0 + offset_.x(),
        crop.center().y() - size.height() / 2.0 + offset_.y(),
        size.width(), size.height());
}

void AvatarCropWidget::clampOffset()
{
    if (!image_.isNull()) {
        const QRectF crop = cropRect();
        const QSizeF rendered = renderedImageSize();
        const qreal maxX = qMax<qreal>(0.0, (rendered.width() - crop.width()) / 2.0);
        const qreal maxY = qMax<qreal>(0.0, (rendered.height() - crop.height()) / 2.0);
        offset_.setX(std::clamp(offset_.x(), -maxX, maxX));
        offset_.setY(std::clamp(offset_.y(), -maxY, maxY));
        normalizedOffset_ = QPointF(
            maxX > 0.0 ? offset_.x() / maxX : 0.0,
            maxY > 0.0 ? offset_.y() / maxY : 0.0);
    }
}
