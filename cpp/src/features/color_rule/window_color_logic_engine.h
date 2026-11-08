#pragma once
#include <QHash>
#include "config/config_types.h"

struct WindowLogicTrigger {
    ColorRule rule;
    quint64 token{};
};

// All timestamps are caller-supplied monotonic milliseconds.
class WindowColorLogicEngine final {
public:
    void configure(const QList<ColorRule>& rules, bool preserveUnchanged = false);
    QList<WindowLogicTrigger> observe(const QString& replicaId, const PixelStatistics& statistics,
        qint64 now, int validityMs, bool notificationOnly = false);
    void invalidate(const QString& replicaId, qint64 now);
    void expire(qint64 now);
    void resetObservations();
    void complete(quint64 token, bool succeeded);
    void suppress(quint64 token);
    bool toggleRules(int firstRule, int secondRule);
    bool conditionMet(int ruleNumber, qint64 now) const;
    bool ruleActive(int ruleNumber) const;
    bool pending(quint64 token) const;
    QList<PixelColorTarget> requiredColors(const QString& replicaId) const;
    int triggerCount(int ruleNumber) const;
private:
    struct Sample { PixelStatistics statistics; qint64 expiresAt{}; };
    struct State {
        ColorRule rule;
        bool active{};
        bool met{};
        bool replay{};
        quint64 pending{};
        int count{};
        QList<std::optional<TriggerCondition>> conditions;
    };
    bool conditionMet(const State& state, qint64 now) const;
    void refreshConditions(qint64 now);
    QList<State> states_;
    QHash<QString, Sample> samples_;
    quint64 nextToken_{}; // Never reset: stale feedback cannot complete a new trigger.
};
