#include "features/region_preset/region_preset_config.h"

bool addRegionPreset(AppConfig& config, RegionPreset preset)
{
    preset.name = preset.name.trimmed();
    if (preset.name.isEmpty() || preset.region.width() <= 0 || preset.region.height() <= 0) {
        return false;
    }
    for (const auto& current : config.regionPresets) {
        if (current.name.compare(preset.name, Qt::CaseInsensitive) == 0) {
            return false;
        }
    }
    config.regionPresets.append(std::move(preset));
    return true;
}

bool renameRegionPreset(AppConfig& config, int index, const QString& name)
{
    const auto trimmed = name.trimmed();
    if (index < 0 || index >= config.regionPresets.size() || trimmed.isEmpty()) {
        return false;
    }
    for (int current = 0; current < config.regionPresets.size(); ++current) {
        if (current != index &&
            config.regionPresets[current].name.compare(trimmed, Qt::CaseInsensitive) == 0) {
            return false;
        }
    }
    config.regionPresets[index].name = trimmed;
    return true;
}

bool removeRegionPreset(AppConfig& config, int index)
{
    if (index < 0 || index >= config.regionPresets.size()) {
        return false;
    }
    config.regionPresets.removeAt(index);
    return true;
}
