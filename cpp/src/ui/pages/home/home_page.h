#pragma once
#include <QWidget>
#include "config/config_types.h"
class QLabel;
class QButtonGroup;
class QRadioButton;
class QCheckBox;
class QDoubleSpinBox;
class QVBoxLayout;
class HomePage final : public QWidget
{
    Q_OBJECT
  public:
    explicit HomePage(QWidget *parent = nullptr);
    void setConfig(const AppConfig &config);
    void setPresets(const AppConfig &config);
    QLabel *countLabel() const noexcept { return countLabel_; }
    void setReplicaCount(int count);
  signals:
    void createRequested();
    void closeAllRequested();
    void activateSourceChanged(bool checked);
    void keepInTrayChanged(bool checked);
    void restoreWithGameChanged(bool checked);
    void raiseOnClickChanged(bool checked);
    void cooldownChanged(double seconds);
    void mouseQuietPeriodChanged(double seconds);
    void renamePresetRequested(int slot);
    void savePresetRequested(int slot);
    void loadPresetRequested(int slot);
    void deletePresetRequested(int slot);
    void defaultPresetRequested(int slot);

  private:
    void buildPresetBar(int slot, QVBoxLayout *parent);
    QLabel *countLabel_{};
    QButtonGroup *presetButtonGroup_{};
    QRadioButton *presetRadio_[kPresetCount]{};
    QLabel *presetNameLabel_[kPresetCount]{};
    QCheckBox *keepInTrayCheckBox_{};
    QCheckBox *restoreWindowsWithGameCheckBox_{};
    QCheckBox *raiseReplicaOnClickCheckBox_{};
    QCheckBox *activateSourceOnColorLogicTriggerCheckBox_{};
    QDoubleSpinBox *automaticActivationCooldownSpinBox_{};
    QDoubleSpinBox *automaticActivationMouseQuietPeriodSpinBox_{};
};
