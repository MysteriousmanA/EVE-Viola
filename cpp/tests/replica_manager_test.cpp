#include <memory>
#include <utility>

#include <QtTest>

#include "features/replica/replica_manager.h"

namespace {

struct ReplicaStats {
    int created{0};
    int closed{0};
    int destroyed{0};
};

class FakeReplica final : public CloseableReplica {
public:
    FakeReplica(
        std::shared_ptr<ReplicaStats> stats,
        ReplicaManager::CloseCallback onClosed)
        : stats_(std::move(stats))
        , onClosed_(std::move(onClosed))
    {
        ++stats_->created;
    }

    ~FakeReplica() override
    {
        ++stats_->destroyed;
    }

    void closeReplica() noexcept override
    {
        ++stats_->closed;
        onClosed_();
    }

private:
    std::shared_ptr<ReplicaStats> stats_;
    ReplicaManager::CloseCallback onClosed_;
};

} // namespace

class ReplicaManagerTest final : public QObject {
    Q_OBJECT

private slots:
    void createsMoreThanOneHundredWithMonotonicIds()
    {
        ReplicaManager manager;
        auto stats = std::make_shared<ReplicaStats>();
        QList<ReplicaId> ids;

        for (int index = 0; index < 101; ++index) {
            ids.append(manager.create(
                [stats](ReplicaId, ReplicaManager::CloseCallback onClosed) {
                    return std::make_unique<FakeReplica>(stats, std::move(onClosed));
                }));
        }

        QCOMPARE(manager.count(), std::size_t{101});
        QCOMPARE(stats->created, 101);
        QCOMPARE(ids.first(), ReplicaId{1});
        QCOMPARE(ids.last(), ReplicaId{101});
        QCOMPARE(manager.ids(), ids);
    }

    void removesOnlyTheRequestedReplica()
    {
        ReplicaManager manager;
        auto stats = std::make_shared<ReplicaStats>();
        const auto factory =
            [stats](ReplicaId, ReplicaManager::CloseCallback onClosed) {
                return std::make_unique<FakeReplica>(stats, std::move(onClosed));
            };
        const auto first = manager.create(factory);
        const auto second = manager.create(factory);
        const auto third = manager.create(factory);

        QVERIFY(manager.remove(second));

        QCOMPARE(manager.ids(), QList<ReplicaId>({first, third}));
        QCOMPARE(manager.count(), std::size_t{2});
        QCOMPARE(stats->destroyed, 1);
        QVERIFY(!manager.remove(second));
    }

    void closeAllIsSafeWhenCloseCallbacksRemoveEntries()
    {
        ReplicaManager manager;
        auto stats = std::make_shared<ReplicaStats>();
        for (int index = 0; index < 101; ++index) {
            static_cast<void>(manager.create(
                [stats](ReplicaId, ReplicaManager::CloseCallback onClosed) {
                    return std::make_unique<FakeReplica>(stats, std::move(onClosed));
                }));
        }

        manager.closeAll();

        QCOMPARE(manager.count(), std::size_t{0});
        QVERIFY(manager.ids().isEmpty());
        QCOMPARE(stats->closed, 101);
        QCOMPARE(stats->destroyed, 101);
    }
};

QTEST_GUILESS_MAIN(ReplicaManagerTest)

#include "replica_manager_test.moc"
