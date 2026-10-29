#include "features/color_rule/color_rule_config.h"

QString triggerConditionRelationName(TriggerConditionRelation relation)
{
    return relation == TriggerConditionRelation::Or
        ? QStringLiteral("或（任一满足）") : QStringLiteral("与（同时满足）");
}

QString triggerFeedbackName(TriggerFeedback feedback)
{
    switch (feedback) {
    case TriggerFeedback::None:
        return QStringLiteral("无");
    case TriggerFeedback::Popup:
        return QStringLiteral("弹窗");
    case TriggerFeedback::Audio:
        return QStringLiteral("播放音频");
    }
    return {};
}

std::optional<TriggerFeedback> triggerFeedbackFromName(const QString& name)
{
    if (name.compare(QStringLiteral("弹窗"), Qt::CaseInsensitive) == 0) {
        return TriggerFeedback::Popup;
    }
    if (name.compare(QStringLiteral("播放音频"), Qt::CaseInsensitive) == 0) {
        return TriggerFeedback::Audio;
    }
    if (name.compare(QStringLiteral("无"), Qt::CaseInsensitive) == 0) {
        return TriggerFeedback::None;
    }
    return std::nullopt;
}

QString postTriggerActionName(PostTriggerAction action)
{
    switch (action) {
    case PostTriggerAction::Loop:
        return QStringLiteral("正向循环");
    case PostTriggerAction::Jump:
        return QStringLiteral("跳转至另一状态");
    case PostTriggerAction::ReverseWait:
        return QStringLiteral("反向等待");
    }
    return {};
}

std::optional<PostTriggerAction> postTriggerActionFromName(const QString& name)
{
    if (name.compare(QStringLiteral("跳转至另一状态"), Qt::CaseInsensitive) == 0 ||
        name.compare(QStringLiteral("跳转至状态"), Qt::CaseInsensitive) == 0) {
        return PostTriggerAction::Jump;
    }
    if (name.compare(QStringLiteral("正向循环"), Qt::CaseInsensitive) == 0 ||
        name.compare(QStringLiteral("恢复原状态"), Qt::CaseInsensitive) == 0) {
        return PostTriggerAction::Loop;
    }
    if (name.compare(QStringLiteral("反向等待"), Qt::CaseInsensitive) == 0) {
        return PostTriggerAction::ReverseWait;
    }
    return std::nullopt;
}

int addColorRule(AppConfig& config, ColorRule rule)
{
    int nextNumber = qMax(1, config.nextColorRuleNumber);
    for (const auto& current : config.colorRules) {
        nextNumber = qMax(nextNumber, current.number + 1);
    }
    rule.number = nextNumber;
    config.colorRules.append(std::move(rule));
    config.nextColorRuleNumber = nextNumber + 1;
    return nextNumber;
}

bool updateColorRule(AppConfig& config, int number, ColorRule rule)
{
    for (auto& current : config.colorRules) {
        if (current.number == number) {
            rule.number = number;
            current = std::move(rule);
            return true;
        }
    }
    return false;
}

bool removeColorRule(AppConfig& config, int number)
{
    for (int index = 0; index < config.colorRules.size(); ++index) {
        if (config.colorRules[index].number == number) {
            config.colorRules.removeAt(index);
            return true;
        }
    }
    return false;
}
