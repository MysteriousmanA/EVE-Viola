#include "ui/replica_window.h"

#include <QLabel>
#include <QTimer>

void ReplicaWindow::initializeRegionCapture()
{
    connect(&statisticsCoordinator_, &StatisticsCoordinator::frameReady, this,
        [this](ReplicaId id, quint64 request, const QImage& frame) {
            if (id == replicaId_ && frameCapturePending_ && request == frameCaptureRequest_) {
                frameCapturePending_ = false;
                acceptFrozenFrame(frame);
            }
        });
    connect(&statisticsCoordinator_, &StatisticsCoordinator::frameUnavailable, this,
        [this](ReplicaId id, quint64 request, const QString& error) {
            if (id == replicaId_ && frameCapturePending_ && request == frameCaptureRequest_) {
                frameCapturePending_ = false;
                showFailure(QStringLiteral("无法冻结画面"), NativeException(error, -4));
            }
        });
}

bool ReplicaWindow::isCapturingRegion() const noexcept
{
    return capturingRegion_;
}

const std::optional<QRect>& ReplicaWindow::selectedRegion() const noexcept
{
    return selectedRegion_;
}

const std::optional<QRect>& ReplicaWindow::capturedRegion() const noexcept
{
    return capturedRegion_;
}

QSize ReplicaWindow::frozenFrameSize() const noexcept
{
    return frozenFrame_.size();
}

QString ReplicaWindow::regionDescription() const noexcept
{
    if (!capturedRegion_.has_value()) {
        return QStringLiteral("未截取");
    }
    const auto& region = *capturedRegion_;
    return QStringLiteral("%1×%2 @ (%3,%4)")
        .arg(region.width())
        .arg(region.height())
        .arg(region.left())
        .arg(region.top());
}

void ReplicaWindow::beginRegionCapture()
{
    if (!thumbnail_.has_value() || !source_.has_value() || capturingRegion_ || frameCapturePending_) {
        return;
    }
    cancelAdvancedLogic();
    emptyLabel_->hide();
    frameCapturePending_ = true;
    frameCaptureRequest_ = statisticsCoordinator_.requestFrame(
        replicaId_, static_cast<std::uintptr_t>(winId()));
    if (frameCaptureRequest_ == 0) {
        frameCapturePending_ = false;
        showFailure(QStringLiteral("无法冻结画面"), NativeException(QStringLiteral("捕获服务不可用"), -4));
    }
}

void ReplicaWindow::acceptFrozenFrame(const QImage& frame)
{
    if (!source_ || !thumbnail_ || closedNotified_) { return; }
    frozenFrame_ = frame;
    if (frozenFrame_.isNull()) {
        showFailure(QStringLiteral("无法冻结画面"), NativeException(QStringLiteral("捕获窗口画面"), -4));
        return;
    }

    capturingRegion_ = true;
    sourceCheckTimer_->stop();
    selectedRegion_.reset();
    captureOrigin_ = {};
    captureCurrent_ = {};
    const QRect source = capturedRegion_.value_or(
        QRect(0, 0, sourceSize_.width(), sourceSize_.height()));
    thumbnail_->update(source, thumbnailDestinationRect(), 255, false);
    update();
}

void ReplicaWindow::cancelRegionCapture()
{
    frameCapturePending_ = false;
    statisticsCoordinator_.cancelFrame(replicaId_);
    finishRegionCapture(false);
}

void ReplicaWindow::setRegionPresets(const QList<RegionPreset>& presets)
{
    regionPresets_ = presets;
}

void ReplicaWindow::applyRegionPreset(const RegionPreset& preset)
{
    if (frameCapturePending_) { cancelRegionCapture(); }
    if (!thumbnail_.has_value() || !source_.has_value()) {
        return;
    }
    const QRect bounds(0, 0, sourceSize_.width(), sourceSize_.height());
    const QRect applied = preset.region.normalized().intersected(bounds);
    if (applied.isEmpty()) {
        emit regionCaptureChanged(QStringLiteral("未发现区域，已重置"));
        return;
    }
    cancelAdvancedLogic();
    capturedRegion_ = applied;
    selectedRegion_.reset();
    applyContentAspectRatio();
    updateThumbnail();
    emit regionCaptured(applied);
    emit regionCaptureChanged(regionDescription());
    emit pixelStatisticsUnavailable();
    setStatisticsInterval(preset.statisticsIntervalMs);
    syncStatisticsRegistration();
}

void ReplicaWindow::setCapturedRegion(const QRect& region)
{
    if (frameCapturePending_) { cancelRegionCapture(); }
    if (!thumbnail_.has_value() || !source_.has_value() || region.isEmpty()) {
        return;
    }
    try {
        const QSize currentSourceSize = thumbnail_->sourceSize();
        if (currentSourceSize.isValid() && !currentSourceSize.isEmpty()) {
            sourceSize_ = currentSourceSize;
        }
    } catch (const NativeException&) {
        return;
    }
    const QRect bounds(0, 0, sourceSize_.width(), sourceSize_.height());
    const QRect applied = region.normalized().intersected(bounds);
    if (applied.isEmpty()) {
        emit regionCaptureChanged(QStringLiteral("未发现区域，已重置"));
        return;
    }
    cancelAdvancedLogic();
    capturedRegion_ = applied;
    selectedRegion_.reset();
    applyContentAspectRatio();
    updateThumbnail();
    emit regionCaptured(applied);
    emit regionCaptureChanged(regionDescription());
    emit pixelStatisticsUnavailable();
    syncStatisticsRegistration();
}

void ReplicaWindow::finishRegionCapture(bool confirmed)
{
    if (!capturingRegion_) {
        return;
    }
    const auto region = selectedRegion_;
    capturingRegion_ = false;
    frozenFrame_ = QImage{};

    if (confirmed && region.has_value() && !region->isEmpty()) {
        const auto sourceRegion = mapToSource(*region);
        if (sourceRegion.has_value()) {
            cancelAdvancedLogic();
            capturedRegion_ = sourceRegion;
            applyContentAspectRatio();
            emit regionCaptured(*sourceRegion);
            emit regionCaptureChanged(regionDescription());
            emit pixelStatisticsUnavailable();
            syncStatisticsRegistration();
        }
    }
    resumeFromCapture();
}

void ReplicaWindow::resumeFromCapture()
{
    selectedRegion_.reset();
    captureOrigin_ = {};
    captureCurrent_ = {};
    if (!thumbnail_.has_value() || !source_.has_value()) {
        emptyLabel_->show();
        return;
    }
    sourceCheckTimer_->start();
    updateThumbnail();
    update();
}

std::optional<QRect> ReplicaWindow::mapToSource(
    const QRect& windowRegion) const
{
    if (sourceSize_.width() <= 0 || sourceSize_.height() <= 0) {
        return std::nullopt;
    }
    const QRect displaySource = capturedRegion_.value_or(
        QRect(0, 0, sourceSize_.width(), sourceSize_.height()));
    const auto fitted = displayRect();
    if (fitted.width() <= 0 || fitted.height() <= 0) {
        return std::nullopt;
    }
    const auto scaleX =
        static_cast<double>(displaySource.width()) / fitted.width();
    const auto scaleY =
        static_cast<double>(displaySource.height()) / fitted.height();
    const auto left = displaySource.left()
        + qRound((windowRegion.left() - fitted.left()) * scaleX);
    const auto top = displaySource.top()
        + qRound((windowRegion.top() - fitted.top()) * scaleY);
    const auto right = displaySource.left()
        + qRound((windowRegion.right() - fitted.left()) * scaleX);
    const auto bottom = displaySource.top()
        + qRound((windowRegion.bottom() - fitted.top()) * scaleY);
    return QRect(left, top, right - left + 1, bottom - top + 1);
}
