#include "platform/native_gateway.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include <QScopeGuard>
#include <QJsonObject>

#include "app/activation_coordinator.h"
#include "runtime/runtime_logger.h"
#include "runtime/performance_recorder.h"

namespace {

[[nodiscard]] std::string exceptionMessage(
    const QString& operation,
    std::int32_t status)
{
    return QStringLiteral("%1失败，原生状态码 %2")
        .arg(operation)
        .arg(status)
        .toUtf8()
        .toStdString();
}

void checkStatus(std::int32_t status, const QString& operation)
{
    if (status != 0) {
        throw NativeException(operation, status);
    }
}

void recordCaptureMetrics(const EveCaptureMetrics& metrics, std::uintptr_t hwnd, std::int32_t status)
{
    PerformanceRecorder::record("native.capture_total", metrics.total_ns, hwnd, metrics.bytes, status != 0);
    if (metrics.succeeded != 0) {
        PerformanceRecorder::record("native.setup", metrics.setup_ns, hwnd);
        PerformanceRecorder::record("native.render", metrics.render_ns, hwnd);
        PerformanceRecorder::record("native.crop", metrics.crop_ns, hwnd);
        PerformanceRecorder::record("native.readback", metrics.readback_ns, hwnd, metrics.bytes);
        PerformanceRecorder::record("native.cleanup", metrics.cleanup_ns, hwnd);
        PerformanceRecorder::record("native.bitblt_fallback", 0, hwnd, metrics.used_bitblt_fallback);
    }
}

std::int32_t captureWithMetrics(
    const NativeFunctions& functions, std::uintptr_t hwnd,
    std::int32_t* width, std::int32_t* height, std::uint8_t** pixels)
{
    if (!PerformanceRecorder::enabled() || functions.captureWindowProfiled == nullptr) {
        return functions.captureWindow(hwnd, width, height, pixels);
    }
    EveCaptureMetrics metrics{};
    const auto status = functions.captureWindowProfiled(hwnd, width, height, pixels, &metrics);
    recordCaptureMetrics(metrics, hwnd, status);
    return status;
}

struct WindowListDeleter {
    decltype(&eve_viola_free_window_list) freeWindowList{};

    void operator()(EveWindowInfo* items) const noexcept
    {
        freeWindowList(items);
    }
};


} // namespace

NativeFunctions NativeFunctions::linked() noexcept
{
    return NativeFunctions{
        eve_viola_api_version,
        eve_viola_enumerate_eve_windows,
        eve_viola_free_window_list,
        eve_viola_thumbnail_create,
        eve_viola_thumbnail_source_size,
        eve_viola_thumbnail_update,
        eve_viola_thumbnail_destroy,
        eve_viola_activate_window,
        eve_viola_capture_window,
        eve_viola_free_pixels,
        eve_viola_capture_window_profiled,
        eve_viola_capture_frame,
        eve_viola_release_frame,
        eve_viola_resume_thumbnail_source,
    };
}

bool NativeFunctions::isComplete() const noexcept
{
    return apiVersion != nullptr
        && enumerateWindows != nullptr
        && freeWindowList != nullptr
        && createThumbnail != nullptr
        && thumbnailSourceSize != nullptr
        && updateThumbnail != nullptr
        && destroyThumbnail != nullptr
        && activateWindow != nullptr
        && captureWindow != nullptr
        && freePixels != nullptr;
}

NativeException::NativeException(
    QString operation,
    std::int32_t status,
    std::optional<std::uintptr_t> observedForegroundHwnd)
    : std::runtime_error(exceptionMessage(operation, status))
    , operation_(std::move(operation))
    , status_(status)
    , observedForegroundHwnd_(observedForegroundHwnd)
{
}

const QString& NativeException::operation() const noexcept
{
    return operation_;
}

std::int32_t NativeException::status() const noexcept
{
    return status_;
}

std::optional<std::uintptr_t> NativeException::observedForegroundHwnd() const noexcept
{
    return observedForegroundHwnd_;
}

ThumbnailHandle::ThumbnailHandle(
    NativeFunctions functions,
    std::uintptr_t handle) noexcept
    : functions_(functions)
    , handle_(handle)
{
}

ThumbnailHandle::~ThumbnailHandle() noexcept
{
    reset();
}

ThumbnailHandle::ThumbnailHandle(ThumbnailHandle&& other) noexcept
    : functions_(other.functions_)
    , handle_(std::exchange(other.handle_, 0))
{
}

ThumbnailHandle& ThumbnailHandle::operator=(ThumbnailHandle&& other) noexcept
{
    if (this != &other) {
        reset();
        functions_ = other.functions_;
        handle_ = std::exchange(other.handle_, 0);
    }
    return *this;
}

bool ThumbnailHandle::isValid() const noexcept
{
    return handle_ != 0;
}

std::uintptr_t ThumbnailHandle::nativeHandle() const noexcept
{
    return handle_;
}

QSize ThumbnailHandle::sourceSize() const
{
    std::int32_t width = 0;
    std::int32_t height = 0;
    checkStatus(
        functions_.thumbnailSourceSize(handle_, &width, &height),
        QStringLiteral("读取源窗口尺寸"));
    return QSize(width, height);
}

void ThumbnailHandle::update(
    const QRect& source,
    const QRect& destination,
    std::uint8_t opacity,
    bool visible) const
{
    const PerformanceScope timing("dwm.update_api");
    const EveRect sourceRect{
        source.left(),
        source.top(),
        source.left() + source.width(),
        source.top() + source.height(),
    };
    const EveRect rect{
        destination.left(),
        destination.top(),
        destination.left() + destination.width(),
        destination.top() + destination.height(),
    };
    checkStatus(
        functions_.updateThumbnail(handle_, sourceRect, rect, opacity, visible ? 1 : 0),
        QStringLiteral("更新 DWM 小窗"));
}

void ThumbnailHandle::reset() noexcept
{
    const auto handle = std::exchange(handle_, 0);
    if (handle != 0 && functions_.destroyThumbnail != nullptr) {
        static_cast<void>(functions_.destroyThumbnail(handle));
    }
}

NativeGateway::NativeGateway(NativeFunctions functions)
    : functions_(functions)
    , activation_(std::make_unique<ActivationCoordinator>(
          [this](std::uintptr_t hwnd) { return activateWindow(hwnd); }))
{
    if (!functions_.isComplete()) {
        throw NativeException(QStringLiteral("native function table"), -1);
    }
    const auto version = functions_.apiVersion();
    if (version != kExpectedNativeApiVersion) {
        throw NativeException(
            QStringLiteral("API version"),
            static_cast<std::int32_t>(version));
    }
}

QList<WindowInfo> NativeGateway::listEveWindows() const
{
    EveWindowInfo* rawItems = nullptr;
    std::uint32_t count = 0;
    checkStatus(
        functions_.enumerateWindows(&rawItems, &count),
        QStringLiteral("enumerate windows"));

    if (rawItems == nullptr) {
        functions_.freeWindowList(nullptr);
        return {};
    }

    const std::unique_ptr<EveWindowInfo[], WindowListDeleter> items(
        rawItems,
        WindowListDeleter{functions_.freeWindowList});
    QList<WindowInfo> windows;
    windows.reserve(static_cast<qsizetype>(count));
    for (std::uint32_t index = 0; index < count; ++index) {
        const auto& record = items[index];
        const auto* end = std::find(
            std::begin(record.title),
            std::end(record.title),
            L'\0');
        const auto length = static_cast<qsizetype>(end - std::begin(record.title));
        windows.append(WindowInfo{
            record.hwnd,
            QString::fromWCharArray(record.title, length),
        });
    }
    return windows;
}

ThumbnailHandle NativeGateway::createThumbnail(
    std::uintptr_t destinationHwnd,
    std::uintptr_t sourceHwnd) const
{
    std::uintptr_t handle = 0;
    checkStatus(
        functions_.createThumbnail(destinationHwnd, sourceHwnd, &handle),
        QStringLiteral("注册 DWM 小窗"));
    return ThumbnailHandle(functions_, handle);
}

void NativeGateway::resumeThumbnailSource(std::uintptr_t hwnd) const
{
    if (functions_.resumeThumbnailSource != nullptr) {
        checkStatus(functions_.resumeThumbnailSource(hwnd), QStringLiteral("恢复后台预览源窗口"));
    }
}

std::uintptr_t NativeGateway::activateWindow(std::uintptr_t hwnd) const
{
    std::uintptr_t observedForegroundHwnd = 0;
    const std::int32_t status = functions_.activateWindow(
        hwnd, &observedForegroundHwnd);
    if (status != 0) {
        throw NativeException(
            QStringLiteral("激活 EVE 窗口"), status, observedForegroundHwnd);
    }
    return observedForegroundHwnd;
}

void NativeGateway::noteInputPressed() noexcept { activation_->noteInputPressed(); }
void NativeGateway::noteInputReleased() noexcept { activation_->noteInputReleased(); }
void NativeGateway::setAutomaticActivationMouseQuietPeriodMs(int milliseconds) noexcept
{ activation_->setAutomaticActivationMouseQuietPeriodMs(milliseconds); }
void NativeGateway::queueAutomaticActivation(std::uint64_t id, std::uintptr_t hwnd,
    int cooldown, QString event, QString name, QString title)
{ activation_->queueAutomaticActivation(id, hwnd, cooldown, std::move(event), std::move(name), std::move(title)); }
void NativeGateway::setAutomaticTriggerActive(std::uint64_t id, bool active)
{ activation_->setAutomaticTriggerActive(id, active); }
void NativeGateway::cancelAutomaticActivation(std::uint64_t id)
{ activation_->cancelAutomaticActivation(id); }
void NativeGateway::flushAutomaticActivations() { activation_->flushAutomaticActivations(); }
bool NativeGateway::activateWindowWithCooldown(std::uintptr_t hwnd, int cooldown, std::uintptr_t* observed)
{ return activation_->activateWindowWithCooldown(hwnd, cooldown, observed); }
NativeGateway::~NativeGateway() = default;
ActivationCoordinator& NativeGateway::activationCoordinator() noexcept { return *activation_; }

QImage NativeGateway::captureWindow(std::uintptr_t hwnd) const
{
    const PerformanceScope timing("capture.freeze_total", hwnd);
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::uint8_t* pixels = nullptr;
    const std::int32_t status = captureWithMetrics(functions_, hwnd, &width, &height, &pixels);
    const auto releasePixels = qScopeGuard([this, &pixels, hwnd] {
        if (pixels != nullptr) {
            const PerformanceScope releaseTiming("capture.release", hwnd);
            functions_.freePixels(pixels);
        }
    });
    checkStatus(status, QStringLiteral("捕获窗口画面"));
    if (pixels == nullptr || width <= 0 || height <= 0) {
        throw NativeException(QStringLiteral("捕获窗口画面"), -4);
    }

    QImage image(
        pixels, width, height, width * 4, QImage::Format_RGB32);
    const PerformanceScope copyTiming("capture.freeze_copy", hwnd);
    const QImage copy = image.copy();
    return copy;
}

std::function<QImage(std::uintptr_t)> NativeGateway::frameCaptureFunction() const
{
    // The worker owns only a function table; it never borrows a UI service.
    return [functions = functions_](std::uintptr_t hwnd) {
        NativeGateway gateway(functions);
        return gateway.captureWindow(hwnd);
    };
}

QList<std::optional<PixelStatistics>> NativeGateway::captureWindowStatistics(
    std::uintptr_t hwnd,
    const QList<std::optional<QRect>>& regions,
    const QList<PixelColorTarget>& targets) const
{
    const PerformanceScope timing("statistics.total", hwnd);
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::uint8_t* pixels = nullptr;
    EveCaptureFrame frame{};
    const bool useFrame = functions_.captureFrame != nullptr && functions_.releaseFrame != nullptr;
    const auto releasePixels = qScopeGuard([this, &pixels, &frame, useFrame, hwnd] {
        const PerformanceScope releaseTiming("capture.release", hwnd);
        if (useFrame) {
            functions_.releaseFrame(&frame);
        } else if (pixels != nullptr) {
            functions_.freePixels(pixels);
        }
    });
    std::int32_t status = 0;
    qsizetype stride = 0;
    const uchar* view = nullptr;
    if (useFrame) {
        EveCaptureMetrics metrics{};
        const bool profile = PerformanceRecorder::enabled();
        status = functions_.captureFrame(hwnd, &frame, profile ? &metrics : nullptr);
        width = frame.width;
        height = frame.height;
        stride = frame.bytes_per_line;
        view = frame.pixels;
        if (profile) { recordCaptureMetrics(metrics, hwnd, status); }
    } else {
        status = captureWithMetrics(functions_, hwnd, &width, &height, &pixels);
        stride = static_cast<qsizetype>(width) * 4;
        view = pixels;
    }
    checkStatus(status, QStringLiteral("捕获窗口画面"));
    if (view == nullptr || width <= 0 || height <= 0
        || stride < static_cast<qsizetype>(width) * 4) {
        throw NativeException(QStringLiteral("捕获窗口画面"), -4);
    }

    // Multiple replicas can request the same region from one captured source.
    // Scan each exact region once, then fan its result back out to every replica.
    QList<std::optional<QRect>> uniqueRegions;
    QList<qsizetype> resultRegionIndexes;
    uniqueRegions.reserve(regions.size());
    resultRegionIndexes.reserve(regions.size());
    for (const auto& region : regions) {
        const auto existing = std::find(
            uniqueRegions.cbegin(), uniqueRegions.cend(), region);
        if (existing != uniqueRegions.cend()) {
            resultRegionIndexes.append(existing - uniqueRegions.cbegin());
            continue;
        }
        resultRegionIndexes.append(uniqueRegions.size());
        uniqueRegions.append(region);
    }

    QList<std::optional<PixelStatistics>> uniqueResults;
    uniqueResults.reserve(uniqueRegions.size());
    for (const auto& region : uniqueRegions) {
        const qint64 startNs = PerformanceRecorder::enabled() ? PerformanceRecorder::nowNs() : 0;
        const auto statistics = calculatePixelStatistics(
            view,
            width,
            height,
            stride,
            region,
            targets);
        if (startNs != 0) {
            PerformanceRecorder::record("statistics.scan", PerformanceRecorder::nowNs() - startNs,
                                        hwnd, static_cast<quint64>(statistics.pixelCount));
        }
        if (statistics.pixelCount == 0) {
            uniqueResults.append(std::nullopt);
        } else {
            uniqueResults.append(statistics);
        }
    }

    QList<std::optional<PixelStatistics>> results;
    results.reserve(regions.size());
    for (const qsizetype index : resultRegionIndexes) {
        results.append(uniqueResults.at(index));
    }
    return results;
}
