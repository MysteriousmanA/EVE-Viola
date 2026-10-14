#include "features/replica/replica_manager.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

ReplicaId ReplicaManager::create(const Factory& factory)
{
    if (!factory) {
        throw std::invalid_argument("Replica factory is empty");
    }
    if (nextId_ == 0) {
        throw std::overflow_error("Replica ID space exhausted");
    }

    const auto id = nextId_;
    auto replica = factory(id, [this, id] {
        static_cast<void>(remove(id));
    });
    if (!replica) {
        throw std::runtime_error("Replica factory returned null");
    }

    replicas_.emplace(id, std::move(replica));
    ++nextId_;
    return id;
}

bool ReplicaManager::remove(ReplicaId id) noexcept
{
    return replicas_.erase(id) != 0;
}

void ReplicaManager::closeAll() noexcept
{
    auto closing = std::move(replicas_);
    replicas_.clear();
    for (auto& [id, replica] : closing) {
        Q_UNUSED(id);
        replica->closeReplica();
    }
}

std::size_t ReplicaManager::count() const noexcept
{
    return replicas_.size();
}

QList<ReplicaId> ReplicaManager::ids() const
{
    QList<ReplicaId> result;
    result.reserve(static_cast<qsizetype>(replicas_.size()));
    for (const auto& [id, replica] : replicas_) {
        Q_UNUSED(replica);
        result.append(id);
    }
    std::sort(result.begin(), result.end());
    return result;
}
