#include "ui/replica_window.h"

#include <utility>
#include <QLabel>
#include <QShowEvent>
#include <QTimer>

void ReplicaWindow::initializeSourceMonitoring()
{
    sourceCheckTimer_ = new QTimer(this);
    sourceCheckTimer_->setInterval(kSourceCheckIntervalMs);
    connect(sourceCheckTimer_, &QTimer::timeout, this, &ReplicaWindow::updateThumbnail);
}

const std::optional<WindowInfo>& ReplicaWindow::source() const noexcept
{
    return source_;
}

QSize ReplicaWindow::sourceFrameSize() const
{
    if (thumbnail_.has_value()) {
        try {
            const QSize currentSize = thumbnail_->sourceSize();
            if (currentSize.isValid() && !currentSize.isEmpty()) {
                return currentSize;
            }
        } catch (const NativeException&) {
        }
    }
    return sourceSize_;
}

void ReplicaWindow::selectSource(const WindowInfo& source)
{
    selectSourceImpl(source, true);
}

void ReplicaWindow::selectSourceFull(const WindowInfo& source)
{
    selectSourceImpl(source, false);
}

void ReplicaWindow::selectSourceImpl(const WindowInfo& source, bool preserveRegion)
{
    cancelRegionCapture();
    gateway_.cancelAutomaticActivation(replicaId_);
    statisticsCoordinator_.remove(replicaId_);
    emit pixelStatisticsUnavailable();
    thumbnail_.reset();
    source_.reset();
    selectedRegion_.reset();
    const auto previousRegion = preserveRegion
        ? capturedRegion_
        : std::optional<QRect>{};
    capturedRegion_.reset();
    try {
        auto thumbnail = gateway_.createThumbnail(
            static_cast<std::uintptr_t>(winId()),
            source.hwnd);
        const auto sourceSize = thumbnail.sourceSize();
        thumbnail_.emplace(std::move(thumbnail));
        sourceSize_ = sourceSize;
        source_ = source;
        emptyLabel_->hide();
        sourceNameOverlay_->setText(sourceNameFromTitle(source.title));
        positionSourceNameOverlay();
        sourceNameOverlay_->setVisible(
            sourceNameVisible_ && !sourceNameOverlay_->text().isEmpty());
        sourceNameOverlay_->raise();
        bool regionDiscarded = false;
        if (previousRegion.has_value()) {
            const QRect bounds(0, 0, sourceSize_.width(), sourceSize_.height());
            const QRect applied = previousRegion->normalized().intersected(bounds);
            if (!applied.isEmpty()) {
                capturedRegion_ = applied;
            } else {
                regionDiscarded = true;
            }
        }
        emit sourceTitleChanged(source.title);
        emit regionCaptureChanged(regionDiscarded
            ? QStringLiteral("未发现区域，已重置")
            : regionDescription());
        applyContentAspectRatio();
        updateThumbnail();
        sourceCheckTimer_->start();
        syncStatisticsRegistration();
    } catch (const NativeException& error) {
        thumbnail_.reset();
        source_.reset();
        gateway_.cancelAutomaticActivation(replicaId_);
        sourceCheckTimer_->stop();
        sourceNameOverlay_->hide();
        statisticsCoordinator_.remove(replicaId_);
        emit pixelStatisticsUnavailable();
        showFailure(QStringLiteral("无法显示窗口"), error);
    }
}

void ReplicaWindow::updateThumbnail()
{
    if (!thumbnail_.has_value()) {
        return;
    }
    const QRect source = capturedRegion_.value_or(
        QRect(0, 0, sourceSize_.width(), sourceSize_.height()));
    const QRect destination = thumbnailDestinationRect();
    try {
        // DWM thumbnails are composed above the QWidget backing store. Once a
        // capture starts, showing the live thumbnail again would cover the
        // frozen QImage and make the selection appear to resume playing.
        const bool visible = !sourceNameVisible_ && !capturingRegion_;
        thumbnail_->update(source, destination, 255, visible);
    } catch (const NativeException&) {
        if (popupRevealed_) {
            return;
        }
        const QString unavailableTitle = source_.has_value() ? source_->title : QString{};
        thumbnail_.reset();
        source_.reset();
        sourceCheckTimer_->stop();
        sourceNameOverlay_->hide();
        statisticsCoordinator_.remove(replicaId_);
        emit pixelStatisticsUnavailable();
        emptyLabel_->setText(
            unavailableTitle.isEmpty()
                ? QStringLiteral("等待游戏窗口")
                : QStringLiteral("等待游戏窗口：%1").arg(unavailableTitle));
        emptyLabel_->show();
        emit sourceUnavailable(unavailableTitle);
        close();
    }
}

void ReplicaWindow::showFailure(
    const QString& heading,
    const NativeException& error)
{
    emptyLabel_->setText(
        QStringLiteral("%1\n%2").arg(heading, QString::fromUtf8(error.what())));
    emptyLabel_->show();
}

void ReplicaWindow::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    updateThumbnail();
}
