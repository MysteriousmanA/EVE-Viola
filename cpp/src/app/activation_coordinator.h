#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <QJsonObject>
#include <QString>

// Mouse and keyboard activity is recorded by the hook thread. All other operations
// run on the application thread. Clock and activation are injectable policy ports.
class ActivationCoordinator final {
public:
    using Activate = std::function<std::uintptr_t(std::uintptr_t)>;
    using Clock = std::function<std::int64_t()>;
    explicit ActivationCoordinator(Activate activate, Clock clock = {});
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
    [[nodiscard]] bool activateWindowWithCooldown(
        std::uintptr_t hwnd,
        int cooldownMs,
        std::uintptr_t* observedForegroundHwnd = nullptr);
private:
    struct AutomaticActivationRequest {
        std::uintptr_t hwnd{};
        int cooldownMs{};
        QString triggerEventId;
        QString smallWindowName;
        QString sourceWindowTitle;
    };

    void logAutomaticActivationOutcome(
        std::uint64_t replicaId,
        const AutomaticActivationRequest& request,
        const QString& result,
        const QString& reason,
        QJsonObject windowStateBefore = {},
        std::optional<std::uintptr_t> activationCheckForegroundHwnd = std::nullopt) const;
    [[nodiscard]] bool hasRecentInputActivity() const noexcept;

    Activate activate_;
    Clock clock_;
    std::atomic<int> activeInputs_{};
    std::atomic<std::int64_t> lastInputActivityMs_{};
    std::atomic<int> automaticActivationMouseQuietPeriodMs_{};
    std::optional<std::int64_t> lastCooldownActivation_;
    std::unordered_map<std::uint64_t, AutomaticActivationRequest>
        pendingAutomaticActivations_;
    std::unordered_set<std::uint64_t> activeAutomaticTriggers_;
};
