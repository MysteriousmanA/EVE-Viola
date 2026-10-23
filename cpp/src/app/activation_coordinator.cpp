#include "app/activation_coordinator.h"
#include <algorithm>
#include <iterator>
#include <utility>
#include <chrono>
#include "config/app_config.h"
#include "platform/native_gateway.h"
#include "runtime/runtime_logger.h"
#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
namespace {
[[nodiscard]] std::int64_t steadyClockMilliseconds() noexcept
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

#ifdef Q_OS_WIN
QJsonObject describeWindow(const QString& prefix, HWND hwnd)
{
    const bool isWindow = hwnd != nullptr && IsWindow(hwnd) != FALSE;
    QString title;
    QString processName;
    DWORD processId = 0;
    bool processNameAvailable = false;
    bool isVisible = false;
    bool isTopmost = false;

    if (isWindow) {
        wchar_t titleBuffer[1024]{};
        const int titleLength = GetWindowTextW(
            hwnd, titleBuffer, static_cast<int>(std::size(titleBuffer)));
        if (titleLength > 0) {
            title = QString::fromWCharArray(titleBuffer, titleLength);
        }

        GetWindowThreadProcessId(hwnd, &processId);
        isVisible = IsWindowVisible(hwnd) != FALSE;
        const auto extendedStyle = static_cast<ULONG_PTR>(
            GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
        isTopmost = (extendedStyle & WS_EX_TOPMOST) != 0;

        if (processId != 0) {
            HANDLE process = OpenProcess(
                PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
            if (process != nullptr) {
                wchar_t imagePath[32768]{};
                DWORD imagePathLength = static_cast<DWORD>(std::size(imagePath));
                if (QueryFullProcessImageNameW(
                        process, 0, imagePath, &imagePathLength) != FALSE) {
                    QString fullPath = QString::fromWCharArray(
                        imagePath, static_cast<qsizetype>(imagePathLength));
                    const qsizetype separator = fullPath.lastIndexOf(QChar(u'\\'));
                    processName = separator >= 0
                        ? fullPath.mid(separator + 1)
                        : std::move(fullPath);
                    processNameAvailable = !processName.isEmpty();
                }
                CloseHandle(process);
            }
        }
    }

    return QJsonObject{
        {prefix + QStringLiteral("_hwnd"),
         RuntimeLogger::formatWindowHandle(
             reinterpret_cast<std::uintptr_t>(hwnd))},
        {prefix + QStringLiteral("_title"), title},
        {prefix + QStringLiteral("_process_id"), static_cast<double>(processId)},
        {prefix + QStringLiteral("_process_name"), processName},
        {prefix + QStringLiteral("_process_name_available"), processNameAvailable},
        {prefix + QStringLiteral("_is_window"), isWindow},
        {prefix + QStringLiteral("_is_visible"), isVisible},
        {prefix + QStringLiteral("_is_topmost"), isTopmost},
    };
}

BOOL CALLBACK findFrontmostVisibleWindow(HWND hwnd, LPARAM parameter)
{
    if (IsWindowVisible(hwnd) == FALSE) {
        return TRUE;
    }
    *reinterpret_cast<HWND*>(parameter) = hwnd;
    return FALSE;
}
#endif

QJsonObject captureWindowState(const QString& phase)
{
#ifdef Q_OS_WIN
    QJsonObject fields = describeWindow(
        phase + QStringLiteral("_foreground"), GetForegroundWindow());

    HWND frontmostWindow = nullptr;
    EnumWindows(findFrontmostVisibleWindow,
                reinterpret_cast<LPARAM>(&frontmostWindow));
    const QJsonObject frontmostFields = describeWindow(
        phase + QStringLiteral("_z_order_first_visible"), frontmostWindow);
    for (auto iterator = frontmostFields.constBegin();
         iterator != frontmostFields.constEnd();
         ++iterator) {
        fields.insert(iterator.key(), iterator.value());
    }
    return fields;
#else
    Q_UNUSED(phase);
    return {};
#endif
}

QJsonObject describeActivationCheckForeground(
    std::optional<std::uintptr_t> hwnd)
{
#ifdef Q_OS_WIN
    if (hwnd.has_value()) {
        return describeWindow(
            QStringLiteral("activation_check_foreground"),
            reinterpret_cast<HWND>(*hwnd));
    }
#else
    Q_UNUSED(hwnd);
#endif
    return {};
}

} // namespace
ActivationCoordinator::ActivationCoordinator(Activate activate, Clock clock)
    : activate_(std::move(activate))
    , clock_(clock ? std::move(clock) : Clock{steadyClockMilliseconds})
    , automaticActivationMouseQuietPeriodMs_(kDefaultAutomaticActivationMouseQuietPeriodMs)
{
}
void ActivationCoordinator::noteInputPressed() noexcept
{
    lastInputActivityMs_.store(clock_());
    activeInputs_.fetch_add(1);
}

void ActivationCoordinator::noteInputReleased() noexcept
{
    lastInputActivityMs_.store(clock_());
    int activeInputs = activeInputs_.load();
    while (activeInputs > 0
           && !activeInputs_.compare_exchange_weak(
               activeInputs, activeInputs - 1)) {
    }
}

void ActivationCoordinator::setAutomaticActivationMouseQuietPeriodMs(
    int milliseconds) noexcept
{
    automaticActivationMouseQuietPeriodMs_.store(
        isValidAutomaticActivationMouseQuietPeriodMs(milliseconds)
            ? milliseconds
            : kDefaultAutomaticActivationMouseQuietPeriodMs);
}

bool ActivationCoordinator::hasRecentInputActivity() const noexcept
{
    if (activeInputs_.load() > 0) {
        return true;
    }

    const std::int64_t lastActivityMs =
        lastInputActivityMs_.load();
    const int quietPeriodMs = automaticActivationMouseQuietPeriodMs_.load();
    return lastActivityMs > 0
        && clock_() - lastActivityMs < quietPeriodMs;
}

void ActivationCoordinator::queueAutomaticActivation(
    std::uint64_t replicaId,
    std::uintptr_t hwnd,
    int cooldownMs,
    QString triggerEventId,
    QString smallWindowName,
    QString sourceWindowTitle)
{
    if (replicaId == 0 || hwnd == 0) {
        return;
    }
    const auto existing = pendingAutomaticActivations_.find(replicaId);
    if (existing != pendingAutomaticActivations_.end()) {
        logAutomaticActivationOutcome(
            replicaId,
            existing->second,
            QStringLiteral("skipped"),
            QStringLiteral("superseded_by_newer_trigger"));
    }
    pendingAutomaticActivations_[replicaId] = {
        hwnd,
        cooldownMs,
        std::move(triggerEventId),
        std::move(smallWindowName),
        std::move(sourceWindowTitle)};
}

void ActivationCoordinator::setAutomaticTriggerActive(
    std::uint64_t replicaId,
    bool active)
{
    if (replicaId == 0) {
        return;
    }
    if (active) {
        activeAutomaticTriggers_.insert(replicaId);
    } else {
        activeAutomaticTriggers_.erase(replicaId);
    }
}

void ActivationCoordinator::cancelAutomaticActivation(std::uint64_t replicaId)
{
    const auto pending = pendingAutomaticActivations_.find(replicaId);
    if (pending != pendingAutomaticActivations_.end()) {
        logAutomaticActivationOutcome(
            replicaId,
            pending->second,
            QStringLiteral("skipped"),
            QStringLiteral("activation_cancelled"));
        pendingAutomaticActivations_.erase(pending);
    }
    activeAutomaticTriggers_.erase(replicaId);
}

void ActivationCoordinator::logAutomaticActivationOutcome(
    std::uint64_t replicaId,
    const AutomaticActivationRequest& request,
    const QString& result,
    const QString& reason,
    QJsonObject windowStateBefore,
    std::optional<std::uintptr_t> activationCheckForegroundHwnd) const
{
    QJsonObject fields{
        {QStringLiteral("origin"), QStringLiteral("color_logic")},
        {QStringLiteral("trigger_event_id"), request.triggerEventId},
        {QStringLiteral("replica_id"), static_cast<double>(replicaId)},
        {QStringLiteral("small_window"), request.smallWindowName},
        {QStringLiteral("target_window_title"), request.sourceWindowTitle},
        {QStringLiteral("target_hwnd"), RuntimeLogger::formatWindowHandle(request.hwnd)},
        {QStringLiteral("result"), result},
        {QStringLiteral("reason"), reason},
    };

    for (auto iterator = windowStateBefore.constBegin();
         iterator != windowStateBefore.constEnd();
         ++iterator) {
        fields.insert(iterator.key(), iterator.value());
    }
    const QJsonObject activationCheckFields = describeActivationCheckForeground(
        activationCheckForegroundHwnd);
    for (auto iterator = activationCheckFields.constBegin();
         iterator != activationCheckFields.constEnd();
         ++iterator) {
        fields.insert(iterator.key(), iterator.value());
    }
    fields.insert(
        QStringLiteral("activation_check_foreground_available"),
        activationCheckForegroundHwnd.has_value());
    const QJsonObject windowStateAfter = captureWindowState(QStringLiteral("after"));
    for (auto iterator = windowStateAfter.constBegin();
         iterator != windowStateAfter.constEnd();
         ++iterator) {
        fields.insert(iterator.key(), iterator.value());
    }
    RuntimeLogger::writeEvent(QStringLiteral("window_activation"), std::move(fields));
}

void ActivationCoordinator::flushAutomaticActivations()
{
    const QJsonObject windowStateBefore = captureWindowState(QStringLiteral("before"));
    auto pending = std::move(pendingAutomaticActivations_);
    pendingAutomaticActivations_.clear();

    std::unordered_set<std::uint64_t> triggered = activeAutomaticTriggers_;
    for (const auto& [replicaId, request] : pending) {
        Q_UNUSED(request);
        triggered.insert(replicaId);
    }

    if (triggered.size() != 1 || pending.size() != 1) {
        for (const auto& [replicaId, request] : pending) {
            logAutomaticActivationOutcome(
                replicaId,
                request,
                QStringLiteral("skipped"),
                QStringLiteral("multiple_color_logic_windows_active"),
                windowStateBefore);
        }
        return;
    }

    const auto& [replicaId, request] = *pending.cbegin();
    if (hasRecentInputActivity()) {
        logAutomaticActivationOutcome(
            replicaId,
            request,
            QStringLiteral("skipped"),
            QStringLiteral("recent_input_activity"),
            windowStateBefore);
        return;
    }

    std::uintptr_t activationCheckForegroundHwnd = 0;
    try {
        if (activateWindowWithCooldown(
                request.hwnd,
                request.cooldownMs,
                &activationCheckForegroundHwnd)) {
            logAutomaticActivationOutcome(
                replicaId,
                request,
                QStringLiteral("success"),
                QString{},
                windowStateBefore,
                activationCheckForegroundHwnd);
        } else {
            logAutomaticActivationOutcome(
                replicaId,
                request,
                QStringLiteral("skipped"),
                QStringLiteral("activation_cooldown"),
                windowStateBefore);
        }
    } catch (const NativeException& error) {
        logAutomaticActivationOutcome(
            replicaId,
            request,
            QStringLiteral("failed"),
            QStringLiteral("%1 (status %2)").arg(error.operation()).arg(error.status()),
            windowStateBefore,
            error.observedForegroundHwnd());
        throw;
    }
}

bool ActivationCoordinator::activateWindowWithCooldown(
    std::uintptr_t hwnd,
    int cooldownMs,
    std::uintptr_t* observedForegroundHwnd)
{
    const auto now = clock_();
    if (cooldownMs > 0
        && lastCooldownActivation_.has_value()
        && (now - *lastCooldownActivation_)
            < cooldownMs) {
        return false;
    }

    const std::uintptr_t observed = activate_(hwnd);
    if (observedForegroundHwnd != nullptr) {
        *observedForegroundHwnd = observed;
    }
    lastCooldownActivation_ = now;
    return true;
}
