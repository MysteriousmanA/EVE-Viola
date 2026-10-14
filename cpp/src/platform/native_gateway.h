#pragma once

#include <memory>
#include <functional>
#include <chrono>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

#include <QImage>
#include <QList>
#include <QJsonObject>
#include <QMetaType>
#include <QRect>
#include <QSize>
#include <QString>

#include "eve_viola_native.h"
#include "features/replica/pixel_statistics_calculator.h"

class ActivationCoordinator;

inline constexpr std::uint32_t kExpectedNativeApiVersion = 3;

struct WindowInfo {
    std::uintptr_t hwnd{};
    QString title;

    friend bool operator==(const WindowInfo&, const WindowInfo&) = default;
};

Q_DECLARE_METATYPE(WindowInfo)

struct NativeFunctions {
    decltype(&eve_viola_api_version) apiVersion{};
    decltype(&eve_viola_enumerate_eve_windows) enumerateWindows{};
    decltype(&eve_viola_free_window_list) freeWindowList{};
    decltype(&eve_viola_thumbnail_create) createThumbnail{};
    decltype(&eve_viola_thumbnail_source_size) thumbnailSourceSize{};
    decltype(&eve_viola_thumbnail_update) updateThumbnail{};
    decltype(&eve_viola_thumbnail_destroy) destroyThumbnail{};
    decltype(&eve_viola_activate_window) activateWindow{};
    decltype(&eve_viola_capture_window) captureWindow{};
    decltype(&eve_viola_free_pixels) freePixels{};
    // Optional so existing injected function tables keep their behavior.
    decltype(&eve_viola_capture_window_profiled) captureWindowProfiled{};
    decltype(&eve_viola_capture_frame) captureFrame{};
    decltype(&eve_viola_release_frame) releaseFrame{};

    static NativeFunctions linked() noexcept;
    [[nodiscard]] bool isComplete() const noexcept;
};

class NativeException final : public std::runtime_error {
public:
    NativeException(
        QString operation,
        std::int32_t status,
        std::optional<std::uintptr_t> observedForegroundHwnd = std::nullopt);

    [[nodiscard]] const QString& operation() const noexcept;
    [[nodiscard]] std::int32_t status() const noexcept;
    [[nodiscard]] std::optional<std::uintptr_t> observedForegroundHwnd() const noexcept;

private:
    QString operation_;
    std::int32_t status_{};
    std::optional<std::uintptr_t> observedForegroundHwnd_;
};

class ThumbnailHandle final {
public:
    ThumbnailHandle() = default;
    ~ThumbnailHandle() noexcept;

    ThumbnailHandle(const ThumbnailHandle&) = delete;
    ThumbnailHandle& operator=(const ThumbnailHandle&) = delete;
    ThumbnailHandle(ThumbnailHandle&& other) noexcept;
    ThumbnailHandle& operator=(ThumbnailHandle&& other) noexcept;

    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] std::uintptr_t nativeHandle() const noexcept;
    [[nodiscard]] QSize sourceSize() const;
    void update(
        const QRect& source,
        const QRect& destination,
        std::uint8_t opacity = 255,
        bool visible = true) const;

private:
    friend class NativeGateway;

    ThumbnailHandle(NativeFunctions functions, std::uintptr_t handle) noexcept;
    void reset() noexcept;

    NativeFunctions functions_{};
    std::uintptr_t handle_{};
};

class NativeGateway final {
public:
    explicit NativeGateway(NativeFunctions functions = NativeFunctions::linked());
    ~NativeGateway();
    [[nodiscard]] ActivationCoordinator& activationCoordinator() noexcept;

    [[nodiscard]] QList<WindowInfo> listEveWindows() const;
    [[nodiscard]] ThumbnailHandle createThumbnail(
        std::uintptr_t destinationHwnd,
        std::uintptr_t sourceHwnd) const;
    std::uintptr_t activateWindow(std::uintptr_t hwnd) const;
    void noteEveMouseButtonPressed() noexcept;
    void noteEveMouseButtonReleased() noexcept;
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
    [[nodiscard]] QImage captureWindow(std::uintptr_t hwnd) const;
    [[nodiscard]] std::function<QImage(std::uintptr_t)> frameCaptureFunction() const;
    [[nodiscard]] QList<std::optional<PixelStatistics>> captureWindowStatistics(
        std::uintptr_t hwnd,
        const QList<std::optional<QRect>>& regions) const;

private:
    NativeFunctions functions_;
    std::unique_ptr<ActivationCoordinator> activation_;
};
