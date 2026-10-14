#pragma once

#include "features/replica/replica_manager.h"
#include "features/replica/replica_workspace.h"

// Owns replica identity, lifetime and persistent state. Widgets are projections
// of this state; temporary visibility suppression never edits the configuration.
class ReplicaController final {
public:
    [[nodiscard]] ReplicaId create(const ReplicaManager::Factory& factory);
    [[nodiscard]] bool remove(ReplicaId id) noexcept;
    void closeAll() noexcept;
    void clear() noexcept;
    [[nodiscard]] std::size_t count() const noexcept;
    [[nodiscard]] bool contains(ReplicaId id) const noexcept;
    [[nodiscard]] QList<ReplicaId> ids() const;
    void setDesiredConfig(ReplicaId id, const ReplicaConfig& config);
    [[nodiscard]] ReplicaConfig desiredConfig(ReplicaId id) const;
    void update(ReplicaId id, const std::function<void(ReplicaConfig&)>& change);
    [[nodiscard]] ReplicaConfig setUserVisible(ReplicaId id, bool visible);
    [[nodiscard]] QList<ReplicaConfig> snapshot() const;
    void appendDormant(const ReplicaConfig& config);
    [[nodiscard]] const QList<ReplicaConfig>& dormantConfigs() const noexcept;
    [[nodiscard]] bool removeDormantAt(int index) noexcept;
    void clearDormant() noexcept;
    [[nodiscard]] bool reorder(ReplicaId sourceId, ReplicaId targetId, bool insertAfter) noexcept;
    [[nodiscard]] QList<QList<ReplicaId>> groups(const ReplicaWorkspaceKeyProvider& keyProvider) const;

private:
    ReplicaManager manager_;
    ReplicaWorkspace workspace_;
};
