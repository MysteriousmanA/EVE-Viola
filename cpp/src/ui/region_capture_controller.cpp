#include "ui/region_capture_controller.h"
#include <algorithm>
#include <exception>
#include <utility>
#include <QGuiApplication>
#include <QScreen>
#include <QRectF>
#include <QWidget>
#include "config/app_config.h"
#include "platform/window_catalog.h"
#include "ui/replica_window.h"
#include "ui/screen_region_capture_overlay.h"
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif
namespace {
struct ClientScreenGeometry {
    QRect screenGeometry;
    QRectF clientGeometry;
    QRect nativeClientBounds;
    QSize nativeClientSize;
    QString screenName;
};

std::optional<ClientScreenGeometry> locateClientScreenGeometry(std::uintptr_t hwnd)
{
#ifdef Q_OS_WIN
    const HWND nativeWindow = reinterpret_cast<HWND>(hwnd);
    if (nativeWindow == nullptr || IsWindow(nativeWindow) == FALSE) {
        return std::nullopt;
    }

    RECT clientRect{};
    POINT clientOrigin{};
    if (GetClientRect(nativeWindow, &clientRect) == FALSE
        || ClientToScreen(nativeWindow, &clientOrigin) == FALSE) {
        return std::nullopt;
    }
    const int clientWidth = clientRect.right - clientRect.left;
    const int clientHeight = clientRect.bottom - clientRect.top;
    if (clientWidth <= 0 || clientHeight <= 0) {
        return std::nullopt;
    }

    const HMONITOR monitor = MonitorFromWindow(nativeWindow, MONITOR_DEFAULTTONEAREST);
    MONITORINFOEXW monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (monitor == nullptr
        || GetMonitorInfoW(monitor, reinterpret_cast<LPMONITORINFO>(&monitorInfo)) == FALSE) {
        return std::nullopt;
    }

    const QString monitorName = QString::fromWCharArray(monitorInfo.szDevice);
    QScreen* screen = nullptr;
    for (QScreen* candidate : QGuiApplication::screens()) {
        if (candidate->name().compare(monitorName, Qt::CaseInsensitive) == 0) {
            screen = candidate;
            break;
        }
    }
    if (screen == nullptr || screen->devicePixelRatio() <= 0.0) {
        return std::nullopt;
    }

    const QRect nativeMonitorBounds(
        monitorInfo.rcMonitor.left,
        monitorInfo.rcMonitor.top,
        monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
        monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top);
    const QRect nativeClientBounds(
        clientOrigin.x,
        clientOrigin.y,
        clientWidth,
        clientHeight);
    const QPoint clientOffset = nativeClientBounds.topLeft() - nativeMonitorBounds.topLeft();
    const qreal ratio = screen->devicePixelRatio();
    const QPointF clientLogicalOrigin = QPointF(screen->geometry().topLeft())
        + QPointF(clientOffset.x() / ratio, clientOffset.y() / ratio);
    const QSizeF clientLogicalSize(clientWidth / ratio, clientHeight / ratio);

    return ClientScreenGeometry{
        screen->geometry(),
        QRectF(clientLogicalOrigin, clientLogicalSize),
        nativeClientBounds,
        QSize(clientWidth, clientHeight),
        monitorName,
    };
#else
    Q_UNUSED(hwnd);
    return std::nullopt;
#endif
}

#ifdef Q_OS_WIN
struct FrontmostEveWindowSearch {
    const QList<WindowInfo>* windows{};
    std::uintptr_t hwnd{};
};

BOOL CALLBACK findFrontmostEveWindow(HWND hwnd, LPARAM parameter)
{
    if (IsWindowVisible(hwnd) == FALSE || IsIconic(hwnd) != FALSE) {
        return TRUE;
    }
    auto* search = reinterpret_cast<FrontmostEveWindowSearch*>(parameter);
    const auto match = std::find_if(
        search->windows->cbegin(), search->windows->cend(), [hwnd](const WindowInfo& window) {
            return window.hwnd == reinterpret_cast<std::uintptr_t>(hwnd);
        });
    if (match == search->windows->cend()) {
        return TRUE;
    }
    search->hwnd = match->hwnd;
    return FALSE;
}
#endif

std::optional<WindowInfo> frontmostEveWindow(const QList<WindowInfo>& windows)
{
    if (windows.isEmpty()) {
        return std::nullopt;
    }
#ifdef Q_OS_WIN
    const auto foreground = reinterpret_cast<std::uintptr_t>(GetForegroundWindow());
    const auto activeMatch = std::find_if(
        windows.cbegin(), windows.cend(), [foreground](const WindowInfo& window) {
            return window.hwnd == foreground;
        });
    if (activeMatch != windows.cend()) {
        return *activeMatch;
    }

    FrontmostEveWindowSearch search{&windows, 0};
    static_cast<void>(EnumWindows(
        findFrontmostEveWindow,
        reinterpret_cast<LPARAM>(&search)));
    const auto frontmostMatch = std::find_if(
        windows.cbegin(), windows.cend(), [&search](const WindowInfo& window) {
            return window.hwnd == search.hwnd;
        });
    if (frontmostMatch != windows.cend()) {
        return *frontmostMatch;
    }
#endif
    return windows.front();
}

} // namespace
RegionCaptureController::RegionCaptureController(NativeGateway& gateway, WindowCatalog& catalog,
    RuntimePolicy& policy, QWidget* host, Lookup lookup, Replicas replicas,
    Message message, SavePreset savePreset, std::function<void()> applyPolicy)
    : QObject(host), gateway_(gateway), catalog_(catalog), policy_(policy), host_(host)
    , lookup_(std::move(lookup)), replicas_(std::move(replicas))
    , showMessage_(std::move(message)), savePreset_(std::move(savePreset)), applyPolicy_(std::move(applyPolicy))
{
}
RegionCaptureController::~RegionCaptureController() { finishScreenRegionCapture({}, false); }
void RegionCaptureController::replicaClosed(ReplicaId id)
{
    if (screenRegionCaptureActive_ && !screenRegionCaptureCreatesPreset_
        && (screenRegionCaptureTarget_ == nullptr || screenRegionCaptureTarget_->replicaId() == id)) {
        finishScreenRegionCapture({}, false);
    }
}
void RegionCaptureController::beginScreenRegionCapture(ReplicaId id)
{
    if (screenRegionCaptureActive_) {
        return;
    }
    ReplicaWindow* target = lookup_(id);
    if (target == nullptr || !target->source().has_value()) {
        showMessage_(QStringLiteral("请先为该小窗选择游戏窗口"), 4000);
        return;
    }

    const std::uintptr_t sourceHwnd = target->source()->hwnd;
    const QSize sourceSize = target->sourceFrameSize();
    startScreenRegionCapture(target, sourceHwnd, sourceSize, false);
}

void RegionCaptureController::beginRegionPresetCapture()
{
    if (screenRegionCaptureActive_) {
        return;
    }

    try {
        const auto source = frontmostEveWindow(catalog_.windows(true));
        if (!source.has_value()) {
            showMessage_(QStringLiteral("没有找到可框选的 EVE 游戏窗口"), 5000);
            return;
        }

        QSize sourceSize;
        for (ReplicaWindow* window : replicas_()) {
            if (window != nullptr && window->source().has_value()
                && window->source()->hwnd == source->hwnd) {
                sourceSize = window->sourceFrameSize();
                break;
            }
        }
        if (sourceSize.width() <= 0 || sourceSize.height() <= 0) {
            const auto thumbnail = gateway_.createThumbnail(
                static_cast<std::uintptr_t>(host_->winId()), source->hwnd);
            sourceSize = thumbnail.sourceSize();
        }
        startScreenRegionCapture(nullptr, source->hwnd, sourceSize, true);
    } catch (const NativeException& error) {
        showMessage_(
            QStringLiteral("无法读取游戏画面尺寸：%1")
                .arg(QString::fromUtf8(error.what())),
            5000);
    }
}

void RegionCaptureController::startScreenRegionCapture(
    ReplicaWindow* target,
    std::uintptr_t sourceHwnd,
    const QSize& sourceSize,
    bool createRegionPreset)
{
    if (screenRegionCaptureActive_) {
        return;
    }
    const auto geometry = locateClientScreenGeometry(sourceHwnd);
    if (!geometry.has_value() || sourceSize.width() <= 0 || sourceSize.height() <= 0) {
        showMessage_(
            QStringLiteral("无法定位游戏画面，请确认游戏窗口仍然打开并已最大化"), 5000);
        return;
    }

    screenRegionCaptureActive_ = true;
    screenRegionCaptureTarget_ = target;
    screenRegionCaptureSourceHwnd_ = sourceHwnd;
    screenRegionCaptureScreenName_ = geometry->screenName;
    screenRegionCaptureNativeClientBounds_ = geometry->nativeClientBounds;
    screenRegionCaptureNativeClientSize_ = geometry->nativeClientSize;
    screenRegionCaptureSourceSize_ = sourceSize;
    screenRegionCaptureCreatesPreset_ = createRegionPreset;
    policy_.regionCapture = true;
    applyPolicy_();

    auto* overlay = new ScreenRegionCaptureOverlay(
        geometry->screenGeometry,
        geometry->clientGeometry,
        sourceSize,
        host_);
    screenRegionCaptureOverlay_ = overlay;
    connect(
        overlay,
        &ScreenRegionCaptureOverlay::selectionFinished,
        this,
        &RegionCaptureController::finishScreenRegionCapture);
    connect(overlay, &QObject::destroyed, this, [this] {
        if (screenRegionCaptureActive_) {
            finishScreenRegionCapture({}, false);
        }
    });
    overlay->show();
    overlay->raise();
    overlay->activateWindow();
    overlay->setFocus(Qt::OtherFocusReason);
}

void RegionCaptureController::finishScreenRegionCapture(
    const QRect& sourceRegion,
    bool accepted)
{
    if (!screenRegionCaptureActive_) {
        return;
    }

    const QPointer<ReplicaWindow> target = screenRegionCaptureTarget_;
    const bool createRegionPreset = screenRegionCaptureCreatesPreset_;
    bool canApply = accepted;
    if (canApply && createRegionPreset) {
        // The preset capture has no ReplicaWindow whose DWM thumbnail can be
        // re-queried. Keep the region measured at capture start and only reject
        // if the source HWND itself has gone away.
        canApply = locateClientScreenGeometry(screenRegionCaptureSourceHwnd_).has_value();
    } else if (canApply) {
        canApply = target != nullptr
            && target->source().has_value()
            && target->source()->hwnd == screenRegionCaptureSourceHwnd_
            && target->sourceFrameSize() == screenRegionCaptureSourceSize_;
    }
    if (canApply && !createRegionPreset) {
        const auto currentGeometry =
            locateClientScreenGeometry(screenRegionCaptureSourceHwnd_);
        canApply = currentGeometry.has_value()
            && currentGeometry->screenName == screenRegionCaptureScreenName_
            && currentGeometry->nativeClientBounds == screenRegionCaptureNativeClientBounds_
            && currentGeometry->nativeClientSize == screenRegionCaptureNativeClientSize_;
    }

    const QPointer<ScreenRegionCaptureOverlay> overlay = screenRegionCaptureOverlay_;
    screenRegionCaptureActive_ = false;
    screenRegionCaptureOverlay_.clear();
    screenRegionCaptureTarget_.clear();
    screenRegionCaptureSourceHwnd_ = 0;
    if (overlay != nullptr && overlay->isVisible()) {
        overlay->close();
    }

    if (canApply && !sourceRegion.isEmpty() && createRegionPreset) {
        restoreScreenRegionCaptureState();
        savePreset_(sourceRegion, kDefaultStatisticsIntervalMs, true);
        return;
    }
    if (canApply && target != nullptr && !sourceRegion.isEmpty()) {
        target->setCapturedRegion(sourceRegion);
        showMessage_(QStringLiteral("截取区域已更新"), 3000);
    } else if (accepted && createRegionPreset) {
        showMessage_(
            QStringLiteral("游戏窗口已关闭，未保存区域预设"), 5000);
    } else if (accepted) {
        showMessage_(
            QStringLiteral("游戏窗口位置或尺寸已变化，已取消本次框选"), 5000);
    }

    restoreScreenRegionCaptureState();
}

void RegionCaptureController::restoreScreenRegionCaptureState()
{
    policy_.regionCapture = false;
    applyPolicy_();
    screenRegionCaptureScreenName_.clear();
    screenRegionCaptureNativeClientBounds_ = {};
    screenRegionCaptureNativeClientSize_ = {};
    screenRegionCaptureSourceSize_ = {};
    screenRegionCaptureCreatesPreset_ = false;

}
