#pragma once
#include <QObject>
#include <functional>
#include <QHash>
#include "config/config_types.h"
class QWidget;
class ColorRuleController final : public QObject
{
    Q_OBJECT
  public:
    ColorRuleController(AppConfig &config, QWidget *host);
    void setReplicaProvider(std::function<QList<ReplicaConfig>()> provider) { replicaProvider_ = std::move(provider); }
    void setRegionPresetNameProvider(std::function<QHash<QString, QString>()> provider) { presetNameProvider_ = std::move(provider); }
    void createColorRule();
    void editColorRuleNumber(int number);
    void deleteColorRuleNumber(int number);
  signals:
    void rulesChanged();
    void persistRequested();

  private:
    AppConfig &config_;
    QWidget *host_;
    std::function<QList<ReplicaConfig>()> replicaProvider_;
    std::function<QHash<QString, QString>()> presetNameProvider_;
};
