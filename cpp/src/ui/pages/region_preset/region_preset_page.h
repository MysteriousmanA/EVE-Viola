#pragma once

#include <array>

#include <QWidget>

#include "config/config_types.h"

class QPushButton;
class QComboBox;
class QCheckBox;
class QHBoxLayout;
class QLabel;
class QTableWidget;

class RegionPresetPage final : public QWidget
{
    Q_OBJECT

  public:
    explicit RegionPresetPage(QWidget *parent = nullptr);
    void setPresets(const QList<RegionPreset> &presets);

  signals:
    void captureRequested();
    void saveRequested();
    void renameRequested(int row);
    void deleteRequested(int row);
    void statisticsIntervalChanged(int row, int milliseconds);

  private:
    QTableWidget *table_{};
};
