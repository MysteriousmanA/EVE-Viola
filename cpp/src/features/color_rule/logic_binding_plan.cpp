#include "features/color_rule/logic_binding_plan.h"
#include <QHash>
#include <QSet>

QString logicRegionPresetId(const QString& name)
{
    return QStringLiteral("region-preset:") + name;
}

QList<ReplicaConfig> logicRegionTemplates(const QList<RegionPreset>& presets)
{
    QList<ReplicaConfig> result;
    for (const auto& preset : presets) {
        ReplicaConfig window;
        window.id = logicRegionPresetId(preset.name);
        window.capturedRegion = preset.region;
        window.statisticsIntervalMs = preset.statisticsIntervalMs;
        if (!preset.colorTolerances.isEmpty()) { window.statisticsColors = preset.colorTolerances; }
        window.visible = false;
        result.append(window);
    }
    return result;
}

void migrateLogicRegionReferences(AppConfig& config)
{
    const auto migrate = [&](QString& id) {
        if (id.startsWith(QStringLiteral("region-preset:"))) { return; }
        for (const auto& window : config.replicas) {
            if (window.id != id || !window.capturedRegion.isValid()) { continue; }
            for (const auto& preset : config.regionPresets) {
                if (preset.region == window.capturedRegion) { id = logicRegionPresetId(preset.name); return; }
            }
        }
    };
    for (auto& rule : config.colorRules) {
        for (auto& condition : rule.windowConditions) { migrate(condition.replicaId); }
        migrate(rule.targetReplicaId);
        for (auto& setting : rule.windowSwitches) { migrate(setting.replicaId); }
    }
}

LogicBindingPlan planLogicBinding(const LogicBindingConfig& binding,
    const QList<ColorRule>& definitions, const QList<ReplicaConfig>& templates, int firstRuntimeNumber)
{
    LogicBindingPlan plan;
    QHash<int, ColorRule> rules;
    for (const auto& rule : definitions) { rules.insert(rule.number, rule); }
    QList<int> chain;
    QList<int> pending{binding.ruleNumber};
    for (int index = 0; index < pending.size(); ++index) {
        const int current = pending[index];
        if (current <= 0 || chain.contains(current)) { continue; }
        if (!rules.contains(current)) { plan.error = QStringLiteral("图色逻辑或切换目标已删除"); return plan; }
        chain.append(current);
        const auto& rule = rules[current];
        if (rule.postTriggerAction == PostTriggerAction::Jump) { pending.append(rule.jumpToRule); }
        for (const auto& setting : rule.windowSwitches) {
            if (setting.middleClickRule > 0) { pending.append(setting.middleClickRule); }
        }
    }
    if (chain.isEmpty()) { plan.error = QStringLiteral("请选择图色逻辑"); return plan; }
    QHash<QString, ReplicaConfig> candidates;
    for (const auto& replica : templates) { if (replica.sourceTitle.isEmpty()) { candidates.insert(replica.id, replica); } }
    QSet<QString> required;
    for (int number : chain) {
        const auto& rule = rules[number];
        if (rule.windowConditions.isEmpty()) { plan.error = QStringLiteral("逻辑尚未配置条件槽"); return plan; }
        for (const auto& condition : rule.windowConditions) { required.insert(condition.replicaId); }
        for (const auto& setting : rule.windowSwitches) { required.insert(setting.replicaId); }
    }
    QHash<QString, QString> ids;
    for (const auto& id : required) {
        if (id.isEmpty() || !candidates.contains(id)) {
            plan.error = QStringLiteral("引用的区域预设已删除，请重新选择"); return plan;
        }
        auto window = candidates.value(id);
        if (!window.capturedRegion.isValid() || window.capturedRegion.isEmpty()) {
            plan.error = QStringLiteral("区域预设的范围无效"); return plan;
        }
        window.id = binding.windowIds.value(id, QStringLiteral("logic:%1:%2").arg(binding.id, id));
        window.sourceTitle = binding.sourceTitle;
        ids.insert(id, window.id);
        plan.windows.append(window);
    }
    QHash<int, int> numbers;
    for (int i = 0; i < chain.size(); ++i) { numbers.insert(chain[i], firstRuntimeNumber + i); }
    for (int number : chain) {
        auto rule = rules[number];
        rule.definitionNumber = rule.number;
        rule.number = numbers.value(number);
        rule.enabled = binding.enabled && number == binding.ruleNumber;
        rule.targetReplicaId = ids.value(rule.windowConditions.first().replicaId);
        for (auto& condition : rule.windowConditions) {
            condition.tolerance = colorToleranceFor(candidates.value(condition.replicaId).statisticsColors, condition.pixel);
            condition.replicaId = ids.value(condition.replicaId);
        }
        for (auto& setting : rule.windowSwitches) {
            setting.replicaId = ids.value(setting.replicaId);
            setting.middleClickRule = numbers.value(setting.middleClickRule);
        }
        rule.jumpToRule = numbers.value(rule.jumpToRule);
        plan.rules.append(rule);
    }
    return plan;
}

void migrateLogicBindings(AppConfig& config)
{
    QHash<QString, ReplicaConfig> oldWindows;
    for (const auto& replica : config.replicas) { oldWindows.insert(replica.id, replica); }
    const auto makeTemplate = [&](const QString& id) -> QString {
        if (!oldWindows.contains(id)) { return id; }
        const auto original = oldWindows.value(id);
        if (!original.capturedRegion.isValid() || original.capturedRegion.isEmpty()) { return id; }
        for (const auto& preset : config.regionPresets) {
            if (preset.region == original.capturedRegion) { return logicRegionPresetId(preset.name); }
        }
        QString name = QStringLiteral("迁移区域 %1").arg(config.regionPresets.size() + 1);
        QSet<QString> names;
        for (const auto& preset : config.regionPresets) { names.insert(preset.name); }
        while (names.contains(name)) { name += QStringLiteral("副本"); }
        config.regionPresets.append({name, original.capturedRegion, original.statisticsIntervalMs});
        return logicRegionPresetId(name);
    };
    for (auto& rule : config.colorRules) {
        QSet<QString> sources;
        bool legacy = false;
        for (const auto& condition : rule.windowConditions) {
            const auto window = oldWindows.constFind(condition.replicaId);
            if (window != oldWindows.cend() && !window->sourceTitle.isEmpty()) {
                sources.insert(window->sourceTitle); legacy = true;
            }
        }
        const auto target = oldWindows.constFind(rule.targetReplicaId);
        if (target != oldWindows.cend() && !target->sourceTitle.isEmpty()) { legacy = true; sources.insert(target->sourceTitle); }
        if (!legacy) { continue; }
        bool exists = false;
        for (const auto& binding : config.logicBindings) { exists = exists || binding.ruleNumber == rule.number; }
        if (!exists) {
            LogicBindingConfig binding; binding.ruleNumber = rule.number;
            binding.enabled = rule.enabled && sources.size() == 1;
            if (sources.size() == 1) { binding.sourceTitle = *sources.cbegin(); }
            config.logicBindings.append(binding);
        }
        for (auto& condition : rule.windowConditions) { condition.replicaId = makeTemplate(condition.replicaId); }
        rule.targetReplicaId = makeTemplate(rule.targetReplicaId);
        for (auto& setting : rule.windowSwitches) { setting.replicaId = makeTemplate(setting.replicaId); }
    }
}
