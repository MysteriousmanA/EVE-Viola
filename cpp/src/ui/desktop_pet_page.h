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
#include "features/desktop_pet/desktop_pet_mode.h"

class QLabel;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;
class QPushButton;
class QSlider;
class QTableWidget;
class QTimer;
class QVBoxLayout;
class AvatarCropWidget;
class AvatarPreviewWidget;
class FloatingPetWindow;
class AstrBotNotifier;
struct AstrBotSettings;

class DesktopPetPage final : public QWidget {
    Q_OBJECT

public:
    explicit DesktopPetPage(QString settingsDirectory, QWidget* parent = nullptr);
    ~DesktopPetPage() override;

    void setColorRules(const QList<ColorRule>& rules);
    void showRuleMessage(int ruleNumber, const QString& sourceWindowTitle);
    DesktopPetMode mode() const { return mode_; }

public slots:
    void setMode(DesktopPetMode mode);

signals:
    void modeChanged(DesktopPetMode mode);
    void stopSoftwareRequested();

private:
    [[nodiscard]] std::pair<QLabel*, QLabel*> buildLayout();
    void initializeMessagePersistence();
    void connectEditorSignals(QLabel* zoomValue, QLabel* sizeValue);
    void connectFloatingPetSignals();
    void restoreEnabledPet();
    void buildNotificationSettings(QVBoxLayout* layout, QWidget* parent);
    void connectNotificationSignals();
    void updateNotificationMode();
    AstrBotSettings astrBotSettings() const;

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
    AstrBotNotifier* astrBotNotifier_{};
    QCheckBox* localMessagesEnabled_{};
    QComboBox* modeCombo_{};
    QLabel* modeDescription_{};
    QCheckBox* astrBotEnabled_{};
    QLineEdit* astrBotServerUrl_{};
    QLineEdit* astrBotApiKey_{};
    QLineEdit* astrBotUmo_{};
    QSpinBox* astrBotCooldown_{};
    QPushButton* astrBotTestButton_{};
    QLabel* astrBotStatus_{};
    QList<ColorRule> colorRules_;
    QHash<int, QString> ruleMessages_;
    QStringList activeTriggerMessages_;
    QHash<QString, QStringList> activeTriggerAccounts_;
    bool petPositionInitialized_{false};
    bool petEnabled_{false};
    bool settingsWritable_{true};
    DesktopPetMode mode_{DesktopPetMode::Resident};
};
