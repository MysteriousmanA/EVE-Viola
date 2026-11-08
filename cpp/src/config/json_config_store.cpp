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
#include <QFile>
#include <QSet>

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
    const auto tolerances = parseColorTolerances(root.value(QStringLiteral("colorTolerances")).toArray());
    if (!tolerances.isEmpty()) { loaded.colorTolerances = tolerances; }
    loaded.regionPresets =
        parseRegionPresets(root.value(QStringLiteral("regionPresets")).toArray());
    if (!readJsonObject(colorRulesPath, &root, error)) {
        return false;
    }
    const bool hasColorRulesSection = root.contains(QStringLiteral("colorRules"));
    loaded.colorRules = parseColorRules(root.value(QStringLiteral("colorRules")).toArray(),
                                        &loaded.nextColorRuleNumber);
    QSet<QString> bindingIds;
    for (const auto& value : root.value(QStringLiteral("logicBindings")).toArray()) {
        const auto item = value.toObject();
        LogicBindingConfig binding;
        binding.id = item.value(QStringLiteral("id")).toString();
        if (binding.id.isEmpty() || bindingIds.contains(binding.id)) {
            binding.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
        bindingIds.insert(binding.id);
        binding.ruleNumber = item.value(QStringLiteral("ruleNumber")).toInt();
        binding.sourceTitle = item.value(QStringLiteral("sourceTitle")).toString();
        // Legacy HWNDs are session-specific and must never influence a new run.
        binding.sourceProcessName = item.value(QStringLiteral("sourceProcessName"))
            .toString(QStringLiteral("exefile.exe"));
        const auto windowIds = item.value(QStringLiteral("windowIds")).toObject();
        for (auto it = windowIds.begin(); it != windowIds.end(); ++it) {
            if (!it.value().toString().isEmpty()) { binding.windowIds.insert(it.key(), it.value().toString()); }
        }
        binding.enabled = item.value(QStringLiteral("enabled")).toBool(true);
        if (binding.ruleNumber > 0) { loaded.logicBindings.append(binding); }
    }
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

    struct Section {
        QString name;
        QJsonObject object;
        bool changed = false;
    };
    QList<Section> sections{
        {QStringLiteral("regionPresets.json"), regionPresetsSectionToJson(config.regionPresets)},
        {QStringLiteral("colorRules.json"), colorRulesSectionToJson(config.colorRules, config.nextColorRuleNumber, config.logicBindings)},
        {QStringLiteral("pagePartition.json"), pagePartitionConfigToJson(config.pagePartition)},
        {QStringLiteral("replicas.json"), replicasSectionToJson(config)},
    };
    for (int index = 0; index < kPresetCount; ++index) {
        sections.append({QStringLiteral("preset%1.json").arg(index + 1),
                         presetToJson(config.presets[index])});
    }
    sections[0].object.insert(QStringLiteral("colorTolerances"), colorTolerancesToJson(config.colorTolerances));

    // Read each section once, using the same snapshot for validation and change
    // detection. Validate the entire batch before replacing any existing file.
    for (auto& section : sections) {
        const auto path = sectionFilePath(directoryPath_, section.name);
        QJsonObject previous;
        if (!readJsonObject(path, &previous, error)) {
            return false;
        }
        section.changed = previous != section.object || !QFile::exists(path);
    }
    for (const auto& section : sections) {
        if (section.changed && !writeJsonObjectAtomically(
                sectionFilePath(directoryPath_, section.name), section.object, error)) {
            return false;
        }
    }
    return true;
}
