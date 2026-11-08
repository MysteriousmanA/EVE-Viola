#include "ui/logic_binding_runtime.h"
#include "ui/logic_binding_source.h"
#include "features/color_rule/logic_binding_plan.h"
#include "ui/window_color_logic_runtime.h"
#include "ui/replica_window.h"

LogicBindingRuntime::LogicBindingRuntime(WindowColorLogicRuntime& logic, QObject* parent)
    : QObject(parent), logic_(logic) {}

LogicBindingRuntime::~LogicBindingRuntime() { stop(); }

void LogicBindingRuntime::configure(const QList<ColorRule>& rules,
    const QList<LogicBindingConfig>& bindings, const QList<ReplicaConfig>& templates,
    const QList<WindowInfo>& sources, const QHash<QString, ReplicaWindow*>& replicas, bool force)
{
    QHash<QString, WindowInfo> resolved;
    for (const auto& binding : bindings) {
        if (const auto* source = resolveLogicBindingSource(binding, sources)) {
            resolved.insert(binding.id, *source);
        }
    }
    if (!force && rules_ == rules && bindings_ == bindings && templates_ == templates
        && resolved_ == resolved && replicas_ == replicas) { return; }

    rules_ = rules;
    bindings_ = bindings;
    templates_ = templates;
    resolved_ = resolved;
    replicas_ = replicas;
    statuses_.clear();
    bindingWindowIds_.clear();

    QList<ColorRule> runtimeRules;
    QList<ColorRule> canonicalRules;
    QList<int> runtimeNumbers;
    QHash<int, int> canonicalNumbers;
    int nextRuleNumber = 1;
    for (const auto& binding : bindings) {
        if (!binding.enabled) { statuses_.insert(binding.id, QStringLiteral("已停用")); continue; }
        if (binding.sourceTitle.isEmpty()) {
            statuses_.insert(binding.id, QStringLiteral("待选择源窗口"));
            continue;
        }
        if (!resolved.contains(binding.id)) {
            statuses_.insert(binding.id, QStringLiteral("源窗口失联或标题不唯一"));
            continue;
        }

        auto plan = planLogicBinding(binding, rules, templates, nextRuleNumber);
        if (!plan.error.isEmpty()) { statuses_.insert(binding.id, plan.error); continue; }

        const auto source = resolved.value(binding.id);
        bool ready = true;
        QStringList windowIds;
        for (const auto& config : plan.windows) {
            auto* window = replicas.value(config.id, nullptr);
            if (!window || !window->source() || window->source()->hwnd != source.hwnd) {
                ready = false;
                break;
            }
            windowIds.append(config.id);
        }
        if (!ready) {
            statuses_.insert(binding.id, QStringLiteral("关联小窗尚未就绪"));
            continue;
        }

        bindingWindowIds_.insert(binding.id, windowIds);
        QHash<int, int> definitionNumbers;
        for (const auto& rule : plan.rules) { definitionNumbers.insert(rule.number, rule.definitionNumber); }
        for (const auto& rule : plan.rules) {
            // Two list entries can reference opposite states of the same chain and
            // share all their small windows. They must share execution state too.
            auto canonical = rule;
            canonical.number = rule.definitionNumber;
            canonical.enabled = false;
            canonical.jumpToRule = definitionNumbers.value(rule.jumpToRule);
            for (auto& setting : canonical.windowSwitches) {
                setting.middleClickRule = definitionNumbers.value(setting.middleClickRule);
            }
            const auto existing = canonicalRules.indexOf(canonical);
            if (existing >= 0) {
                canonicalNumbers.insert(rule.number, runtimeRules.at(existing).number);
            } else {
                // Rule identity follows its definition and windows, not its position
                // among the currently available game processes.
                const auto previous = canonicalRules_.indexOf(canonical);
                const int number = previous >= 0 ? runtimeNumbers_.at(previous) : nextRuntimeNumber_++;
                auto runtimeRule = rule;
                runtimeRule.number = number;
                canonicalRules.append(canonical);
                runtimeNumbers.append(number);
                runtimeRules.append(runtimeRule);
                canonicalNumbers.insert(rule.number, number);
            }
        }
        nextRuleNumber += plan.rules.size();
        statuses_.insert(binding.id, QStringLiteral("已绑定 · %1 个小窗").arg(plan.windows.size()));
    }

    QHash<QString, QPair<int, int>> middleClickSwitches;
    for (auto& rule : runtimeRules) {
        rule.jumpToRule = canonicalNumbers.value(rule.jumpToRule);
        for (auto& setting : rule.windowSwitches) {
            setting.middleClickRule = canonicalNumbers.value(setting.middleClickRule);
            if (setting.middleClickRule > 0 && !middleClickSwitches.contains(setting.replicaId)) {
                middleClickSwitches.insert(setting.replicaId, {rule.number, setting.middleClickRule});
            }
        }
    }
    canonicalRules_ = canonicalRules;
    runtimeNumbers_ = runtimeNumbers;
    logic_.configure(runtimeRules, middleClickSwitches);
    for (auto it = bindingWindowIds_.cbegin(); it != bindingWindowIds_.cend(); ++it) {
        for (const auto& windowId : it.value()) {
            auto* window = replicas_.value(windowId, nullptr);
            if (!window) { continue; }
            logic_.attach(windowId, window);
        }
    }
}

QList<ReplicaWindow*> LogicBindingRuntime::windowsForBinding(const QString& id) const
{
    QList<ReplicaWindow*> result;
    for (const auto& windowId : bindingWindowIds_.value(id)) {
        if (auto* window = replicas_.value(windowId, nullptr)) { result.append(window); }
    }
    return result;
}

void LogicBindingRuntime::stop()
{
    bindingWindowIds_.clear();
    replicas_.clear();
    statuses_.clear();
    rules_.clear();
    bindings_.clear();
    templates_.clear();
    resolved_.clear();
    logic_.configure({});
}
