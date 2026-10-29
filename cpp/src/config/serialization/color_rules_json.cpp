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
        };
    };
    return {
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
        {QStringLiteral("priority"), static_cast<int>(rule.priority)},
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
        rule.priority = static_cast<ColorRulePriority>(qBound(0,
            object.value(QStringLiteral("priority")).toInt(), 2));
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
QJsonObject colorRulesSectionToJson(const QList<ColorRule>& rules, int nextNumber)
{
    QJsonArray values;
    for (const auto& rule : rules) {
        values.append(colorRuleToJson(rule));
    }
    return {{QStringLiteral("colorRules"), values},
            {QStringLiteral("nextColorRuleNumber"), nextNumber}};
}
} // namespace ConfigSerialization
