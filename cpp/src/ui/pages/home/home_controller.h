#pragma once
#include <QObject>
#include "config/config_types.h"
class QWidget;
class HomePage;
class HomeController final : public QObject
{
    Q_OBJECT
  public:
    HomeController(HomePage &page, AppConfig &config, QWidget *host);
    void setReplicaCount(int count) { replicaCount_ = count; }
  signals:
    void snapshotRequested();
    void rebuildRequested();
    void persistRequested();
    void closeAllRequested();
    void activateSourceChanged(bool checked);
    void keepInTrayChanged(bool checked);
    void restoreWithGameChanged(bool checked);
    void raiseOnClickChanged(bool checked);
    void cooldownChanged(int milliseconds);
    void mouseQuietPeriodChanged(int milliseconds);

  private:
    void savePresetTo(int slot);
    void renamePresetAt(int slot);
    void deletePresetAt(int slot);
    void setDefaultPresetSlot(int slot);
    void loadPreset(int slot);
    HomePage &page_;
    AppConfig &config_;
    QWidget *host_;
    int replicaCount_{};
};
