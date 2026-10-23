#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include "config/config_types.h"

struct AdvancedLogicEvent {
    quint64 id{};
    quint64 owner{};
    QString sourceKey;
    QString sourceTitle;
    int ruleNumber{};
    ColorRulePriority priority{ColorRulePriority::Normal};
};

struct AdvancedLogicDecision {
    quint64 eventId{};
    bool execute{true};
    ColorRulePriority priority{ColorRulePriority::Normal};
};

// Time is supplied by the caller and must be monotonic (milliseconds).
class AdvancedLogicEngine final {
public:
    QList<AdvancedLogicDecision> configure(
        const AdvancedLogicConfig& config, const QList<ColorRule>& rules);
    QList<AdvancedLogicDecision> submit(const AdvancedLogicEvent& event, qint64 now);
    QList<AdvancedLogicDecision> advance(qint64 now);
    QList<AdvancedLogicDecision> cancelOwner(quint64 owner);
    void clear();
    [[nodiscard]] qint64 nextDeadline() const;

private:
    struct StoredEvent { AdvancedLogicEvent event; bool decided{}; };
    struct Candidate {
        int sequence{};
        QString sourceKey;
        qint64 startedAt{};
        QList<quint64> events;
    };
    bool matches(const WhitelistNode& node, const AdvancedLogicEvent& event) const;
    void decide(quint64 id, bool execute, QList<AdvancedLogicDecision>& decisions);
    void releaseUnreferenced(QList<AdvancedLogicDecision>& decisions);
    static void order(QList<AdvancedLogicDecision>& decisions);

    QList<WhitelistSequence> sequences_;
    QHash<quint64, StoredEvent> events_;
    QList<Candidate> candidates_;
};
