#pragma once
#include <QJsonObject>
#include "features/advanced_logic/advanced_logic_types.h"

namespace ConfigSerialization {
AdvancedLogicConfig parseAdvancedLogicConfig(const QJsonObject& object);
QJsonObject advancedLogicConfigToJson(const AdvancedLogicConfig& config);
}
