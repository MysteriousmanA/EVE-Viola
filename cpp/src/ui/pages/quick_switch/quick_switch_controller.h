#pragma once
#include <QObject>
#include "config/config_types.h"
class QuickSwitchPage;
class QuickSwitchService;
class WindowCatalog;
class AccountSwitchController;
class QuickSwitchController final : public QObject
{
    Q_OBJECT
  public:
    QuickSwitchController(QuickSwitchPage &page, AppConfig &config, QuickSwitchService &service,
                          WindowCatalog &catalog, AccountSwitchController &accounts);
    void refresh();
  signals:
    void persistRequested();

  private:
    QuickSwitchPage *quickSwitchPage_;
    AppConfig &config_;
    QuickSwitchService *quickSwitchService_;
    WindowCatalog *windowCatalog_;
    AccountSwitchController *accountSwitchController_;
};
