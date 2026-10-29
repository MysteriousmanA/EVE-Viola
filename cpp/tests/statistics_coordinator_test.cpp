#include <atomic>
#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>
#include <thread>
#include <chrono>
#include <QScopeGuard>
#include <QTimer>

#include <QtTest>

#include "features/replica/statistics_coordinator.h"

namespace {

struct FrameGate {
    std::atomic<bool> entered{};
    std::atomic<bool> release{};
};
FrameGate* frameGate{};
std::int32_t gatedFrame(std::uintptr_t, std::int32_t* width,
    std::int32_t* height, std::uint8_t** pixels)
{
    frameGate->entered = true;
    while (!frameGate->release) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    *width = 2;
    *height = 2;
    *pixels = new std::uint8_t[16]{};
    return 0;
}
NativeFunctions frameFunctions()
{
    auto functions = NativeFunctions::linked();
    functions.captureWindow = gatedFrame;
    functions.captureWindowProfiled = nullptr;
    functions.freePixels = [](std::uint8_t* pixels) { delete[] pixels; };
    return functions;
}

class ManualExecutor final {
public:
    void submit(std::function<void()> task)
    {
        tasks_.push_back(std::move(task));
    }

    [[nodiscard]] int pendingCount() const noexcept
    {
        return static_cast<int>(tasks_.size());
    }

    void runFirst()
    {
        auto task = std::move(tasks_.front());
        tasks_.erase(tasks_.begin());
        task();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    }

private:
    std::vector<std::function<void()>> tasks_;
};

} // namespace

class StatisticsCoordinatorTest final : public QObject {
    Q_OBJECT

private slots:
    void preservesPerReplicaTargetsInPlannedBatches()
    {
        qint64 nowMs = 0;
        ManualExecutor executor;
        QList<PixelStatisticsRequest> captured;
        StatisticsCoordinator coordinator([&] { return nowMs; },
            PlannedStatisticsCapture{[&](std::uintptr_t, const QList<PixelStatisticsRequest>& requests) {
                captured = requests;
                return QList<std::optional<PixelStatistics>>(requests.size(), PixelStatistics{1, 1, 0});
            }}, [&](std::function<void()> task) { executor.submit(std::move(task)); });
        const QList<PixelColorTarget> first{{QStringLiteral("#123456"), 0}};
        const QList<PixelColorTarget> second{{QStringLiteral("#abcdef"), 25}};
        coordinator.upsert(1, 77, QRect(0, 0, 1, 1), 1500, first);
        coordinator.upsert(2, 77, QRect(1, 0, 1, 1), 1500, second);
        coordinator.processDue();
        QCOMPARE(executor.pendingCount(), 1);
        executor.runFirst();
        QCOMPARE(captured.size(), 2);
        for (const auto& request : captured) {
            QCOMPARE(request.targets, request.region->x() == 0 ? first : second);
        }
    }

    void deliversOnlyToTheMatchingLiveRecipient()
    {
        qint64 nowMs = 0;
        ManualExecutor executor;
        StatisticsCoordinator coordinator([&] { return nowMs; },
            [](std::uintptr_t, const QList<std::optional<QRect>>& regions) {
                return QList<std::optional<PixelStatistics>>(regions.size(), PixelStatistics{1, 1, 0});
            }, [&](std::function<void()> task) { executor.submit(std::move(task)); });
        QObject firstOwner;
        auto secondOwner = std::make_unique<QObject>();
        QObject unrelatedOwner;
        int first = 0, second = 0, unrelated = 0;
        coordinator.setResultHandler(1, &firstOwner, [&](const auto& result) {
            QVERIFY(result.has_value()); ++first;
        });
        coordinator.setResultHandler(2, secondOwner.get(), [&](const auto&) { ++second; });
        coordinator.setResultHandler(99, &unrelatedOwner, [&](const auto&) { ++unrelated; });
        coordinator.upsert(1, 77, std::nullopt, 1500);
        coordinator.upsert(2, 77, std::nullopt, 1500);
        coordinator.processDue();
        secondOwner.reset(); // Destruction while a batch is queued must cancel delivery.
        executor.runFirst();
        QCOMPARE(first, 1);
        QCOMPARE(second, 0);
        QCOMPARE(unrelated, 0);
        coordinator.remove(1);
        coordinator.upsert(1, 77, std::nullopt, 1500);
        nowMs = 1500;
        coordinator.processDue();
        executor.runFirst();
        QCOMPARE(first, 2); // Disabling/re-enabling registration retains the subscriber.
    }

    void replacingRecipientAndRemovingItDuringFailureAreSafe()
    {
        qint64 nowMs = 0;
        ManualExecutor executor;
        StatisticsCoordinator coordinator([&] { return nowMs; },
            [](std::uintptr_t, const QList<std::optional<QRect>>&) -> QList<std::optional<PixelStatistics>> {
                throw std::runtime_error("capture failed");
            }, [&](std::function<void()> task) { executor.submit(std::move(task)); });
        auto oldOwner = std::make_unique<QObject>();
        QObject newOwner;
        int oldCalls = 0, failures = 0;
        coordinator.setResultHandler(1, oldOwner.get(), [&](const auto&) { ++oldCalls; });
        coordinator.setResultHandler(1, &newOwner, [&](const auto& result) {
            QVERIFY(!result.has_value());
            ++failures;
            coordinator.setResultHandler(1, nullptr, {});
        });
        oldOwner.reset(); // Must not remove the replacement's handler.
        coordinator.upsert(1, 77, std::nullopt, 1500);
        coordinator.processDue();
        executor.runFirst();
        QCOMPARE(failures, 1);
        QCOMPARE(oldCalls, 0);
        nowMs = 1500;
        coordinator.processDue();
        executor.runFirst();
        QCOMPARE(failures, 1);
    }

    void mergesColorTargetsAndDiscardsResultsWhenOnlyToleranceChanges()
    {
        qint64 nowMs = 0;
        ManualExecutor executor;
        QList<PixelColorTarget> capturedTargets;
        StatisticsCoordinator coordinator(
            [&] { return nowMs; },
            [&](std::uintptr_t, const QList<std::optional<QRect>>& regions,
                const QList<PixelColorTarget>& targets) {
                capturedTargets = targets;
                QImage image(1, 1, QImage::Format_RGB32);
                image.fill(QColor(QStringLiteral("#00ff00")));
                return QList<std::optional<PixelStatistics>>(regions.size(),
                    calculatePixelStatistics(image, std::nullopt, targets));
            },
            [&](std::function<void()> task) { executor.submit(std::move(task)); });
        QSignalSpy ready(&coordinator, &StatisticsCoordinator::colorStatisticsReady);
        coordinator.upsert(1, 11, std::nullopt, 1500, {{QStringLiteral("#00ff00"), 10}});
        coordinator.upsert(2, 11, std::nullopt, 1500, {{QStringLiteral("#0000ff"), 20}});
        coordinator.processDue();
        coordinator.upsert(1, 11, std::nullopt, 1500, {{QStringLiteral("#00ff00"), 0}});
        executor.runFirst();
        QCOMPARE(ready.count(), 1);
        QCOMPARE(ready.first()[0].toULongLong(), qulonglong{2});
        QVERIFY(capturedTargets.contains({QStringLiteral("#00ff00"), 10}));
        QVERIFY(capturedTargets.contains({QStringLiteral("#0000ff"), 20}));
        nowMs = 1500;
        coordinator.processDue();
        executor.runFirst();
        QCOMPARE(ready.count(), 3); // Rephasing still respects the source cooldown.
        const auto result = qvariant_cast<PixelStatistics>(ready.last()[1]);
        QCOMPARE(result.count({QStringLiteral("#00ff00"), 0}), qsizetype{1});
    }

    void changingStatisticsRegistrationDoesNotCancelAnInteractiveFrame()
    {
        FrameGate gate;
        frameGate = &gate;
        NativeGateway gateway(frameFunctions());
        StatisticsCoordinator coordinator(gateway);
        const auto release = qScopeGuard([&gate] { gate.release = true; });
        QSignalSpy ready(&coordinator, &StatisticsCoordinator::frameReady);
        QVERIFY(coordinator.requestFrame(1, 101) != 0);
        QTRY_VERIFY(gate.entered.load());
        coordinator.remove(1);
        gate.release = true;
        QTRY_COMPARE(ready.count(), 1);
        QTRY_VERIFY(coordinator.isIdle());
    }
    void frameCaptureKeepsUiResponsiveAndRejectsCancelledResults()
    {
        FrameGate gate;
        frameGate = &gate;
        NativeGateway gateway(frameFunctions());
        StatisticsCoordinator coordinator(gateway);
        const auto release = qScopeGuard([&gate] { gate.release = true; });
        QSignalSpy ready(&coordinator, &StatisticsCoordinator::frameReady);
        const auto first = coordinator.requestFrame(1, 101);
        QVERIFY(first != 0);
        QTRY_VERIFY_WITH_TIMEOUT(gate.entered.load(), 1000);
        bool uiCallback = false;
        QTimer::singleShot(0, &coordinator, [&uiCallback] { uiCallback = true; });
        QCoreApplication::processEvents();
        QVERIFY(uiCallback);
        coordinator.cancelFrame(1);
        QCOMPARE(coordinator.requestFrame(1, 101), quint64{0});
        gate.release = true;
        QTRY_VERIFY(coordinator.isIdle());
        QCOMPARE(ready.count(), 0);
        const auto second = coordinator.requestFrame(1, 101);
        QVERIFY(second > first);
        QTRY_COMPARE(ready.count(), 1);
        QCOMPARE(ready.first()[1].toULongLong(), second);
        QCOMPARE(qvariant_cast<QImage>(ready.first()[2]).size(), QSize(2, 2));
    }
    void stopInvalidatesFramesWithoutBlockingAndSignalsIdleAfterDrain()
    {
        FrameGate gate;
        frameGate = &gate;
        NativeGateway gateway(frameFunctions());
        StatisticsCoordinator coordinator(gateway);
        const auto release = qScopeGuard([&gate] { gate.release = true; });
        QSignalSpy ready(&coordinator, &StatisticsCoordinator::frameReady);
        QSignalSpy idle(&coordinator, &StatisticsCoordinator::idle);
        QVERIFY(coordinator.requestFrame(1, 101) != 0);
        QVERIFY(coordinator.requestFrame(2, 102) != 0);
        QCOMPARE(coordinator.requestFrame(3, 103), quint64{0});
        QTRY_VERIFY(gate.entered.load());
        coordinator.stop();
        QVERIFY(!coordinator.isIdle());
        QCOMPARE(coordinator.requestFrame(3, 103), quint64{0});
        gate.release = true;
        QTRY_VERIFY(coordinator.isIdle());
        QCOMPARE(ready.count(), 0);
        QCOMPARE(idle.count(), 1);
    }
    void staggersThirtyOneAndAHalfSecondRegistrationsAcrossThePeriod()
    {
        qint64 nowMs = 0;
        ManualExecutor executor;
        QList<qint64> captureTimes;
        StatisticsCoordinator coordinator(
            [&nowMs] { return nowMs; },
            [&nowMs, &captureTimes](std::uintptr_t, const QList<std::optional<QRect>>& regions) {
                captureTimes.append(nowMs);
                return QList<std::optional<PixelStatistics>>(
                    regions.size(), PixelStatistics{1, 0, 0});
            },
            [&executor](std::function<void()> task) { executor.submit(std::move(task)); });

        for (ReplicaId id = 1; id <= 30; ++id) {
            coordinator.upsert(id, 1000 + id, std::nullopt, 1500);
        }

        for (nowMs = 0; nowMs < 1500; ++nowMs) {
            coordinator.processDue();
            while (executor.pendingCount() > 0) {
                executor.runFirst();
            }
        }

        QCOMPARE(captureTimes.size(), 30);
        QVERIFY(captureTimes.last() - captureTimes.first() >= 1350);
    }

    void mergesSameSourceRegionsIntoOneCapture()
    {
        qint64 nowMs = 0;
        ManualExecutor executor;
        int captureCalls = 0;
        int capturedRegions = 0;
        StatisticsCoordinator coordinator(
            [&nowMs] { return nowMs; },
            [&captureCalls, &capturedRegions](std::uintptr_t, const QList<std::optional<QRect>>& regions) {
                ++captureCalls;
                capturedRegions = regions.size();
                return QList<std::optional<PixelStatistics>>(
                    regions.size(), PixelStatistics{1, 1, 0});
            },
            [&executor](std::function<void()> task) { executor.submit(std::move(task)); });

        coordinator.upsert(1, 77, QRect(0, 0, 1, 1), 1500);
        coordinator.upsert(2, 77, QRect(1, 0, 1, 1), 1500);
        coordinator.processDue();
        QCOMPARE(executor.pendingCount(), 1);

        executor.runFirst();
        QCOMPARE(captureCalls, 1);
        QCOMPARE(capturedRegions, 2);
        QCOMPARE(coordinator.diagnostics().mergedCaptures, quint64{1});
    }

    void neverRunsMoreThanTwoCaptureBatchesAtOnce()
    {
        qint64 nowMs = 5000;
        ManualExecutor executor;
        StatisticsCoordinator coordinator(
            [&nowMs] { return nowMs; },
            [](std::uintptr_t, const QList<std::optional<QRect>>& regions) {
                return QList<std::optional<PixelStatistics>>(
                    regions.size(), PixelStatistics{1, 0, 0});
            },
            [&executor](std::function<void()> task) { executor.submit(std::move(task)); });

        coordinator.upsert(1, 11, std::nullopt, 1500);
        coordinator.upsert(2, 22, std::nullopt, 1500);
        coordinator.upsert(3, 33, std::nullopt, 1500);
        nowMs += 1500;
        coordinator.processDue();

        QCOMPARE(executor.pendingCount(), 2);
        QCOMPARE(coordinator.diagnostics().maximumActiveBatches, 2);
        executor.runFirst();
        coordinator.processDue();
        QCOMPARE(executor.pendingCount(), 2);
        while (executor.pendingCount() > 0) {
            executor.runFirst();
        }
    }

    void limitsMixedIntervalsForTheSameSourceToOneCapturePerOneAndAHalfSeconds()
    {
        qint64 nowMs = 0;
        ManualExecutor executor;
        QList<qint64> mixedSourceCaptures;
        QList<int> batchSizes;
        StatisticsCoordinator coordinator(
            [&nowMs] { return nowMs; },
            [&nowMs, &mixedSourceCaptures, &batchSizes](std::uintptr_t hwnd, const QList<std::optional<QRect>>& regions) {
                if (hwnd == 77) {
                    mixedSourceCaptures.append(nowMs);
                    batchSizes.append(static_cast<int>(regions.size()));
                }
                return QList<std::optional<PixelStatistics>>(regions.size(), PixelStatistics{1, 0, 0});
            },
            [&executor](std::function<void()> task) { executor.submit(std::move(task)); });
        coordinator.upsert(1, 11, std::nullopt, 1500);
        coordinator.upsert(2, 77, QRect(0, 0, 1, 1), 1500);
        coordinator.upsert(3, 77, QRect(1, 0, 1, 1), 2500);
        for (nowMs = 0; nowMs < 6000; nowMs += 250) {
            coordinator.processDue();
            while (executor.pendingCount() > 0) { executor.runFirst(); }
        }
        QCOMPARE(mixedSourceCaptures, QList<qint64>({750, 2250, 3750, 5250}));
        QCOMPARE(batchSizes.first(), 2);
        QCOMPARE(batchSizes.last(), 1);
        QCOMPARE(coordinator.diagnostics().mergedCaptures, quint64{2});
    }

    void discardsResultsFromAnOlderRegistrationVersion()
    {
        qint64 nowMs = 0;
        ManualExecutor executor;
        StatisticsCoordinator coordinator(
            [&nowMs] { return nowMs; },
            [](std::uintptr_t, const QList<std::optional<QRect>>& regions) {
                return QList<std::optional<PixelStatistics>>(
                    regions.size(), PixelStatistics{1, 1, 0});
            },
            [&executor](std::function<void()> task) { executor.submit(std::move(task)); });
        QSignalSpy ready(&coordinator, &StatisticsCoordinator::statisticsReady);
        QObject recipient;
        int deliveries = 0;
        coordinator.setResultHandler(1, &recipient, [&](const auto&) { ++deliveries; });

        coordinator.upsert(1, 11, QRect(0, 0, 1, 1), 1500);
        coordinator.processDue();
        QCOMPARE(executor.pendingCount(), 1);
        coordinator.upsert(1, 11, QRect(1, 0, 1, 1), 1500);

        executor.runFirst();
        QCOMPARE(ready.count(), 0);
        QCOMPARE(deliveries, 0);
        coordinator.processDue();
        QCOMPARE(executor.pendingCount(), 0);
        nowMs = 1500;
        coordinator.processDue();
        QCOMPARE(executor.pendingCount(), 1);
        executor.runFirst();
        QCOMPARE(ready.count(), 1);
        QCOMPARE(deliveries, 1);
    }

    void clampsFastRequestsAndSharesTheFrameWithoutSpeedingUpSlowRequests()
    {
        qint64 nowMs = 0;
        ManualExecutor executor;
        QList<qint64> captures;
        StatisticsCoordinator coordinator([&] { return nowMs; },
            [&](std::uintptr_t, const QList<std::optional<QRect>>& regions) {
                captures.append(nowMs);
                return QList<std::optional<PixelStatistics>>(regions.size(), PixelStatistics{});
            }, [&](std::function<void()> task) { executor.submit(std::move(task)); });
        QSignalSpy ready(&coordinator, &StatisticsCoordinator::statisticsReady);
        coordinator.upsert(1, 77, QRect(0, 0, 1, 1), 100);
        coordinator.upsert(2, 77, QRect(1, 0, 1, 1), 500);
        coordinator.upsert(3, 77, QRect(2, 0, 1, 1), 2500);
        QList<qint64> slowDeliveries;
        connect(&coordinator, &StatisticsCoordinator::statisticsReady, this,
            [&](ReplicaId id, qint64, qint64) { if (id == 3) { slowDeliveries.append(nowMs); } });
        for (nowMs = 0; nowMs < 6000; nowMs += 100) {
            coordinator.processDue();
            while (executor.pendingCount() > 0) { executor.runFirst(); }
        }
        QCOMPARE(captures, QList<qint64>({0, 1500, 3000, 4500}));
        QCOMPARE(slowDeliveries, QList<qint64>({0, 3000}));
        QCOMPARE(ready.count(), 10);
    }

    void editsAndRestartCannotBypassCooldownAfterASlowFailedCapture()
    {
        qint64 nowMs = 0;
        ManualExecutor executor;
        int attempts = 0;
        StatisticsCoordinator coordinator([&] { return nowMs; },
            [&](std::uintptr_t, const QList<std::optional<QRect>>&) -> QList<std::optional<PixelStatistics>> {
                ++attempts;
                throw std::runtime_error("capture failed");
            }, [&](std::function<void()> task) { executor.submit(std::move(task)); });
        coordinator.upsert(1, 77, std::nullopt, 500);
        coordinator.processDue();
        nowMs = 3000; // Simulate a queued or slow capture.
        executor.runFirst();
        coordinator.upsert(1, 77, QRect(0, 0, 1, 1), 500);
        coordinator.processDue();
        QCOMPARE(executor.pendingCount(), 0);
        coordinator.stop();
        coordinator.start();
        coordinator.upsert(2, 77, std::nullopt, 100);
        nowMs = 4499;
        coordinator.processDue();
        QCOMPARE(executor.pendingCount(), 0);
        nowMs = 4500;
        coordinator.processDue();
        QCOMPARE(executor.pendingCount(), 1);
        executor.runFirst();
        QCOMPARE(attempts, 2);
    }

    void skipsExpiredIntervalsWhileThePreviousBatchIsRunning()
    {
        qint64 nowMs = 0;
        ManualExecutor executor;
        StatisticsCoordinator coordinator(
            [&nowMs] { return nowMs; },
            [](std::uintptr_t, const QList<std::optional<QRect>>& regions) {
                return QList<std::optional<PixelStatistics>>(
                    regions.size(), PixelStatistics{1, 0, 0});
            },
            [&executor](std::function<void()> task) { executor.submit(std::move(task)); });

        coordinator.upsert(1, 11, std::nullopt, 1500);
        coordinator.processDue();
        QCOMPARE(executor.pendingCount(), 1);
        nowMs = 5000;
        coordinator.processDue();

        QCOMPARE(executor.pendingCount(), 1);
        QCOMPARE(coordinator.diagnostics().skippedIntervals, quint64{3});
        executor.runFirst();
    }

    void reportsFailureOnceAndRetriesOnTheNextNormalPeriod()
    {
        qint64 nowMs = 0;
        ManualExecutor executor;
        int attempts = 0;
        StatisticsCoordinator coordinator(
            [&nowMs] { return nowMs; },
            [&attempts](std::uintptr_t, const QList<std::optional<QRect>>& regions) {
                ++attempts;
                if (attempts == 1) {
                    throw std::runtime_error("capture failed");
                }
                return QList<std::optional<PixelStatistics>>(
                    regions.size(), PixelStatistics{1, 0, 1});
            },
            [&executor](std::function<void()> task) { executor.submit(std::move(task)); });
        QSignalSpy unavailable(&coordinator, &StatisticsCoordinator::statisticsUnavailable);
        QSignalSpy ready(&coordinator, &StatisticsCoordinator::statisticsReady);

        coordinator.upsert(1, 11, std::nullopt, 1500);
        coordinator.processDue();
        executor.runFirst();
        QCOMPARE(unavailable.count(), 1);
        QCOMPARE(coordinator.diagnostics().failedBatches, quint64{1});

        nowMs = 1500;
        coordinator.processDue();
        executor.runFirst();
        QCOMPARE(ready.count(), 1);
        QCOMPARE(attempts, 2);
    }
};

QTEST_GUILESS_MAIN(StatisticsCoordinatorTest)

#include "statistics_coordinator_test.moc"
