#pragma once
#include "config/config_types.h"

struct LogicBindingPlan {
    QList<ColorRule> rules;
    QList<ReplicaConfig> windows;
    QString error;
};
LogicBindingPlan planLogicBinding(const LogicBindingConfig& binding,
    const QList<ColorRule>& definitions, const QList<ReplicaConfig>& templates, int firstRuntimeNumber);
void migrateLogicBindings(AppConfig& config);
QString logicRegionPresetId(const QString& name);
QList<ReplicaConfig> logicRegionTemplates(const QList<RegionPreset>& presets);
void migrateLogicRegionReferences(AppConfig& config);
