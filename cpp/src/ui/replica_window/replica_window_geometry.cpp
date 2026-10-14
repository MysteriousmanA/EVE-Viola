#include "ui/replica_window.h"

#include <algorithm>
#include <cmath>
#include <QMoveEvent>
#include <QResizeEvent>

QRect ReplicaWindow::aspectFit(
    const QSize& source,
    const QSize& target) noexcept
{
    if (source.width() <= 0 || source.height() <= 0
        || target.width() <= 0 || target.height() <= 0) {
        return {};
    }

    const auto scale = std::min(
        static_cast<double>(target.width()) / source.width(),
        static_cast<double>(target.height()) / source.height());
    const auto width = std::max(1, qRound(source.width() * scale));
    const auto height = std::max(1, qRound(source.height() * scale));
    const auto left = (target.width() - width) / 2;
    const auto top = (target.height() - height) / 2;
    return QRect(left, top, width, height);
}

QRect ReplicaWindow::deviceAspectFit(
    const QSize& source,
    const QSize& logicalTarget,
    qreal devicePixelRatio) noexcept
{
    if (devicePixelRatio <= 0.0) {
        return aspectFit(source, logicalTarget);
    }
    const QSize deviceTarget(
        qRound(logicalTarget.width() * devicePixelRatio),
        qRound(logicalTarget.height() * devicePixelRatio));
    return aspectFit(source, deviceTarget);
}

void ReplicaWindow::resizeEvent(QResizeEvent* event)
{
    if (contentAspectRatio_ > 0.0 && !capturingRegion_) {
        enforceAspectRatio();
    }
    QWidget::resizeEvent(event);
    positionSourceNameOverlay();
    updateThumbnail();
    emit sizeChanged(size());
}

void ReplicaWindow::moveEvent(QMoveEvent* event)
{
    QWidget::moveEvent(event);
    emit positionChanged(event->pos());
    updateThumbnail();
}

void ReplicaWindow::moveTo(const QPoint& pos)
{
    move(pos);
}

void ReplicaWindow::applyContentAspectRatio()
{
    QSize content;
    if (capturedRegion_.has_value()) {
        content = capturedRegion_->size();
    } else {
        content = sourceSize_;
    }
    if (content.width() <= 0 || content.height() <= 0) {
        contentAspectRatio_ = 0.0;
        return;
    }
    contentAspectRatio_ =
        static_cast<double>(content.width()) / content.height();
    updateMinimumSize();

    const auto current = size();
    const auto newHeight = std::max(
        minimumHeight(),
        qRound(static_cast<double>(current.width()) / contentAspectRatio_));
    if (newHeight != current.height()) {
        resize(current.width(), newHeight);
    }
}

void ReplicaWindow::updateMinimumSize()
{
    if (contentAspectRatio_ <= 0.0) {
        setMinimumSize(kMinimumWindowEdge, kMinimumWindowEdge);
        return;
    }
    if (contentAspectRatio_ >= 1.0) {
        setMinimumSize(
            std::max(kMinimumWindowEdge,
                     static_cast<int>(std::ceil(kMinimumWindowEdge * contentAspectRatio_))),
            kMinimumWindowEdge);
    } else {
        setMinimumSize(
            kMinimumWindowEdge,
            std::max(kMinimumWindowEdge,
                     static_cast<int>(std::ceil(kMinimumWindowEdge / contentAspectRatio_))));
    }
}

void ReplicaWindow::enforceAspectRatio()
{
    if (contentAspectRatio_ <= 0.0) {
        return;
    }
    const auto current = size();
    if (current.height() <= 0) {
        return;
    }
    const auto newHeight = std::max(
        minimumHeight(),
        qRound(static_cast<double>(current.width()) / contentAspectRatio_));
    if (newHeight != current.height()) {
        resize(current.width(), newHeight);
    }
}

QRect ReplicaWindow::contentRect() const noexcept
{
    return rect().adjusted(kBorderWidth, kBorderWidth, -kBorderWidth, -kBorderWidth);
}

QRect ReplicaWindow::displayRect() const
{
    const QSize displaySource = capturedRegion_.has_value()
        ? capturedRegion_->size()
        : sourceSize_;
    QRect fitted = aspectFit(displaySource, contentRect().size());
    fitted.translate(contentRect().topLeft());
    return fitted;
}

QRect ReplicaWindow::thumbnailDestinationRect() const
{
    const QSize displaySource = capturedRegion_.has_value()
        ? capturedRegion_->size()
        : sourceSize_;
    const qreal ratio = devicePixelRatioF();
    const QRect contentDevice(
        qRound(kBorderWidth * ratio),
        qRound(kBorderWidth * ratio),
        qRound((width() - 2 * kBorderWidth) * ratio),
        qRound((height() - 2 * kBorderWidth) * ratio));
    QRect fitted = deviceAspectFit(displaySource, contentDevice.size(), 1.0);
    fitted.translate(contentDevice.topLeft());
    return fitted;
}

void ReplicaWindow::setWheelZoomLocked(bool locked)
{
    wheelZoomLocked_ = locked;
}

void ReplicaWindow::resizeToScalePercentage(double percentage)
{
    if (frameCapturePending_) { cancelRegionCapture(); }
    if (!capturedRegion_.has_value() || capturedRegion_->isEmpty()
        || !std::isfinite(percentage) || percentage <= 0.0) {
        return;
    }

    const double sourceWidth = capturedRegion_->width();
    const double sourceHeight = capturedRegion_->height();
    const double requestedScale = percentage / 100.0;
    const double minimumScale = std::max(
        static_cast<double>(minimumWidth()) / sourceWidth,
        static_cast<double>(minimumHeight()) / sourceHeight);
    const double maximumScale = std::min(
        static_cast<double>(maximumWidth()) / sourceWidth,
        static_cast<double>(maximumHeight()) / sourceHeight);
    const double scale = std::clamp(
        requestedScale, std::min(minimumScale, maximumScale), maximumScale);
    const int targetWidth = qBound(
        minimumWidth(), qRound(sourceWidth * scale), maximumWidth());
    const int targetHeight = qBound(
        minimumHeight(), qRound(sourceHeight * scale), maximumHeight());
    resize(targetWidth, targetHeight);
}
