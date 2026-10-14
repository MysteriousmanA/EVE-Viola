#pragma once

#include "config/config_types.h"
#include <QJsonArray>
#include <QJsonObject>

namespace ConfigSerialization
{
QJsonObject regionPresetToJson(const RegionPreset& preset);
[[nodiscard]] QList<RegionPreset> parseRegionPresets(const QJsonArray& array);
QJsonObject regionPresetsSectionToJson(const QList<RegionPreset>& presets);
} // namespace ConfigSerialization
