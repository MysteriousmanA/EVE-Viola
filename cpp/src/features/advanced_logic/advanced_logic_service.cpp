#include "features/advanced_logic/advanced_logic_service.h"
#include "runtime/runtime_logger.h"

AdvancedLogicService::AdvancedLogicService(QObject* parent) : QObject(parent)
{
    clock_.start();
    timer_.setSingleShot(true);
    timer_.setTimerType(Qt::PreciseTimer);
    connect(&timer_, &QTimer::timeout, this, [this] {
        dispatch(engine_.advance(clock_.elapsed()));
        schedule();
    });
}

void AdvancedLogicService::setConfiguration(
    const AdvancedLogicConfig& config, const QList<ColorRule>& rules)
{
    if (config_ == config && rules_ == rules) { return; }
    if (rules_ != rules) {
        // Editing detection/feedback rules invalidates snapshots taken under the old rules.
        auto abandoned = std::move(pending_);
        pending_.clear();
        engine_.clear();
        for (const auto& pending : abandoned) {
            if (pending.owner) { pending.callback(false); }
        }
    }
    config_ = config;
    rules_ = rules;
    dispatch(engine_.configure(config, rules));
    schedule();
}

void AdvancedLogicService::submit(QObject* owner, quint64 ownerId, const QString& sourceKey,
    const QString& sourceTitle, int ruleNumber, std::function<void(bool)> callback)
{
    if (stopped_) { callback(false); return; }
    if (!owner) { return; }
    if (!owners_.contains(ownerId)) {
        owners_.insert(ownerId, owner);
        connect(owner, &QObject::destroyed, this, [this, ownerId] {
            owners_.remove(ownerId);
            cancelOwner(ownerId);
        });
    }
    const auto id = ++nextId_;
    ColorRulePriority priority = ColorRulePriority::Normal;
    for (const auto& rule : rules_) {
        if (rule.number == ruleNumber) { priority = rule.priority; break; }
    }
    pending_.insert(id, {owner, ownerId, ruleNumber, std::move(callback)});
    dispatch(engine_.submit({id, ownerId, sourceKey, sourceTitle, ruleNumber, priority}, clock_.elapsed()));
    schedule();
}

void AdvancedLogicService::dispatch(const QList<AdvancedLogicDecision>& decisions)
{
    for (const auto& decision : decisions) {
        auto it = pending_.find(decision.eventId);
        if (it == pending_.end()) { continue; }
        auto pending = std::move(it.value());
        pending_.erase(it);
        if (pending.owner) {
            RuntimeLogger::writeEvent(QStringLiteral("advanced_logic_decision"), QJsonObject{
                {QStringLiteral("event_id"), QString::number(decision.eventId)},
                {QStringLiteral("replica_id"), static_cast<double>(pending.ownerId)},
                {QStringLiteral("rule_number"), pending.ruleNumber},
                {QStringLiteral("execute"), decision.execute},
                {QStringLiteral("priority"), static_cast<int>(decision.priority)}});
            pending.callback(decision.execute);
            emit eventResolved(pending.ruleNumber, decision.execute);
        }
    }
}

void AdvancedLogicService::schedule()
{
    const auto deadline = engine_.nextDeadline();
    if (stopped_ || deadline < 0) { timer_.stop(); return; }
    timer_.start(static_cast<int>(qMax<qint64>(1, deadline - clock_.elapsed())));
}

void AdvancedLogicService::cancelOwner(quint64 ownerId)
{
    const auto ids = pending_.keys();
    for (const auto id : ids) {
        if (pending_.value(id).ownerId == ownerId) { pending_.remove(id); }
    }
    dispatch(engine_.cancelOwner(ownerId));
    schedule();
}

void AdvancedLogicService::stop()
{
    stopped_ = true;
    timer_.stop();
    engine_.clear();
    pending_.clear();
}
