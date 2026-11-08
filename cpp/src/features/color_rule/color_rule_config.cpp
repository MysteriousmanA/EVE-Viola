#include "features/color_rule/color_rule_config.h"
#include <QUuid>
#include <QSet>

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
        return QStringLiteral("切换至另一状态");
    case PostTriggerAction::ReverseWait:
        return QStringLiteral("反向等待");
    }
    return {};
}

std::optional<PostTriggerAction> postTriggerActionFromName(const QString& name)
{
    if (name.compare(QStringLiteral("切换至另一状态"), Qt::CaseInsensitive) == 0 ||
        name.compare(QStringLiteral("跳转至另一状态"), Qt::CaseInsensitive) == 0 ||
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

void migrateWindowColorLogic(AppConfig& config)
{
    const auto originalRules = config.colorRules;
    QSet<int> claimed;
    const auto migrate = [&](QList<ReplicaConfig>& replicas) {
        QSet<QString> ids;
        for (auto& replica : replicas) {
            if (replica.id.isEmpty() || ids.contains(replica.id)) {
                replica.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            }
            ids.insert(replica.id);
            int migrated[2]{};
            for (int slot = 0; slot < 2; ++slot) {
                for (const auto& original : originalRules) {
                    if (original.number != replica.colorLogicSlotRule[slot]) { continue; }
                    ColorRule rule = original;
                    rule.windowConditions.clear();
                    for (auto condition : {original.triggerCondition1, original.triggerCondition2}) {
                        if (!condition.isEmpty()) {
                            condition.replicaId = replica.id;
                            rule.windowConditions.append(condition);
                        }
                    }
                    rule.targetReplicaId = replica.id;
                    rule.enabled = replica.colorLogicEnabled && replica.activeColorLogicSlot == slot + 1;
                    rule.note += QStringLiteral(" · %1 / %2").arg(replica.sourceTitle).arg(slot + 1);
                    if (!claimed.contains(original.number)) {
                        static_cast<void>(updateColorRule(config, original.number, rule));
                        migrated[slot] = original.number;
                        claimed.insert(original.number);
                    } else { migrated[slot] = addColorRule(config, rule); }
                    break;
                }
            }
            for (int slot = 0; slot < 2; ++slot) {
                for (auto& rule : config.colorRules) {
                    if (migrated[slot] && rule.number == migrated[slot]) {
                        rule.jumpToRule = migrated[1 - slot];
                        // An empty legacy destination waited without executing another rule.
                        if (rule.postTriggerAction == PostTriggerAction::Jump && !rule.jumpToRule) {
                            rule.postTriggerAction = PostTriggerAction::ReverseWait;
                        }
                    }
                }
            }
            replica.colorLogicSlotRule[0] = replica.colorLogicSlotRule[1] = 0;
            replica.colorLogicEnabled = false;
        }
    };
    migrate(config.replicas);
    for (auto& preset : config.presets) { migrate(preset.replicas); }
}
