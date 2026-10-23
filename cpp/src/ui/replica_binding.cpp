#include "ui/replica_binding.h"
#include "features/replica/replica_controller.h"
#include "ui/replica_row.h"
#include "ui/replica_window.h"
#include <QPointer>

void ReplicaBinding::attach(ReplicaController& controller, ReplicaWindow& window,
    ReplicaRow& row, QObject* lifetime, std::function<bool()> restoring)
{
    const auto commit = [&controller, id = window.replicaId(), restoring = std::move(restoring)](
        const std::function<void(ReplicaConfig&)>& change) {
        if (!restoring()) { controller.update(id, change); }
    };
    QObject::connect(&window, &ReplicaWindow::settingsChanged, lifetime, [commit, &window] {
        commit([&window](ReplicaConfig& state) {
            state.colorLogicSlotRule[0] = window.colorLogicSlotRule(1);
            state.colorLogicSlotRule[1] = window.colorLogicSlotRule(2);
            state.colorLogicEnabled = window.colorLogicEnabled();
            state.activeColorLogicSlot = window.activeColorLogicSlot();
            state.slotColor1 = window.colorLogicSlotColor(1);
            state.slotColor2 = window.colorLogicSlotColor(2);
            state.sourceNameVisible = window.sourceNameVisible();
            state.statisticsColors = window.statisticsColors();
        });
    });
    QObject::connect(&window, &ReplicaWindow::sourceTitleChanged, lifetime,
        [commit, &window](const QString&) {
            if (window.source()) {
                commit([&window](ReplicaConfig& state) { state.sourceTitle = window.source()->title; });
            }
        });
    QObject::connect(&window, &ReplicaWindow::regionCaptureChanged, lifetime,
        [commit, &window](const QString&) {
            commit([&window](ReplicaConfig& state) { state.capturedRegion = window.capturedRegion().value_or(QRect{}); });
        });
    QObject::connect(&window, &ReplicaWindow::positionChanged, lifetime,
        [commit](const QPoint& position) { commit([position](ReplicaConfig& state) { state.position = position; }); });
    QObject::connect(&window, &ReplicaWindow::sizeChanged, lifetime,
        [commit](const QSize& size) { commit([size](ReplicaConfig& state) { state.size = size; }); });
    QObject::connect(&window, &ReplicaWindow::statisticsIntervalChanged, lifetime,
        [commit](int interval) { commit([interval](ReplicaConfig& state) { state.statisticsIntervalMs = interval; }); });
    QObject::connect(&window, &ReplicaWindow::colorLogicEnabledChanged, lifetime,
        [commit](bool enabled) { commit([enabled](ReplicaConfig& state) { state.colorLogicEnabled = enabled; }); });
    QObject::connect(&window, &ReplicaWindow::activeColorLogicSlotChanged, lifetime,
        [commit](int slot) { commit([slot](ReplicaConfig& state) { state.activeColorLogicSlot = slot; }); });
    QObject::connect(&row, &ReplicaRow::colorLogicEnabledChanged, lifetime,
        [commit](bool enabled) { commit([enabled](ReplicaConfig& state) { state.colorLogicEnabled = enabled; }); });
    QObject::connect(&row, &ReplicaRow::activeColorLogicSlotChanged, lifetime,
        [commit](int slot) { commit([slot](ReplicaConfig& state) { state.activeColorLogicSlot = slot; }); });
    QObject::connect(&row, &ReplicaRow::colorLogicSlotChanged, lifetime,
        [commit](int slot, int rule) {
            if (slot >= 1 && slot <= 2) {
                commit([slot, rule](ReplicaConfig& state) { state.colorLogicSlotRule[slot - 1] = rule; });
            }
        });
    QObject::connect(&row, &ReplicaRow::sourceNameVisibilityChanged, lifetime,
        [commit](bool visible) { commit([visible](ReplicaConfig& state) { state.sourceNameVisible = visible; }); });
    QObject::connect(&row, &ReplicaRow::collapseChanged, lifetime,
        [commit](bool collapsed) { commit([collapsed](ReplicaConfig& state) { state.collapsed = collapsed; }); });
    QObject::connect(&row, &ReplicaRow::colorLogicSlotColorChanged, lifetime,
        [commit](int slot, const QColor& color) {
            commit([slot, color](ReplicaConfig& state) {
                if (slot == 1) { state.slotColor1 = color; }
                else if (slot == 2) { state.slotColor2 = color; }
            });
        });
    // Qt may defer geometry events while a widget is hidden. Record the
    // accepted command result immediately so autosave still sees that edit.
    const QPointer<ReplicaWindow> target(&window);
    const QPointer<ReplicaRow> view(&row);
    QObject::connect(&row, &ReplicaRow::positionEditRequested, lifetime,
        [commit, target, view](const QPoint&) {
            if (target) {
                commit([target](ReplicaConfig& state) { state.position = target->pos(); });
                if (view) { view->setWindowPosition(target->pos()); }
            }
        });
    QObject::connect(&row, &ReplicaRow::scalePercentageEdited, lifetime,
        [commit, target, view](double) {
            if (target) {
                commit([target](ReplicaConfig& state) { state.size = target->size(); });
                if (view) { view->setWindowSize(target->size()); }
            }
        });
}
