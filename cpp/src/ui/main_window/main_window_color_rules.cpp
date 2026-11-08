#include "ui/common/editor_dialogs.h"
#include "ui/main_window.h"
#include "features/color_rule/logic_binding_plan.h"
#include "ui/window_color_logic_runtime.h"

#include "ui/pages/color_rule/color_rule_page.h"
#include "ui/pages/color_rule/color_rule_controller.h"
#include "ui/desktop_pet_page.h"
#include "ui/replica_row.h"
#include "ui/replica_window.h"
#include "ui/pixel_color_editor.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

QWidget* MainWindow::buildColorRulePage()
{
    colorRulePage_ = new ColorRulePage;
    connect(colorRulePage_, &ColorRulePage::colorTolerancesRequested, this, [this] {
        auto draft = config_;
        EditorDialogs::Dialog dialog(this);
        dialog.setObjectName(QStringLiteral("colorToleranceSettingsDialog"));
        dialog.setWindowTitle(QStringLiteral("颜色容差设置"));
        dialog.resize(520, 240);
        auto* layout = new QVBoxLayout(&dialog);
        auto* description = new QLabel(QStringLiteral(
            "通用设置适用于所有未单独设置的区域。区域覆盖对该预设的所有小窗和逻辑条件生效。"));
        description->setWordWrap(true);
        layout->addWidget(description);
        auto* scope = new QComboBox;
        scope->setObjectName(QStringLiteral("colorToleranceScope"));
        scope->addItem(QStringLiteral("通用设置"));
        for (const auto& preset : draft.regionPresets) { scope->addItem(preset.name); }
        layout->addWidget(scope);
        auto* inherit = new QCheckBox(QStringLiteral("使用通用设置"));
        inherit->setObjectName(QStringLiteral("inheritColorTolerances"));
        layout->addWidget(inherit);
        auto* edit = new QPushButton(QStringLiteral("设置颜色及容差…"));
        edit->setObjectName(QStringLiteral("editColorTolerances"));
        layout->addWidget(edit);
        auto* summary = new QLabel;
        summary->setWordWrap(true);
        layout->addWidget(summary);
        const auto refresh = [&] {
            const int index = scope->currentIndex() - 1;
            inherit->setVisible(index >= 0);
            inherit->setChecked(index >= 0 && draft.regionPresets[index].colorTolerances.isEmpty());
            edit->setEnabled(index < 0 || !inherit->isChecked());
            const auto targets = regionColorTolerances(draft, index < 0 ? QString{} : draft.regionPresets[index].name);
            QStringList entries;
            for (const auto& target : targets) {
                entries.append(QStringLiteral("%1：%2").arg(target.color,
                    target.tolerance < 0 ? QStringLiteral("预设范围") : QString::number(target.tolerance)));
            }
            summary->setText(entries.join(QStringLiteral("；")));
        };
        connect(scope, &QComboBox::currentIndexChanged, &dialog, refresh);
        connect(inherit, &QCheckBox::clicked, &dialog, [&](bool checked) {
            const int index = scope->currentIndex() - 1;
            if (index < 0) { return; }
            draft.regionPresets[index].colorTolerances = checked ? QList<PixelColorTarget>{} : draft.colorTolerances;
            refresh();
        });
        connect(edit, &QPushButton::clicked, &dialog, [&] {
            const int index = scope->currentIndex() - 1;
            auto& targets = index < 0 ? draft.colorTolerances : draft.regionPresets[index].colorTolerances;
            if (const auto result = promptStatisticsColors(&dialog, targets)) { targets = *result; refresh(); }
        });
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        refresh();
        if (dialog.exec() != QDialog::Accepted) { return; }
        config_.colorTolerances = draft.colorTolerances;
        // Nested editors run an event loop; only commit the matching preset's policy.
        for (auto& preset : config_.regionPresets) {
            for (const auto& edited : draft.regionPresets) {
                if (preset.name == edited.name) { preset.colorTolerances = edited.colorTolerances; break; }
            }
        }
        refreshColorLogicWindows();
        persistConfiguration();
    });
    colorRuleController_ = new ColorRuleController(config_, this);
    colorRuleController_->setReplicaProvider([this] { return colorLogicTemplates(); });
    colorRuleController_->setRegionPresetNameProvider([this] { return colorLogicRegionPresetNames(); });
    connect(colorRuleController_, &ColorRuleController::rulesChanged, this, &MainWindow::refreshColorRules);
    connect(colorRuleController_, &ColorRuleController::persistRequested, this, &MainWindow::persistConfiguration);
    connect(colorRulePage_, &ColorRulePage::addRequested, colorRuleController_, &ColorRuleController::createColorRule);
    connect(
        colorRulePage_, &ColorRulePage::editRequested,
        colorRuleController_, &ColorRuleController::editColorRuleNumber);
    connect(
        colorRulePage_, &ColorRulePage::deleteRequested,
        colorRuleController_, &ColorRuleController::deleteColorRuleNumber);
    return colorRulePage_;
}

void MainWindow::refreshColorRules()
{
    colorRulePage_->setRules(config_.colorRules);
    desktopPetPage_->setColorRules(config_.colorRules);
    for (auto* window : windows_) {
        window->setColorRules(config_.colorRules);
    }
    refreshColorLogicWindows();
}

void MainWindow::refreshColorLogicWindows()
{
    if (!colorRulePage_) { return; }
    const auto replicas = colorLogicTemplates();
    colorRulePage_->setReplicas(replicas, colorLogicRegionPresetNames());
    colorRulePage_->setRules(config_.colorRules);
    // Resolve by binding identity first, so fine-tuned window rectangles retain their policy.
    for (auto it = windows_.cbegin(); it != windows_.cend(); ++it) {
        if (!it.value()) { continue; }
        const auto saved = replicaController_.desiredConfig(it.key());
        QString name;
        bool bound = false;
        for (const auto& binding : config_.logicBindings) {
            for (auto entry = binding.windowIds.cbegin(); entry != binding.windowIds.cend(); ++entry) {
                if (entry.value() == saved.id) {
                    name = entry.key().mid(QStringLiteral("region-preset:").size());
                    bound = true;
                    break;
                }
            }
            if (bound) { break; }
        }
        auto* row = rows_.value(it.key());
        if (!bound && row) { name = row->currentRegionPresetName(); }
        const auto targets = regionColorTolerances(config_, name);
        it.value()->setStatisticsColors(targets);
        if (row && row->statisticsColors() != targets) { row->setStatisticsColors(targets); }
    }
    refreshLogicBindings();
    for (auto it = windows_.cbegin(); it != windows_.cend(); ++it) {
        const auto config = replicaController_.desiredConfig(it.key());
        windowColorLogicRuntime_->attach(config.id, it.value());
        QStringList references;
        for (const auto& rule : config_.colorRules) {
            bool referenced = rule.targetReplicaId == config.id;
            for (const auto& condition : rule.windowConditions) {
                referenced = referenced || condition.replicaId == config.id;
            }
            for (const auto& setting : rule.windowSwitches) {
                referenced = referenced || setting.replicaId == config.id;
            }
            if (referenced) { references.append(QStringLiteral("#%1 %2").arg(rule.number).arg(rule.note)); }
        }
        if (auto row = rows_.value(it.key())) {
            row->setColorLogicSummary(references.isEmpty() ? QStringLiteral("未被图色逻辑引用")
                : QStringLiteral("关联图色逻辑：%1").arg(references.join(QStringLiteral("、"))));
        }
    }
}

QHash<QString, QString> MainWindow::colorLogicRegionPresetNames() const
{
    QHash<QString, QString> names;
    for (const auto& preset : config_.regionPresets) {
        names.insert(logicRegionPresetId(preset.name), preset.name);
    }
    return names;
}

QList<ReplicaConfig> MainWindow::colorLogicTemplates() const
{
    auto presets = config_.regionPresets;
    for (auto& preset : presets) { preset.colorTolerances = regionColorTolerances(config_, preset.name); }
    return logicRegionTemplates(presets);
}
