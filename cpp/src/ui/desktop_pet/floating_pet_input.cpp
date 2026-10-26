#include "ui/desktop_pet/floating_pet_window.h"

#include <algorithm>
#include <QAction>
#include <QActionGroup>
#include <memory>
#include <QApplication>
#include <QContextMenuEvent>
#include <QGuiApplication>
#include <QMenu>
#include <QMouseEvent>
#include <QScreen>
#include <QScrollBar>
#include <QTextBrowser>
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
    if (historyVisible_) {
        auto* scrollBar = historyBubble_->verticalScrollBar();
        int distance = event->pixelDelta().y();
        if (distance == 0) {
            historyWheelRemainder_ += event->angleDelta().y()
                * scrollBar->singleStep() * QApplication::wheelScrollLines();
            distance = historyWheelRemainder_ / 120;
            historyWheelRemainder_ %= 120;
        }
        scrollBar->setValue(scrollBar->value() - distance);
        // Consume the event even at a scroll boundary so it never resizes the avatar.
        event->accept();
        return;
    }
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

QMenu* FloatingPetWindow::buildContextMenu()
{
    auto* menu = new QMenu(this);
    auto* modes = new QActionGroup(menu);
    modes->setExclusionPolicy(QActionGroup::ExclusionPolicy::Exclusive);
    for (auto mode : {DesktopPetMode::Resident, DesktopPetMode::Fish, DesktopPetMode::DoNotDisturb}) {
        auto* action = menu->addAction(desktopPetModeName(mode));
        action->setObjectName(QStringLiteral("desktopPetMode_%1").arg(desktopPetModeKey(mode)));
        action->setCheckable(true);
        modes->addAction(action);
        action->setChecked(mode_ == mode);
        connect(action, &QAction::triggered, this, [this, mode] { setMode(mode); });
    }
    menu->addSeparator();
    auto* disable = menu->addAction(QStringLiteral("禁用桌宠"));
    disable->setObjectName(QStringLiteral("desktopPetDisable"));
    connect(disable, &QAction::triggered, this, &FloatingPetWindow::hideByUser);
    auto* stop = menu->addAction(QStringLiteral("停止软件"));
    stop->setObjectName(QStringLiteral("desktopPetStopSoftware"));
    connect(stop, &QAction::triggered, this, &FloatingPetWindow::stopSoftwareRequested);
    return menu;
}

void FloatingPetWindow::contextMenuEvent(QContextMenuEvent* event)
{
    std::unique_ptr<QMenu> menu(buildContextMenu());
    menu->exec(event->globalPos());
}
