#pragma once

#include <functional>

#include <QHash>
#include <QList>
#include <QString>

#include "config/app_config.h"
#include "features/replica/replica_manager.h"

using ReplicaWorkspaceKeyProvider = std::function<QString(ReplicaId)>;
using ReplicaWorkspaceConfigCollector =
    std::function<ReplicaConfig(ReplicaId, ReplicaConfig)>;

class ReplicaWorkspace final {
public:
    void add(ReplicaId id, const ReplicaConfig& desired = {});
    [[nodiscard]] bool remove(ReplicaId id) noexcept;
    void clear() noexcept;
    void clearActive() noexcept;

    [[nodiscard]] bool contains(ReplicaId id) const noexcept;
    [[nodiscard]] QList<ReplicaId> ids() const;

    void setDesiredConfig(ReplicaId id, const ReplicaConfig& config);
    [[nodiscard]] ReplicaConfig desiredConfig(ReplicaId id) const;

    void appendDormant(const ReplicaConfig& config);
    [[nodiscard]] const QList<ReplicaConfig>& dormantConfigs() const noexcept;
    [[nodiscard]] bool removeDormantAt(int index) noexcept;
    void clearDormant() noexcept;

    [[nodiscard]] bool reorder(
        ReplicaId sourceId,
        ReplicaId targetId,
        bool insertAfter) noexcept;
    [[nodiscard]] QList<QList<ReplicaId>> groups(
        const ReplicaWorkspaceKeyProvider& keyProvider) const;
    [[nodiscard]] QList<ReplicaConfig> collectAndCommitConfigurations(
        const ReplicaWorkspaceConfigCollector& collector);

private:
    QList<ReplicaId> orderedIds_;
    QHash<ReplicaId, ReplicaConfig> desiredConfigs_;
    QList<ReplicaConfig> dormantConfigs_;
};
