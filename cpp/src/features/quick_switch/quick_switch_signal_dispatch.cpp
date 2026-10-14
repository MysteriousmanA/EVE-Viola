#include "features/quick_switch/quick_switch_service_impl.h"

#include <utility>
#include <QMetaObject>
#include <QPointer>

#ifdef Q_OS_WIN

void QuickSwitchService::Impl::postError(QString message)
{
    const QPointer<QuickSwitchService> target(owner);
    QMetaObject::invokeMethod(
        owner,
        [target, message = std::move(message)] {
            if (target) {
                emit target->hookError(message);
            }
        },
        Qt::QueuedConnection);
}

void QuickSwitchService::Impl::postSwitch(bool next, HWND foreground)
{
    const quintptr hwnd = reinterpret_cast<quintptr>(foreground);
    const QPointer<QuickSwitchService> target(owner);
    QMetaObject::invokeMethod(
        owner,
        [target, next, hwnd] {
            if (target) {
                emit target->switchRequested(next, hwnd);
            }
        },
        Qt::QueuedConnection);
}

void QuickSwitchService::Impl::postRadialHoldStarted(quint64 sessionId, HWND foreground)
{
    const quintptr hwnd = reinterpret_cast<quintptr>(foreground);
    const QPointer<QuickSwitchService> target(owner);
    QMetaObject::invokeMethod(
        owner,
        [target, sessionId, hwnd] {
            if (target) {
                emit target->radialSwitchHoldStarted(sessionId, hwnd);
            }
        },
        Qt::QueuedConnection);
}

void QuickSwitchService::Impl::postRadialHoldEnded(
    quint64 sessionId,
    bool heldLongEnough,
    bool foregroundStillMatches,
    bool cancelled,
    HWND foreground)
{
    const quintptr hwnd = reinterpret_cast<quintptr>(foreground);
    const QPointer<QuickSwitchService> target(owner);
    QMetaObject::invokeMethod(
        owner,
        [target, sessionId, heldLongEnough, foregroundStillMatches, cancelled, hwnd] {
            if (target) {
                emit target->radialSwitchHoldEnded(
                    sessionId,
                    heldLongEnough,
                    foregroundStillMatches,
                    cancelled,
                    hwnd);
            }
        },
        Qt::QueuedConnection);
}

void QuickSwitchService::Impl::postInputCaptured(QString key)
{
    const QPointer<QuickSwitchService> target(owner);
    QMetaObject::invokeMethod(
        owner,
        [target, key = std::move(key)] {
            if (target) {
                emit target->inputCaptured(key);
            }
        },
        Qt::QueuedConnection);
}

#endif
