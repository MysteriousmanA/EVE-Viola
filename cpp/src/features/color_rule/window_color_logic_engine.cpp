#include "features/color_rule/window_color_logic_engine.h"
#include "features/color_rule/trigger_condition.h"
#include "features/replica/pixel_statistics.h"
#include <utility>

void WindowColorLogicEngine::configure(const QList<ColorRule>& rules, bool preserveUnchanged)
{
    QList<State> updated;
    for (const auto& rule : rules) {
        const State* previous = nullptr;
        if (preserveUnchanged) {
            for (const auto& state : states_) {
                if (state.rule == rule) { previous = &state; break; }
            }
        }
        if (previous) { updated.append(*previous); }
        else {
            State state{rule, rule.enabled};
            for (const auto& condition : rule.windowConditions) {
                state.conditions.append(parseTriggerCondition(condition.expression));
            }
            updated.append(std::move(state));
        }
    }
    states_ = updated;
    if (!preserveUnchanged) { samples_.clear(); }
    for (auto it = samples_.begin(); it != samples_.end();) {
        if (requiredColors(it.key()).isEmpty()) { it = samples_.erase(it); }
        else { ++it; }
    }
}

bool WindowColorLogicEngine::ruleActive(int number) const
{
    for (const auto& state : states_) {
        if (state.rule.number == number) { return state.active; }
    }
    return false;
}

bool WindowColorLogicEngine::conditionMet(int number, qint64 now) const
{
    for (const auto& state : states_) {
        if (state.rule.number != number) { continue; }
        return conditionMet(state, now);
    }
    return false;
}

bool WindowColorLogicEngine::conditionMet(const State& state, qint64 now) const
{
    if (!state.active || state.rule.windowConditions.isEmpty()) { return false; }
    const bool any = state.rule.triggerConditionRelation == TriggerConditionRelation::Or;
    qsizetype index = 0;
    for (const auto& condition : state.rule.windowConditions) {
        const auto& parsed = state.conditions.at(index++);
        const auto sample = samples_.constFind(condition.replicaId);
        bool met = false;
        if (!condition.replicaId.isEmpty() && !condition.isEmpty()
            && sample != samples_.cend() && now < sample->expiresAt) {
            const auto count = sample->statistics.count({condition.pixel, condition.tolerance});
            met = count >= 0 && parsed && !parsed->isEmpty()
                && evaluateTriggerCondition(*parsed, static_cast<double>(count));
        }
        if (any && met) { return true; }
        if (!any && !met) { return false; }
    }
    return !any;
}

void WindowColorLogicEngine::refreshConditions(qint64 now)
{
    for (auto& state : states_) {
        const bool met = conditionMet(state, now);
        if (!met) { state.met = false; state.replay = false; }
    }
}

QList<WindowLogicTrigger> WindowColorLogicEngine::observe(const QString& id,
    const PixelStatistics& statistics, qint64 now, int validityMs, bool notificationOnly)
{
    samples_.insert(id, {statistics, now + qMax(1, validityMs)});
    refreshConditions(now);
    QList<WindowLogicTrigger> result;
    for (auto& state : states_) {
        bool affected = false;
        for (const auto& condition : state.rule.windowConditions) {
            affected = affected || condition.replicaId == id;
        }
        if (!affected) { continue; }
        const bool met = conditionMet(state, now);
        const bool trigger = met && !state.pending && (!state.met || state.replay);
        state.met = met;
        if (trigger) {
            state.replay = false;
            state.pending = ++nextToken_;
            if (!notificationOnly) { ++state.count; }
            result.append({state.rule, state.pending});
        }
    }
    return result;
}

void WindowColorLogicEngine::invalidate(const QString& id, qint64 now)
{
    samples_.remove(id);
    refreshConditions(now);
}
void WindowColorLogicEngine::expire(qint64 now) { refreshConditions(now); }

void WindowColorLogicEngine::resetObservations()
{
    samples_.clear();
    for (auto& state : states_) {
        state.met = false; state.replay = false; state.pending = 0;
    }
}

void WindowColorLogicEngine::complete(quint64 token, bool succeeded)
{
    int jump = 0;
    for (auto& state : states_) {
        if (!token || state.pending != token) { continue; }
        state.pending = 0;
        if (!succeeded || !state.met) { state.replay = false; return; }
        if (state.rule.postTriggerAction == PostTriggerAction::Loop) { state.replay = state.met; }
        if (state.rule.postTriggerAction == PostTriggerAction::Jump) {
            jump = state.rule.jumpToRule;
            state.active = false;
            state.met = false;
        }
        break;
    }
    if (jump > 0) {
        for (auto& state : states_) {
            if (state.rule.number == jump) { state.active = true; state.met = false; state.replay = false; }
        }
    }
}
void WindowColorLogicEngine::suppress(quint64 token) { complete(token, false); }
bool WindowColorLogicEngine::toggleRules(int firstRule, int secondRule)
{
    if (firstRule <= 0 || secondRule <= 0 || firstRule == secondRule) { return false; }
    State* first = nullptr;
    State* second = nullptr;
    for (auto& state : states_) {
        if (state.rule.number == firstRule) { first = &state; }
        if (state.rule.number == secondRule) { second = &state; }
    }
    if (!first || !second || (!first->active && !second->active)) { return false; }
    const bool activateSecond = first->active;
    for (State* state : {first, second}) {
        state->active = state == second ? activateSecond : !activateSecond;
        state->met = false;
        state->replay = false;
        state->pending = 0;
    }
    return true;
}
bool WindowColorLogicEngine::pending(quint64 token) const
{
    for (const auto& state : states_) { if (token && state.pending == token) { return true; } }
    return false;
}
QList<PixelColorTarget> WindowColorLogicEngine::requiredColors(const QString& id) const
{
    QList<PixelColorTarget> result;
    for (const auto& state : states_) {
        for (const auto& c : state.rule.windowConditions) {
            const PixelColorTarget target{c.pixel, c.tolerance};
            if (c.replicaId == id && !c.isEmpty() && pixelColorValue(c.pixel).isValid()
                && !result.contains(target)) { result.append(target); }
        }
    }
    return result;
}
int WindowColorLogicEngine::triggerCount(int number) const
{
    for (const auto& state : states_) { if (state.rule.number == number) { return state.count; } }
    return 0;
}
