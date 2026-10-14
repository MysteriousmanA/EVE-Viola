#include "ui/replica_window.h"

#include <algorithm>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTimer>
#include <QWheelEvent>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

void ReplicaWindow::initializeInput()
{
    middlePressTimer_ = new QTimer(this);
    middlePressTimer_->setSingleShot(true);
    middlePressTimer_->setInterval(kMiddlePressLongMs);
    connect(middlePressTimer_, &QTimer::timeout, this, [this] {
        middleLongPressHandled_ = true;
        setColorLogicEnabled(!colorLogicEngine_.enabled());
        emit colorLogicEnabledChanged(colorLogicEngine_.enabled());
    });

    leftPressTimer_ = new QTimer(this);
    leftPressTimer_->setSingleShot(true);
    leftPressTimer_->setInterval(kLeftPressLongMs);
    connect(leftPressTimer_, &QTimer::timeout, this, [this] {
        dragging_ = true;
    });
}

void ReplicaWindow::mousePressEvent(QMouseEvent* event)
{
    if (frameCapturePending_) { event->accept(); return; }
    if (capturingRegion_) {
        if (event->button() == Qt::LeftButton) {
            captureOrigin_ = event->position().toPoint();
            captureCurrent_ = captureOrigin_;
            selectedRegion_.reset();
            update();
            event->accept();
        }
        return;
    }

    if (event->button() == Qt::MiddleButton) {
        middleLongPressHandled_ = false;
        middlePressTimer_->start();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        dragging_ = false;
        dragOrigin_ = event->globalPosition().toPoint() - frameGeometry().topLeft();
        leftPressTimer_->start();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void ReplicaWindow::mouseMoveEvent(QMouseEvent* event)
{
    if (frameCapturePending_) { event->accept(); return; }
    if (capturingRegion_) {
        if (event->buttons().testFlag(Qt::LeftButton)) {
            captureCurrent_ = event->position().toPoint();
            update();
            event->accept();
        }
        return;
    }

    if (dragging_ && event->buttons().testFlag(Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - dragOrigin_);
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void ReplicaWindow::mouseReleaseEvent(QMouseEvent* event)
{
    if (frameCapturePending_) { event->accept(); return; }
    if (capturingRegion_) {
        if (event->button() == Qt::LeftButton) {
            captureCurrent_ = event->position().toPoint();
            selectedRegion_ = QRect(captureOrigin_, captureCurrent_).normalized();
            update();
        }
        event->accept();
        return;
    }

    if (event->button() == Qt::MiddleButton) {
        if (middlePressTimer_->isActive()) {
            middlePressTimer_->stop();
            toggleActiveColorLogicSlot();
        }
        middleLongPressHandled_ = false;
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        const bool shortPress = leftPressTimer_->isActive();
        leftPressTimer_->stop();
        dragging_ = false;
        if (shortPress) {
            handleSourceClick();
        }
        event->accept();
        return;
    }

    dragging_ = false;
    QWidget::mouseReleaseEvent(event);
}

bool ReplicaWindow::nativeEvent(
    const QByteArray& eventType,
    void* message,
    qintptr* result)
{
#ifdef Q_OS_WIN
    const auto* nativeMessage = static_cast<MSG*>(message);
    const UINT mouseMessage = nativeMessage != nullptr
        ? HIWORD(nativeMessage->lParam)
        : 0;
    if (!capturingRegion_
        && nativeMessage != nullptr
        && nativeMessage->message == WM_MOUSEACTIVATE
        && (mouseMessage == WM_LBUTTONDOWN || mouseMessage == WM_MBUTTONDOWN)) {
        // A top-level window is normally raised by Windows before Qt receives
        // a left or middle click. Suppress that implicit activation so the
        // explicit raise() for an enabled left click is the only
        // replica-ordering operation; middle-button color-logic controls never
        // change the replica stacking order.
        *result = MA_NOACTIVATE;
        return true;
    }
#endif
    return QWidget::nativeEvent(eventType, message, result);
}

void ReplicaWindow::wheelEvent(QWheelEvent* event)
{
    if (capturingRegion_ || frameCapturePending_ || wheelZoomLocked_) {
        event->accept();
        return;
    }

    const auto delta = event->angleDelta().y();
    if (delta == 0) {
        QWidget::wheelEvent(event);
        return;
    }

    const auto factor = delta > 0 ? kZoomStep : 1.0 / kZoomStep;
    const auto current = size();
    const auto width = std::max(minimumWidth(), qRound(current.width() * factor));
    const auto height = std::max(minimumHeight(), qRound(current.height() * factor));
    resize(width, height);
    event->accept();
}

void ReplicaWindow::keyPressEvent(QKeyEvent* event)
{
    if (frameCapturePending_ && event->key() == Qt::Key_Escape) {
        cancelRegionCapture();
        event->accept();
        return;
    }
    if (capturingRegion_) {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            finishRegionCapture(selectedRegion_.has_value());
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Escape) {
            finishRegionCapture(false);
            event->accept();
            return;
        }
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

bool ReplicaWindow::raiseOnClickEnabled() const noexcept
{
    return raiseOnClickEnabled_;
}

void ReplicaWindow::setRaiseOnClickEnabled(bool enabled)
{
    raiseOnClickEnabled_ = enabled;
}
