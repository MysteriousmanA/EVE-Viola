#pragma once

#include <functional>
#include <optional>

#include <QColor>
#include <QImage>
#include <QPoint>
#include <QPointer>
#include <QRect>
#include <QWidget>

#include "config/app_config.h"
#include "ui/audio_feedback_manager.h"
#include "features/color_rule/color_logic_engine.h"
#include "features/desktop_pet/desktop_pet_mode.h"
#include "platform/native_gateway.h"
#include "features/replica/pixel_statistics.h"
#include "features/replica/replica_manager.h"
#include "features/replica/statistics_coordinator.h"

class QLabel;
class QMenu;
class QCloseEvent;
class QContextMenuEvent;
class QKeyEvent;
class QMouseEvent;
class QMoveEvent;
class QPaintEvent;
class QResizeEvent;
class QShowEvent;
class QTimer;
class QWheelEvent;
class AdvancedLogicService;

class ReplicaWindow final : public QWidget {
    Q_OBJECT
    Q_PROPERTY(
        int automaticActivationCooldownMs
        READ automaticActivationCooldownMs
        WRITE setAutomaticActivationCooldownMs)
    Q_PROPERTY(
        bool automaticActivationEnabled
        READ automaticActivationEnabled
        WRITE setAutomaticActivationEnabled)

    friend class ReplicaWindowTest;

public:
    using ClosedCallback = std::function<void(ReplicaId)>;

    explicit ReplicaWindow(
        ReplicaId replicaId,
        NativeGateway& gateway,
        StatisticsCoordinator& statisticsCoordinator,
        AudioFeedbackManager& audioFeedbackManager,
        ClosedCallback onClosed = {},
        QWidget* parent = nullptr);
    ~ReplicaWindow() override;

    [[nodiscard]] static QRect aspectFit(
        const QSize& source,
        const QSize& target) noexcept;
    [[nodiscard]] static QRect deviceAspectFit(
        const QSize& source,
        const QSize& logicalTarget,
        qreal devicePixelRatio) noexcept;

    [[nodiscard]] ReplicaId replicaId() const noexcept;
    [[nodiscard]] const std::optional<WindowInfo>& source() const noexcept;
    [[nodiscard]] QSize sourceFrameSize() const;
    [[nodiscard]] QLabel* emptyLabel() const noexcept;
    [[nodiscard]] QMenu* buildContextMenu();

    void selectSource(const WindowInfo& source);
    void selectSourceFull(const WindowInfo& source);
    void setCapturedRegion(const QRect& region);

    [[nodiscard]] bool isCapturingRegion() const noexcept;
    [[nodiscard]] const std::optional<QRect>& selectedRegion() const noexcept;
    [[nodiscard]] const std::optional<QRect>& capturedRegion() const noexcept;
    [[nodiscard]] QSize frozenFrameSize() const noexcept;
    [[nodiscard]] int statisticsIntervalMs() const noexcept;
    [[nodiscard]] QList<PixelColorTarget> statisticsColors() const { return statisticsColors_; }
    [[nodiscard]] bool colorLogicEnabled() const noexcept;
    [[nodiscard]] int activeColorLogicSlot() const noexcept;
    [[nodiscard]] int colorLogicTriggerCount(int slotIndex) const noexcept;
    [[nodiscard]] int colorLogicSlotRule(int slotIndex) const noexcept;
    [[nodiscard]] QColor colorLogicSlotColor(int slotIndex) const noexcept;
    [[nodiscard]] bool sourceNameVisible() const noexcept;
    [[nodiscard]] QColor borderColor() const noexcept;
    [[nodiscard]] bool isFlashActive() const noexcept;
    [[nodiscard]] bool raiseOnClickEnabled() const noexcept;
    [[nodiscard]] bool automaticActivationEnabled() const noexcept;
    [[nodiscard]] int automaticActivationCooldownMs() const noexcept;
    void setAdvancedLogicService(AdvancedLogicService* service);

public slots:
    void beginRegionCapture();
    void cancelRegionCapture();
    void setRegionPresets(const QList<RegionPreset>& presets);
    void applyRegionPreset(const RegionPreset& preset);
    void setStatisticsInterval(int milliseconds);
    void setStatisticsColors(const QList<PixelColorTarget>& colors);
    void setColorRules(const QList<ColorRule>& rules);
    void setColorLogicSlot(int slotIndex, int ruleNumber);
    void setColorLogicEnabled(bool enabled);
    void setActiveColorLogicSlot(int slotIndex);
    void setColorLogicSlotColor(int slotIndex, const QColor& color);
    void setSourceNameVisible(bool visible);
    void setGlobalFlashState(bool dimmed);
    void setRaiseOnClickEnabled(bool enabled);
    void setAutomaticActivationEnabled(bool enabled);
    void setAutomaticActivationCooldownMs(int milliseconds);
    void setDesktopPetMode(DesktopPetMode mode);
    void setPresentationSuppressed(bool suppressed);
    void setWheelZoomLocked(bool locked);
    void resizeToScalePercentage(double percentage);
    void moveTo(const QPoint& pos);

signals:
    void sourceTitleChanged(const QString& title);
    void sourceUnavailable(const QString& title);
    void regionCaptureChanged(const QString& description);
    void closed(ReplicaId replicaId);
    void regionCaptured(const QRect& sourceRegion);
    void pixelStatisticsChanged(qint64 redCount, qint64 whiteCount);
    void colorStatisticsChanged(const PixelStatistics& statistics);
    void pixelStatisticsUnavailable();
    void statisticsIntervalChanged(int milliseconds);
    void colorLogicEnabledChanged(bool enabled);
    void settingsChanged();
    void colorLogicTriggered(int ruleNumber);
    void colorLogicNotificationRequested(int ruleNumber);
    void activeColorLogicSlotChanged(int slotIndex);
    void visibilityChanged(bool visible);
    void flashStateChanged(bool active);
    void positionChanged(const QPoint& pos);
    void sizeChanged(const QSize& size);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void moveEvent(QMoveEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void setVisible(bool visible) override;
    bool nativeEvent(
        const QByteArray& eventType,
        void* message,
        qintptr* result) override;

private:
    static constexpr double kZoomStep = 1.15;
    static constexpr int kBorderWidth = 3;
    static constexpr int kMinimumWindowEdge = 30;
    static constexpr int kLeftPressLongMs = 150;
    static constexpr int kMiddlePressLongMs = 1000;
    static constexpr int kFlashIntervalMs = 350;
    static constexpr int kSourceCheckIntervalMs = 2000;

    void initializeStatistics();
    void initializeRegionCapture();
    void initializeInput();
    void initializeFlash();
    void initializeSourceMonitoring();
    void initializeAudioFeedback();

    void updateThumbnail();
    void renewThumbnail();
    void selectSourceImpl(const WindowInfo& source, bool preserveRegion);
    void setFlashActive(bool active);
    void showFailure(const QString& heading, const NativeException& error);
    void finishRegionCapture(bool confirmed);
    void resumeFromCapture();
    void applyContentAspectRatio();
    void updateMinimumSize();
    void enforceAspectRatio();
    [[nodiscard]] QString regionDescription() const noexcept;
    [[nodiscard]] QRect contentRect() const noexcept;
    [[nodiscard]] QRect displayRect() const;
    [[nodiscard]] QRect thumbnailDestinationRect() const;
    [[nodiscard]] std::optional<QRect> mapToSource(
        const QRect& windowRegion) const;
    void syncStatisticsRegistration();
    void acceptFrozenFrame(const QImage& frame);
    void evaluateColorLogic(const PixelStatistics& statistics);
    void handleSourceClick();
    void handleColorLogicTrigger(const ColorLogicEffect& effect);
    void applyColorLogicEffects(const QList<ColorLogicEffect>& effects);
    void cancelAdvancedLogic();
    void drainAdvancedLogic();
    void applyColorLogicBorder();
    void toggleActiveColorLogicSlot();
    void positionSourceNameOverlay();
    [[nodiscard]] static QString sourceNameFromTitle(const QString& title);

    ReplicaId replicaId_{};
    NativeGateway& gateway_;
    StatisticsCoordinator& statisticsCoordinator_;
    AudioFeedbackManager& audioFeedbackManager_;
    ClosedCallback onClosed_;
    std::optional<ThumbnailHandle> thumbnail_;
    std::optional<WindowInfo> source_;
    QSize sourceSize_;
    QLabel* emptyLabel_{};
    QLabel* sourceNameOverlay_{};
    bool sourceNameVisible_{false};
    QPoint dragOrigin_;
    double contentAspectRatio_{0.0};
    QTimer* leftPressTimer_{};
    bool dragging_{false};
    QTimer* middlePressTimer_{};
    bool middleLongPressHandled_{false};
    bool closedNotified_{false};

    bool capturingRegion_{false};
    bool frameCapturePending_{false};
    quint64 frameCaptureRequest_{};
    QImage frozenFrame_;
    QPoint captureOrigin_;
    QPoint captureCurrent_;
    std::optional<QRect> selectedRegion_;
    std::optional<QRect> capturedRegion_;
    QList<RegionPreset> regionPresets_;
    bool statisticsEnabled_{true};
    int statisticsIntervalMs_{kDefaultStatisticsIntervalMs};
    qint64 lastRedCount_{};
    qint64 lastWhiteCount_{};
    PixelStatistics lastStatistics_;
    QList<PixelColorTarget> statisticsColors_{defaultPixelColorTargets()};

    ColorLogicEngine colorLogicEngine_;
    QPointer<AdvancedLogicService> advancedLogicService_;
    struct PendingAdvancedFeedback {
        ColorLogicEffect effect;
        PixelStatistics statistics;
    };
    QHash<quint64, PendingAdvancedFeedback> pendingAdvancedFeedbacks_;
    QList<quint64> approvedAdvancedFeedbacks_;
    bool drainingAdvancedLogic_{};
    QList<ColorRule> appliedColorRules_;
    QColor colorLogicSlotColor_[2]{QColor(46, 204, 64), QColor(41, 121, 255)};
    QColor borderColor_{QColor(255, 255, 255)};
    quint64 nextAudioRequestToken_{};
    quint64 activeAudioRequestToken_{};
    quint64 activeAudioFeedbackId_{};
    bool popupRevealed_{false};
    quint64 popupRevealedFeedbackId_{};
    QTimer* flashTimer_{};
    QTimer* sourceCheckTimer_{};
    bool flashActive_{false};
    bool flashOn_{false};
    bool globalDimmed_{false};
    bool raiseOnClickEnabled_{true};
    bool wheelZoomLocked_{false};
    bool automaticActivationEnabled_{true};
    DesktopPetMode desktopPetMode_{DesktopPetMode::Resident};
    bool presentationSuppressed_{false};
    bool audioFeedbackEnabled_{true};
    int automaticActivationCooldownMs_{kDefaultAutomaticActivationCooldownMs};
    QColor flashColor_{QColor(255, 255, 255)};
};
