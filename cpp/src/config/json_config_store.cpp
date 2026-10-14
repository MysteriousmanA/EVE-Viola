#include "config/json_config_store.h"
#include "config/config_types.h"
#include "config/json_document_io.h"
#include "config/serialization/region_presets_json.h"
#include "config/serialization/color_rules_json.h"
#include "config/serialization/replicas_json.h"
#include "config/serialization/page_partition_json.h"
#include "config/serialization/presets_json.h"
#include "config/serialization/quick_switch_json.h"
#include "config/serialization/application_options_json.h"
#include <QDir>

using namespace ConfigDocumentIO;
using namespace ConfigSerialization;

JsonConfigStore::JsonConfigStore(QString directoryPath) : directoryPath_(std::move(directoryPath))
{
}

const QString& JsonConfigStore::directoryPath() const noexcept
{
    return directoryPath_;
}

bool JsonConfigStore::load(AppConfig& config, QString* error) const
{
    if (error != nullptr) {
        error->clear();
    }
    AppConfig loaded;
    QJsonObject root;
    const QString regionPresetsPath =
        sectionFilePath(directoryPath_, QStringLiteral("regionPresets.json"));
    const QString colorRulesPath =
        sectionFilePath(directoryPath_, QStringLiteral("colorRules.json"));
    const QString replicasPath = sectionFilePath(directoryPath_, QStringLiteral("replicas.json"));
    const QString pagePartitionPath =
        sectionFilePath(directoryPath_, QStringLiteral("pagePartition.json"));
    if (!readJsonObject(regionPresetsPath, &root, error)) {
        return false;
    }
    const bool hasRegionPresetsSection = root.contains(QStringLiteral("regionPresets"));
    loaded.regionPresets =
        parseRegionPresets(root.value(QStringLiteral("regionPresets")).toArray());
    if (!readJsonObject(colorRulesPath, &root, error)) {
        return false;
    }
    const bool hasColorRulesSection = root.contains(QStringLiteral("colorRules"));
    loaded.colorRules = parseColorRules(root.value(QStringLiteral("colorRules")).toArray(),
                                        &loaded.nextColorRuleNumber);
    loaded.nextColorRuleNumber = qMax(loaded.nextColorRuleNumber,
                                      root.value(QStringLiteral("nextColorRuleNumber")).toInt(1));
    if (!readJsonObject(pagePartitionPath, &root, error)) {
        return false;
    }
    loaded.pagePartition = parsePagePartitionConfig(root);
    if (!readJsonObject(replicasPath, &root, error)) {
        return false;
    }
    const bool hasReplicasSection = root.contains(QStringLiteral("replicas"));
    loaded.replicas = parseReplicas(root.value(QStringLiteral("replicas")).toArray());
    parseApplicationOptions(root, loaded);
    parseQuickSwitchConfig(root, loaded);
    std::array<QJsonObject, kPresetCount> presetRoots;
    for (int index = 0; index < kPresetCount; ++index) {
        if (!readJsonObject(
                sectionFilePath(directoryPath_, QStringLiteral("preset%1.json").arg(index + 1)),
                &root, error)) {
            return false;
        }
        presetRoots[index] = root;
        if (root.isEmpty()) {
            continue;
        }
        loaded.presets[index] = parsePresetObject(root);
    }

    // Older cache versions duplicated global sections inside preset files.
    // Fall back when the split file is absent or does not contain that section.
    if (!hasRegionPresetsSection) {
        for (const auto& presetRoot : presetRoots) {
            if (presetRoot.contains(QStringLiteral("regionPresets"))) {
                loaded.regionPresets =
                    parseRegionPresets(presetRoot.value(QStringLiteral("regionPresets")).toArray());
                break;
            }
        }
    }
    if (!hasColorRulesSection) {
        for (const auto& presetRoot : presetRoots) {
            if (presetRoot.contains(QStringLiteral("colorRules"))) {
                loaded.colorRules =
                    parseColorRules(presetRoot.value(QStringLiteral("colorRules")).toArray(),
                                    &loaded.nextColorRuleNumber);
                loaded.nextColorRuleNumber =
                    qMax(loaded.nextColorRuleNumber,
                         presetRoot.value(QStringLiteral("nextColorRuleNumber")).toInt(1));
                break;
            }
        }
    }
    if (!hasReplicasSection && loaded.defaultPresetSlot == 0) {
        for (int index = 0; index < kPresetCount; ++index) {
            if (loaded.presets[index].hasConfig) {
                loaded.defaultPresetSlot = index + 1;
                loaded.replicas = loaded.presets[index].replicas;
                break;
            }
        }
    }
    config = std::move(loaded);
    return true;
}

bool JsonConfigStore::save(const AppConfig& config, QString* error) const
{
    if (error != nullptr) {
        error->clear();
    }
    QDir directory(directoryPath_);
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        return fail(error, QStringLiteral("无法创建配置目录：%1").arg(directory.path()));
    }

    // Validate every existing section before replacing any of them. A failed
    // load must never turn a subsequent autosave into a destructive reset.
    AppConfig previous;
    if (!load(previous, error)) {
        return false;
    }

    if (!writeJsonObject(sectionFilePath(directoryPath_, QStringLiteral("regionPresets.json")),
                         regionPresetsSectionToJson(config.regionPresets), error)) {
        return false;
    }
    if (!writeJsonObject(sectionFilePath(directoryPath_, QStringLiteral("colorRules.json")),
                         colorRulesSectionToJson(config.colorRules, config.nextColorRuleNumber),
                         error)) {
        return false;
    }
    if (!writeJsonObject(sectionFilePath(directoryPath_, QStringLiteral("pagePartition.json")),
                         pagePartitionConfigToJson(config.pagePartition), error)) {
        return false;
    }
    if (!writeJsonObject(sectionFilePath(directoryPath_, QStringLiteral("replicas.json")),
                         replicasSectionToJson(config), error)) {
        return false;
    }

    for (int index = 0; index < kPresetCount; ++index) {
        if (!writeJsonObject(
                sectionFilePath(directoryPath_, QStringLiteral("preset%1.json").arg(index + 1)),
                presetToJson(config.presets[index]), error)) {
            return false;
        }
    }
    return true;
}
