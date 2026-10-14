#pragma once
#include <QObject>
#include "config/config_types.h"
class QWidget;
class RegionPresetController final : public QObject
{
    Q_OBJECT
  public:
    RegionPresetController(AppConfig &config, QWidget *host);
    void saveRegionPreset(const QRect &region, int statisticsIntervalMs,
                          bool confirmReplacementOnDuplicate);
    void renameRegionPresetAt(int row);
    void deleteRegionPresetAt(int row);
    void updateRegionPresetInterval(int row, int milliseconds);
  signals:
    void presetsChanged();
    void persistRequested();
    void replicaPersistRequested();
    void presetReplaced(const QString &previousName, const RegionPreset &preset);
    void intervalChanged(const QString &name, int milliseconds);
    void statusMessage(const QString &message, int timeout);

  private:
    AppConfig &config_;
    QWidget *host_;
};
