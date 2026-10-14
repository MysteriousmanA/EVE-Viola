#include "ui/audio_feedback_manager.h"

#include <algorithm>
#include <utility>

#include <QAudioDevice>
#include <QAudioOutput>
#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QMediaDevices>
#include <QMediaPlayer>
#include <QTimer>
#include <QUrl>

#include "runtime/runtime_logger.h"

namespace {

class QtAudioChannelBackend final : public AudioChannelBackend {
public:
    explicit QtAudioChannelBackend(QObject* parent)
        : AudioChannelBackend(parent)
    {
    }

    bool playFile(quint64 attemptId, const QString& path) override
    {
        const QFileInfo file(path);
        if (path.trimmed().isEmpty() || !file.isFile() || !file.isReadable()) {
            return false;
        }

        stopPlayback();

        const auto outputDevice = QMediaDevices::defaultAudioOutput();
        if (outputDevice.isNull()) {
            QTimer::singleShot(0, this, [this, attemptId] {
                emit playbackFailed(attemptId, QStringLiteral("no_audio_output"));
            });
            return true;
        }

        audioOutput_ = new QAudioOutput(outputDevice, this);
        player_ = new QMediaPlayer(this);
        player_->setAudioOutput(audioOutput_);
        connect(
            player_,
            &QMediaPlayer::playbackStateChanged,
            this,
            [this, attemptId](QMediaPlayer::PlaybackState state) {
                if (state == QMediaPlayer::PlayingState) {
                    emit playbackStarted(attemptId);
                }
            });
        connect(
            player_,
            &QMediaPlayer::mediaStatusChanged,
            this,
            [this, attemptId](QMediaPlayer::MediaStatus status) {
                if (status == QMediaPlayer::EndOfMedia) {
                    emit playbackFinished(attemptId);
                } else if (status == QMediaPlayer::InvalidMedia) {
                    emit playbackFailed(attemptId, QStringLiteral("invalid_media"));
                }
            });
        connect(
            player_,
            &QMediaPlayer::errorOccurred,
            this,
            [this, attemptId](QMediaPlayer::Error error, const QString& errorText) {
                emit playbackFailed(
                    attemptId,
                    QStringLiteral("player_error_%1:%2")
                        .arg(static_cast<int>(error))
                        .arg(errorText));
            });

        player_->setSource(QUrl::fromLocalFile(file.absoluteFilePath()));
        player_->play();
        return true;
    }

    void stopPlayback() override
    {
        if (player_ != nullptr) {
            auto* player = std::exchange(player_, nullptr);
            player->stop();
            player->deleteLater();
        }
        if (audioOutput_ != nullptr) {
            auto* output = std::exchange(audioOutput_, nullptr);
            output->deleteLater();
        }
    }

private:
    QAudioOutput* audioOutput_{};
    QMediaPlayer* player_{};
};

} // namespace

AudioFeedbackManager::AudioFeedbackManager(QObject* parent)
    : QObject(parent)
{
    initialize([](QObject* channelParent) {
        return new QtAudioChannelBackend(channelParent);
    }, RecoveryPolicy{});
}

AudioFeedbackManager::AudioFeedbackManager(ChannelFactory factory, QObject* parent)
    : QObject(parent)
{
    initialize(std::move(factory), RecoveryPolicy{});
}

AudioFeedbackManager::AudioFeedbackManager(
    ChannelFactory factory,
    RecoveryPolicy recoveryPolicy,
    QObject* parent)
    : QObject(parent)
{
    initialize(std::move(factory), recoveryPolicy);
}

void AudioFeedbackManager::initialize(
    ChannelFactory factory,
    RecoveryPolicy recoveryPolicy)
{
    setObjectName(QStringLiteral("audioFeedbackManager"));
    recoveryPolicy.startupTimeoutMs = std::max(1, recoveryPolicy.startupTimeoutMs);
    recoveryPolicy.retryDelayMs = std::max(0, recoveryPolicy.retryDelayMs);
    recoveryPolicy.maxAttempts = std::max(1, recoveryPolicy.maxAttempts);
    recoveryPolicy_ = recoveryPolicy;

    for (qsizetype index = 0; index < static_cast<qsizetype>(channels_.size()); ++index) {
        auto* backend = factory(this);
        auto& channel = channels_[index];
        channel.backend = backend;
        channel.startupTimer = new QTimer(this);
        channel.startupTimer->setSingleShot(true);
        channel.startupTimer->setInterval(recoveryPolicy_.startupTimeoutMs);
        connect(channel.startupTimer, &QTimer::timeout, this, [this, index] {
            const auto attemptId = channels_[index].attemptId;
            failAttempt(index, attemptId, QStringLiteral("startup_timeout"));
        });
        channel.retryTimer = new QTimer(this);
        channel.retryTimer->setSingleShot(true);
        connect(channel.retryTimer, &QTimer::timeout, this, [this, index] {
            auto& current = channels_[index];
            if (current.key.isEmpty() || !current.retryPending) {
                return;
            }
            current.retryPending = false;
            if (!startAttempt(index)) {
                finishChannel(index, current.attemptId, false);
            }
        });
        connect(
            backend,
            &AudioChannelBackend::playbackStarted,
            this,
            [this, index](quint64 attemptId) {
                markChannelStarted(index, attemptId);
            },
            Qt::QueuedConnection);
        connect(
            backend,
            &AudioChannelBackend::playbackFinished,
            this,
            [this, index](quint64 attemptId) {
                finishChannel(index, attemptId, true);
            },
            Qt::QueuedConnection);
        connect(
            backend,
            &AudioChannelBackend::playbackFailed,
            this,
            [this, index](quint64 attemptId, const QString& reason) {
                failAttempt(index, attemptId, reason);
            },
            Qt::QueuedConnection);
    }
}

bool AudioFeedbackManager::startAttempt(qsizetype index)
{
    auto& channel = channels_[index];
    channel.attemptId = ++nextAttemptId_;
    ++channel.attempts;
    channel.started = false;
    channel.retryPending = false;

    RuntimeLogger::writeEvent(
        QStringLiteral("audio_playback_attempt"),
        QJsonObject{
            {QStringLiteral("channel"), static_cast<int>(index)},
            {QStringLiteral("attempt"), channel.attempts},
            {QStringLiteral("max_attempts"), recoveryPolicy_.maxAttempts},
            {QStringLiteral("file"), QFileInfo(channel.path).fileName()},
        });

    if (!channel.backend->playFile(channel.attemptId, channel.path)) {
        RuntimeLogger::writeEvent(
            QStringLiteral("audio_playback_failed"),
            QJsonObject{
                {QStringLiteral("channel"), static_cast<int>(index)},
                {QStringLiteral("attempt"), channel.attempts},
                {QStringLiteral("reason"), QStringLiteral("backend_rejected")},
            });
        return false;
    }
    channel.startupTimer->start();
    return true;
}

AudioRequestDisposition AudioFeedbackManager::request(
    ReplicaId replicaId,
    quint64 token,
    const QString& audioPath)
{
    if (replicaId == 0 || token == 0 || audioPath.trimmed().isEmpty()) {
        return AudioRequestDisposition::FailedImmediately;
    }

    const QString requestedPath = audioPath.trimmed();
    const QString key = normalizedPathKey(requestedPath);
    const Waiter waiter{replicaId, token};
    for (auto& channel : channels_) {
        if (!channel.key.isEmpty() && channel.key == key) {
            if (!channel.waiters.contains(waiter)) {
                channel.waiters.append(waiter);
            }
            return AudioRequestDisposition::Joined;
        }
    }

    for (qsizetype index = 0; index < static_cast<qsizetype>(channels_.size()); ++index) {
        auto& channel = channels_[index];
        if (!channel.key.isEmpty()) {
            continue;
        }
        channel.key = key;
        channel.path = requestedPath;
        channel.waiters = {waiter};
        channel.attempts = 0;
        if (!startAttempt(index)) {
            resetChannelState(channel);
            return AudioRequestDisposition::FailedImmediately;
        }
        return AudioRequestDisposition::Joined;
    }
    return AudioRequestDisposition::IgnoredBusy;
}

void AudioFeedbackManager::cancel(ReplicaId replicaId, quint64 token)
{
    for (qsizetype index = 0; index < static_cast<qsizetype>(channels_.size()); ++index) {
        auto& channel = channels_[index];
        const qsizetype removed = channel.waiters.removeAll(Waiter{replicaId, token});
        if (removed > 0 && channel.waiters.isEmpty()) {
            clearAndStop(index);
        }
    }
}

void AudioFeedbackManager::cancelReplica(ReplicaId replicaId)
{
    for (qsizetype index = 0; index < static_cast<qsizetype>(channels_.size()); ++index) {
        auto& channel = channels_[index];
        channel.waiters.removeIf([replicaId](const Waiter& waiter) {
            return waiter.replicaId == replicaId;
        });
        if (!channel.key.isEmpty() && channel.waiters.isEmpty()) {
            clearAndStop(index);
        }
    }
}

void AudioFeedbackManager::markChannelStarted(qsizetype index, quint64 attemptId)
{
    auto& channel = channels_[index];
    if (!channel.key.isEmpty() && channel.attemptId == attemptId) {
        channel.started = true;
        channel.startupTimer->stop();
        RuntimeLogger::writeEvent(
            QStringLiteral("audio_playback_started"),
            QJsonObject{
                {QStringLiteral("channel"), static_cast<int>(index)},
                {QStringLiteral("attempt"), channel.attempts},
            });
    }
}

void AudioFeedbackManager::failAttempt(
    qsizetype index,
    quint64 attemptId,
    const QString& reason)
{
    auto& channel = channels_[index];
    if (channel.key.isEmpty()
        || channel.attemptId != attemptId
        || channel.retryPending) {
        return;
    }

    channel.startupTimer->stop();
    if (!channel.started && channel.attempts < recoveryPolicy_.maxAttempts) {
        channel.backend->stopPlayback();
        // Invalidate every signal still queued by the failed player before the
        // retry timer creates a replacement player.
        channel.attemptId = 0;
        channel.retryPending = true;
        const int delay = recoveryPolicy_.retryDelayMs * channel.attempts;
        RuntimeLogger::writeEvent(
            QStringLiteral("audio_playback_retry"),
            QJsonObject{
                {QStringLiteral("channel"), static_cast<int>(index)},
                {QStringLiteral("failed_attempt"), channel.attempts},
                {QStringLiteral("next_attempt"), channel.attempts + 1},
                {QStringLiteral("delay_ms"), delay},
                {QStringLiteral("reason"), reason.left(500)},
            });
        channel.retryTimer->start(delay);
        return;
    }

    RuntimeLogger::writeEvent(
        QStringLiteral("audio_playback_failed"),
        QJsonObject{
            {QStringLiteral("channel"), static_cast<int>(index)},
            {QStringLiteral("attempts"), channel.attempts},
            {QStringLiteral("started"), channel.started},
            {QStringLiteral("reason"), reason.left(500)},
        });
    finishChannel(index, attemptId, false);
}

void AudioFeedbackManager::finishChannel(qsizetype index, quint64 attemptId, bool succeeded)
{
    auto& channel = channels_[index];
    if (channel.key.isEmpty() || channel.attemptId != attemptId) {
        return;
    }
    const auto waiters = std::exchange(channel.waiters, {});
    const bool completed = succeeded;
    channel.startupTimer->stop();
    channel.retryTimer->stop();
    if (completed) {
        RuntimeLogger::writeEvent(
            QStringLiteral("audio_playback_completed"),
            QJsonObject{
                {QStringLiteral("channel"), static_cast<int>(index)},
                {QStringLiteral("attempts"), channel.attempts},
            });
    }
    resetChannelState(channel);
    channel.backend->stopPlayback();
    for (const auto& waiter : waiters) {
        if (completed) {
            emit feedbackCompleted(waiter.replicaId, waiter.token);
        } else {
            emit feedbackFailed(waiter.replicaId, waiter.token);
        }
    }
}

void AudioFeedbackManager::clearAndStop(qsizetype index)
{
    auto& channel = channels_[index];
    if (channel.key.isEmpty()) {
        return;
    }
    channel.startupTimer->stop();
    channel.retryTimer->stop();
    resetChannelState(channel);
    channel.backend->stopPlayback();
}

void AudioFeedbackManager::resetChannelState(Channel& channel)
{
    channel.key.clear();
    channel.path.clear();
    channel.waiters.clear();
    channel.attemptId = 0;
    channel.attempts = 0;
    channel.started = false;
    channel.retryPending = false;
}

QString AudioFeedbackManager::normalizedPathKey(const QString& path)
{
    const QFileInfo information(path);
    QString normalized = information.canonicalFilePath();
    if (normalized.isEmpty()) {
        normalized = information.absoluteFilePath();
    }
    return QDir::cleanPath(QDir::fromNativeSeparators(normalized)).toCaseFolded();
}
