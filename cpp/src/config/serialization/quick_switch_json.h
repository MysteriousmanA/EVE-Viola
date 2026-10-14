#pragma once

#include "config/config_types.h"
#include <QJsonArray>
#include <QJsonObject>

namespace ConfigSerialization
{
[[nodiscard]] QList<RadialAccountConfig> parseRadialAccounts(const QJsonArray& array);
QJsonArray radialAccountsToJson(const QList<RadialAccountConfig>& configs);
void parseQuickSwitchConfig(const QJsonObject& object, AppConfig& config);
QJsonObject quickSwitchConfigToJson(const AppConfig& config);
} // namespace ConfigSerialization
