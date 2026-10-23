#pragma once

#include <QColor>
#include <QFrame>
#include <QPoint>
#include <QSize>

#include "config/app_config.h"
#include "platform/native_gateway.h"
#include "features/replica/replica_manager.h"

class QLabel;
class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QPushButton;
class QLineEdit;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QMouseEvent;

class ReplicaRow final : public QFrame {
    Q_OBJECT

public:
    explicit ReplicaRow(ReplicaId replicaId, QWidget* parent = nullptr);

    [[nodiscard]] ReplicaId replicaId() const noexcept;
    [[nodiscard]] int colorLogicSlotRule(int slotIndex) const noexcept;
    [[nodiscard]] bool sourceNameVisible() const noexcept;
    [[nodiscard]] bool visibilityChecked() const noexcept;
    [[nodiscard]] bool collapsed() const noexcept;
    [[nodiscard]] QString currentRegionPresetName() const noexcept;
    [[nodiscard]] QString currentRegionFilterName() const;
    [[nodiscard]] QComboBox* sourceCombo() const noexcept;
    [[nodiscard]] QList<PixelColorTarget> statisticsColors() const { return statisticsColors_; }

public slots:
    void setSourceTitle(const QString& title);
    void setSourceWindow(const WindowInfo& window);
    void setSourceOptions(const QList<WindowInfo>& windows);
    void setCaptureRegion(const QString& description);
    void setRegionPresets(const QList<RegionPreset>& presets);
    void setCurrentRegion(const QRect& region);
    void setCurrentRegionPresetName(const QString& name);
    void setWindowPosition(const QPoint& pos);
    void setWindowSize(const QSize& size);
    void setPixelStatistics(qint64 redCount, qint64 whiteCount);
    void setColorStatistics(const PixelStatistics& statistics);
    void setStatisticsColors(const QList<PixelColorTarget>& colors);
    void clearPixelStatistics();
    void setColorRules(const QList<ColorRule>& rules);
    void setColorLogicSlot(int slotIndex, int ruleNumber);
    void setColorLogicEnabled(bool enabled);
    void setActiveColorLogicSlot(int slotIndex);
    void setVisibilityChecked(bool visible);
    void setSourceNameVisible(bool visible);
    void setColorLogicSlotColor(int slotIndex, const QColor& color);
    void setStatisticsInterval(int milliseconds);
    void setCollapsed(bool collapsed);
    void setGroupPresentation(
        int memberIndex,
        int memberCount,
        bool representative,
        double estimatedCapturePixelsPerSecond,
        double estimatedScanPixelsPerSecond);
    [[nodiscard]] QColor colorLogicSlotColor(int slotIndex) const noexcept;

signals:
    void closeRequested();
    void copyRequested();
    void syncToRequested();
    void statisticsIntervalChanged(int milliseconds);
    void statisticsColorsChanged(const QList<PixelColorTarget>& colors);
    void replicaVisibilityChanged(bool visible);
    void sourceNameVisibilityChanged(bool visible);
    void saveRegionPresetRequested();
    void regionRecaptureRequested();
    void sourceSelectionRequested(const WindowInfo& window);
    void sourceOptionsRequested();
    void regionEditRequested(const QRect& region);
    void positionEditRequested(const QPoint& pos);
    void regionPresetSelected(const RegionPreset& preset);
    void collapseChanged(bool collapsed);
    void colorLogicSlotChanged(int slotIndex, int ruleNumber);
    void colorLogicEnabledChanged(bool enabled);
    void activeColorLogicSlotChanged(int slotIndex);
    void colorLogicSlotColorChanged(int slotIndex, const QColor& color);
    void scalePercentageEdited(double percentage);
    void reorderRequested(ReplicaId sourceId, ReplicaId targetId, bool insertAfter);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void initializeDragHandle();
    void initializeSourceSelection();
    void initializeRegionEditor();
    void initializeGeometryEditor();
    void initializeRegionPresetControls();
    void initializeStatisticsControls();
    void initializeVisibilityControls();
    void initializeColorLogicControls();
    void buildContentLayout();
    void initializeActionControls();
    void buildRowLayout();

    void rebuildColorLogicSlot(QComboBox* combo, int excludedNumber);
    void applyColorButton(QPushButton* button, const QColor& color);
    void syncRegionPresetCombo(const QString& preferredName = QString());
    void refreshWindowScale(bool force = false);

    ReplicaId replicaId_{};
    QLabel* badge_{};
    QWidget* sourceCell_{};
    QWidget* sourceDetails_{};
    QLabel* groupCountLabel_{};
    QWidget* contentHost_{};
    QComboBox* sourceCombo_{};
    QString sourceTitle_;
    QLineEdit* regionEdit_{};
    QString lastRegionText_{QStringLiteral("未截取")};
    QLineEdit* positionEdit_{};
    QPoint lastPosition_{};
    QLineEdit* sizeEdit_{};
    QDoubleSpinBox* scaleEdit_{};
    bool scaleEditHasUserInput_{false};
    QSize currentWindowSize_{};
    QPoint dragStartPosition_{};
    QComboBox* regionPresetCombo_{};
    QList<RegionPreset> regionPresets_;
    QString currentRegionPresetName_;
    QRect currentRegion_;
    QPushButton* closeButton_{};
    QPushButton* copyButton_{};
    QPushButton* collapseButton_{};
    QPushButton* syncToButton_{};
    QLabel* performanceEstimateLabel_{};
    QWidget* syncActionRow_{};
    bool collapsed_{false};
    QWidget* pixelStatisticsHost_{};
    QPushButton* statisticsColorsButton_{};
    QList<QLabel*> pixelStatisticsLabels_;
    QList<PixelColorTarget> statisticsColors_{defaultPixelColorTargets()};
    QComboBox* statisticsInterval_{};
    QComboBox* colorLogicSlotA_{};
    QComboBox* colorLogicSlotB_{};
    QCheckBox* colorLogicEnabledCheck_{};
    QComboBox* activeSlotCombo_{};
    QPushButton* colorLogicSlotColorA_{};
    QPushButton* colorLogicSlotColorB_{};
    QColor slotColorA_{QColor(46, 204, 64)};
    QColor slotColorB_{QColor(41, 121, 255)};
    QCheckBox* visibilityCheckBox_{};
    QCheckBox* sourceNameVisibilityCheckBox_{};
    QPushButton* savePresetButton_{};
    QPushButton* regionRecaptureButton_{};
    QList<ColorRule> colorRules_;
};
