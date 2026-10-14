#include "config/serialization/region_presets_json.h"
#include "config/config_validation.h"

namespace ConfigSerialization
{
QJsonObject regionPresetToJson(const RegionPreset& preset)
{
    return {
        {QStringLiteral("name"), preset.name},
        {QStringLiteral("x"), preset.region.x()},
        {QStringLiteral("y"), preset.region.y()},
        {QStringLiteral("width"), preset.region.width()},
        {QStringLiteral("height"), preset.region.height()},
        {QStringLiteral("statisticsIntervalMs"), preset.statisticsIntervalMs},
    };
}

QList<RegionPreset> parseRegionPresets(const QJsonArray& array)
{
    QList<RegionPreset> result;
    for (const auto& value : array) {
        const auto object = value.toObject();
        int statisticsIntervalMs = object.value(QStringLiteral("statisticsIntervalMs"))
                                       .toInt(kDefaultStatisticsIntervalMs);
        if (!isValidStatisticsIntervalMs(statisticsIntervalMs)) {
            statisticsIntervalMs = kDefaultStatisticsIntervalMs;
        }
        const RegionPreset preset{
            object.value(QStringLiteral("name")).toString().trimmed(),
            QRect(object.value(QStringLiteral("x")).toInt(),
                  object.value(QStringLiteral("y")).toInt(),
                  object.value(QStringLiteral("width")).toInt(),
                  object.value(QStringLiteral("height")).toInt()),
            statisticsIntervalMs,
        };
        if (!preset.name.isEmpty() && preset.region.width() > 0 && preset.region.height() > 0) {
            result.append(preset);
        }
    }
    return result;
}
QJsonObject regionPresetsSectionToJson(const QList<RegionPreset>& presets)
{
    QJsonArray values;
    for (const auto& preset : presets) {
        values.append(regionPresetToJson(preset));
    }
    return {{QStringLiteral("regionPresets"), values}};
}
} // namespace ConfigSerialization
