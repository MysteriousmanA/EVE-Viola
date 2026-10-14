#pragma once

#include <QList>
#include <QPointer>
#include <QString>
#include <QWidget>

#include "config/app_config.h"
#include "features/page_partition/page_partition_model.h"

class QComboBox;
class QCheckBox;
class QLabel;
class QPushButton;
class QScreen;
class QSpinBox;
class QTableWidget;

class PagePartitionPage final : public QWidget {
    Q_OBJECT

public:
    explicit PagePartitionPage(
        PagePartitionConfig config = {},
        QWidget* parent = nullptr);
    ~PagePartitionPage() override;

    [[nodiscard]] const PagePartitionConfig& partitionConfig() const noexcept;
    void setPartitionConfig(const PagePartitionConfig& config);
    void setEveClientPresent(bool present);
    void setCaptureOverlaySuppressed(bool suppressed);
    void setFishModeSuppressed(bool suppressed);

signals:
    void configurationChanged();

private:
    void addLine();
    void redrawSelectedLine();
    void deleteSelectedLine();
    void clearAllLines();
    void undoLastChange();
    void toggleVisibility();
    void clearSelection();
    void chooseDefaultColor();
    void chooseSelectedColor();
    void editCell(int row, int column);
    void moveLine(int sourceRow, int targetRow);
    void setSelectedLineFromRow(int row);
    void refreshScreenChoices();
    void commitLineChange(const QString& status);
    void commitSettingsChange(const QString& status);
    void refreshTable();
    void refreshOverlays();
    void refreshButtons();
    void refreshColorButtons();
    [[nodiscard]] QScreen* selectedScreen() const noexcept;
    [[nodiscard]] int selectedLineIndex() const;

    PagePartitionModel model_;
    QList<QPointer<QScreen>> screenChoices_;
    QList<QWidget*> overlays_;
    QWidget* drawOverlay_{};
    QTableWidget* table_{};
    QComboBox* screenCombo_{};
    QCheckBox* onlyShowWhenEveDetectedCheck_{};
    QPushButton* addButton_{};
    QSpinBox* widthSpin_{};
    QPushButton* defaultColorButton_{};
    QPushButton* selectedColorButton_{};
    QPushButton* visibilityButton_{};
    QPushButton* redrawButton_{};
    QPushButton* deleteButton_{};
    QPushButton* undoButton_{};
    QLabel* statusLabel_{};
    QString selectedLineId_;
    bool refreshing_{false};
    bool eveClientPresent_{false};
    bool captureOverlaySuppressed_{false};
    bool fishModeSuppressed_{false};
    bool drawOverlayWasVisibleBeforeSuppression_{false};
};
