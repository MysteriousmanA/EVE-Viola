#include "features/replica/replica_workspace.h"

#include <algorithm>

void ReplicaWorkspace::add(ReplicaId id, const ReplicaConfig& desired)
{
    if (id == 0 || contains(id)) {
        return;
    }
    orderedIds_.append(id);
    desiredConfigs_.insert(id, desired);
}

bool ReplicaWorkspace::remove(ReplicaId id) noexcept
{
    const auto index = orderedIds_.indexOf(id);
    if (index < 0) {
        return false;
    }
    orderedIds_.removeAt(index);
    desiredConfigs_.remove(id);
    return true;
}

void ReplicaWorkspace::clear() noexcept
{
    orderedIds_.clear();
    desiredConfigs_.clear();
    dormantConfigs_.clear();
}

void ReplicaWorkspace::clearActive() noexcept
{
    orderedIds_.clear();
    desiredConfigs_.clear();
}

bool ReplicaWorkspace::contains(ReplicaId id) const noexcept
{
    return orderedIds_.contains(id);
}

QList<ReplicaId> ReplicaWorkspace::ids() const
{
    return orderedIds_;
}

void ReplicaWorkspace::setDesiredConfig(ReplicaId id, const ReplicaConfig& config)
{
    if (contains(id)) {
        desiredConfigs_.insert(id, config);
    }
}

ReplicaConfig ReplicaWorkspace::desiredConfig(ReplicaId id) const
{
    return desiredConfigs_.value(id);
}

void ReplicaWorkspace::appendDormant(const ReplicaConfig& config)
{
    dormantConfigs_.append(config);
}

const QList<ReplicaConfig>& ReplicaWorkspace::dormantConfigs() const noexcept
{
    return dormantConfigs_;
}

bool ReplicaWorkspace::removeDormantAt(int index) noexcept
{
    if (index < 0 || index >= dormantConfigs_.size()) {
        return false;
    }
    dormantConfigs_.removeAt(index);
    return true;
}

void ReplicaWorkspace::clearDormant() noexcept
{
    dormantConfigs_.clear();
}

bool ReplicaWorkspace::reorder(
    ReplicaId sourceId,
    ReplicaId targetId,
    bool insertAfter) noexcept
{
    const int sourceIndex = orderedIds_.indexOf(sourceId);
    const int targetIndex = orderedIds_.indexOf(targetId);
    if (sourceIndex < 0 || targetIndex < 0 || sourceIndex == targetIndex) {
        return false;
    }

    orderedIds_.removeAt(sourceIndex);
    int insertionIndex = orderedIds_.indexOf(targetId) + (insertAfter ? 1 : 0);
    if (insertionIndex < 0) {
        return false;
    }
    insertionIndex = std::clamp(
        insertionIndex,
        0,
        static_cast<int>(orderedIds_.size()));
    orderedIds_.insert(insertionIndex, sourceId);
    return true;
}

QList<QList<ReplicaId>> ReplicaWorkspace::groups(
    const ReplicaWorkspaceKeyProvider& keyProvider) const
{
    QList<QList<ReplicaId>> result;
    if (!keyProvider) {
        return result;
    }

    QString previousKey;
    for (const ReplicaId id : orderedIds_) {
        const QString key = keyProvider(id);
        if (result.isEmpty() || key != previousKey) {
            result.append(QList<ReplicaId>{});
        }
        result.last().append(id);
        previousKey = key;
    }
    return result;
}

QList<ReplicaConfig> ReplicaWorkspace::collectAndCommitConfigurations(
    const ReplicaWorkspaceConfigCollector& collector)
{
    if (!collector) {
        return {};
    }

    struct CollectedConfiguration final {
        ReplicaId id;
        ReplicaConfig config;
    };

    QList<CollectedConfiguration> collected;
    collected.reserve(orderedIds_.size());
    for (const ReplicaId id : orderedIds_) {
        collected.append({id, collector(id, desiredConfigs_.value(id))});
    }
    for (const auto& item : collected) {
        desiredConfigs_.insert(item.id, item.config);
    }

    QList<ReplicaConfig> result;
    result.reserve(collected.size() + dormantConfigs_.size());
    for (const auto& item : collected) {
        result.append(item.config);
    }
    result.append(dormantConfigs_);
    return result;
}
