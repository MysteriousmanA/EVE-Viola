#pragma once

#include <cstddef>
#include <cstdint>

#include <QHash>
#include <QList>
#include <QMainWindow>
#include <QPointer>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>

#include "config/app_config.h"
#include "ui/audio_feedback_manager.h"
#include "platform/native_gateway.h"
#include "features/replica/replica_manager.h"
#include "features/replica/replica_controller.h"
#include "runtime/runtime_policy.h"
#include "features/replica/statistics_coordinator.h"

class AccountSwitchController;
class RegionCaptureController;
class WindowCatalog;
class QLabel;
class QFrame;
class QCloseEvent;
class QVBoxLayout;
class QScrollArea;
class QWidget;
class QTimer;
class QRadioButton;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QMenu;
class QSystemTrayIcon;
class ReplicaRow;
class ReplicaWindow;
class HomePage;
class HomeController;
class ReplicaListPage;
class RegionPresetPage;
class RegionPresetController;
class ColorRulePage;
class ColorRuleController;
class QuickSwitchPage;
class QuickSwitchController;
class RadialSwitchOverlay;
class PagePartitionPage;
class ExpeditionTimingPage;
class DesktopPetPage;
class QuickSwitchService;
class ScreenRegionCaptureOverlay;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(NativeGateway& gateway, QWidget* parent = nullptr);
    MainWindow(NativeGateway& gateway, QString configDirectory, QWidget* parent = nullptr);
    ~MainWindow() override;

    [[nodiscard]] ReplicaWindow* createReplica();
    [[nodiscard]] ReplicaWindow* duplicateReplica(ReplicaId id);
    [[nodiscard]] std::size_t activeReplicaCount() const noexcept;
    [[nodiscard]] QList<ReplicaWindow*> replicaWindows() const;
    [[nodiscard]] QList<ReplicaRow*> replicaRows() const;
    [[nodiscard]] QLabel* countLabel() const noexcept;

public slots:
    void showAndActivate();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    [[nodiscard]] bool loadConfiguration(QString& loadError);
    void showConfigurationLoadWarning(const QString& loadError);
    void initializeAccountSwitchController();
    void initializeRegionCaptureController();
    void connectWindowCatalogSignals();
    void connectStatisticsShutdown();
    void connectQuickSwitchSignals();
    void buildWindowLayout();
    void initializePersistenceTimer();
    void initializeReplicaOverlapOrder();
    void startQuickSwitch();

    void onReplicaClosed(ReplicaId id);
    void closeAllReplicas();
    void forceQuit();
    void refreshSummary();
    void updateFlashAggregate();
    ReplicaWindow* restoreReplica(
        const ReplicaConfig& config,
        const QList<WindowInfo>& availableWindows,
        const WindowInfo* sourceOverride = nullptr,
        bool useDefaultPosition = false);
    void restoreConfiguredReplicas();
    [[nodiscard]] ReplicaWindow* createReplica(
        const QList<WindowInfo>& sourceOptions);
    [[nodiscard]] ReplicaConfig replicaConfigFor(
        ReplicaId id,
        ReplicaConfig config) const;
    void collectReplicaConfigs();
    void applyRuntimePolicy();
    void schedulePersistReplicas();
    void refreshReplicaListSize();
    void scheduleReplicaOverlapOrder();
    void synchronizeReplicaOverlapOrder();
    void refreshReplicaGroups();
    void refreshReplicaFilters();
    void synchronizeReplicaGroupCollapse(ReplicaId id, bool collapsed);
    void reorderReplicaRow(ReplicaId sourceId, ReplicaId targetId, bool insertAfter);
    void refreshPresetBars();
    void rebuildReplicas();
    void setupTray();
    void setupGameRestoreMonitoring();
    void setupPagePartitionEveMonitoring();
    void updatePagePartitionEvePresence();
    void restorePendingGameWindows();
    void retryReplicaRegionRestore(ReplicaId id, int remainingAttempts);
    [[nodiscard]] QWidget* buildHomePage();
    [[nodiscard]] QWidget* buildReplicaListPage();
    [[nodiscard]] QWidget* buildRegionPresetPage();
    [[nodiscard]] QWidget* buildColorRulePage();
    [[nodiscard]] QWidget* buildQuickSwitchPage();
    [[nodiscard]] QWidget* buildPagePartitionPage();
    [[nodiscard]] QWidget* buildExpeditionTimingPage();
    [[nodiscard]] QWidget* buildDesktopPetPage();
    void setFishModeEnabled(bool enabled);
    void connectReplicaSignals(ReplicaWindow* window, ReplicaRow* row);
    void showSyncReplicasDialog(ReplicaId id);
    void saveRegionPresetFromReplica(ReplicaId id);
    void saveRegionPreset(
        const QRect& region,
        int statisticsIntervalMs,
        bool confirmReplacementOnDuplicate);
    void refreshConfigurationPages();
    void refreshRegionPresets();
    void refreshColorRules();
    void persistConfiguration();

    NativeGateway& gateway_;
    WindowCatalog* windowCatalog_{};
    AccountSwitchController* accountSwitchController_{};
    RegionCaptureController* regionCaptureController_{};
    StatisticsCoordinator* statisticsCoordinator_{};
    AudioFeedbackManager* audioFeedbackManager_{};
    ReplicaController replicaController_;
    RuntimePolicy runtimePolicy_;
    JsonConfigStore configStore_;
    AppConfig config_;
    QHash<ReplicaId, ReplicaWindow*> windows_;
    QHash<ReplicaId, ReplicaRow*> rows_;
    QHash<ReplicaId, Qt::WindowStates> suppressedWindowStates_;
    HomePage* homePage_{};
    HomeController* homeController_{};
    ReplicaListPage* replicaListPage_{};
    RegionPresetPage* regionPresetPage_{};
    RegionPresetController* regionPresetController_{};
    ColorRulePage* colorRulePage_{};
    ColorRuleController* colorRuleController_{};
    QuickSwitchPage* quickSwitchPage_{};
    QuickSwitchController* quickSwitchController_{};
    RadialSwitchOverlay* radialSwitchOverlay_{};
    PagePartitionPage* pagePartitionPage_{};
    DesktopPetPage* desktopPetPage_{};
    QuickSwitchService* quickSwitchService_{};
    QTimer* persistTimer_{};
    QTimer* replicaOverlapOrderTimer_{};
    QSystemTrayIcon* trayIcon_{};
    QMenu* trayMenu_{};
    bool quitRequested_{false};
    bool closing_{false};
    bool configurationWritable_{true};
    bool restoringReplicas_{false};
};
