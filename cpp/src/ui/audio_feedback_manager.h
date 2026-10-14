#pragma once

#include <array>
#include <functional>

#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>

#include "features/replica/replica_manager.h"

class AudioChannelBackend : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;
    ~AudioChannelBackend() override = default;

    virtual bool playFile(quint64 attemptId, const QString& path) = 0;
    virtual void stopPlayback() = 0;

signals:
    void playbackStarted(quint64 attemptId);
    void playbackFinished(quint64 attemptId);
    void playbackFailed(quint64 attemptId, const QString& reason);
};

enum class AudioRequestDisposition {
    Joined,
    IgnoredBusy,
    FailedImmediately,
};

class AudioFeedbackManager final : public QObject {
    Q_OBJECT

public:
    using ChannelFactory = std::function<AudioChannelBackend*(QObject*)>;

    struct RecoveryPolicy {
        int startupTimeoutMs{1500};
        int retryDelayMs{400};
        int maxAttempts{4};
    };

    explicit AudioFeedbackManager(QObject* parent = nullptr);
    AudioFeedbackManager(ChannelFactory factory, QObject* parent = nullptr);
    AudioFeedbackManager(
        ChannelFactory factory,
        RecoveryPolicy recoveryPolicy,
        QObject* parent = nullptr);

    [[nodiscard]] AudioRequestDisposition request(
        ReplicaId replicaId,
        quint64 token,
        const QString& audioPath);
    void cancel(ReplicaId replicaId, quint64 token);
    void cancelReplica(ReplicaId replicaId);

signals:
    void feedbackCompleted(ReplicaId replicaId, quint64 token);
    void feedbackFailed(ReplicaId replicaId, quint64 token);

private:
    struct Waiter {
        ReplicaId replicaId{};
        quint64 token{};

        friend bool operator==(const Waiter&, const Waiter&) = default;
    };

    struct Channel {
        AudioChannelBackend* backend{};
        QTimer* startupTimer{};
        QTimer* retryTimer{};
        QString key;
        QString path;
        QList<Waiter> waiters;
        quint64 attemptId{};
        int attempts{};
        bool started{};
        bool retryPending{};
    };

    void initialize(ChannelFactory factory, RecoveryPolicy recoveryPolicy);
    [[nodiscard]] bool startAttempt(qsizetype index);
    void markChannelStarted(qsizetype index, quint64 attemptId);
    void failAttempt(qsizetype index, quint64 attemptId, const QString& reason);
    void finishChannel(qsizetype index, quint64 attemptId, bool succeeded);
    void clearAndStop(qsizetype index);
    void resetChannelState(Channel& channel);
    [[nodiscard]] static QString normalizedPathKey(const QString& path);

    std::array<Channel, 2> channels_;
    RecoveryPolicy recoveryPolicy_;
    quint64 nextAttemptId_{};
};
