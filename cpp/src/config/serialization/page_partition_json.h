#pragma once

#include "config/config_types.h"
#include <QJsonArray>
#include <QJsonObject>

namespace ConfigSerialization
{
PagePartitionConfig parsePagePartitionConfig(const QJsonObject& object);
QJsonObject pagePartitionConfigToJson(const PagePartitionConfig& config);
} // namespace ConfigSerialization
