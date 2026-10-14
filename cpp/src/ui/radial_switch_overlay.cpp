#include "ui/radial_switch_overlay.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include <QFontMetrics>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRegion>
#include <QScreen>
#include <QTimer>
#include <QtMath>

namespace {

constexpr qreal kPi = 3.14159265358979323846;

QPainterPath sectorPath(const QPointF& center, qreal radius, qreal start, qreal end)
{
    QPainterPath path;
    path.moveTo(center);
    const int steps = qMax(8, qCeil((end - start) * radius / 8.0));
    for (int step = 0; step <= steps; ++step) {
        const qreal angle = start + (end - start) * step / steps;
        path.lineTo(center.x() + radius * std::cos(angle),
                    center.y() + radius * std::sin(angle));
    }
    path.closeSubpath();
    return path;
}

QPainterPath annularSectorPath(
    const QPointF& center,
    qreal innerRadius,
    qreal outerRadius,
    qreal start,
    qreal end)
{
    QPainterPath path;
    const int steps = qMax(8, qCeil((end - start) * outerRadius / 8.0));
    path.moveTo(center.x() + outerRadius * std::cos(start),
                center.y() + outerRadius * std::sin(start));
    for (int step = 1; step <= steps; ++step) {
        const qreal angle = start + (end - start) * step / steps;
        path.lineTo(center.x() + outerRadius * std::cos(angle),
                    center.y() + outerRadius * std::sin(angle));
    }
    path.lineTo(center.x() + innerRadius * std::cos(end),
                center.y() + innerRadius * std::sin(end));
    for (int step = steps - 1; step >= 0; --step) {
        const qreal angle = start + (end - start) * step / steps;
        path.lineTo(center.x() + innerRadius * std::cos(angle),
                    center.y() + innerRadius * std::sin(angle));
    }
    path.closeSubpath();
    return path;
}

} // namespace

RadialSwitchOverlay::RadialSwitchOverlay(QWidget* parent)
    : QWidget(parent,
          Qt::Tool
              | Qt::FramelessWindowHint
              | Qt::WindowStaysOnTopHint
              | Qt::WindowTransparentForInput
              | Qt::WindowDoesNotAcceptFocus
              | Qt::NoDropShadowWindowHint)
{
    setObjectName(QStringLiteral("radialSwitchOverlay"));
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFocusPolicy(Qt::NoFocus);
    setMouseTracking(true);
    flashTimer_ = new QTimer(this);
    flashTimer_->setInterval(240);
    connect(flashTimer_, &QTimer::timeout, this, [this] {
        if (--flashTicksRemaining_ <= 0) {
            flashTimer_->stop();
            flashingIndex_ = -1;
            flashPhase_ = false;
        } else {
            flashPhase_ = !flashPhase_;
        }
        update();
    });
}

bool RadialSwitchOverlay::showForClients(
    const QList<WindowInfo>& clients,
    const QPoint& globalAnchor,
    bool interactivePreview,
    std::function<void(int)> previewSelectionHandler,
    int centerImageDiameter)
{
    clients_ = clients;
    flashTimer_->stop();
    flashingIndex_ = -1;
    flashTicksRemaining_ = 0;
    flashPhase_ = false;
    interactivePreview_ = interactivePreview;
    petRingMode_ = centerImageDiameter > 0;
    centerImageDiameter_ = qMax(0, centerImageDiameter);
    previewSelectionHandler_ = std::move(previewSelectionHandler);
    Qt::WindowFlags flags = windowFlags();
    flags.setFlag(Qt::WindowTransparentForInput, !interactivePreview_);
    flags.setFlag(Qt::WindowDoesNotAcceptFocus, !interactivePreview_);
    setWindowFlags(flags);
    setAttribute(Qt::WA_TransparentForMouseEvents, !interactivePreview_);
    setAttribute(Qt::WA_ShowWithoutActivating, !interactivePreview_);
    setFocusPolicy(interactivePreview_ ? Qt::StrongFocus : Qt::NoFocus);
    if (clients_.isEmpty()) {
        hide();
        selectedIndex_ = -1;
        return false;
    }

    QScreen* screen = QGuiApplication::screenAt(globalAnchor);
    if (screen == nullptr) {
        screen = QGuiApplication::primaryScreen();
    }
    if (screen == nullptr) {
        clients_.clear();
        hide();
        selectedIndex_ = -1;
        return false;
    }

    monitorGeometry_ = screen->geometry();
    updateRadii();
    const int margin = qCeil(outerRadius_) + 12;
    const int minX = monitorGeometry_.left() + margin;
    const int maxX = monitorGeometry_.right() - margin;
    const int minY = monitorGeometry_.top() + margin;
    const int maxY = monitorGeometry_.bottom() - margin;
    centerGlobal_ = petRingMode_
        ? globalAnchor
        : QPoint(
              minX <= maxX ? qBound(minX, globalAnchor.x(), maxX) : monitorGeometry_.center().x(),
              minY <= maxY ? qBound(minY, globalAnchor.y(), maxY) : monitorGeometry_.center().y());
    anchorGlobal_ = globalAnchor;
    cursorGlobal_ = globalAnchor;
    selectedIndex_ = -1;
    updateOverlayGeometry();
    updatePetRingMask();
    show();
    raise();
    if (interactivePreview_ && !petRingMode_) {
        activateWindow();
        setFocus(Qt::OtherFocusReason);
    }
    update();
    return true;
}

bool RadialSwitchOverlay::isPetRingVisible() const noexcept
{
    return petRingMode_ && isVisible();
}

void RadialSwitchOverlay::movePetCenter(
    const QPoint& globalCenter,
    int centerImageDiameter)
{
    if (!isPetRingVisible()) {
        return;
    }

    centerGlobal_ = globalCenter;
    anchorGlobal_ = globalCenter;
    cursorGlobal_ = globalCenter;
    centerImageDiameter_ = qMax(1, centerImageDiameter);
    selectedIndex_ = -1;

    QScreen* screen = QGuiApplication::screenAt(globalCenter);
    if (screen != nullptr && monitorGeometry_ != screen->geometry()) {
        monitorGeometry_ = screen->geometry();
    }
    updateRadii();
    updateOverlayGeometry();
    updatePetRingMask();
    update();
}

void RadialSwitchOverlay::flashPetAccount(std::uintptr_t hwnd)
{
    if (!isPetRingVisible() || hwnd == 0) {
        return;
    }
    for (qsizetype index = 0; index < clients_.size(); ++index) {
        if (clients_.at(index).hwnd == hwnd) {
            flashingIndex_ = static_cast<int>(index);
            flashTicksRemaining_ = 12;
            flashPhase_ = true;
            flashTimer_->start();
            update();
            return;
        }
    }
}

void RadialSwitchOverlay::updateCursorPosition(const QPoint& globalPosition)
{
    if (!isVisible()) {
        return;
    }
    cursorGlobal_ = globalPosition;
    const qreal movement = std::hypot(
        globalPosition.x() - anchorGlobal_.x(),
        globalPosition.y() - anchorGlobal_.y());
    selectedIndex_ = movement < 12.0
        ? -1
        : indexAt(globalPosition);
    update();
}

int RadialSwitchOverlay::selectedIndex() const noexcept
{
    return selectedIndex_;
}

void RadialSwitchOverlay::updateRadii()
{
    outerRadius_ = qMax<qreal>(
        72.0,
        qMin<qreal>(168.0,
                    qMin(monitorGeometry_.width(), monitorGeometry_.height()) / 2.0 - 20.0));
    if (petRingMode_) {
        outerRadius_ = qMin<qreal>(
            outerRadius_, qMax<qreal>(72.0, centerImageDiameter_ / 2.0 + 84.0));
        innerRadius_ = qMin<qreal>(
            centerImageDiameter_ / 2.0 + 10.0, outerRadius_ - 16.0);
        innerRadius_ = qMax<qreal>(24.0, innerRadius_);
    } else {
        innerRadius_ = qMin<qreal>(48.0, outerRadius_ * 0.34);
    }
}

void RadialSwitchOverlay::updateOverlayGeometry()
{
    if (petRingMode_) {
        const int margin = qCeil(outerRadius_) + 12;
        screenGeometry_ = QRect(
            centerGlobal_.x() - margin,
            centerGlobal_.y() - margin,
            margin * 2 + 1,
            margin * 2 + 1);
    } else {
        screenGeometry_ = monitorGeometry_;
    }
    setGeometry(screenGeometry_);
}

void RadialSwitchOverlay::updatePetRingMask()
{
    if (!petRingMode_) {
        clearMask();
        return;
    }

    const QPoint localCenter = centerGlobal_ - screenGeometry_.topLeft();
    const int outer = qCeil(outerRadius_) + 2;
    const int inner = qMax(0, qFloor(innerRadius_) - 2);
    QRegion ring(
        QRect(localCenter.x() - outer,
              localCenter.y() - outer,
              outer * 2 + 1,
              outer * 2 + 1),
        QRegion::Ellipse);
    if (inner > 0) {
        ring -= QRegion(
            QRect(localCenter.x() - inner,
                  localCenter.y() - inner,
                  inner * 2 + 1,
                  inner * 2 + 1),
            QRegion::Ellipse);
    }
    setMask(ring);
}

int RadialSwitchOverlay::indexAt(const QPoint& globalPosition) const noexcept
{
    if (clients_.isEmpty()) {
        return -1;
    }
    const qreal dx = globalPosition.x() - centerGlobal_.x();
    const qreal dy = globalPosition.y() - centerGlobal_.y();
    const qreal distance = std::hypot(dx, dy);
    if (distance < innerRadius_ || (petRingMode_ && distance > outerRadius_)) {
        return -1;
    }
    qreal angle = std::atan2(dy, dx) + kPi / 2.0;
    if (angle < 0.0) {
        angle += 2.0 * kPi;
    }
    const qreal segmentAngle = 2.0 * kPi / clients_.size();
    return qMin(static_cast<int>(clients_.size()) - 1,
                static_cast<int>(angle / segmentAngle));
}

void RadialSwitchOverlay::paintEvent(QPaintEvent*)
{
    if (clients_.isEmpty() || outerRadius_ <= 0.0) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    if (!petRingMode_) {
        painter.fillRect(rect(), QColor(0, 0, 0, 18));
    }

    const QPointF center = centerGlobal_ - screenGeometry_.topLeft();
    const qreal segmentAngle = 2.0 * kPi / clients_.size();
    QFont labelFont = painter.font();
    labelFont.setBold(true);
    labelFont.setPixelSize(qBound(
        8, 13 - static_cast<int>(clients_.size()) / 3, 13));
    painter.setFont(labelFont);
    const QFontMetrics metrics(labelFont);

    for (qsizetype index = 0; index < clients_.size(); ++index) {
        const qreal start = -kPi / 2.0 + index * segmentAngle;
        const qreal end = start + segmentAngle;
        const bool selected = index == selectedIndex_;
        const bool flashing = petRingMode_ && index == flashingIndex_ && flashPhase_;
        painter.setPen(QPen(flashing ? QColor(255, 227, 112, 255)
                                     : selected ? QColor(139, 224, 255, 245)
                                                : QColor(108, 143, 158, 210),
                            flashing ? 3.2 : selected ? 2.5 : 1.2));
        painter.setBrush(flashing ? QColor(191, 132, 22, 248)
                                  : selected ? QColor(23, 130, 173, 238)
                                             : QColor(12, 27, 36, 225));
        painter.drawPath(petRingMode_
                ? annularSectorPath(center, innerRadius_, outerRadius_, start, end)
                : sectorPath(center, outerRadius_, start, end));

        const qreal middle = start + segmentAngle / 2.0;
        const qreal labelRadius = petRingMode_
            ? (innerRadius_ + outerRadius_) / 2.0
            : (innerRadius_ + outerRadius_) * 0.58;
        const QPointF labelCenter(
            center.x() + labelRadius * std::cos(middle),
            center.y() + labelRadius * std::sin(middle));
        const int labelWidth = qMax(
            34,
            qMin(150, qRound(2.0 * labelRadius * std::sin(segmentAngle / 2.0) - 10.0)));
        const QRect labelRect(
            qRound(labelCenter.x() - labelWidth / 2.0),
            qRound(labelCenter.y() - 18.0),
            labelWidth,
            36);
        const QString accountName = clients_.at(index).title.trimmed().isEmpty()
            ? QStringLiteral("账号 %1").arg(index + 1)
            : clients_.at(index).title.trimmed();
        const QString displayName = metrics.elidedText(
            accountName, Qt::ElideRight, labelRect.width());
        painter.setPen(QColor(242, 248, 251));
        painter.drawText(labelRect, Qt::AlignCenter, displayName);
    }

    if (!petRingMode_) {
        painter.setPen(QPen(QColor(188, 222, 235, 235), 1.5));
        painter.setBrush(QColor(8, 19, 27, 246));
        painter.drawEllipse(center, innerRadius_, innerRadius_);
        painter.setPen(QColor(231, 242, 247));
        QFont centerFont = painter.font();
        centerFont.setBold(false);
        centerFont.setPixelSize(11);
        painter.setFont(centerFont);
        painter.drawText(
            QRectF(center.x() - innerRadius_ + 3,
                   center.y() - 24,
                   innerRadius_ * 2.0 - 6,
                   48),
            Qt::AlignCenter | Qt::TextWordWrap,
            interactivePreview_
                ? QStringLiteral("点击扇区设位\n右键 / Esc 退出")
                : QStringLiteral("移动高亮\n松开切换"));
    }
}

void RadialSwitchOverlay::mouseMoveEvent(QMouseEvent* event)
{
    if (interactivePreview_) {
        updateCursorPosition(event->globalPosition().toPoint());
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void RadialSwitchOverlay::mousePressEvent(QMouseEvent* event)
{
    if (interactivePreview_) {
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void RadialSwitchOverlay::mouseReleaseEvent(QMouseEvent* event)
{
    if (!interactivePreview_) {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    if (event->button() == Qt::RightButton) {
        hide();
        event->accept();
        return;
    }
    updateCursorPosition(event->globalPosition().toPoint());
    const int chosenIndex = selectedIndex_;
    auto handler = previewSelectionHandler_;
    hide();
    if (event->button() == Qt::LeftButton && chosenIndex >= 0 && handler) {
        handler(chosenIndex);
    }
    event->accept();
}

void RadialSwitchOverlay::keyPressEvent(QKeyEvent* event)
{
    if (interactivePreview_ && event->key() == Qt::Key_Escape) {
        hide();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}
