#include <QtTest>

#include "features/replica/replica_workspace.h"

class ReplicaWorkspaceTest final : public QObject {
    Q_OBJECT

private slots:
    void preservesInsertionOrderAndDesiredConfig()
    {
        ReplicaWorkspace workspace;
        ReplicaConfig config;
        config.sourceTitle = QStringLiteral("EVE - Alpha");

        workspace.add(2);
        workspace.add(1, config);
        workspace.add(1);

        const QList<ReplicaId> expectedIds{2, 1};
        QCOMPARE(workspace.ids(), expectedIds);
        QCOMPARE(workspace.desiredConfig(1), config);
        QVERIFY(workspace.contains(2));
        QVERIFY(!workspace.contains(3));
    }

    void reordersOnlyWhenBothIdsExist()
    {
        ReplicaWorkspace workspace;
        workspace.add(1);
        workspace.add(2);
        workspace.add(3);

        QVERIFY(workspace.reorder(1, 3, true));
        const QList<ReplicaId> movedToEnd{2, 3, 1};
        QCOMPARE(workspace.ids(), movedToEnd);
        QVERIFY(workspace.reorder(1, 2, false));
        const QList<ReplicaId> restoredOrder{1, 2, 3};
        QCOMPARE(workspace.ids(), restoredOrder);
        QVERIFY(!workspace.reorder(1, 99, false));
        QCOMPARE(workspace.ids(), restoredOrder);
    }

    void groupsByContiguousKey()
    {
        ReplicaWorkspace workspace;
        workspace.add(1);
        workspace.add(2);
        workspace.add(3);
        workspace.add(4);

        const auto groups = workspace.groups([](ReplicaId id) {
            if (id == 1 || id == 2) {
                return QStringLiteral("source:100");
            }
            if (id == 3) {
                return QStringLiteral("source:200");
            }
            return QStringLiteral("source:100");
        });

        const QList<QList<ReplicaId>> expectedGroups{
            QList<ReplicaId>{1, 2},
            QList<ReplicaId>{3},
            QList<ReplicaId>{4},
        };
        QCOMPARE(groups, expectedGroups);
    }

    void collectionIsOrderedCommitsDesiredStateAndAppendsDormantConfigs()
    {
        ReplicaWorkspace workspace;
        ReplicaConfig desiredTwo;
        desiredTwo.sourceTitle = QStringLiteral("saved:2");
        ReplicaConfig desiredOne;
        desiredOne.sourceTitle = QStringLiteral("saved:1");
        workspace.add(2, desiredTwo);
        workspace.add(1, desiredOne);
        ReplicaConfig dormant;
        dormant.sourceTitle = QStringLiteral("EVE - Dormant");
        workspace.appendDormant(dormant);

        QList<ReplicaId> collectedIds;
        const auto snapshot = workspace.collectAndCommitConfigurations(
            [&collectedIds](ReplicaId id, ReplicaConfig config) {
                collectedIds.append(id);
                config.sourceTitle = QStringLiteral("active:%1").arg(id);
                return config;
            });

        const QList<ReplicaId> expectedIds{2, 1};
        QCOMPARE(collectedIds, expectedIds);
        QCOMPARE(workspace.desiredConfig(2).sourceTitle, QStringLiteral("active:2"));
        QCOMPARE(workspace.desiredConfig(1).sourceTitle, QStringLiteral("active:1"));

        QCOMPARE(snapshot.size(), 3);
        QCOMPARE(snapshot[0].sourceTitle, QStringLiteral("active:2"));
        QCOMPARE(snapshot[1].sourceTitle, QStringLiteral("active:1"));
        QCOMPARE(snapshot[2], dormant);
    }

    void emptyCollectorReturnsEmptyResult()
    {
        ReplicaWorkspace workspace;
        ReplicaConfig dormant;
        dormant.sourceTitle = QStringLiteral("EVE - Dormant");
        workspace.appendDormant(dormant);

        const auto snapshot = workspace.collectAndCommitConfigurations({});

        QVERIFY(snapshot.isEmpty());
    }

    void removalAlsoDropsDesiredState()
    {
        ReplicaWorkspace workspace;
        workspace.add(1);
        workspace.appendDormant({});

        QVERIFY(workspace.remove(1));
        QVERIFY(!workspace.contains(1));
        const ReplicaConfig emptyConfig;
        QCOMPARE(workspace.desiredConfig(1), emptyConfig);
        QVERIFY(!workspace.remove(1));
        workspace.clear();
        QVERIFY(workspace.ids().isEmpty());
        QVERIFY(workspace.dormantConfigs().isEmpty());
    }
};

QTEST_GUILESS_MAIN(ReplicaWorkspaceTest)
#include "replica_workspace_test.moc"
