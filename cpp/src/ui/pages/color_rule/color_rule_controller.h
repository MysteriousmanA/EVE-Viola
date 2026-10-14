#pragma once
#include <QObject>
#include "config/config_types.h"
class QWidget;
class ColorRuleController final : public QObject
{
    Q_OBJECT
  public:
    ColorRuleController(AppConfig &config, QWidget *host);
    void createColorRule();
    void editColorRuleNumber(int number);
    void deleteColorRuleNumber(int number);
  signals:
    void rulesChanged();
    void persistRequested();

  private:
    AppConfig &config_;
    QWidget *host_;
};
