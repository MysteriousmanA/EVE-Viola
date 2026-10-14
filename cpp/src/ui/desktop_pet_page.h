#pragma once

#include <utility>

#include <QHash>
#include <QImage>
#include <QList>
#include <QPoint>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QWidget>

#include "config/app_config.h"
#include "features/desktop_pet/desktop_pet_settings_store.h"

class QLabel;
class QPushButton;
class QSlider;
class QTableWidget;
class QTimer;
class AvatarCropWidget;
class AvatarPreviewWidget;
class FloatingPetWindow;

class DesktopPetPage final : public QWidget {
    Q_OBJECT

public:
    explicit DesktopPetPage(QString settingsDirectory, QWidget* parent = nullptr);
    ~DesktopPetPage() override;

    void setColorRules(const QList<ColorRule>& rules);
    void showRuleMessage(int ruleNumber, const QString& sourceWindowTitle);

signals:
    void accountWheelRequested(const QPoint& petCenter, int avatarSize);
    void accountWheelGeometryChanged(const QPoint& petCenter, int avatarSize);
    void accountWheelDismissRequested();
    void fishModeToggled(bool enabled);

private:
    [[nodiscard]] std::pair<QLabel*, QLabel*> buildLayout();
    void initializeMessagePersistence();
    void connectEditorSignals(QLabel* zoomValue, QLabel* sizeValue);
    void connectFloatingPetSignals();
    void restoreEnabledPet();

    void chooseImage();
    void refreshPreview();
    void togglePet();
    void refreshPet();
    void updatePetButton();
    void loadSettings();
    void saveSettings() const;
    void setDefaultPetPosition();

    DesktopPetSettingsStore settingsStore_;
    QImage sourceImage_;
    AvatarCropWidget* cropWidget_{};
    AvatarPreviewWidget* previewWidget_{};
    FloatingPetWindow* petWindow_{};
    QLabel* imageNameLabel_{};
    QLabel* statusLabel_{};
    QPushButton* uploadButton_{};
    QPushButton* petButton_{};
    QSlider* zoomSlider_{};
    QSlider* sizeSlider_{};
    QTableWidget* rulesTable_{};
    QLabel* rulesEmptyState_{};
    QTimer* messagePersistTimer_{};
    QList<ColorRule> colorRules_;
    QHash<int, QString> ruleMessages_;
    QStringList activeTriggerMessages_;
    QHash<QString, QStringList> activeTriggerAccounts_;
    bool petPositionInitialized_{false};
    bool petEnabled_{false};
    bool settingsWritable_{true};
    bool fishModeEnabled_{false};
};
