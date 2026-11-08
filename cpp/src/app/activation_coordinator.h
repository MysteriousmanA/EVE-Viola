#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <optional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <QJsonObject>
#include <QString>
#include <QFuture>

// The application thread owns policy and pending requests; the async worker only
// receives copies of activation inputs. Input hooks update atomic activity state.
// Injected activation and clock callbacks must support worker-thread invocation.
class ActivationCoordinator final {
public:
    using Activate = std::function<std::uintptr_t(std::uintptr_t)>;
    using Clock = std::function<std::int64_t()>;
    explicit ActivationCoordinator(Activate activate, Clock clock = {});
    ~ActivationCoordinator();
    void noteInputPressed() noexcept;
    void noteInputReleased() noexcept;
    void setAutomaticActivationMouseQuietPeriodMs(int milliseconds) noexcept;
    void queueAutomaticActivation(
        std::uint64_t replicaId,
        std::uintptr_t hwnd,
        int cooldownMs,
        QString triggerEventId = {},
        QString smallWindowName = {},
        QString sourceWindowTitle = {});
    void setAutomaticTriggerActive(std::uint64_t replicaId, bool active);
    void cancelAutomaticActivation(std::uint64_t replicaId);
    void flushAutomaticActivations();
    void flushAutomaticActivationsAsync();
    [[nodiscard]] bool activateWindowWithCooldown(
        std::uintptr_t hwnd,
        int cooldownMs,
        std::uintptr_t* observedForegroundHwnd = nullptr);
private:
    friend class ArchitectureServicesTest;
    struct AutomaticActivationRequest {
        std::uintptr_t hwnd{};
        int cooldownMs{};
        QString triggerEventId;
        QString smallWindowName;
        QString sourceWindowTitle;
    };

    static void logAutomaticActivationOutcome(
        std::uint64_t replicaId,
        const AutomaticActivationRequest& request,
        const QString& result,
        const QString& reason,
        QJsonObject windowStateBefore = {},
        std::optional<std::uintptr_t> activationCheckForegroundHwnd = std::nullopt);
    void flushAutomaticActivations(bool asynchronous);
    void collectAutomaticActivation();
    [[nodiscard]] bool hasRecentInputActivity() const noexcept;

    Activate activate_;
    Clock clock_;
    QFuture<std::optional<std::int64_t>> automaticActivation_;
    std::shared_ptr<std::atomic<bool>> automaticActivationCancelled_;
    std::uint64_t automaticActivationReplicaId_{};
    std::atomic<int> activeInputs_{};
    std::atomic<std::int64_t> lastInputActivityMs_{};
    std::atomic<int> automaticActivationMouseQuietPeriodMs_{};
    std::optional<std::int64_t> lastCooldownActivation_;
    std::unordered_map<std::uint64_t, AutomaticActivationRequest>
        pendingAutomaticActivations_;
    std::unordered_set<std::uint64_t> activeAutomaticTriggers_;
};
