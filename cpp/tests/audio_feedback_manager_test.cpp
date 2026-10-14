#include <utility>

#include <QtTest>
#include <QCoreApplication>
#include <QTemporaryDir>

#include "ui/audio_feedback_manager.h"

namespace {

class FakeAudioChannel final : public AudioChannelBackend {
public:
    explicit FakeAudioChannel(QObject* parent = nullptr)
        : AudioChannelBackend(parent)
    {
    }

    bool playFile(quint64 attemptId, const QString& path) override
    {
        ++playCount;
        currentAttemptId = attemptId;
        lastPath = path;
        if (emitStarted) {
            emit playbackStarted(attemptId);
        }
        return acceptsPlayback;
    }

    void stopPlayback() override
    {
        ++stopCount;
    }

    void finish()
    {
        emit playbackFinished(currentAttemptId);
    }

    void finishAttempt(quint64 attemptId)
    {
        emit playbackFinished(attemptId);
    }

    void fail()
    {
        emit playbackFailed(currentAttemptId, QStringLiteral("test_failure"));
    }

    int playCount{};
    int stopCount{};
    bool acceptsPlayback{true};
    bool emitStarted{true};
    QString lastPath;
    quint64 currentAttemptId{};
};

} // namespace

class AudioFeedbackManagerTest final : public QObject {
    Q_OBJECT

private slots:
    void mergesSamePathAndAllowsTwoDifferentFiles()
    {
        QList<FakeAudioChannel*> channels;
        AudioFeedbackManager manager([&channels](QObject* parent) {
            auto* channel = new FakeAudioChannel(parent);
            channels.append(channel);
            return channel;
        });

        QCOMPARE(manager.request(1, 11, QStringLiteral("C:/alerts/a.wav")),
                 AudioRequestDisposition::Joined);
        QCOMPARE(manager.request(2, 22, QStringLiteral("c:/ALERTS/./a.wav")),
                 AudioRequestDisposition::Joined);
        QCOMPARE(channels[0]->playCount, 1);
        QCOMPARE(channels[0]->lastPath, QStringLiteral("C:/alerts/a.wav"));
        QCOMPARE(channels[1]->playCount, 0);

        QCOMPARE(manager.request(3, 33, QStringLiteral("C:/alerts/b.wav")),
                 AudioRequestDisposition::Joined);
        QCOMPARE(channels[1]->playCount, 1);
    }

    void ignoresThirdDifferentFileWhenBothChannelsAreBusy()
    {
        QList<FakeAudioChannel*> channels;
        AudioFeedbackManager manager([&channels](QObject* parent) {
            auto* channel = new FakeAudioChannel(parent);
            channels.append(channel);
            return channel;
        });

        QCOMPARE(manager.request(1, 1, QStringLiteral("a.wav")), AudioRequestDisposition::Joined);
        QCOMPARE(manager.request(2, 2, QStringLiteral("b.wav")), AudioRequestDisposition::Joined);
        QCOMPARE(manager.request(3, 3, QStringLiteral("c.wav")),
                 AudioRequestDisposition::IgnoredBusy);
        QCOMPARE(channels[0]->playCount + channels[1]->playCount, 2);
    }

    void completesAllSharedWaitersExactlyOnce()
    {
        QList<FakeAudioChannel*> channels;
        AudioFeedbackManager manager([&channels](QObject* parent) {
            auto* channel = new FakeAudioChannel(parent);
            channels.append(channel);
            return channel;
        });
        QSignalSpy completed(&manager, &AudioFeedbackManager::feedbackCompleted);

        QCOMPARE(manager.request(1, 10, QStringLiteral("shared.wav")),
                 AudioRequestDisposition::Joined);
        QCOMPARE(manager.request(2, 20, QStringLiteral("shared.wav")),
                 AudioRequestDisposition::Joined);
        channels[0]->finish();
        channels[0]->finish();
        QCoreApplication::processEvents();

        QCOMPARE(completed.count(), 2);
        QCOMPARE(completed[0][0].toULongLong(), qulonglong{1});
        QCOMPARE(completed[0][1].toULongLong(), qulonglong{10});
        QCOMPARE(completed[1][0].toULongLong(), qulonglong{2});
        QCOMPARE(completed[1][1].toULongLong(), qulonglong{20});
    }

    void cancellationStopsOnlyAfterTheLastWaiterLeaves()
    {
        QList<FakeAudioChannel*> channels;
        AudioFeedbackManager manager([&channels](QObject* parent) {
            auto* channel = new FakeAudioChannel(parent);
            channels.append(channel);
            return channel;
        });

        QCOMPARE(manager.request(1, 10, QStringLiteral("shared.wav")),
                 AudioRequestDisposition::Joined);
        QCOMPARE(manager.request(2, 20, QStringLiteral("shared.wav")),
                 AudioRequestDisposition::Joined);
        manager.cancel(1, 10);
        QCOMPARE(channels[0]->stopCount, 0);
        manager.cancel(2, 20);
        QCOMPARE(channels[0]->stopCount, 1);
    }

    void trimsPathBeforeStartingPlayback()
    {
        QList<FakeAudioChannel*> channels;
        AudioFeedbackManager manager([&channels](QObject* parent) {
            auto* channel = new FakeAudioChannel(parent);
            channels.append(channel);
            return channel;
        });

        QCOMPARE(manager.request(1, 1, QStringLiteral("  alert.wav  ")),
                 AudioRequestDisposition::Joined);
        QCOMPARE(channels[0]->lastPath, QStringLiteral("alert.wav"));
    }

    void missingFileFailsBeforeReservingAChannel()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("missing.wav"));
        AudioFeedbackManager manager;

        QCOMPARE(manager.request(1, 1, path), AudioRequestDisposition::FailedImmediately);
        QCOMPARE(manager.request(2, 2, path), AudioRequestDisposition::FailedImmediately);
    }

    void failedPlaybackReleasesTheChannel()
    {
        QList<FakeAudioChannel*> channels;
        AudioFeedbackManager manager([&channels](QObject* parent) {
            auto* channel = new FakeAudioChannel(parent);
            channels.append(channel);
            return channel;
        });
        QSignalSpy completed(&manager, &AudioFeedbackManager::feedbackCompleted);
        QSignalSpy failed(&manager, &AudioFeedbackManager::feedbackFailed);

        QCOMPARE(manager.request(1, 10, QStringLiteral("bad.wav")),
                 AudioRequestDisposition::Joined);
        channels[0]->fail();
        QCoreApplication::processEvents();
        QCOMPARE(completed.count(), 0);
        QCOMPARE(failed.count(), 1);
        QCOMPARE(manager.request(2, 20, QStringLiteral("next.wav")),
                 AudioRequestDisposition::Joined);
        QCOMPARE(channels[0]->playCount, 2);
    }

    void retriesFailureBeforePlaybackStartsAndCompletesOnce()
    {
        QList<FakeAudioChannel*> channels;
        AudioFeedbackManager manager(
            [&channels](QObject* parent) {
                auto* channel = new FakeAudioChannel(parent);
                channels.append(channel);
                return channel;
            },
            AudioFeedbackManager::RecoveryPolicy{
                .startupTimeoutMs = 1000,
                .retryDelayMs = 0,
                .maxAttempts = 3,
            });
        QSignalSpy completed(&manager, &AudioFeedbackManager::feedbackCompleted);
        QSignalSpy failed(&manager, &AudioFeedbackManager::feedbackFailed);

        channels[0]->emitStarted = false;
        QCOMPARE(manager.request(1, 10, QStringLiteral("first-start.wav")),
                 AudioRequestDisposition::Joined);
        const quint64 failedAttemptId = channels[0]->currentAttemptId;
        channels[0]->fail();
        channels[0]->emitStarted = true;

        QTRY_COMPARE(channels[0]->playCount, 2);
        channels[0]->finishAttempt(failedAttemptId);
        QCoreApplication::processEvents();
        QCOMPARE(completed.count(), 0);
        channels[0]->finish();
        QTRY_COMPARE(completed.count(), 1);
        QCOMPARE(failed.count(), 0);
    }

    void startupWatchdogRetriesThenReleasesTheChannel()
    {
        QList<FakeAudioChannel*> channels;
        AudioFeedbackManager manager(
            [&channels](QObject* parent) {
                auto* channel = new FakeAudioChannel(parent);
                channels.append(channel);
                return channel;
            },
            AudioFeedbackManager::RecoveryPolicy{
                .startupTimeoutMs = 10,
                .retryDelayMs = 0,
                .maxAttempts = 2,
            });
        QSignalSpy failed(&manager, &AudioFeedbackManager::feedbackFailed);

        channels[0]->emitStarted = false;
        QCOMPARE(manager.request(1, 10, QStringLiteral("stuck.wav")),
                 AudioRequestDisposition::Joined);

        QTRY_COMPARE(channels[0]->playCount, 2);
        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(manager.request(2, 20, QStringLiteral("next.wav")),
                 AudioRequestDisposition::Joined);
        QCOMPARE(channels[0]->playCount, 3);
    }

    void completedPlaybackDoesNotRequireAQueuedStartedSignal()
    {
        QList<FakeAudioChannel*> channels;
        AudioFeedbackManager manager([&channels](QObject* parent) {
            auto* channel = new FakeAudioChannel(parent);
            channels.append(channel);
            return channel;
        });
        QSignalSpy completed(&manager, &AudioFeedbackManager::feedbackCompleted);
        QSignalSpy failed(&manager, &AudioFeedbackManager::feedbackFailed);

        channels[0]->emitStarted = false;
        QCOMPARE(manager.request(1, 10, QStringLiteral("fast.wav")),
                 AudioRequestDisposition::Joined);
        channels[0]->finish();
        QCoreApplication::processEvents();

        QCOMPARE(completed.count(), 1);
        QCOMPARE(failed.count(), 0);
    }
};

QTEST_GUILESS_MAIN(AudioFeedbackManagerTest)

#include "audio_feedback_manager_test.moc"
