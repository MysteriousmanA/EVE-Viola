#include "features/advanced_logic/advanced_logic_engine.h"

#include <algorithm>
#include <limits>
#include <QSet>

QList<AdvancedLogicDecision> AdvancedLogicEngine::configure(
    const AdvancedLogicConfig& config, const QList<ColorRule>& rules)
{
    candidates_.clear();
    QList<AdvancedLogicDecision> decisions;
    releaseUnreferenced(decisions);
    sequences_.clear();
    QSet<int> validRules;
    for (const auto& rule : rules) { validRules.insert(rule.number); }
    if (config.enabled) {
        for (auto sequence : config.sequences) {
            if (!sequence.enabled || sequence.nodes.isEmpty()
                || std::any_of(sequence.nodes.cbegin(), sequence.nodes.cend(),
                    [&](const WhitelistNode& node) { return !validRules.contains(node.ruleNumber); })) {
                continue;
            }
            sequence.timeoutMs = qBound(100, sequence.timeoutMs, 3600000);
            sequences_.append(std::move(sequence));
        }
    }
    order(decisions);
    return decisions;
}

bool AdvancedLogicEngine::matches(const WhitelistNode& node, const AdvancedLogicEvent& event) const
{
    return node.ruleNumber == event.ruleNumber
        && (node.sourceTitle.trimmed().isEmpty() || node.sourceTitle.trimmed() == event.sourceTitle);
}

void AdvancedLogicEngine::decide(
    quint64 id, bool execute, QList<AdvancedLogicDecision>& decisions)
{
    auto it = events_.find(id);
    if (it == events_.end() || it->decided) { return; }
    it->decided = true;
    decisions.append({id, execute, it->event.priority});
}

void AdvancedLogicEngine::order(QList<AdvancedLogicDecision>& decisions)
{
    std::sort(decisions.begin(), decisions.end(), [](const auto& left, const auto& right) {
        return left.priority != right.priority ? left.priority > right.priority
                                              : left.eventId < right.eventId;
    });
}

void AdvancedLogicEngine::releaseUnreferenced(QList<AdvancedLogicDecision>& decisions)
{
    QSet<quint64> referenced;
    for (const auto& candidate : candidates_) {
        for (const auto id : candidate.events) { referenced.insert(id); }
    }
    const auto ids = events_.keys();
    for (const auto id : ids) {
        if (!referenced.contains(id)) {
            decide(id, true, decisions);
            events_.remove(id);
        }
    }
}

QList<AdvancedLogicDecision> AdvancedLogicEngine::advance(qint64 now)
{
    candidates_.removeIf([&](const Candidate& candidate) {
        return now > candidate.startedAt + sequences_[candidate.sequence].timeoutMs;
    });
    QList<AdvancedLogicDecision> decisions;
    releaseUnreferenced(decisions);
    order(decisions);
    return decisions;
}

QList<AdvancedLogicDecision> AdvancedLogicEngine::submit(const AdvancedLogicEvent& event, qint64 now)
{
    auto decisions = advance(now);
    if (events_.contains(event.id)) { return decisions; }
    events_.insert(event.id, {event, false});
    if (event.priority == ColorRulePriority::Immediate) { decide(event.id, true, decisions); }

    QList<Candidate> next;
    QList<Candidate> completed;
    for (auto candidate : candidates_) {
        const auto& sequence = sequences_[candidate.sequence];
        if (sequence.sameSource && candidate.sourceKey != event.sourceKey) {
            next.append(std::move(candidate));
            continue;
        }
        if (matches(sequence.nodes[candidate.events.size()], event)) {
            // In permissive mode this event may be consumed by a different
            // whitelist. Keep the unconsumed prefix available for a later match.
            if (sequence.ignoreUnrelated) { next.append(candidate); }
            candidate.events.append(event.id);
            if (candidate.events.size() == sequence.nodes.size()) {
                completed.append(std::move(candidate));
            } else { next.append(std::move(candidate)); }
        } else if (sequence.ignoreUnrelated) { next.append(std::move(candidate)); }
    }
    for (int index = 0; index < sequences_.size(); ++index) {
        if (matches(sequences_[index].nodes.first(), event)) {
            Candidate candidate{index, event.sourceKey, now, {event.id}};
            if (sequences_[index].nodes.size() == 1) { completed.append(std::move(candidate)); }
            else { next.append(std::move(candidate)); }
        }
    }
    candidates_ = std::move(next);
    if (!completed.isEmpty()) {
        // The first completed whitelist wins. List order resolves simultaneous matches.
        std::stable_sort(completed.begin(), completed.end(), [](const auto& a, const auto& b) {
            if (a.sequence != b.sequence) { return a.sequence < b.sequence; }
            if (a.startedAt != b.startedAt) { return a.startedAt < b.startedAt; }
            return std::lexicographical_compare(a.events.cbegin(), a.events.cend(),
                b.events.cbegin(), b.events.cend());
        });
        const auto& winner = completed.first();
        const auto& sequence = sequences_[winner.sequence];
        QSet<quint64> consumed;
        for (int index = 0; index < winner.events.size(); ++index) {
            const auto id = winner.events[index];
            decide(id, sequence.nodes[index].effect == WhitelistNodeEffect::Keep, decisions);
            consumed.insert(id);
        }
        candidates_.removeIf([&](const Candidate& candidate) {
            return std::any_of(candidate.events.cbegin(), candidate.events.cend(),
                [&](quint64 id) { return consumed.contains(id); });
        });
    }
    // Bound repeated immediate events even when a long whitelist never completes.
    while (candidates_.size() > 2048) { candidates_.removeFirst(); }
    releaseUnreferenced(decisions);
    order(decisions);
    return decisions;
}

QList<AdvancedLogicDecision> AdvancedLogicEngine::cancelOwner(quint64 owner)
{
    QSet<quint64> removed;
    for (auto it = events_.cbegin(); it != events_.cend(); ++it) {
        if (it->event.owner == owner) { removed.insert(it.key()); }
    }
    candidates_.removeIf([&](const Candidate& candidate) {
        return std::any_of(candidate.events.cbegin(), candidate.events.cend(),
            [&](quint64 id) { return removed.contains(id); });
    });
    for (auto id : removed) { events_.remove(id); }
    QList<AdvancedLogicDecision> decisions;
    releaseUnreferenced(decisions);
    order(decisions);
    return decisions;
}

void AdvancedLogicEngine::clear()
{
    candidates_.clear();
    events_.clear();
}

qint64 AdvancedLogicEngine::nextDeadline() const
{
    qint64 result = std::numeric_limits<qint64>::max();
    for (const auto& candidate : candidates_) {
        result = qMin(result, candidate.startedAt + sequences_[candidate.sequence].timeoutMs + 1);
    }
    return candidates_.isEmpty() ? -1 : result;
}
