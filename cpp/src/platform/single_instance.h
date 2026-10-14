#pragma once

#include <functional>

#include <QString>

class QLocalServer;

[[nodiscard]] QString singleInstanceKeyForApplication(
    const QString& applicationFilePath);

class SingleInstance final {
public:
    enum class State {
        Primary,
        Secondary,
        Error,
    };

    explicit SingleInstance(QString key);
    ~SingleInstance();

    SingleInstance(const SingleInstance&) = delete;
    SingleInstance& operator=(const SingleInstance&) = delete;

    [[nodiscard]] State state() const noexcept;
    [[nodiscard]] QString errorString() const;
    [[nodiscard]] bool notifyPrimary();
    void setActivationHandler(std::function<void()> handler);

private:
    void acceptConnections();

    QString key_;
    QString error_;
    QLocalServer* server_{};
    void* mutexHandle_{};
    State state_{State::Error};
    bool activationPending_{false};
    std::function<void()> activationHandler_;
};
