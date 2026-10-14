#include "config/serialization/presets_json.h"
#include "config/serialization/replicas_json.h"

namespace ConfigSerialization
{
QJsonObject presetToJson(const PresetConfig& preset)
{
    QJsonArray replicas;
    for (const auto& replica : preset.replicas) {
        replicas.append(replicaToJson(replica));
    }
    return {
        {QStringLiteral("name"), preset.name},
        {QStringLiteral("hasConfig"), preset.hasConfig},
        {QStringLiteral("replicas"), replicas},
    };
}

PresetConfig parsePresetObject(const QJsonObject& object)
{
    PresetConfig preset;
    preset.name = object.value(QStringLiteral("name")).toString();
    preset.hasConfig = object.value(QStringLiteral("hasConfig")).toBool(false);
    preset.replicas = parseReplicas(object.value(QStringLiteral("replicas")).toArray());
    return preset;
}
} // namespace ConfigSerialization
