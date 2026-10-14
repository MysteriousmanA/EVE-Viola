#pragma once

#include "config/config_types.h"

[[nodiscard]] bool addRegionPreset(AppConfig& config, RegionPreset preset);
[[nodiscard]] bool renameRegionPreset(AppConfig& config, int index, const QString& name);
[[nodiscard]] bool removeRegionPreset(AppConfig& config, int index);
