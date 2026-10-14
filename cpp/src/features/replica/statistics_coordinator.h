#pragma once

#include <cstdint>
#include <functional>
#include <optional>

#include <QHash>
#include <QList>
#include <QObject>
#include <QRect>
#include <QSet>
#include <QThreadPool>

#include "platform/native_gateway.h"
#include "features/replica/replica_manager.h"

class QTimer;

using StatisticsClock = std::function<qint64()>;
using StatisticsCapture = std::function<QList<std::optional<PixelStatistics>>(
    std::uintptr_t,
    const QList<std::optional<QRect>>&)>;
using StatisticsExecutor = std::function<void(std::function<void()>)>;

struct StatisticsDiagnostics {
    quint64 completedBatches{};
    quint64 failedBatches{};
    quint64 skippedIntervals{};
    quint64 mergedCaptures{};
    int activeBatches{};
    int maximumActiveBatches{};
};

class StatisticsCoordinator final : public QObject {
    Q_OBJECT

public:
    explicit StatisticsCoordinator(NativeGateway& gateway, QObject* parent = nullptr);
    StatisticsCoordinator(
        StatisticsClock clock,
        StatisticsCapture capture,
        StatisticsExecutor executor,
        QObject* parent = nullptr);
    StatisticsCoordinator(
        StatisticsClock clock, StatisticsCapture capture, StatisticsExecutor executor,
        std::function<QImage(std::uintptr_t)> frameCapture, QObject* parent);
    ~StatisticsCoordinator() override;

    void upsert(
        ReplicaId id,
        std::uintptr_t hwnd,
        std::optional<QRect> region,
        int intervalMs);
    void remove(ReplicaId id);
    void start();
    void stop();
    void processDue();
    [[nodiscard]] quint64 requestFrame(ReplicaId id, std::uintptr_t hwnd);
    void cancelFrame(ReplicaId id);
    [[nodiscard]] bool isIdle() const noexcept;

    [[nodiscard]] StatisticsDiagnostics diagnostics() const noexcept;

signals:
    void statisticsReady(ReplicaId id, qint64 redCount, qint64 whiteCount);
    void statisticsUnavailable(ReplicaId id);
    void frameReady(ReplicaId id, quint64 request, const QImage& frame);
    void frameUnavailable(ReplicaId id, quint64 request, const QString& error);
    void idle();

private:
    struct Registration {
        ReplicaId id{};
        std::uintptr_t hwnd{};
        std::optional<QRect> region;
        int intervalMs{};
        quint64 version{};
        qint64 nextDueMs{};
        bool inFlight{};
    };

    struct Snapshot {
        ReplicaId id{};
        std::uintptr_t hwnd{};
        std::optional<QRect> region;
        quint64 version{};
        qint64 requestedAtMs{};
    };

    void initialize();
    void rephaseRegistrations();
    void dispatchBatch(std::uintptr_t hwnd, const QList<ReplicaId>& ids, qint64 nowMs);
    void completeBatch(
        std::uintptr_t hwnd,
        const QList<Snapshot>& snapshots,
        const QList<std::optional<PixelStatistics>>& results,
        bool failed);
    void armTimer();

    StatisticsClock clock_;
    StatisticsCapture capture_;
    StatisticsExecutor executor_;
    std::function<QImage(std::uintptr_t)> frameCapture_;
    QHash<ReplicaId, quint64> frameRequests_;
    QHash<ReplicaId, std::uintptr_t> inFlightFrames_;
    int activeFrameCaptures_{};
    quint64 nextFrameRequest_{1};
    QHash<ReplicaId, Registration> registrations_;
    QSet<std::uintptr_t> inFlightSources_;
    QThreadPool threadPool_;
    QTimer* timer_{};
    StatisticsDiagnostics diagnostics_;
    quint64 nextVersion_{1};
    bool running_{false};
    bool ownsExecutor_{false};
};
