#pragma once

#include "config/config_types.h"
#include <QJsonArray>
#include <QJsonObject>

namespace ConfigSerialization
{
QJsonObject presetToJson(const PresetConfig& preset);
PresetConfig parsePresetObject(const QJsonObject& object);
} // namespace ConfigSerialization
