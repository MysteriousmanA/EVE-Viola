#include "features/region_preset/preset_service.h"

const WindowInfo* PresetService::findSource(const QList<WindowInfo>& windows, const QString& title)
{
    const WindowInfo* result = nullptr;
    for (const auto& window : windows) {
        if (window.title == title) {
            if (result != nullptr) { return nullptr; }
            result = &window;
        }
    }
    return result;
}
PresetService::RestorePlan PresetService::planRestore(const QList<ReplicaConfig>& replicas,
    const QList<WindowInfo>& windows, bool restoreWithGame)
{
    RestorePlan plan;
    for (const auto& replica : replicas) {
        if (restoreWithGame && !replica.sourceTitle.isEmpty()
            && findSource(windows, replica.sourceTitle) == nullptr) {
            plan.dormant.append(replica);
        } else {
            plan.active.append(replica);
        }
    }
    return plan;
}
QList<ReplicaConfig> PresetService::takeRestorable(ReplicaController& controller,
    const QList<WindowInfo>& windows)
{
    QList<ReplicaConfig> result;
    for (int index = controller.dormantConfigs().size() - 1; index >= 0; --index) {
        const auto config = controller.dormantConfigs().at(index);
        if (findSource(windows, config.sourceTitle) != nullptr) {
            result.append(config);
            static_cast<void>(controller.removeDormantAt(index));
        }
    }
    return result;
}
bool PresetService::save(AppConfig& config, int slot, const QList<ReplicaConfig>& replicas)
{
    if (slot < 0 || slot >= kPresetCount) { return false; }
    auto& preset = config.presets[slot];
    if (preset.name.isEmpty()) { preset.name = QStringLiteral("预设 %1").arg(slot + 1); }
    preset.hasConfig = true;
    preset.replicas = replicas;
    return true;
}
bool PresetService::load(AppConfig& config, int slot)
{
    if (slot < 0 || slot >= kPresetCount || !config.presets[slot].hasConfig) { return false; }
    config.replicas = config.presets[slot].replicas;
    return true;
}
bool PresetService::remove(AppConfig& config, int slot)
{
    if (slot < 0 || slot >= kPresetCount) { return false; }
    config.presets[slot] = PresetConfig{};
    if (config.defaultPresetSlot == slot + 1) { config.defaultPresetSlot = 0; }
    return true;
}
