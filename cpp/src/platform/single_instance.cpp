#include "platform/single_instance.h"

#include <QCryptographicHash>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QDir>
#include <QThread>

#include <utility>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace {

QString mutexName(const QString& key)
{
    return QStringLiteral("Local\\%1.Mutex").arg(key);
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

    server_ = new QLocalServer;
    server_->setSocketOptions(QLocalServer::UserAccessOption);
    QLocalServer::removeServer(key_);
    if (!server_->listen(key_)) {
        error_ = server_->errorString();
        return;
    }
    QObject::connect(server_, &QLocalServer::newConnection, server_, [this] {
        acceptConnections();
    });
    state_ = State::Primary;
}

SingleInstance::~SingleInstance()
{
    delete server_;
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
    for (int attempt = 0; attempt < 20; ++attempt) {
        QLocalSocket socket;
        socket.connectToServer(key_, QIODevice::WriteOnly);
        if (socket.waitForConnected(100)) {
            socket.write("activate", 8);
            socket.flush();
            static_cast<void>(socket.waitForBytesWritten(200));
            socket.disconnectFromServer();
            return true;
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

void SingleInstance::acceptConnections()
{
    while (server_ != nullptr && server_->hasPendingConnections()) {
        auto* socket = server_->nextPendingConnection();
        socket->readAll();
        socket->disconnectFromServer();
        socket->deleteLater();
        if (activationHandler_) {
            activationHandler_();
        } else {
            activationPending_ = true;
        }
    }
}
