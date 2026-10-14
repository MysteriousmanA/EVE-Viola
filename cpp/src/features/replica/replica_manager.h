#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>

#include <QList>
#include <QtTypes>

using ReplicaId = quint64;

class CloseableReplica {
public:
    virtual ~CloseableReplica() = default;
    virtual void closeReplica() noexcept = 0;
};

class ReplicaManager final {
public:
    using CloseCallback = std::function<void()>;
    using Factory = std::function<std::unique_ptr<CloseableReplica>(
        ReplicaId,
        CloseCallback)>;

    [[nodiscard]] ReplicaId create(const Factory& factory);
    [[nodiscard]] bool remove(ReplicaId id) noexcept;
    void closeAll() noexcept;

    [[nodiscard]] std::size_t count() const noexcept;
    [[nodiscard]] QList<ReplicaId> ids() const;

private:
    ReplicaId nextId_{1};
    std::unordered_map<ReplicaId, std::unique_ptr<CloseableReplica>> replicas_;
};
