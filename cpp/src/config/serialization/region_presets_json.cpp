#include "config/serialization/region_presets_json.h"
#include "config/config_validation.h"

namespace ConfigSerialization
{
QJsonArray colorTolerancesToJson(const QList<PixelColorTarget>& targets)
{
    QJsonArray array;
    for (const auto& target : targets) {
        array.append(QJsonObject{{QStringLiteral("color"), target.color},
                                 {QStringLiteral("tolerance"), target.tolerance}});
    }
    return array;
}

QList<PixelColorTarget> parseColorTolerances(const QJsonArray& array)
{
    QList<PixelColorTarget> targets;
    for (const auto& value : array) {
        const auto object = value.toObject();
        const auto color = object.value(QStringLiteral("color")).toString();
        if (pixelColorValue(color).isValid()) {
            targets.append({color, object.value(QStringLiteral("tolerance")).toInt(-1)});
        }
    }
    return targets.isEmpty() ? targets : normalizedPixelColorTargets(targets);
}

QJsonObject regionPresetToJson(const RegionPreset& preset)
{
    return {
        {QStringLiteral("name"), preset.name},
        {QStringLiteral("x"), preset.region.x()},
        {QStringLiteral("y"), preset.region.y()},
        {QStringLiteral("width"), preset.region.width()},
        {QStringLiteral("height"), preset.region.height()},
        {QStringLiteral("statisticsIntervalMs"), preset.statisticsIntervalMs},
        {QStringLiteral("colorTolerances"), colorTolerancesToJson(preset.colorTolerances)},
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
            parseColorTolerances(object.value(QStringLiteral("colorTolerances")).toArray()),
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
