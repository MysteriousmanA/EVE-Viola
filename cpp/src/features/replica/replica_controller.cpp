#include "features/replica/replica_controller.h"
#include <stdexcept>
#include <utility>

ReplicaId ReplicaController::create(const ReplicaManager::Factory& factory)
{
    if (!factory) { throw std::invalid_argument("Replica factory is empty"); }
    const auto id = manager_.create([this, &factory](ReplicaId id, ReplicaManager::CloseCallback remove) {
        return factory(id, [this, id, remove = std::move(remove)] {
            static_cast<void>(workspace_.remove(id));
            remove();
        });
    });
    workspace_.add(id);
    return id;
}

bool ReplicaController::remove(ReplicaId id) noexcept
{
    const bool removed = workspace_.remove(id);
    return manager_.remove(id) || removed;
}

void ReplicaController::closeAll() noexcept
{
    manager_.closeAll();
    workspace_.clearActive();
}
void ReplicaController::clear() noexcept
{
    closeAll();
    workspace_.clear();
}
std::size_t ReplicaController::count() const noexcept { return manager_.count(); }
bool ReplicaController::contains(ReplicaId id) const noexcept { return workspace_.contains(id); }
QList<ReplicaId> ReplicaController::ids() const { return workspace_.ids(); }
void ReplicaController::setDesiredConfig(ReplicaId id, const ReplicaConfig& config)
{
    workspace_.setDesiredConfig(id, config);
}
ReplicaConfig ReplicaController::desiredConfig(ReplicaId id) const { return workspace_.desiredConfig(id); }
void ReplicaController::update(ReplicaId id, const std::function<void(ReplicaConfig&)>& change)
{
    if (!contains(id) || !change) { return; }
    auto config = desiredConfig(id);
    change(config);
    setDesiredConfig(id, config);
}
ReplicaConfig ReplicaController::setUserVisible(ReplicaId id, bool visible)
{
    update(id, [visible](ReplicaConfig& config) {
        config.visible = visible;
        if (!visible) {
            config.colorLogicSlotRule[1] = 0;
            config.activeColorLogicSlot = 1;
            config.colorLogicEnabled = true;
        }
    });
    return desiredConfig(id);
}
QList<ReplicaConfig> ReplicaController::snapshot() const
{
    QList<ReplicaConfig> result;
    for (const auto id : ids()) { result.append(desiredConfig(id)); }
    result.append(dormantConfigs());
    return result;
}
void ReplicaController::appendDormant(const ReplicaConfig& config) { workspace_.appendDormant(config); }
const QList<ReplicaConfig>& ReplicaController::dormantConfigs() const noexcept { return workspace_.dormantConfigs(); }
bool ReplicaController::removeDormantAt(int index) noexcept { return workspace_.removeDormantAt(index); }
void ReplicaController::clearDormant() noexcept { workspace_.clearDormant(); }
bool ReplicaController::reorder(ReplicaId sourceId, ReplicaId targetId, bool insertAfter) noexcept
{
    return workspace_.reorder(sourceId, targetId, insertAfter);
}
QList<QList<ReplicaId>> ReplicaController::groups(const ReplicaWorkspaceKeyProvider& keyProvider) const
{
    return workspace_.groups(keyProvider);
}
