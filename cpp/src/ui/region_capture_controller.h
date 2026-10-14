#pragma once
#include <functional>
#include <QObject>
#include <QPointer>
#include <QRect>
#include <QSize>
#include "platform/native_gateway.h"
#include "features/replica/replica_manager.h"
#include "runtime/runtime_policy.h"
class QWidget;
class ReplicaWindow;
class WindowCatalog;
class ScreenRegionCaptureOverlay;
class RegionCaptureController final : public QObject {
    Q_OBJECT
public:
    using Lookup = std::function<ReplicaWindow*(ReplicaId)>;
    using Replicas = std::function<QList<ReplicaWindow*>()>;
    using Message = std::function<void(const QString&, int)>;
    using SavePreset = std::function<void(const QRect&, int, bool)>;
    RegionCaptureController(NativeGateway& gateway, WindowCatalog& catalog,
        RuntimePolicy& policy, QWidget* host, Lookup lookup, Replicas replicas,
        Message message, SavePreset savePreset, std::function<void()> applyPolicy);
    ~RegionCaptureController() override;
    void replicaClosed(ReplicaId id);
    void beginScreenRegionCapture(ReplicaId id);
    void beginRegionPresetCapture();
    void startScreenRegionCapture(
        ReplicaWindow* target,
        std::uintptr_t sourceHwnd,
        const QSize& sourceSize,
        bool createRegionPreset);
    void finishScreenRegionCapture(const QRect& sourceRegion, bool accepted);
    void restoreScreenRegionCaptureState();
private:
    NativeGateway& gateway_;
    WindowCatalog& catalog_;
    RuntimePolicy& policy_;
    QWidget* host_{};
    Lookup lookup_;
    Replicas replicas_;
    Message showMessage_;
    SavePreset savePreset_;
    std::function<void()> applyPolicy_;
    QPointer<ScreenRegionCaptureOverlay> screenRegionCaptureOverlay_;
    QPointer<ReplicaWindow> screenRegionCaptureTarget_;
    std::uintptr_t screenRegionCaptureSourceHwnd_{};
    QString screenRegionCaptureScreenName_;
    QRect screenRegionCaptureNativeClientBounds_;
    QSize screenRegionCaptureNativeClientSize_;
    QSize screenRegionCaptureSourceSize_;
    bool screenRegionCaptureActive_{};
    bool screenRegionCaptureCreatesPreset_{};
};
