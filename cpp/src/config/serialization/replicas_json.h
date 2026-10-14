#pragma once

#include "config/config_types.h"
#include <QJsonArray>
#include <QJsonObject>

namespace ConfigSerialization
{
QJsonObject replicaToJson(const ReplicaConfig& replica);
[[nodiscard]] QList<ReplicaConfig> parseReplicas(const QJsonArray& array);
QJsonObject replicasSectionToJson(const AppConfig& config);
} // namespace ConfigSerialization
