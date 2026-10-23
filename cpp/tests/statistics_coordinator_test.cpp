#include <atomic>
#include <functional>
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
        coordinator.upsert(1, 11, std::nullopt, 1000, {{QStringLiteral("#00ff00"), 10}});
        coordinator.upsert(2, 11, std::nullopt, 1000, {{QStringLiteral("#0000ff"), 20}});
        coordinator.processDue();
        coordinator.upsert(1, 11, std::nullopt, 1000, {{QStringLiteral("#00ff00"), 0}});
        executor.runFirst();
        QCOMPARE(ready.count(), 1);
        QCOMPARE(ready.first()[0].toULongLong(), qulonglong{2});
        QVERIFY(capturedTargets.contains({QStringLiteral("#00ff00"), 10}));
        QVERIFY(capturedTargets.contains({QStringLiteral("#0000ff"), 20}));
        coordinator.processDue();
        executor.runFirst();
        QCOMPARE(ready.count(), 3); // Updating registration rephases both replicas.
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
    void staggersThirtyOneSecondRegistrationsAcrossThePeriod()
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
            coordinator.upsert(id, 1000 + id, std::nullopt, 1000);
        }

        for (nowMs = 0; nowMs < 1000; ++nowMs) {
            coordinator.processDue();
            while (executor.pendingCount() > 0) {
                executor.runFirst();
            }
        }

        QCOMPARE(captureTimes.size(), 30);
        QVERIFY(captureTimes.last() - captureTimes.first() >= 900);
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

        coordinator.upsert(1, 77, QRect(0, 0, 1, 1), 1000);
        coordinator.upsert(2, 77, QRect(1, 0, 1, 1), 1000);
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

        coordinator.upsert(1, 11, std::nullopt, 1000);
        coordinator.upsert(2, 22, std::nullopt, 1000);
        coordinator.upsert(3, 33, std::nullopt, 1000);
        nowMs += 1000;
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

    void alignsMixedIntervalsForTheSameSourceWithoutChangingTheirPeriods()
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
        coordinator.upsert(1, 11, std::nullopt, 1000);
        coordinator.upsert(2, 77, QRect(0, 0, 1, 1), 1000);
        coordinator.upsert(3, 77, QRect(1, 0, 1, 1), 2500);
        for (nowMs = 0; nowMs < 6000; nowMs += 250) {
            coordinator.processDue();
            while (executor.pendingCount() > 0) { executor.runFirst(); }
        }
        QCOMPARE(mixedSourceCaptures, QList<qint64>({500, 1500, 2500, 3000, 3500, 4500, 5500}));
        QCOMPARE(batchSizes.first(), 2);
        QCOMPARE(batchSizes.last(), 2);
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

        coordinator.upsert(1, 11, QRect(0, 0, 1, 1), 1000);
        coordinator.processDue();
        QCOMPARE(executor.pendingCount(), 1);
        coordinator.upsert(1, 11, QRect(1, 0, 1, 1), 1000);

        executor.runFirst();
        QCOMPARE(ready.count(), 0);
        coordinator.processDue();
        QCOMPARE(executor.pendingCount(), 1);
        executor.runFirst();
        QCOMPARE(ready.count(), 1);
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

        coordinator.upsert(1, 11, std::nullopt, 1000);
        coordinator.processDue();
        QCOMPARE(executor.pendingCount(), 1);
        nowMs = 3500;
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

        coordinator.upsert(1, 11, std::nullopt, 1000);
        coordinator.processDue();
        executor.runFirst();
        QCOMPARE(unavailable.count(), 1);
        QCOMPARE(coordinator.diagnostics().failedBatches, quint64{1});

        nowMs = 1000;
        coordinator.processDue();
        executor.runFirst();
        QCOMPARE(ready.count(), 1);
        QCOMPARE(attempts, 2);
    }
};

QTEST_GUILESS_MAIN(StatisticsCoordinatorTest)

#include "statistics_coordinator_test.moc"
