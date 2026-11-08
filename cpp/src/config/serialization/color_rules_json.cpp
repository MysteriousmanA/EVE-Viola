#include "config/serialization/color_rules_json.h"
#include "features/color_rule/color_rule_config.h"

namespace ConfigSerialization
{
QJsonObject colorRuleToJson(const ColorRule& rule)
{
    const auto conditionToJson = [](const ColorRuleCondition& condition) {
        if (condition.isEmpty()) {
            return QJsonObject{};
        }
        return QJsonObject{
            {QStringLiteral("pixel"), condition.pixel},
            {QStringLiteral("expression"), condition.expression},
            {QStringLiteral("tolerance"), condition.tolerance},
            {QStringLiteral("replicaId"), condition.replicaId},
        };
    };
    QJsonArray conditions;
    for (const auto& condition : rule.windowConditions) { conditions.append(conditionToJson(condition)); }
    QJsonArray switches;
    for (const auto& setting : rule.windowSwitches) {
        switches.append(QJsonObject{
            {QStringLiteral("replicaId"), setting.replicaId},
            {QStringLiteral("borderColor"), setting.borderColor.isValid()
                ? setting.borderColor.name(QColor::HexArgb) : QString{}},
            {QStringLiteral("middleClickRule"), setting.middleClickRule},
        });
    }
    return {
        {QStringLiteral("windowConditions"), conditions},
        {QStringLiteral("windowSwitches"), switches},
        {QStringLiteral("targetReplicaId"), rule.targetReplicaId},
        {QStringLiteral("enabled"), rule.enabled},
        {QStringLiteral("jumpToRule"), rule.jumpToRule},
        {QStringLiteral("number"), rule.number},
        {QStringLiteral("note"), rule.note},
        {QStringLiteral("triggerCondition1"), conditionToJson(rule.triggerCondition1)},
        {QStringLiteral("triggerCondition2"), conditionToJson(rule.triggerCondition2)},
        {QStringLiteral("triggerConditionRelation"),
         rule.triggerConditionRelation == TriggerConditionRelation::Or
             ? QStringLiteral("or") : QStringLiteral("and")},
        {QStringLiteral("triggerFeedbackType"), triggerFeedbackName(rule.triggerFeedback)},
        {QStringLiteral("triggerFeedbackAudio"), rule.triggerFeedbackAudio},
        {QStringLiteral("postTriggerAction"), postTriggerActionName(rule.postTriggerAction)},
    };
}

QList<ColorRule> parseColorRules(const QJsonArray& array, int* nextNumber)
{
    QList<ColorRule> result;
    for (const auto& value : array) {
        const auto object = value.toObject();
        QString feedbackName = object.value(QStringLiteral("triggerFeedbackType")).toString();
        if (feedbackName.isEmpty()) {
            feedbackName = object.value(QStringLiteral("triggerFeedback")).toString();
        }
        QString actionName = object.value(QStringLiteral("postTriggerAction")).toString();
        if (actionName.isEmpty()) {
            actionName = object.value(QStringLiteral("conditionLimit")).toString();
        }
        const auto parseCondition = [&object](const QString& key) {
            const auto conditionObject = object.value(key).toObject();
            return ColorRuleCondition{
                conditionObject.value(QStringLiteral("pixel")).toString(),
                conditionObject.value(QStringLiteral("expression")).toString(),
                qBound(-1, conditionObject.value(QStringLiteral("tolerance")).toInt(-1), 255),
            };
        };
        ColorRule rule;
        rule.number = object.value(QStringLiteral("number")).toInt();
        rule.enabled = object.value(QStringLiteral("enabled")).toBool(true);
        rule.targetReplicaId = object.value(QStringLiteral("targetReplicaId")).toString();
        rule.jumpToRule = object.value(QStringLiteral("jumpToRule")).toInt();
        for (const auto& value : object.value(QStringLiteral("windowConditions")).toArray()) {
            const auto c = value.toObject();
            rule.windowConditions.append({c.value(QStringLiteral("pixel")).toString(),
                c.value(QStringLiteral("expression")).toString(),
                qBound(-1, c.value(QStringLiteral("tolerance")).toInt(-1), 255),
                c.value(QStringLiteral("replicaId")).toString()});
        }
        for (const auto& value : object.value(QStringLiteral("windowSwitches")).toArray()) {
            const auto setting = value.toObject();
            const QColor color(setting.value(QStringLiteral("borderColor")).toString());
            const QString id = setting.value(QStringLiteral("replicaId")).toString();
            if (!id.isEmpty()) {
                rule.windowSwitches.append({id, color,
                    setting.value(QStringLiteral("middleClickRule")).toInt()});
            }
        }
        rule.note = object.value(QStringLiteral("note")).toString();
        rule.triggerCondition1 = parseCondition(QStringLiteral("triggerCondition1"));
        rule.triggerCondition2 = parseCondition(QStringLiteral("triggerCondition2"));
        rule.triggerConditionRelation =
            object.value(QStringLiteral("triggerConditionRelation")).toString() == QStringLiteral("or")
                ? TriggerConditionRelation::Or : TriggerConditionRelation::And;
        if (!object.contains(QStringLiteral("triggerCondition1"))) {
            rule.triggerCondition1 = {
                object.value(QStringLiteral("triggerPixel")).toString(),
                object.value(QStringLiteral("triggerCondition")).toString(),
            };
        }
        rule.triggerFeedback =
            triggerFeedbackFromName(feedbackName).value_or(TriggerFeedback::None);
        rule.triggerFeedbackAudio = object.value(QStringLiteral("triggerFeedbackAudio")).toString();
        rule.postTriggerAction =
            postTriggerActionFromName(actionName).value_or(PostTriggerAction::Loop);
        if (rule.number > 0) {
            result.append(rule);
            if (nextNumber != nullptr) {
                *nextNumber = qMax(*nextNumber, rule.number + 1);
            }
        }
    }
    return result;
}
QJsonObject colorRulesSectionToJson(const QList<ColorRule>& rules, int nextNumber,
    const QList<LogicBindingConfig>& bindings)
{
    QJsonArray values;
    for (const auto& rule : rules) {
        values.append(colorRuleToJson(rule));
    }
    QJsonArray instances;
    for (const auto& binding : bindings) {
        QJsonObject windowIds;
        for (auto it = binding.windowIds.cbegin(); it != binding.windowIds.cend(); ++it) {
            windowIds.insert(it.key(), it.value());
        }
        instances.append(QJsonObject{{QStringLiteral("id"), binding.id},
            {QStringLiteral("windowIds"), windowIds},
            {QStringLiteral("ruleNumber"), binding.ruleNumber},
            {QStringLiteral("sourceTitle"), binding.sourceTitle},
            {QStringLiteral("sourceProcessName"), binding.sourceProcessName},
            {QStringLiteral("enabled"), binding.enabled}});
    }
    return {{QStringLiteral("logicBindings"), instances},
            {QStringLiteral("colorRules"), values},
            {QStringLiteral("nextColorRuleNumber"), nextNumber}};
}
} // namespace ConfigSerialization
