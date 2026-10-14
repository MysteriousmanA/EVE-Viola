#include "ui/desktop_pet/floating_pet_window.h"

#include <algorithm>
#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QGuiApplication>
#include <QMenu>
#include <QMouseEvent>
#include <QScreen>
#include <QWheelEvent>

using namespace DesktopPetUi;

void FloatingPetWindow::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        dragging_ = true;
        dragged_ = false;
        pressGlobalPosition_ = event->globalPosition().toPoint();
        dragOffset_ = pressGlobalPosition_ - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void FloatingPetWindow::mouseMoveEvent(QMouseEvent* event)
{
    if (dragging_) {
        const QPoint globalPosition = event->globalPosition().toPoint();
        if ((globalPosition - pressGlobalPosition_).manhattanLength()
            >= QApplication::startDragDistance()) {
            dragged_ = true;
        }
        move(globalPosition - dragOffset_);
        if (dragged_) {
            emit avatarGeometryChanged(
                mapToGlobal(QPoint(width() / 2, avatarSize_ / 2)), avatarSize_);
        }
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void FloatingPetWindow::mouseReleaseEvent(QMouseEvent* event)
{
    if (dragging_ && event->button() == Qt::LeftButton) {
        const bool clicked = !dragged_;
        dragging_ = false;
        emit positionChanged(pos());
        if (clicked) {
            emit leftClicked(mapToGlobal(QPoint(width() / 2, avatarSize_ / 2)),
                             avatarSize_);
        } else {
            emit avatarGeometryChanged(
                mapToGlobal(QPoint(width() / 2, avatarSize_ / 2)), avatarSize_);
        }
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void FloatingPetWindow::wheelEvent(QWheelEvent* event)
{
    const int delta = event->angleDelta().y();
    if (delta == 0) {
        event->ignore();
        return;
    }

    wheelDeltaRemainder_ += delta;
    const int steps = wheelDeltaRemainder_ / 120;
    wheelDeltaRemainder_ %= 120;
    if (steps == 0) {
        event->accept();
        return;
    }

    const int nextSize = std::clamp(avatarSize_ + steps * 12,
                                    kPetSizeMinimum, kPetSizeMaximum);
    if (nextSize != avatarSize_) {
        const QPoint avatarCenter = mapToGlobal(
            QPoint(width() / 2, avatarSize_ / 2));
        avatarSize_ = nextSize;
        updateWindowRegion();

        QPoint nextPosition = avatarCenter
            - QPoint(width() / 2, avatarSize_ / 2);
        if (QScreen* screen = QGuiApplication::screenAt(avatarCenter); screen != nullptr) {
            const QRect available = screen->availableGeometry();
            nextPosition.setX(std::clamp(
                nextPosition.x(), available.left(),
                qMax(available.left(), available.right() - width() + 1)));
            nextPosition.setY(std::clamp(
                nextPosition.y(), available.top(),
                qMax(available.top(), available.bottom() - height() + 1)));
        }
        move(nextPosition);
        update();
        emit avatarGeometryChanged(
            mapToGlobal(QPoint(width() / 2, avatarSize_ / 2)), avatarSize_);
        emit sizeChanged(avatarSize_);
        emit positionChanged(pos());
    }
    event->accept();
}

void FloatingPetWindow::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);
    QAction* fishModeAction = menu.addAction(QStringLiteral("摸鱼模式"));
    fishModeAction->setCheckable(true);
    fishModeAction->setChecked(fishModeEnabled_);
    connect(fishModeAction, &QAction::toggled, this, [this](bool enabled) {
        fishModeEnabled_ = enabled;
        emit fishModeToggled(enabled);
    });
    menu.addSeparator();
    QAction* disableAction = menu.addAction(QStringLiteral("禁用桌宠"));
    if (menu.exec(event->globalPos()) == disableAction) {
        hideByUser();
    }
}
