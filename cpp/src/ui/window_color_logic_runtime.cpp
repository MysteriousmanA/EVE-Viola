#include "ui/window_color_logic_runtime.h"
#include "ui/replica_window.h"
#include "ui/audio_feedback_manager.h"
#include <QScopedValueRollback>

namespace {
// Separate request owners from runtime replica IDs.
quint64 requestOwner(quint64 token) { return (quint64{1} << 63) | token; }
}
WindowColorLogicRuntime::WindowColorLogicRuntime(AudioFeedbackManager& audio, QObject* parent)
    : QObject(parent), audio_(audio)
{
    clock_.start();
    timer_.setInterval(250);
    connect(&timer_, &QTimer::timeout, this, &WindowColorLogicRuntime::refresh);
    timer_.start();
    connect(&audio_, &AudioFeedbackManager::feedbackCompleted, this,
        [this](quint64 owner, quint64 token) { if (owner == requestOwner(token)) { finish(token, true); } });
    connect(&audio_, &AudioFeedbackManager::feedbackFailed, this,
        [this](quint64 owner, quint64 token) { if (owner == requestOwner(token)) { finish(token, false); } });
}
void WindowColorLogicRuntime::configure(const QList<ColorRule>& rules,
    const QHash<QString, QPair<int, int>>& middleClickSwitches)
{
    if (rules == rules_ && middleClickSwitches == middleClickSwitches_) { return; }
    // Catalog changes must not cancel feedback or rearm unchanged rules.
    for (auto token : pending_.keys()) {
        if (!rules.contains(pending_.value(token).rule)) { cancel(token); }
    }
    for (const auto& previous : rules_) {
        if (!rules.contains(previous)) { triggeredRules_.remove(previous.number); }
    }
    rules_ = rules;
    middleClickSwitches_ = middleClickSwitches;
    engine_.configure(rules, true);
    for (auto it = windows_.cbegin(); it != windows_.cend(); ++it) {
        if (it.value()) { it.value()->setLogicRequiredColors(engine_.requiredColors(it.key())); }
    }
    refresh();
}
void WindowColorLogicRuntime::attach(const QString& id, ReplicaWindow* window)
{
    if (id.isEmpty() || !window || windows_.value(id) == window) { return; }
    const auto previousId = idForWindow(window);
    if (!previousId.isEmpty()) { detach(previousId); }
    if (windows_.contains(id)) { detach(id); }
    windows_.insert(id, window);
    window->setGlobalColorLogicManaged(true);
    window->setLogicRequiredColors(engine_.requiredColors(id));
    connect(window, &ReplicaWindow::colorStatisticsChanged, this,
        [this, id](const PixelStatistics& statistics) { observe(id, statistics); });
    connect(window, &ReplicaWindow::pixelStatisticsUnavailable, this, [this, id] { invalidate(id); });
    connect(window, &ReplicaWindow::sharedLogicToggleRequested, this,
        [this, id] { toggleRulesForWindow(id); });
    connect(window, &ReplicaWindow::closed, this, [this, id] { detach(id); });
    connect(window, &ReplicaWindow::destroyed, this, [this, id] { detach(id); });
    refresh();
}
QString WindowColorLogicRuntime::idForWindow(const ReplicaWindow* window) const
{
    for (auto it = windows_.cbegin(); it != windows_.cend(); ++it) {
        if (it.value() == window) { return it.key(); }
    }
    return {};
}
void WindowColorLogicRuntime::detach(const QString& id)
{
    if (auto window = windows_.take(id)) {
        disconnect(window, nullptr, this, nullptr);
        window->setSharedFlashActive(false);
        window->setSharedLogicBorderColor({});
    }
    invalidate(id);
}
void WindowColorLogicRuntime::observe(const QString& id, const PixelStatistics& statistics)
{
    if (stopped_ || mode_ == DesktopPetMode::DoNotDisturb) { return; }
    const auto window = windows_.value(id);
    if (!window || window->statisticsIntervalMs() <= 0) { return; }
    const auto triggers = engine_.observe(id, statistics, clock_.elapsed(),
        qMax(3000, window->statisticsIntervalMs() * 2), mode_ == DesktopPetMode::Fish);
    // Publish every detected trigger before any feedback can finish, jump state,
    // or synchronously notify observers. Eligibility lasts ten seconds from detection.
    for (const auto& trigger : triggers) {
        const auto target = windows_.value(trigger.rule.targetReplicaId);
        if (target && target->source()) {
            recentAccountTriggers_.insert(trigger.rule.number,
                {trigger.rule, static_cast<quintptr>(target->source()->hwnd), clock_.elapsed() + 10000});
        }
    }
    refreshAccountTriggers();
    {
        const QScopedValueRollback guard(batchingRefresh_, true);
        for (const auto& trigger : triggers) {
            const auto target = windows_.value(trigger.rule.targetReplicaId);
            if (!target || !target->source()) { engine_.suppress(trigger.token); continue; }
            if (mode_ == DesktopPetMode::Fish) {
                emit target->colorLogicNotificationRequested(trigger.rule.definitionNumber > 0
                    ? trigger.rule.definitionNumber : trigger.rule.number);
                engine_.suppress(trigger.token);
                continue;
            }
            triggeredRules_.remove(trigger.rule.number);
            pending_.insert(trigger.token, {trigger.rule, trigger.rule.targetReplicaId});
            execute(trigger.token);
        }
    }
    refresh();
}
void WindowColorLogicRuntime::invalidate(const QString& id)
{
    for (auto token : pending_.keys()) {
        if (pending_.value(token).target == id) { cancel(token); }
    }
    engine_.invalidate(id, clock_.elapsed());
    refresh();
}
void WindowColorLogicRuntime::refreshAccountTriggers()
{
    const auto now = clock_.elapsed();
    for (auto it = recentAccountTriggers_.begin(); it != recentAccountTriggers_.end();) {
        const auto target = windows_.value(it->rule.targetReplicaId);
        if (stopped_ || mode_ == DesktopPetMode::DoNotDisturb || now >= it->expiresAt
            || !rules_.contains(it->rule) || !target || !target->source()
            || static_cast<quintptr>(target->source()->hwnd) != it->hwnd) {
            it = recentAccountTriggers_.erase(it);
        } else { ++it; }
    }
    QHash<quintptr, QStringList> accountTriggers;
    if (!stopped_ && mode_ != DesktopPetMode::DoNotDisturb) {
        for (const auto& rule : rules_) {
            const auto recent = recentAccountTriggers_.constFind(rule.number);
            if (recent == recentAccountTriggers_.cend()) { continue; }
            const int number = rule.definitionNumber > 0 ? rule.definitionNumber : rule.number;
            const QString label = rule.note.trimmed().isEmpty()
                ? QStringLiteral("逻辑 #%1").arg(number)
                : QStringLiteral("#%1 %2").arg(number).arg(rule.note.trimmed());
            auto& labels = accountTriggers[recent->hwnd];
            if (!labels.contains(label)) { labels.append(label); }
        }
    }
    if (accountTriggers != accountTriggers_) {
        accountTriggers_ = accountTriggers;
        emit accountTriggersChanged(accountTriggers_);
    }
}
void WindowColorLogicRuntime::refresh()
{
    // Synchronous feedback completion can call refresh once per trigger.
    // Publish the final state once after the whole sample has been dispatched.
    if (batchingRefresh_) { return; }
    engine_.expire(clock_.elapsed());
    refreshAccountTriggers();
    for (auto token : pending_.keys()) {
        const auto pending = pending_.value(token);
        if (!engine_.conditionMet(pending.rule.number, clock_.elapsed())
            || !windows_.value(pending.target) || !windows_.value(pending.target)->source()) { cancel(token); }
    }
    for (int number : triggeredRules_.values()) {
        if (!engine_.conditionMet(number, clock_.elapsed())) { triggeredRules_.remove(number); }
    }
    for (auto it = windows_.cbegin(); it != windows_.cend(); ++it) {
        bool active = false;
        QColor borderColor;
        if (!stopped_ && mode_ == DesktopPetMode::Resident) {
            for (const auto& rule : rules_) {
                bool hasBorderTargets = false;
                bool selectedBorder = false;
                for (const auto& setting : rule.windowSwitches) {
                    if (!setting.borderColor.isValid()) { continue; }
                    hasBorderTargets = true;
                    selectedBorder = selectedBorder || setting.replicaId == it.key();
                }
                // Feedback uses its own target; flashing follows the selected state borders.
                // Preserve the default target for rules without explicit border settings.
                const bool flashTarget = hasBorderTargets ? selectedBorder : rule.targetReplicaId == it.key();
                active = active || (triggeredRules_.contains(rule.number) && flashTarget
                    && engine_.conditionMet(rule.number, clock_.elapsed()));
                if (!borderColor.isValid() && engine_.ruleActive(rule.number)) {
                    for (const auto& setting : rule.windowSwitches) {
                        if (setting.replicaId == it.key() && setting.borderColor.isValid()) {
                            borderColor = setting.borderColor;
                            break;
                        }
                    }
                }
            }
        }
        if (it.value()) {
            it.value()->setSharedLogicBorderColor(borderColor);
            it.value()->setSharedFlashActive(active);
        }
    }
}
void WindowColorLogicRuntime::toggleRulesForWindow(const QString& id)
{
    const auto pair = middleClickSwitches_.value(id);
    if (pair.first <= 0 || pair.second <= 0) { return; }
    for (auto token : pending_.keys()) {
        const int number = pending_.value(token).rule.number;
        if (number == pair.first || number == pair.second) { cancel(token); }
    }
    triggeredRules_.remove(pair.first);
    triggeredRules_.remove(pair.second);
    if (engine_.toggleRules(pair.first, pair.second)) { refresh(); }
}
void WindowColorLogicRuntime::execute(quint64 token)
{
    auto it = pending_.find(token);
    if (it == pending_.end() || it->executing) { return; }
    const auto pending = it.value();
    const auto target = windows_.value(pending.target);
    if (!target || !target->source() || !engine_.conditionMet(pending.rule.number, clock_.elapsed())) {
        cancel(token); return;
    }
    it->executing = true;
    triggeredRules_.insert(pending.rule.number);
    const bool popup = pending.rule.triggerFeedback == TriggerFeedback::Popup;
    target->executeSharedColorLogic(pending.rule.definitionNumber > 0
        ? pending.rule.definitionNumber : pending.rule.number, token, popup,
        [this, token] { finish(token, true); });
    if (pending.rule.triggerFeedback == TriggerFeedback::Audio) {
        if (pending.rule.triggerFeedbackAudio.isEmpty()
            || audio_.request(requestOwner(token), token, pending.rule.triggerFeedbackAudio)
                != AudioRequestDisposition::Joined) { finish(token, false); }
    } else if (!popup) { finish(token, true); }
}
void WindowColorLogicRuntime::finish(quint64 token, bool success)
{
    if (!pending_.contains(token)) { return; }
    if (!engine_.conditionMet(pending_.value(token).rule.number, clock_.elapsed())) {
        cancel(token);
        refresh();
        return;
    }
    const auto completed = pending_.take(token);
    if (completed.rule.postTriggerAction == PostTriggerAction::ReverseWait) {
        triggeredRules_.remove(completed.rule.number);
    }
    engine_.complete(token, success);
    refresh();
}
void WindowColorLogicRuntime::cancel(quint64 token)
{
    if (!pending_.contains(token)) { return; }
    const auto pending = pending_.take(token);
    engine_.suppress(token);
    triggeredRules_.remove(pending.rule.number);
    audio_.cancel(requestOwner(token), token);
    if (auto target = windows_.value(pending.target)) { target->cancelSharedColorLogic(token); }
}
void WindowColorLogicRuntime::setMode(DesktopPetMode mode)
{
    if (mode_ == mode) { return; }
    mode_ = mode;
    for (auto token : pending_.keys()) { cancel(token); }
    triggeredRules_.clear();
    recentAccountTriggers_.clear();
    engine_.resetObservations();
    refresh();
}
void WindowColorLogicRuntime::stop()
{
    stopped_ = true;
    timer_.stop();
    for (auto token : pending_.keys()) { cancel(token); }
    refresh();
}
