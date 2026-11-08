#pragma once
#include <QObject>
#include <QPoint>
#include <QElapsedTimer>
#include <QHash>
#include <QStringList>
#include "config/app_config.h"
#include "platform/native_gateway.h"
class QWidget;
class QTimer;
class QuickSwitchService;
class WindowCatalog;
class RadialSwitchOverlay;
class AccountSwitchController final : public QObject {
    Q_OBJECT
public:
    AccountSwitchController(NativeGateway& gateway, WindowCatalog& catalog,
        const AppConfig& config, QuickSwitchService& service, QWidget* host);
    ~AccountSwitchController() override;
    void stop();
    void setInputCaptureActive(bool active);
    RadialSwitchOverlay* overlay() const noexcept;
    void switchEveClient(bool next, quintptr foregroundHwnd);
    void setAccountTriggers(const QHash<quintptr, QStringList>& triggers);
    void beginRadialSwitchHold(quint64 sessionId, quintptr foregroundHwnd);
    void endRadialSwitchHold(
        quint64 sessionId,
        bool heldLongEnough,
        bool foregroundStillMatches,
        bool cancelled,
        quintptr foregroundHwnd);
    void showRadialSwitchWheel();
    void showDesktopPetAccountWheel(const QPoint& petCenter, int avatarSize);
    void moveDesktopPetAccountWheel(const QPoint& petCenter, int avatarSize);
    void dismissDesktopPetAccountWheel();
    void showRadialAccountPositionPreview(const QString& windowTitle);
    void updateRadialSwitchWheelCursor();
    void cancelRadialSwitchInteraction();
    void activateRadialSwitchClient(const WindowInfo& client, quintptr sourceHwnd);
signals:
    void accountPositionSelected(const QString& title, int position);
private:
    friend class MainWindowTest;
    void recordForegroundVisit();
    bool eligibleTrigger(quintptr hwnd) const;
    QList<WindowInfo> triggerCandidates(const QList<WindowInfo>& clients) const;
    NativeGateway& gateway_;
    WindowCatalog& catalog_;
    const AppConfig& config_;
    QuickSwitchService& quickSwitchService_;
    RadialSwitchOverlay* radialSwitchOverlay_{};
    QTimer* radialSwitchHoldTimer_{};
    QTimer* radialSwitchCursorTimer_{};
    QTimer* foregroundVisitTimer_{};
    QElapsedTimer clock_;
    QElapsedTimer holdClock_;
    QHash<quintptr, qint64> lastVisited_;
    QHash<quintptr, QStringList> accountTriggers_;
    bool stopped_{};
    bool quickSwitchCaptureActive_{};
    bool radialSwitchKeyHeld_{};
    bool radialSwitchCancelled_{};
    bool radialSwitchWheelVisible_{};
    quint64 radialSwitchSessionId_{};
    quintptr radialSwitchForegroundHwnd_{};
    quintptr radialSwitchOriginHwnd_{};
    QPoint radialSwitchAnchor_;
    QList<WindowInfo> radialSwitchClients_;
};
