#include "config/serialization/replicas_json.h"
#include "config/config_validation.h"
#include "config/serialization/application_options_json.h"
#include "config/serialization/quick_switch_json.h"

namespace ConfigSerialization
{
QJsonObject replicaToJson(const ReplicaConfig& replica)
{
    QJsonObject region;
    if (replica.capturedRegion.isValid() && !replica.capturedRegion.isEmpty()) {
        region = {
            {QStringLiteral("x"), replica.capturedRegion.x()},
            {QStringLiteral("y"), replica.capturedRegion.y()},
            {QStringLiteral("width"), replica.capturedRegion.width()},
            {QStringLiteral("height"), replica.capturedRegion.height()},
        };
    }
    const QJsonArray slotRules{
        replica.colorLogicSlotRule[0],
        replica.colorLogicSlotRule[1],
    };
    return {
        {QStringLiteral("sourceTitle"), replica.sourceTitle},
        {QStringLiteral("capturedRegion"), region},
        {QStringLiteral("statisticsIntervalMs"), replica.statisticsIntervalMs},
        {QStringLiteral("colorLogicSlotRules"), slotRules},
        {QStringLiteral("colorLogicEnabled"), replica.colorLogicEnabled},
        {QStringLiteral("activeColorLogicSlot"), replica.activeColorLogicSlot},
        {QStringLiteral("sourceNameVisible"), replica.sourceNameVisible},
        {QStringLiteral("visible"), replica.visible},
        {QStringLiteral("collapsed"), replica.collapsed},
        {QStringLiteral("positionX"), replica.position.x()},
        {QStringLiteral("positionY"), replica.position.y()},
        {QStringLiteral("width"), replica.size.width()},
        {QStringLiteral("height"), replica.size.height()},
        {QStringLiteral("slotColor1"), replica.slotColor1.name()},
        {QStringLiteral("slotColor2"), replica.slotColor2.name()},
    };
}

QList<ReplicaConfig> parseReplicas(const QJsonArray& array)
{
    QList<ReplicaConfig> result;
    for (const auto& value : array) {
        const auto object = value.toObject();
        int statisticsIntervalMs = object.value(QStringLiteral("statisticsIntervalMs"))
                                       .toInt(kDefaultStatisticsIntervalMs);
        if (!isValidStatisticsIntervalMs(statisticsIntervalMs)) {
            statisticsIntervalMs = kDefaultStatisticsIntervalMs;
        }
        const auto regionObject = object.value(QStringLiteral("capturedRegion")).toObject();
        const QRect region(regionObject.value(QStringLiteral("x")).toInt(),
                           regionObject.value(QStringLiteral("y")).toInt(),
                           regionObject.value(QStringLiteral("width")).toInt(),
                           regionObject.value(QStringLiteral("height")).toInt());
        const auto slotRules = object.value(QStringLiteral("colorLogicSlotRules")).toArray();
        ReplicaConfig replica;
        replica.sourceTitle = object.value(QStringLiteral("sourceTitle")).toString();
        if (region.isValid() && !region.isEmpty()) {
            replica.capturedRegion = region;
        }
        replica.statisticsIntervalMs = statisticsIntervalMs;
        replica.colorLogicSlotRule[0] = slotRules.size() > 0 ? slotRules[0].toInt() : 0;
        replica.colorLogicSlotRule[1] = slotRules.size() > 1 ? slotRules[1].toInt() : 0;
        replica.colorLogicEnabled = object.value(QStringLiteral("colorLogicEnabled")).toBool();
        replica.activeColorLogicSlot =
            qBound(1, object.value(QStringLiteral("activeColorLogicSlot")).toInt(1), 2);
        replica.sourceNameVisible = object.value(QStringLiteral("sourceNameVisible")).toBool();
        replica.visible = object.value(QStringLiteral("visible")).toBool(true);
        replica.collapsed = object.value(QStringLiteral("collapsed")).toBool(false);
        replica.position = QPoint(object.value(QStringLiteral("positionX")).toInt(),
                                  object.value(QStringLiteral("positionY")).toInt());
        const QSize size(object.value(QStringLiteral("width")).toInt(360),
                         object.value(QStringLiteral("height")).toInt(240));
        if (size.isValid() && !size.isEmpty()) {
            replica.size = size;
        }
        replica.slotColor1 = QColor(object.value(QStringLiteral("slotColor1")).toString());
        if (!replica.slotColor1.isValid()) {
            replica.slotColor1 = QColor(46, 204, 64);
        }
        replica.slotColor2 = QColor(object.value(QStringLiteral("slotColor2")).toString());
        if (!replica.slotColor2.isValid()) {
            replica.slotColor2 = QColor(41, 121, 255);
        }
        result.append(std::move(replica));
    }
    return result;
}
QJsonObject replicasSectionToJson(const AppConfig& config)
{
    // Global options and hotkeys remain in replicas.json for format compatibility.
    QJsonArray values;
    for (const auto& replica : config.replicas) {
        values.append(replicaToJson(replica));
    }
    QJsonObject section{{QStringLiteral("replicas"), values}};
    const auto options = applicationOptionsToJson(config);
    for (auto it = options.begin(); it != options.end(); ++it) {
        section.insert(it.key(), it.value());
    }
    const auto keys = quickSwitchConfigToJson(config);
    for (auto it = keys.begin(); it != keys.end(); ++it) {
        section.insert(it.key(), it.value());
    }
    return section;
}
} // namespace ConfigSerialization
