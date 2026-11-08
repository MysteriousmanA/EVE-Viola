#include "platform/single_instance.h"

#include <QCryptographicHash>
#include <QFileInfo>
#include <QDir>
#include <QThread>
#include <QWinEventNotifier>

#include <utility>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace {

QString mutexName(const QString& key)
{
    return QStringLiteral("Local\\%1.Mutex").arg(key);
}

QString activationEventName(const QString& key)
{
    return QStringLiteral("Local\\%1.Activate").arg(key);
}

} // namespace

QString singleInstanceKeyForApplication(const QString& applicationFilePath)
{
    const QString normalizedPath = QDir::fromNativeSeparators(
        QFileInfo(applicationFilePath).absoluteFilePath()).toCaseFolded();
    const QByteArray digest = QCryptographicHash::hash(
        normalizedPath.toUtf8(), QCryptographicHash::Sha256);
    return QStringLiteral("EVE-Viola.SingleInstance.v2.%1")
        .arg(QString::fromLatin1(digest.toHex()));
}

SingleInstance::SingleInstance(QString key)
    : key_(std::move(key))
{
    const QString name = mutexName(key_);
    const HANDLE mutex = CreateMutexW(nullptr, FALSE, name.toStdWString().c_str());
    const DWORD createError = GetLastError();
    mutexHandle_ = mutex;
    if (mutex == nullptr) {
        error_ = QStringLiteral("无法创建单实例锁，错误代码 %1").arg(createError);
        return;
    }
    if (createError == ERROR_ALREADY_EXISTS) {
        state_ = State::Secondary;
        return;
    }

    // A single auto-reset event is enough to wake the already-running window.
    // QLocalServer can fail with Access Denied while creating its pipe backlog.
    const QString eventName = activationEventName(key_);
    const HANDLE activationEvent = CreateEventW(
        nullptr, FALSE, FALSE, eventName.toStdWString().c_str());
    if (activationEvent == nullptr) {
        error_ = QStringLiteral("无法创建窗口唤醒事件，错误代码 %1")
                     .arg(GetLastError());
        return;
    }
    activationEventHandle_ = activationEvent;
    activationNotifier_ = new QWinEventNotifier(activationEvent);
    QObject::connect(activationNotifier_, &QWinEventNotifier::activated,
                     activationNotifier_, [this] {
        handleActivation();
    });
    state_ = State::Primary;
}

SingleInstance::~SingleInstance()
{
    delete activationNotifier_;
    if (activationEventHandle_ != nullptr) {
        CloseHandle(static_cast<HANDLE>(activationEventHandle_));
    }
    if (mutexHandle_ != nullptr) {
        CloseHandle(static_cast<HANDLE>(mutexHandle_));
    }
}

SingleInstance::State SingleInstance::state() const noexcept
{
    return state_;
}

QString SingleInstance::errorString() const
{
    return error_;
}

bool SingleInstance::notifyPrimary()
{
    if (state_ != State::Secondary) {
        return false;
    }
    static_cast<void>(AllowSetForegroundWindow(ASFW_ANY));
    const QString eventName = activationEventName(key_);
    for (int attempt = 0; attempt < 20; ++attempt) {
        const HANDLE activationEvent = OpenEventW(
            EVENT_MODIFY_STATE, FALSE, eventName.toStdWString().c_str());
        if (activationEvent != nullptr) {
            const bool notified = SetEvent(activationEvent) != 0;
            CloseHandle(activationEvent);
            return notified;
        }
        QThread::msleep(50);
    }
    return false;
}

void SingleInstance::setActivationHandler(std::function<void()> handler)
{
    activationHandler_ = std::move(handler);
    if (activationPending_ && activationHandler_) {
        activationPending_ = false;
        activationHandler_();
    }
}

void SingleInstance::handleActivation()
{
    if (activationHandler_) {
        activationHandler_();
    } else {
        activationPending_ = true;
    }
}
