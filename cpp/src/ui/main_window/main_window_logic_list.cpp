#include "ui/main_window.h"
#include "features/color_rule/logic_binding_plan.h"
#include "ui/logic_binding_runtime.h"
#include "ui/logic_binding_source.h"
#include "ui/pages/logic_list/logic_list_page.h"
#include "ui/replica_row.h"
#include "ui/replica_window.h"
#include "platform/window_catalog.h"
#include <QComboBox>
#include <QHash>
#include <QScopedValueRollback>
#include <QSet>
#include <QTimer>

QWidget* MainWindow::buildLogicListPage()
{
    logicListPage_ = new LogicListPage;
    const auto changed = [this] {
        config_.replicas = synchronizeLogicBindingReplicas(replicaController_.snapshot(), true);
        schedulePersistReplicas();
        QTimer::singleShot(0, this, [this] {
            if (!closing_) { setupGameRestoreMonitoring(); refreshLogicBindings(); }
        });
    };
    connect(logicListPage_, &LogicListPage::addRequested, this, [this, changed](int number) {
        LogicBindingConfig binding; binding.ruleNumber = number;
        config_.logicBindings.append(binding); changed();
    });
    connect(logicListPage_, &LogicListPage::addForSourcesRequested, this,
        [this, changed](int number, const QList<WindowInfo>& sources) {
            if (sources.isEmpty()) { return; }
            for (const auto& source : sources) {
                LogicBindingConfig binding;
                binding.ruleNumber = number;
                binding.sourceTitle = source.title;
                binding.sourceProcessName = source.processName;
                config_.logicBindings.append(binding);
            }
            changed();
        });
    connect(logicListPage_, &LogicListPage::removeRequested, this, [this, changed](const QString& id) {
        config_.logicBindings.removeIf([&](const auto& binding) { return binding.id == id; }); changed();
    });
    connect(logicListPage_, &LogicListPage::sourceChanged, this,
        [this, changed](const QString& id, const WindowInfo& source) {
            for (auto& binding : config_.logicBindings) {
                if (binding.id == id) { binding.sourceTitle = source.title; binding.sourceProcessName = source.processName; break; }
            }
            changed();
        });
    connect(logicListPage_, &LogicListPage::enabledChanged, this,
        [this, changed](const QString& id, bool enabled) {
            for (auto& binding : config_.logicBindings) { if (binding.id == id) { binding.enabled = enabled; break; } }
            changed();
        });
    connect(logicListPage_, &LogicListPage::refreshRequested, this, [this] { refreshLogicBindings(true); });
    return logicListPage_;
}

QList<ReplicaConfig> MainWindow::synchronizeLogicBindingReplicas(
    const QList<ReplicaConfig>& replicas, bool materialize, bool createMissing)
{
    QList<ReplicaConfig> synchronized = replicas;
    QHash<QString, int> indexById;
    for (int index = 0; index < synchronized.size(); ++index) {
        if (!synchronized[index].id.isEmpty()) { indexById.insert(synchronized[index].id, index); }
    }

    QList<WindowInfo> sources;
    try {
        if (windowCatalog_) { sources = windowCatalog_->windows(); }
    } catch (const NativeException&) {
    }

    QHash<QString, ReplicaConfig> required;
    QHash<QString, WindowInfo> selectedSources;
    const auto templates = colorLogicTemplates();
    int positionIndex = synchronized.size();
    for (auto& binding : config_.logicBindings) {
        // Wait for the source so adding a row cannot create a redundant placeholder.
        if (binding.sourceTitle.isEmpty() && binding.windowIds.isEmpty()) { continue; }
        auto unassigned = binding;
        unassigned.windowIds.clear();
        const auto plan = planLogicBinding(unassigned, config_.colorRules, templates, 1);
        if (!plan.error.isEmpty()) { continue; }

        const auto* chosen = resolveLogicBindingSource(binding, sources);
        const bool sourceResolved = chosen != nullptr;

        for (auto window : plan.windows) {
            const auto templateId = window.id.mid(QStringLiteral("logic:%1:").arg(binding.id).size());
            const auto previousId = binding.windowIds.value(templateId, window.id);
            const auto compatible = [&](const ReplicaConfig& candidate, bool allowUnbound) {
                if (candidate.sourceTitle != binding.sourceTitle
                    && !(allowUnbound && candidate.sourceTitle.isEmpty())) { return false; }
                for (auto it = windows_.cbegin(); it != windows_.cend(); ++it) {
                    if (replicaController_.desiredConfig(it.key()).id != candidate.id || !it.value()) { continue; }
                    const auto source = it.value()->source();
                    if (source && (source->title != binding.sourceTitle
                        || source->processName.compare(binding.sourceProcessName, Qt::CaseInsensitive) != 0)) { return false; }
                }
                bool knownPreset = false;
                for (const auto& other : config_.logicBindings) {
                    for (auto it = other.windowIds.cbegin(); it != other.windowIds.cend(); ++it) {
                        if (it.value() != candidate.id) { continue; }
                        if (it.key() != templateId) { return false; }
                        knownPreset = true;
                        if (other.id != binding.id && candidate.sourceTitle.isEmpty()
                            && other.sourceTitle != binding.sourceTitle) { return false; }
                    }
                }
                if (!knownPreset && candidate.id.startsWith(QStringLiteral("logic:"))) {
                    if (!candidate.id.endsWith(QStringLiteral(":") + templateId)) { return false; }
                    knownPreset = true;
                }
                return knownPreset || candidate.capturedRegion == window.capturedRegion;
            };
            int reuse = -1;
            // Keep a saved association even after the user fine-tunes its rectangle.
            const auto previous = indexById.constFind(previousId);
            if (previous != indexById.cend() && compatible(synchronized.at(*previous), false)) {
                reuse = *previous;
            }
            for (int index = 0; reuse < 0 && index < synchronized.size(); ++index) {
                if (compatible(synchronized.at(index), false)) { reuse = index; }
            }
            if (reuse < 0 && previous != indexById.cend()
                && compatible(synchronized.at(*previous), true)) { reuse = *previous; }
            for (int index = 0; reuse < 0 && index < synchronized.size(); ++index) {
                if (compatible(synchronized.at(index), true)) { reuse = index; }
            }
            // Startup refreshes can run before saved windows have been restored.
            // Keep their saved associations until materialization is requested.
            if (reuse < 0 && !createMissing) { continue; }
            if (reuse >= 0) {
                window.id = synchronized.at(reuse).id;
            } else if (indexById.contains(window.id)) {
                // A different source still uses this instance's original window.
                window.id = QStringLiteral("logic:%1:%2:%3").arg(binding.id,
                    QUuid::createUuid().toString(QUuid::WithoutBraces), templateId);
            }
            binding.windowIds.insert(templateId, window.id);
            const auto existing = indexById.constFind(window.id);
            if (existing == indexById.cend()) {
                window.visible = true;
                window.position = QPoint(x() + width() + 18 + (positionIndex % 10) * 24,
                    y() + 40 + (positionIndex % 10) * 24);
                window.sourceTitle = binding.sourceTitle;
                indexById.insert(window.id, synchronized.size());
                synchronized.append(window);
                ++positionIndex;
            } else {
                const auto targets = window.statisticsColors;
                window = synchronized.at(existing.value());
                window.statisticsColors = targets;
                window.sourceTitle = binding.sourceTitle;
                synchronized[existing.value()] = window;
            }
            required.insert(window.id, window);
            if (sourceResolved) { selectedSources.insert(window.id, *chosen); }
        }
    }

    if (!materialize) { return synchronized; }

    QHash<QString, ReplicaId> activeIds;
    for (const auto id : replicaController_.ids()) {
        const auto configId = replicaController_.desiredConfig(id).id;
        activeIds.insert(configId, id);
        if (auto* row = rows_.value(id, nullptr)) {
            const bool managed = required.contains(configId);
            row->sourceCombo()->setEnabled(!managed);
            row->sourceCombo()->setToolTip(managed ? QStringLiteral("源窗口由逻辑列表统一管理")
                : QStringLiteral("选择该小窗绑定的 EVE 窗口"));
        }
    }
    const auto oldDormant = replicaController_.dormantConfigs();
    QHash<QString, ReplicaConfig> dormantById;
    for (const auto& config : oldDormant) { dormantById.insert(config.id, config); }

    QList<ReplicaConfig> dormantToKeep;
    QList<ReplicaConfig> replicasToRestore;
    QSet<QString> handledDormant;
    for (const auto& dormant : oldDormant) {
        if (activeIds.contains(dormant.id) && required.contains(dormant.id)) { continue; }
        if (!required.contains(dormant.id)) {
            dormantToKeep.append(dormant);
            continue;
        }
        const auto updated = required.value(dormant.id);
        handledDormant.insert(dormant.id);
        if (updated.sourceTitle.isEmpty() || selectedSources.contains(updated.id)
            || !config_.restoreWindowsWithGame) {
            replicasToRestore.append(updated);
        } else {
            dormantToKeep.append(updated);
        }
    }

    for (auto it = required.cbegin(); it != required.cend(); ++it) {
        const auto id = it.key();
        const auto& config = it.value();
        if (activeIds.contains(id)) {
            const auto replicaId = activeIds.value(id);
            replicaController_.setDesiredConfig(replicaId, config);
            auto* window = windows_.value(replicaId, nullptr);
            if (!window) { continue; }
            window->setStatisticsColors(config.statisticsColors);
            if (auto* row = rows_.value(replicaId, nullptr)) {
                if (row->statisticsColors() != config.statisticsColors) { row->setStatisticsColors(config.statisticsColors); }
                row->sourceCombo()->setEnabled(false);
                row->sourceCombo()->setToolTip(QStringLiteral("源窗口由逻辑列表统一管理"));
            }
            const auto selected = selectedSources.constFind(id);
            if (selected != selectedSources.cend()) {
                const auto current = window->source();
                if (!current || current->hwnd != selected->hwnd) {
                    window->selectSource(selected.value());
                    if (config.capturedRegion.isValid() && !config.capturedRegion.isEmpty()) {
                        window->setCapturedRegion(config.capturedRegion);
                    }
                }
            } else {
                const auto current = window->source();
                if (current && (config.sourceTitle.isEmpty() || current->title != config.sourceTitle)) {
                    window->selectSource(WindowInfo{});
                }
                if (!window->source()) {
                    if (auto* row = rows_.value(replicaId, nullptr)) { row->setSourceTitle(config.sourceTitle); }
                }
            }
            continue;
        }
        if (dormantById.contains(id)) { continue; }
        if (!createMissing) { continue; }
        if (config_.restoreWindowsWithGame && !config.sourceTitle.isEmpty()
            && !selectedSources.contains(id)) {
            dormantToKeep.append(config);
        } else {
            replicasToRestore.append(config);
        }
    }

    if (dormantToKeep != oldDormant || !handledDormant.isEmpty()) {
        replicaController_.clearDormant();
        for (const auto& config : dormantToKeep) { replicaController_.appendDormant(config); }
    }
    for (const auto& config : replicasToRestore) { restoreReplica(config, sources); }
    refreshReplicaFilters();
    refreshReplicaGroups();
    return synchronized;
}

void MainWindow::refreshLogicBindings(bool force)
{
    if (refreshingLogicBindings_ || closing_ || !logicListPage_ || !windowCatalog_) { return; }
    const QScopedValueRollback guard(refreshingLogicBindings_, true);
    QList<WindowInfo> sources;
    try { sources = windowCatalog_->windows(force); } catch (const NativeException&) {}
    static_cast<void>(synchronizeLogicBindingReplicas(replicaController_.snapshot(), true, false));
    QHash<QString, ReplicaWindow*> replicas;
    for (const auto id : replicaController_.ids()) {
        auto* window = windows_.value(id, nullptr);
        const auto config = replicaController_.desiredConfig(id);
        if (window && !config.id.isEmpty()) { replicas.insert(config.id, window); }
    }
    logicBindingRuntime_->configure(config_.colorRules, config_.logicBindings,
        colorLogicTemplates(), sources, replicas, force);
    logicListPage_->setConfiguration(config_.colorRules, config_.logicBindings, sources,
        logicBindingRuntime_->statuses());
}
