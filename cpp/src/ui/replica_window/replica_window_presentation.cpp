#include "ui/replica_window.h"

#include <QPainter>
#include <QPaintEvent>
#include <QTimer>

void ReplicaWindow::initializeFlash()
{
    flashTimer_ = new QTimer(this);
    flashTimer_->setInterval(kFlashIntervalMs);
    connect(flashTimer_, &QTimer::timeout, this, [this] {
        flashOn_ = !flashOn_;
        update();
    });
}

void ReplicaWindow::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);

    QPainter painter(this);

    QRegion backdrop(rect());
    if (thumbnail_.has_value() || capturingRegion_) {
        backdrop -= QRegion(displayRect());
    }
    painter.save();
    painter.setClipRegion(backdrop);
    painter.fillRect(rect(), QColor(0x05, 0x09, 0x0C));
    painter.restore();

    if (capturingRegion_ && !frozenFrame_.isNull()) {
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.drawImage(displayRect(), frozenFrame_, thumbnailDestinationRect());

        QRect selection = selectedRegion_.value_or(
            QRect(captureOrigin_, captureCurrent_));
        if (!selection.isEmpty()) {
            selection = selection.normalized();
            painter.fillRect(selection, QColor(0, 170, 255, 40));

            QPen pen(QColor(0, 170, 255), 1, Qt::SolidLine);
            painter.setPen(pen);
            painter.drawRect(selection.adjusted(0, 0, -1, -1));
        }

        painter.setPen(Qt::white);
        painter.drawText(
            rect().adjusted(8, 8, -8, -8),
            Qt::AlignTop | Qt::AlignLeft,
            QStringLiteral("拖动框选区域，回车确认，Esc 取消"));
    }

    QColor border = borderColor_;
    if (flashActive_) {
        border = flashOn_ ? flashColor_ : QColor(0, 0, 0);
    } else if (globalDimmed_) {
        border = QColor(0, 0, 0);
    }
    painter.fillRect(0, 0, width(), kBorderWidth, border);
    painter.fillRect(0, height() - kBorderWidth, width(), kBorderWidth, border);
    painter.fillRect(0, kBorderWidth, kBorderWidth, height() - 2 * kBorderWidth, border);
    painter.fillRect(width() - kBorderWidth, kBorderWidth, kBorderWidth, height() - 2 * kBorderWidth, border);
}

void ReplicaWindow::setPresentationSuppressed(bool suppressed) { presentationSuppressed_ = suppressed; }

void ReplicaWindow::setVisible(bool visible)
{
    visible = visible && desktopPetMode_ == DesktopPetMode::Resident && !presentationSuppressed_;
    if (!visible && frameCapturePending_) { cancelRegionCapture(); }
    QWidget::setVisible(visible);
    if (!visible) {
        popupRevealed_ = false;
        popupRevealedFeedbackId_ = 0;
    }
}

void ReplicaWindow::applyColorLogicBorder()
{
    QColor color = sharedLogicBorderColor_.isValid()
        ? sharedLogicBorderColor_ : QColor(255, 255, 255);
    if (!sharedLogicBorderColor_.isValid() && !globalColorLogicManaged_
        && colorLogicEngine_.enabled()) {
        const int slot = colorLogicEngine_.activeSlot() - 1;
        if (slot >= 0 && slot < 2 && colorLogicSlotColor_[slot].isValid()) {
            color = colorLogicSlotColor_[slot];
        }
    }
    if (color != borderColor_) {
        borderColor_ = color;
        if (flashActive_) { flashColor_ = color; }
        update();
    }
}

void ReplicaWindow::setSharedLogicBorderColor(const QColor& color)
{
    if (sharedLogicBorderColor_ == color) { return; }
    sharedLogicBorderColor_ = color;
    applyColorLogicBorder();
}

QColor ReplicaWindow::borderColor() const noexcept
{
    return borderColor_;
}

bool ReplicaWindow::isFlashActive() const noexcept
{
    return flashActive_;
}

void ReplicaWindow::setFlashActive(bool active)
{
    if (flashActive_ == active) {
        return;
    }
    flashActive_ = active;
    if (active) {
        flashColor_ = borderColor_;
        flashOn_ = true;
        flashTimer_->start();
    } else {
        flashTimer_->stop();
        flashOn_ = false;
    }
    emit flashStateChanged(active);
    update();
}

void ReplicaWindow::setGlobalFlashState(bool dimmed)
{
    if (globalDimmed_ == dimmed) {
        return;
    }
    globalDimmed_ = dimmed;
    update();
}
