#include "ui/replica_window.h"

#include <algorithm>
#include <QFont>
#include <QFontMetrics>
#include <QLabel>
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

    const bool sourceNameMode =
        sourceNameVisible_ && source_.has_value() && !capturingRegion_;
    if (sourceNameMode) {
        painter.fillRect(rect(), QColor(0x05, 0x09, 0x0C));
    } else {
        QRegion backdrop(rect());
        if (thumbnail_.has_value() || capturingRegion_) {
            backdrop -= QRegion(displayRect());
        }
        painter.save();
        painter.setClipRegion(backdrop);
        painter.fillRect(rect(), QColor(0x05, 0x09, 0x0C));
        painter.restore();
    }

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

void ReplicaWindow::setSourceNameVisible(bool visible)
{
    if (sourceNameVisible_ == visible) { return; }
    if (frameCapturePending_) { cancelRegionCapture(); }
    sourceNameVisible_ = visible;
    if (!sourceNameVisible_) {
        sourceNameOverlay_->hide();
    }
    updateThumbnail();
    sourceNameOverlay_->setVisible(
        sourceNameVisible_ && source_.has_value() && !sourceNameOverlay_->text().isEmpty());
    if (sourceNameOverlay_->isVisible()) {
        sourceNameOverlay_->raise();
    }
    update();
    emit settingsChanged();
}

bool ReplicaWindow::sourceNameVisible() const noexcept { return sourceNameVisible_; }

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
    QColor color(255, 255, 255);
    if (colorLogicEngine_.enabled()) {
        const int slot = colorLogicEngine_.activeSlot() - 1;
        if (slot >= 0 && slot < 2 && colorLogicSlotColor_[slot].isValid()) {
            color = colorLogicSlotColor_[slot];
        }
    }
    if (color != borderColor_) {
        borderColor_ = color;
        update();
    }
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

void ReplicaWindow::positionSourceNameOverlay()
{
    static constexpr int kBaseWindowWidth = 360;
    static constexpr int kBaseWindowHeight = 240;
    static constexpr int kBaseFontPointSize = 18;
    static constexpr int kMinimumFontPointSize = 6;

    const double scale = std::clamp(
        std::min(
            static_cast<double>(width()) / kBaseWindowWidth,
            static_cast<double>(height()) / kBaseWindowHeight),
        0.5,
        4.0);

    QFont font = sourceNameOverlay_->font();
    int pointSize = qRound(kBaseFontPointSize * scale);
    const int availableWidth = std::max(
        1,
        width() - 2 * sourceNameOverlay_->margin() - 2 * kBorderWidth);
    QFontMetrics metrics(font);
    while (pointSize > kMinimumFontPointSize
           && metrics.horizontalAdvance(sourceNameOverlay_->text()) > availableWidth) {
        --pointSize;
        font.setPointSize(pointSize);
        metrics = QFontMetrics(font);
    }
    font.setPointSize(pointSize);
    if (font != sourceNameOverlay_->font()) {
        sourceNameOverlay_->setFont(font);
    }

    sourceNameOverlay_->adjustSize();
    sourceNameOverlay_->move(
        (width() - sourceNameOverlay_->width()) / 2,
        (height() - sourceNameOverlay_->height()) / 2);
}

QString ReplicaWindow::sourceNameFromTitle(const QString& title)
{
    const QString trimmed = title.trimmed();
    const QString separator = QStringLiteral(" - ");
    const qsizetype separatorIndex = trimmed.lastIndexOf(separator);
    if (separatorIndex >= 0) {
        return trimmed.mid(separatorIndex + separator.size()).trimmed();
    }
    return trimmed;
}
