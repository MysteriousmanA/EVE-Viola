#include "config/serialization/advanced_logic_json.h"
#include <QJsonArray>
#include <QUuid>

namespace ConfigSerialization {
AdvancedLogicConfig parseAdvancedLogicConfig(const QJsonObject& object)
{
    AdvancedLogicConfig config;
    config.enabled = object.value(QStringLiteral("enabled")).toBool(false);
    for (const auto value : object.value(QStringLiteral("sequences")).toArray()) {
        const auto json = value.toObject();
        WhitelistSequence sequence;
        sequence.id = json.value(QStringLiteral("id")).toString();
        if (sequence.id.isEmpty()) { sequence.id = QUuid::createUuid().toString(QUuid::WithoutBraces); }
        sequence.name = json.value(QStringLiteral("name")).toString();
        sequence.enabled = json.value(QStringLiteral("enabled")).toBool(true);
        sequence.timeoutMs = qBound(100, json.value(QStringLiteral("timeoutMs")).toInt(30000), 3600000);
        sequence.sameSource = json.value(QStringLiteral("sameSource")).toBool(true);
        sequence.ignoreUnrelated = json.value(QStringLiteral("ignoreUnrelated")).toBool(true);
        for (const auto nodeValue : json.value(QStringLiteral("nodes")).toArray()) {
            const auto nodeJson = nodeValue.toObject();
            WhitelistNode node;
            node.ruleNumber = nodeJson.value(QStringLiteral("ruleNumber")).toInt();
            node.effect = nodeJson.value(QStringLiteral("effect")).toString() == QStringLiteral("suppress")
                ? WhitelistNodeEffect::Suppress : WhitelistNodeEffect::Keep;
            node.sourceTitle = nodeJson.value(QStringLiteral("sourceTitle")).toString().trimmed();
            // Preserve missing references so the editor can explain and repair them.
            sequence.nodes.append(std::move(node));
        }
        config.sequences.append(std::move(sequence));
    }
    return config;
}

QJsonObject advancedLogicConfigToJson(const AdvancedLogicConfig& config)
{
    QJsonArray sequences;
    for (const auto& sequence : config.sequences) {
        QJsonArray nodes;
        for (const auto& node : sequence.nodes) {
            nodes.append(QJsonObject{
                {QStringLiteral("ruleNumber"), node.ruleNumber},
                {QStringLiteral("effect"), node.effect == WhitelistNodeEffect::Suppress
                    ? QStringLiteral("suppress") : QStringLiteral("keep")},
                {QStringLiteral("sourceTitle"), node.sourceTitle}});
        }
        sequences.append(QJsonObject{
            {QStringLiteral("id"), sequence.id}, {QStringLiteral("name"), sequence.name},
            {QStringLiteral("enabled"), sequence.enabled}, {QStringLiteral("timeoutMs"), sequence.timeoutMs},
            {QStringLiteral("sameSource"), sequence.sameSource},
            {QStringLiteral("ignoreUnrelated"), sequence.ignoreUnrelated}, {QStringLiteral("nodes"), nodes}});
    }
    return {{QStringLiteral("enabled"), config.enabled}, {QStringLiteral("sequences"), sequences}};
}
}
