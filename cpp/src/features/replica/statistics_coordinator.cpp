#include "features/replica/statistics_coordinator.h"
#include "runtime/performance_recorder.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include <QElapsedTimer>
#include <QMetaObject>
#include <QPointer>
#include <QTimer>
#include <QtConcurrentRun>

namespace {

constexpr int kMaximumCaptureConcurrency = 2;
constexpr int kMinimumTimerDelayMs = 5;
constexpr int kMaximumTimerDelayMs = 1000;

struct BatchOutcome {
    QList<std::optional<PixelStatistics>> results;
    bool failed{};
};

} // namespace

StatisticsCoordinator::StatisticsCoordinator(NativeGateway& gateway, QObject* parent)
    : QObject(parent)
{
    auto elapsed = std::make_shared<QElapsedTimer>();
    elapsed->start();
    clock_ = [elapsed] { return elapsed->elapsed(); };
    capture_ = [&gateway](
                   std::uintptr_t hwnd,
                   const QList<std::optional<QRect>>& regions,
                   const QList<PixelColorTarget>& targets) {
        return gateway.captureWindowStatistics(hwnd, regions, targets);
    };
    frameCapture_ = gateway.frameCaptureFunction();
    ownsExecutor_ = true;
    threadPool_.setMaxThreadCount(kMaximumCaptureConcurrency);
    executor_ = [this](std::function<void()> task) {
        static_cast<void>(QtConcurrent::run(
            &threadPool_,
            [task = std::move(task)]() mutable { task(); }));
    };
    initialize();
}

StatisticsCoordinator::StatisticsCoordinator(
    StatisticsClock clock,
    StatisticsCapture capture,
    StatisticsExecutor executor,
    QObject* parent)
    : StatisticsCoordinator(std::move(clock), std::move(capture), std::move(executor), {}, parent)
{
}

StatisticsCoordinator::StatisticsCoordinator(
    StatisticsClock clock, StatisticsCapture capture, StatisticsExecutor executor,
    std::function<QImage(std::uintptr_t)> frameCapture, QObject* parent)
    : QObject(parent)
    , clock_(std::move(clock))
    , capture_([capture = std::move(capture)](std::uintptr_t hwnd,
          const QList<std::optional<QRect>>& regions, const QList<PixelColorTarget>&) {
          return capture(hwnd, regions);
      })
    , executor_(std::move(executor))
    , frameCapture_(std::move(frameCapture))
{
    initialize();
}

StatisticsCoordinator::StatisticsCoordinator(
    StatisticsClock clock, ColorStatisticsCapture capture, StatisticsExecutor executor,
    QObject* parent)
    : QObject(parent), clock_(std::move(clock)), capture_(std::move(capture)),
      executor_(std::move(executor))
{
    initialize();
}

StatisticsCoordinator::~StatisticsCoordinator()
{
    stop();
    // MainWindow waits asynchronously for idle before destruction. Keep this
    // final ownership barrier for callers that destroy the service directly.
    if (ownsExecutor_) { threadPool_.waitForDone(); }
}

void StatisticsCoordinator::initialize()
{
    setObjectName(QStringLiteral("statisticsCoordinator"));
    timer_ = new QTimer(this);
    timer_->setSingleShot(true);
    timer_->setTimerType(Qt::CoarseTimer);
    connect(timer_, &QTimer::timeout, this, &StatisticsCoordinator::processDue);
    start();
}

void StatisticsCoordinator::upsert(
    ReplicaId id,
    std::uintptr_t hwnd,
    std::optional<QRect> region,
    int intervalMs,
    const QList<PixelColorTarget>& targets)
{
    if (id == 0 || hwnd == 0 || intervalMs <= 0) {
        remove(id);
        return;
    }

    const auto normalizedTargets = normalizedPixelColorTargets(targets);
    const auto existing = registrations_.constFind(id);
    if (existing != registrations_.cend()
        && existing->hwnd == hwnd
        && existing->region == region
        && existing->intervalMs == intervalMs
        && existing->targets == normalizedTargets) {
        return;
    }

    registrations_.insert(id, Registration{
        id,
        hwnd,
        std::move(region),
        intervalMs,
        nextVersion_++,
        clock_(),
        false,
        normalizedTargets,
    });
    rephaseRegistrations();
    armTimer();
}

void StatisticsCoordinator::remove(ReplicaId id)
{
    if (registrations_.remove(id)) {
        rephaseRegistrations();
        armTimer();
    }
}

void StatisticsCoordinator::start()
{
    if (running_) {
        return;
    }
    running_ = true;
    rephaseRegistrations();
    armTimer();
}

void StatisticsCoordinator::stop()
{
    if (!running_ && !ownsExecutor_) {
        return;
    }
    running_ = false;
    if (timer_ != nullptr) {
        timer_->stop();
    }
    registrations_.clear();
    frameRequests_.clear();
}

bool StatisticsCoordinator::isIdle() const noexcept
{
    return diagnostics_.activeBatches == 0 && activeFrameCaptures_ == 0;
}

void StatisticsCoordinator::cancelFrame(ReplicaId id) { frameRequests_.remove(id); }

quint64 StatisticsCoordinator::requestFrame(ReplicaId id, std::uintptr_t hwnd)
{
    if (!running_ || !frameCapture_ || id == 0 || hwnd == 0
        || activeFrameCaptures_ >= kMaximumCaptureConcurrency
        || inFlightFrames_.contains(id) || inFlightFrames_.values().contains(hwnd)
        || inFlightSources_.contains(hwnd)) { return 0; }
    const auto request = nextFrameRequest_++;
    frameRequests_.insert(id, request);
    inFlightFrames_.insert(id, hwnd);
    ++activeFrameCaptures_;
    const auto capture = frameCapture_;
    const QPointer<StatisticsCoordinator> guard(this);
    executor_([capture, guard, id, hwnd, request] {
        QImage image;
        QString error;
        try { image = capture(hwnd); }
        catch (const std::exception& failure) { error = QString::fromUtf8(failure.what()); }
        catch (...) { error = QStringLiteral("Unknown capture failure"); }
        if (!guard) { return; }
        QMetaObject::invokeMethod(guard, [guard, id, request, image = std::move(image), error] {
            if (!guard) { return; }
            --guard->activeFrameCaptures_;
            guard->inFlightFrames_.remove(id);
            if (guard->running_ && guard->frameRequests_.value(id) == request) {
                guard->frameRequests_.remove(id);
                if (error.isEmpty() && !image.isNull()) { emit guard->frameReady(id, request, image); }
                else { emit guard->frameUnavailable(id, request, error); }
            }
            guard->armTimer();
            if (guard->isIdle()) { emit guard->idle(); }
        }, Qt::QueuedConnection);
    });
    return request;
}

StatisticsDiagnostics StatisticsCoordinator::diagnostics() const noexcept
{
    return diagnostics_;
}

void StatisticsCoordinator::rephaseRegistrations()
{
    if (registrations_.isEmpty()) {
        return;
    }

    QList<std::uintptr_t> sources;
    QHash<std::uintptr_t, int> sourceIntervals;
    sources.reserve(registrations_.size());
    for (const auto& registration : std::as_const(registrations_)) {
        if (!sources.contains(registration.hwnd)) {
            sources.append(registration.hwnd);
            sourceIntervals.insert(registration.hwnd, registration.intervalMs);
        } else {
            sourceIntervals[registration.hwnd] = std::min(
                sourceIntervals.value(registration.hwnd), registration.intervalMs);
        }
    }
    std::sort(sources.begin(), sources.end());

    const qint64 nowMs = clock_();
    for (auto iterator = registrations_.begin(); iterator != registrations_.end(); ++iterator) {
        const int sourceIndex = sources.indexOf(iterator->hwnd);
        const qint64 offset = static_cast<qint64>(sourceIndex)
            * sourceIntervals.value(iterator->hwnd) / sources.size();
        iterator->nextDueMs = nowMs + offset;
    }
}

void StatisticsCoordinator::processDue()
{
    if (!running_) {
        return;
    }

    const qint64 nowMs = clock_();
    for (auto iterator = registrations_.begin(); iterator != registrations_.end(); ++iterator) {
        if (!iterator->inFlight) {
            continue;
        }
        while (iterator->nextDueMs <= nowMs) {
            iterator->nextDueMs += iterator->intervalMs;
            ++diagnostics_.skippedIntervals;
            PerformanceRecorder::record("scheduler.skipped_intervals", 0, iterator->hwnd, 1);
        }
    }

    int capacity = kMaximumCaptureConcurrency - diagnostics_.activeBatches;
    while (capacity > 0) {
        std::optional<std::uintptr_t> selectedSource;
        qint64 selectedDue = std::numeric_limits<qint64>::max();
        for (const auto& registration : std::as_const(registrations_)) {
            if (!registration.inFlight
                && registration.nextDueMs <= nowMs
                && !inFlightSources_.contains(registration.hwnd)
                && !inFlightFrames_.values().contains(registration.hwnd)
                && registration.nextDueMs < selectedDue) {
                selectedSource = registration.hwnd;
                selectedDue = registration.nextDueMs;
            }
        }
        if (!selectedSource.has_value()) {
            break;
        }
        if (PerformanceRecorder::enabled()) {
            PerformanceRecorder::record("scheduler.lateness", std::max<qint64>(0, nowMs - selectedDue) * 1000000,
                                        *selectedSource);
        }

        QList<ReplicaId> ids;
        for (const auto& registration : std::as_const(registrations_)) {
            if (!registration.inFlight
                && registration.hwnd == *selectedSource
                && registration.nextDueMs <= nowMs) {
                ids.append(registration.id);
            }
        }
        dispatchBatch(*selectedSource, ids, nowMs);
        --capacity;
    }
    armTimer();
}

void StatisticsCoordinator::dispatchBatch(
    std::uintptr_t hwnd,
    const QList<ReplicaId>& ids,
    qint64 nowMs)
{
    QList<Snapshot> snapshots;
    QList<std::optional<QRect>> regions;
    QList<PixelColorTarget> targets;
    snapshots.reserve(ids.size());
    regions.reserve(ids.size());
    for (const ReplicaId id : ids) {
        auto iterator = registrations_.find(id);
        if (iterator == registrations_.end()) {
            continue;
        }
        iterator->inFlight = true;
        do {
            iterator->nextDueMs += iterator->intervalMs;
        } while (iterator->nextDueMs <= nowMs);
        snapshots.append(Snapshot{id, hwnd, iterator->region, iterator->version, nowMs});
        regions.append(iterator->region);
        for (const auto& target : iterator->targets) {
            if (!targets.contains(target)) { targets.append(target); }
        }
    }
    if (snapshots.isEmpty()) {
        return;
    }

    inFlightSources_.insert(hwnd);
    ++diagnostics_.activeBatches;
    diagnostics_.maximumActiveBatches = std::max(
        diagnostics_.maximumActiveBatches,
        diagnostics_.activeBatches);
    if (snapshots.size() > 1) {
        diagnostics_.mergedCaptures += static_cast<quint64>(snapshots.size() - 1);
        PerformanceRecorder::record("scheduler.merged_captures", 0, hwnd,
                                    static_cast<quint64>(snapshots.size() - 1));
    }

    const QPointer<StatisticsCoordinator> guard(this);
    const auto capture = capture_;
    const qint64 queuedAt = PerformanceRecorder::enabled() ? PerformanceRecorder::nowNs() : 0;
    executor_([guard, capture, hwnd, snapshots, regions, targets, queuedAt] {
        const qint64 startedAt = queuedAt != 0 ? PerformanceRecorder::nowNs() : 0;
        if (queuedAt != 0) {
            PerformanceRecorder::record("scheduler.worker_wait", startedAt - queuedAt, hwnd);
        }
        BatchOutcome outcome;
        try {
            outcome.results = capture(hwnd, regions, targets);
            outcome.failed = outcome.results.size() != snapshots.size();
        } catch (...) {
            outcome.failed = true;
        }
        const qint64 postedAt = queuedAt != 0 ? PerformanceRecorder::nowNs() : 0;
        if (queuedAt != 0) {
            PerformanceRecorder::record("scheduler.batch", postedAt - startedAt, hwnd,
                                        static_cast<quint64>(snapshots.size()), outcome.failed);
        }
        if (guard == nullptr) {
            return;
        }
        QMetaObject::invokeMethod(
            guard,
            [guard, hwnd, snapshots, postedAt, outcome = std::move(outcome)]() mutable {
                if (postedAt != 0) {
                    PerformanceRecorder::record("scheduler.ui_wait", PerformanceRecorder::nowNs() - postedAt, hwnd);
                }
                if (guard != nullptr) {
                    guard->completeBatch(
                        hwnd,
                        snapshots,
                        outcome.results,
                        outcome.failed);
                }
            },
            Qt::QueuedConnection);
    });
}

void StatisticsCoordinator::completeBatch(
    std::uintptr_t hwnd,
    const QList<Snapshot>& snapshots,
    const QList<std::optional<PixelStatistics>>& results,
    bool failed)
{
    const PerformanceScope timing("scheduler.delivery", hwnd);
    inFlightSources_.remove(hwnd);
    diagnostics_.activeBatches = std::max(0, diagnostics_.activeBatches - 1);
    if (failed) {
        ++diagnostics_.failedBatches;
    } else {
        ++diagnostics_.completedBatches;
    }

    if (running_) {
        for (qsizetype index = 0; index < snapshots.size(); ++index) {
            const auto& snapshot = snapshots[index];
            auto iterator = registrations_.find(snapshot.id);
            if (iterator == registrations_.end()
                || iterator->version != snapshot.version
                || iterator->hwnd != snapshot.hwnd
                || iterator->region != snapshot.region) {
                PerformanceRecorder::record("scheduler.stale_results", 0, hwnd, 1);
                continue;
            }
            iterator->inFlight = false;
            if (failed || index >= results.size() || !results[index].has_value()) {
                emit statisticsUnavailable(snapshot.id);
            } else {
                PerformanceRecorder::record("scheduler.result_age",
                    std::max<qint64>(0, clock_() - snapshot.requestedAtMs) * 1000000, hwnd);
                emit statisticsReady(
                    snapshot.id,
                    results[index]->redCount,
                    results[index]->whiteCount);
                emit colorStatisticsReady(snapshot.id, *results[index]);
            }
        }
    }
    armTimer();
    if (isIdle()) { emit idle(); }
}

void StatisticsCoordinator::armTimer()
{
    if (!running_ || registrations_.isEmpty() || timer_ == nullptr) {
        if (timer_ != nullptr) {
            timer_->stop();
        }
        return;
    }
    if (diagnostics_.activeBatches >= kMaximumCaptureConcurrency) {
        timer_->stop();
        return;
    }

    qint64 nextDueMs = std::numeric_limits<qint64>::max();
    for (const auto& registration : std::as_const(registrations_)) {
        if (registration.inFlight || inFlightSources_.contains(registration.hwnd)
            || inFlightFrames_.values().contains(registration.hwnd)) {
            continue;
        }
        nextDueMs = std::min(nextDueMs, registration.nextDueMs);
    }
    if (nextDueMs == std::numeric_limits<qint64>::max()) {
        timer_->stop();
        return;
    }
    const qint64 remaining = nextDueMs - clock_();
    timer_->start(static_cast<int>(std::clamp<qint64>(
        remaining,
        kMinimumTimerDelayMs,
        kMaximumTimerDelayMs)));
}
