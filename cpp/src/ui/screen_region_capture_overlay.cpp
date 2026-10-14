#include "ui/screen_region_capture_overlay.h"

#include <cmath>

#include <QColor>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QShowEvent>

ScreenRegionCaptureOverlay::ScreenRegionCaptureOverlay(
    const QRect& screenGeometry,
    const QRectF& clientGeometry,
    const QSize& sourceSize,
    QWidget* parent)
    : QWidget(
          parent,
          Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
              | Qt::NoDropShadowWindowHint)
    , screenGeometry_(screenGeometry)
    , clientRect_(clientGeometry.translated(
          QPointF(-screenGeometry.x(), -screenGeometry.y())))
    , sourceSize_(sourceSize)
{
    setObjectName(QStringLiteral("screenRegionCaptureOverlay"));
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setGeometry(screenGeometry_);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);
}

void ScreenRegionCaptureOverlay::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    raise();
    activateWindow();
    setFocus(Qt::OtherFocusReason);
}

void ScreenRegionCaptureOverlay::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(0, 0, 0, 72));

    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(83, 220, 165), 2));
    painter.drawRect(clientRect_.adjusted(0, 0, -1, -1));

    if (selecting_) {
        const QRect selection = QRect(selectionOrigin_, selectionCurrent_).normalized();
        painter.fillRect(selection, QColor(0, 170, 255, 52));
        painter.setPen(QPen(QColor(0, 190, 255), 1));
        painter.drawRect(selection.adjusted(0, 0, -1, -1));
    }

    const QRect hintRect(18, 18, 420, 48);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(12, 29, 39, 225));
    painter.drawRoundedRect(hintRect, 7, 7);
    painter.setPen(QColor(235, 243, 248));
    painter.drawText(
        hintRect.adjusted(14, 4, -14, -4),
        Qt::AlignVCenter | Qt::AlignLeft,
        QStringLiteral("在游戏画面内拖动框选区域，Enter 确认，Esc 取消"));
}

void ScreenRegionCaptureOverlay::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || !clientRect_.contains(event->position())) {
        event->accept();
        return;
    }
    selectionOrigin_ = clampToClient(event->position().toPoint());
    selectionCurrent_ = selectionOrigin_;
    selecting_ = true;
    selectionReady_ = false;
    update();
    event->accept();
}

void ScreenRegionCaptureOverlay::mouseMoveEvent(QMouseEvent* event)
{
    if (!selecting_ || !event->buttons().testFlag(Qt::LeftButton)) {
        event->accept();
        return;
    }
    selectionCurrent_ = clampToClient(event->position().toPoint());
    update();
    event->accept();
}

void ScreenRegionCaptureOverlay::mouseReleaseEvent(QMouseEvent* event)
{
    if (!selecting_ || event->button() != Qt::LeftButton) {
        event->accept();
        return;
    }
    selectionCurrent_ = clampToClient(event->position().toPoint());
    selectionReady_ = true;
    update();
    event->accept();
}

void ScreenRegionCaptureOverlay::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        finish(false);
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        finish(selectionReady_);
        event->accept();
        return;
    }
    event->accept();
}

void ScreenRegionCaptureOverlay::closeEvent(QCloseEvent* event)
{
    if (!finished_) {
        finished_ = true;
        emit selectionFinished({}, false);
    }
    event->accept();
}

QPoint ScreenRegionCaptureOverlay::clampToClient(const QPoint& point) const noexcept
{
    const int left = static_cast<int>(std::ceil(clientRect_.left()));
    const int top = static_cast<int>(std::ceil(clientRect_.top()));
    const int right = static_cast<int>(std::ceil(clientRect_.right())) - 1;
    const int bottom = static_cast<int>(std::ceil(clientRect_.bottom())) - 1;
    return QPoint(
        qBound(left, point.x(), qMax(left, right)),
        qBound(top, point.y(), qMax(top, bottom)));
}

QRect ScreenRegionCaptureOverlay::mapSelectionToSource() const noexcept
{
    if (!selectionReady_ || clientRect_.isEmpty()
        || sourceSize_.width() <= 0 || sourceSize_.height() <= 0) {
        return {};
    }

    const QRect selection = QRect(selectionOrigin_, selectionCurrent_)
        .normalized();
    if (selection.isEmpty()) {
        return {};
    }

    const qreal scaleX = sourceSize_.width() / clientRect_.width();
    const qreal scaleY = sourceSize_.height() / clientRect_.height();
    const qreal localLeft = selection.left() - clientRect_.left();
    const qreal localTop = selection.top() - clientRect_.top();
    const qreal localRightExclusive = selection.right() + 1 - clientRect_.left();
    const qreal localBottomExclusive = selection.bottom() + 1 - clientRect_.top();

    const int left = qBound(
        0, static_cast<int>(std::floor(localLeft * scaleX)), sourceSize_.width() - 1);
    const int top = qBound(
        0, static_cast<int>(std::floor(localTop * scaleY)), sourceSize_.height() - 1);
    const int rightExclusive = qBound(
        left + 1,
        static_cast<int>(std::ceil(localRightExclusive * scaleX)),
        sourceSize_.width());
    const int bottomExclusive = qBound(
        top + 1,
        static_cast<int>(std::ceil(localBottomExclusive * scaleY)),
        sourceSize_.height());
    return QRect(left, top, rightExclusive - left, bottomExclusive - top);
}

void ScreenRegionCaptureOverlay::finish(bool accepted)
{
    if (finished_) {
        return;
    }
    finished_ = true;
    const QRect sourceRegion = accepted ? mapSelectionToSource() : QRect{};
    emit selectionFinished(sourceRegion, accepted && !sourceRegion.isEmpty());
    close();
}
