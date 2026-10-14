#include <stdexcept>
#include <QtTest>
#include "app/activation_coordinator.h"
#include "features/region_preset/preset_service.h"
#include "features/replica/replica_controller.h"
#include "runtime/runtime_policy.h"
#include "platform/window_catalog.h"

namespace {
class Replica final : public CloseableReplica {
public:
    explicit Replica(int& closed) : closed_(closed) {}
    void closeReplica() noexcept override { ++closed_; }
private:
    int& closed_;
};
ReplicaId create(ReplicaController& controller, int& closed)
{
    return controller.create([&closed](ReplicaId, ReplicaManager::CloseCallback) {
        return std::make_unique<Replica>(closed);
    });
}
}

class ArchitectureServicesTest final : public QObject {
    Q_OBJECT
private slots:
    void snapshotsPreserveOrderAndDormantConfiguration()
    {
        int closed = 0;
        ReplicaController controller;
        const auto first = create(controller, closed);
        const auto second = create(controller, closed);
        controller.update(first, [](ReplicaConfig& state) { state.sourceTitle = QStringLiteral("Alpha"); });
        controller.update(second, [](ReplicaConfig& state) { state.sourceTitle = QStringLiteral("Beta"); });
        ReplicaConfig dormant;
        dormant.sourceTitle = QStringLiteral("Gamma");
        controller.appendDormant(dormant);
        QVERIFY(controller.reorder(second, first, false));
        const auto snapshot = controller.snapshot();
        QCOMPARE(snapshot.size(), 3);
        QCOMPARE(snapshot[0].sourceTitle, QStringLiteral("Beta"));
        QCOMPARE(snapshot[1].sourceTitle, QStringLiteral("Alpha"));
        QCOMPARE(snapshot[2], dormant);
        controller.closeAll();
        QCOMPARE(closed, 2);
        QCOMPARE(controller.count(), std::size_t{0});
    }
    void hiddenWindowInvariantsBelongToController()
    {
        int closed = 0;
        ReplicaController controller;
        const auto id = create(controller, closed);
        controller.update(id, [](ReplicaConfig& state) {
            state.colorLogicSlotRule[0] = 7;
            state.colorLogicSlotRule[1] = 9;
            state.activeColorLogicSlot = 2;
        });
        const auto hidden = controller.setUserVisible(id, false);
        QVERIFY(!hidden.visible);
        QVERIFY(hidden.colorLogicEnabled);
        QCOMPARE(hidden.colorLogicSlotRule[0], 7);
        QCOMPARE(hidden.colorLogicSlotRule[1], 0);
        QCOMPARE(hidden.activeColorLogicSlot, 1);
        QCOMPARE(controller.snapshot().first(), hidden);
        QVERIFY(controller.setUserVisible(id, true).visible);
    }
    void temporaryModesComposeWithoutChangingUserState()
    {
        RuntimePolicy policy;
        QVERIFY(policy.replicaVisible(true));
        policy.regionCapture = true;
        policy.fishMode = true;
        policy.regionCapture = false;
        QVERIFY(!policy.replicaVisible(true));
        QVERIFY(!policy.audioEnabled());
        QVERIFY(!policy.automaticActivation(true));
        policy.fishMode = false;
        QVERIFY(policy.replicaVisible(true));
        QVERIFY(!policy.replicaVisible(false));
        QVERIFY(!policy.automaticActivation(false));
        policy.regionCapture = true;
        QVERIFY(!policy.automaticActivation(true));
        QVERIFY(policy.audioEnabled());
    }
    void restoreDefersAmbiguousOrMissingSourcesWithoutLosingConfiguration()
    {
        ReplicaConfig known, missing;
        known.sourceTitle = QStringLiteral("Alpha");
        known.capturedRegion = QRect(1200, 20, 640, 480);
        missing.sourceTitle = QStringLiteral("Beta");
        const QList<WindowInfo> duplicate{{1, known.sourceTitle}, {2, known.sourceTitle}};
        const auto ambiguous = PresetService::planRestore({known, missing}, duplicate, true);
        QVERIFY(ambiguous.active.isEmpty());
        QCOMPARE(ambiguous.dormant, QList<ReplicaConfig>({known, missing}));
        const auto plan = PresetService::planRestore({known, missing}, {{3, known.sourceTitle}}, true);
        QCOMPARE(plan.active, QList<ReplicaConfig>({known}));
        QCOMPARE(plan.dormant, QList<ReplicaConfig>({missing}));
        int closed = 0;
        ReplicaController controller;
        controller.appendDormant(known);
        controller.appendDormant(missing);
        QCOMPARE(PresetService::takeRestorable(controller, {{3, known.sourceTitle}}), plan.active);
        QCOMPARE(controller.dormantConfigs(), plan.dormant);
        QVERIFY(PresetService::takeRestorable(controller, {{3, known.sourceTitle}}).isEmpty());
    }
    void presetsKeepSnapshotsIndependentOfLaterEdits()
    {
        AppConfig config;
        ReplicaConfig replica;
        replica.position = QPoint(42, -100);
        QVERIFY(PresetService::save(config, 0, {replica}));
        config.replicas = {ReplicaConfig{}};
        QVERIFY(PresetService::load(config, 0));
        QCOMPARE(config.replicas.first(), replica);
        config.defaultPresetSlot = 1;
        QVERIFY(PresetService::remove(config, 0));
        QCOMPARE(config.defaultPresetSlot, 0);
        QVERIFY(!PresetService::load(config, 0));
        QVERIFY(!PresetService::save(config, 3, {}));
    }
    void activationPolicyUsesInjectedClockAndMouseQuietPeriod()
    {
        qint64 now = 1000;
        QList<std::uintptr_t> activated;
        ActivationCoordinator coordinator([&activated](std::uintptr_t hwnd) {
            activated.append(hwnd); return hwnd;
        }, [&now] { return now; });
        QVERIFY(coordinator.activateWindowWithCooldown(1, 1000));
        QVERIFY(!coordinator.activateWindowWithCooldown(2, 1000));
        now = 2000;
        QVERIFY(coordinator.activateWindowWithCooldown(2, 1000));
        coordinator.setAutomaticActivationMouseQuietPeriodMs(500);
        coordinator.noteEveMouseButtonPressed();
        coordinator.queueAutomaticActivation(1, 3, 0);
        coordinator.flushAutomaticActivations();
        QCOMPARE(activated.size(), 2);
        coordinator.noteEveMouseButtonReleased();
        now += 499;
        coordinator.queueAutomaticActivation(1, 3, 0);
        coordinator.flushAutomaticActivations();
        QCOMPARE(activated.size(), 2);
        ++now;
        coordinator.queueAutomaticActivation(1, 3, 0);
        coordinator.flushAutomaticActivations();
        QCOMPARE(activated.last(), std::uintptr_t{3});
    }
    void catalogSharesSnapshotsAndDoesNotTreatFailuresAsWindowExit()
    {
        qint64 now = 0;
        int calls = 0;
        bool fail = false;
        WindowCatalog catalog([&] {
            ++calls;
            if (fail) { throw std::runtime_error("enumeration failed"); }
            return QList<WindowInfo>{{1, QStringLiteral("Alpha")}};
        }, [&now] { return now; });
        QCOMPARE(catalog.windows().size(), 1);
        QCOMPARE(catalog.windows().size(), 1);
        QCOMPARE(calls, 1);
        now = 100;
        QCOMPARE(catalog.windows().size(), 1);
        QCOMPARE(calls, 2);
        fail = true;
        QVERIFY_EXCEPTION_THROWN(static_cast<void>(catalog.windows(true)), std::runtime_error);
        // The last successful snapshot is still valid inside its cache window.
        QCOMPARE(catalog.windows().first().title, QStringLiteral("Alpha"));
    }
};
QTEST_GUILESS_MAIN(ArchitectureServicesTest)
#include "architecture_services_test.moc"
