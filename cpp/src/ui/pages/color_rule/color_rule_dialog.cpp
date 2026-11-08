#include "ui/common/editor_dialogs.h"
#include "ui/pages/color_rule/color_rule_dialog.h"
#include "ui/pages/color_rule/replica_choice_label.h"
#include "features/color_rule/trigger_condition.h"
#include "features/color_rule/color_rule_config.h"
#include "features/replica/pixel_statistics.h"
#include "ui/pixel_color_editor.h"
#include <QComboBox>
#include <QCheckBox>
#include <QColorDialog>
#include <QScrollArea>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QSet>
#include <QTabWidget>
#include <QVBoxLayout>

std::optional<ColorRule> promptColorRule(QWidget *parent, const ColorRule &initial,
    const QList<ReplicaConfig>& availableReplicas, const QList<ColorRule>& rules,
    const QHash<QString, QString>& regionPresetNames)
{
    QList<ReplicaConfig> replicas;
    for (const auto& replica : availableReplicas) {
        if (replica.sourceTitle.isEmpty() && !regionPresetNames.value(replica.id).isEmpty()) {
            replicas.append(replica);
        }
    }
    EditorDialogs::Dialog dialog(parent);
    dialog.setObjectName(QStringLiteral("colorRuleDialog"));
    dialog.setWindowTitle(initial.number > 0 ? QStringLiteral("编辑图色逻辑模板")
                                             : QStringLiteral("新增图色规则"));
    dialog.setMinimumSize(740, 650);

    auto *layout = new QVBoxLayout(&dialog);
    auto* tabs = new QTabWidget;
    tabs->setObjectName(QStringLiteral("colorRuleEditorTabs"));
    layout->addWidget(tabs, 1);
    auto* logicPage = new QWidget;
    auto *form = new QFormLayout(logicPage);
    tabs->addTab(logicPage, QStringLiteral("逻辑条件"));
    auto *note = new QLineEdit(initial.note);
    note->setPlaceholderText(QStringLiteral("例如：护盾告警"));

    struct ConditionEditor {
        QWidget *widget{};
        PixelColorEditor *color{};
        QComboBox *pixel{};
        QComboBox *window{};
        QComboBox *lowerOp{};
        QSpinBox *lowerValue{};
        QComboBox *boundRelation{};
        QComboBox *upperOp{};
        QSpinBox *upperValue{};
        QString initialExpression;
    };
    const auto createConditionEditor = [&](const ColorRuleCondition &initialCondition, int index) {
        ConditionEditor editor;
        editor.widget = new QWidget;
        editor.widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
        editor.color = new PixelColorEditor({initialCondition.pixel, initialCondition.tolerance}, true);
        editor.pixel = editor.color->combo();
        editor.pixel->setObjectName(QStringLiteral("triggerCondition%1Pixel").arg(index));
        editor.color->tolerance()->setObjectName(QStringLiteral("triggerCondition%1Tolerance").arg(index));
        // Tolerance belongs to the region preset, never to an individual condition.
        editor.color->tolerance()->hide();
        for (auto* label : editor.color->findChildren<QLabel*>()) {
            if (label->text() == QStringLiteral("容差")) { label->hide(); }
        }
        editor.color->setToolTip(QStringLiteral("颜色容差由图色逻辑页面的“颜色容差设置”统一管理。"));

        editor.lowerOp = new QComboBox;
        editor.lowerOp->setObjectName(QStringLiteral("triggerCondition%1LowerOperator").arg(index));
        editor.lowerOp->addItem(QStringLiteral("无"), QString{});
        editor.lowerOp->addItem(QStringLiteral("≥"), QStringLiteral(">="));
        editor.lowerOp->addItem(QStringLiteral(">"), QStringLiteral(">"));
        editor.lowerValue = new QSpinBox;
        editor.lowerValue->setObjectName(QStringLiteral("triggerCondition%1LowerValue").arg(index));
        editor.lowerValue->setRange(kColorRulePixelValueMinimum, kColorRulePixelValueMaximum);
        editor.upperOp = new QComboBox;
        editor.upperOp->setObjectName(QStringLiteral("triggerCondition%1UpperOperator").arg(index));
        editor.upperOp->addItem(QStringLiteral("无"), QString{});
        editor.upperOp->addItem(QStringLiteral("≤"), QStringLiteral("<="));
        editor.upperOp->addItem(QStringLiteral("<"), QStringLiteral("<"));
        editor.upperValue = new QSpinBox;
        editor.upperValue->setObjectName(QStringLiteral("triggerCondition%1UpperValue").arg(index));
        editor.upperValue->setRange(kColorRulePixelValueMinimum, kColorRulePixelValueMaximum);
        editor.boundRelation = new QComboBox;
        editor.boundRelation->setObjectName(QStringLiteral("triggerCondition%1BoundsRelation").arg(index));
        editor.boundRelation->addItem(QStringLiteral("与"), static_cast<int>(TriggerConditionRelation::And));
        editor.boundRelation->addItem(QStringLiteral("或"), static_cast<int>(TriggerConditionRelation::Or));
        editor.boundRelation->setToolTip(QStringLiteral("与：同时满足上下界；或：满足任一边界，例如 >100 或 <10。"));
        editor.initialExpression = initialCondition.expression.trimmed();

        const auto parsedCondition = parseTriggerCondition(editor.initialExpression);
        if (parsedCondition.has_value()) {
            const auto &condition = *parsedCondition;
            editor.boundRelation->setCurrentIndex(editor.boundRelation->findData(static_cast<int>(condition.relation)));
            if (!condition.lowerOperator.isEmpty()) {
                editor.lowerOp->setCurrentIndex(editor.lowerOp->findData(condition.lowerOperator));
                editor.lowerValue->setValue(static_cast<int>(condition.lowerValue));
            }
            if (!condition.upperOperator.isEmpty()) {
                editor.upperOp->setCurrentIndex(editor.upperOp->findData(condition.upperOperator));
                editor.upperValue->setValue(static_cast<int>(condition.upperValue));
            }
        }

        auto *conditionLayout = new QVBoxLayout(editor.widget);
        conditionLayout->setContentsMargins(0, 0, 0, 0);
        conditionLayout->setSpacing(5);
        editor.window = new QComboBox;
        editor.window->setObjectName(QStringLiteral("conditionWindow%1").arg(index));
        editor.window->addItem(QStringLiteral("选择区域预设…"), QString{});
        for (int i = 0; i < replicas.size(); ++i) {
            const auto& replica = replicas[i];
            editor.window->addItem(colorLogicReplicaLabel(replica, i, regionPresetNames), replica.id);
        }
        int selected = editor.window->findData(initialCondition.replicaId);
        if (selected < 0 && !initialCondition.replicaId.isEmpty()) {
            editor.window->addItem(QStringLiteral("缺失区域预设 · %1").arg(initialCondition.replicaId.left(8)),
                initialCondition.replicaId);
            selected = editor.window->count() - 1;
        }
        editor.window->setCurrentIndex(qMax(0, selected));
        auto* windowRow = new QHBoxLayout;
        windowRow->addWidget(new QLabel(QStringLiteral("区域预设")));
        windowRow->addWidget(editor.window, 1);
        auto* remove = new QPushButton(QStringLiteral("移除条件"));
        windowRow->addWidget(remove);
        QObject::connect(remove, &QPushButton::clicked, &dialog, [widget = editor.widget] {
            widget->setProperty("removed", true);
            widget->hide();
        });
        conditionLayout->addLayout(windowRow);
        auto *colorLayout = new QHBoxLayout;
        colorLayout->addWidget(new QLabel(QStringLiteral("像素颜色")));
        colorLayout->addWidget(editor.color, 1);
        conditionLayout->addLayout(colorLayout);
        auto *boundsLayout = new QHBoxLayout;
        boundsLayout->addWidget(new QLabel(QStringLiteral("下界")));
        boundsLayout->addWidget(editor.lowerOp);
        boundsLayout->addWidget(editor.lowerValue);
        boundsLayout->addWidget(editor.boundRelation);
        boundsLayout->addWidget(new QLabel(QStringLiteral("上界")));
        boundsLayout->addWidget(editor.upperOp);
        boundsLayout->addWidget(editor.upperValue);
        boundsLayout->addStretch(1);
        conditionLayout->addLayout(boundsLayout);

        const auto refreshState = [editor] {
            const QString color = editor.pixel->currentText().trimmed();
            const bool active = !color.isEmpty() && color != QStringLiteral("无");
            editor.lowerOp->setEnabled(active);
            editor.upperOp->setEnabled(active);
            editor.lowerValue->setEnabled(active &&
                                          !editor.lowerOp->currentData().toString().isEmpty());
            editor.upperValue->setEnabled(active &&
                                          !editor.upperOp->currentData().toString().isEmpty());
            editor.boundRelation->setEnabled(active && !editor.lowerOp->currentData().toString().isEmpty()
                && !editor.upperOp->currentData().toString().isEmpty());
        };
        QObject::connect(editor.pixel, &QComboBox::currentTextChanged, &dialog, refreshState);
        QObject::connect(editor.lowerOp, &QComboBox::currentIndexChanged, &dialog, refreshState);
        QObject::connect(editor.upperOp, &QComboBox::currentIndexChanged, &dialog, refreshState);
        refreshState();
        return editor;
    };
    QList<ConditionEditor> conditionEditors;
    auto* conditionsHost = new QWidget;
    auto* conditionsLayout = new QVBoxLayout(conditionsHost);
    conditionsLayout->setContentsMargins(0, 0, 0, 0);
    conditionsLayout->setAlignment(Qt::AlignTop);
    auto conditions = initial.windowConditions;
    if (conditions.isEmpty()) {
        conditions.append(initial.triggerCondition1);
        if (!initial.triggerCondition2.isEmpty()) { conditions.append(initial.triggerCondition2); }
    }
    for (const auto& condition : conditions) {
        conditionEditors.append(createConditionEditor(condition, conditionEditors.size() + 1));
        conditionsLayout->addWidget(conditionEditors.last().widget);
    }
    auto* addCondition = new QPushButton(QStringLiteral("＋ 添加区域条件槽"));
    addCondition->setObjectName(QStringLiteral("addWindowCondition"));
    QObject::connect(addCondition, &QPushButton::clicked, &dialog, [&] {
        conditionEditors.append(createConditionEditor({}, conditionEditors.size() + 1));
        conditionsLayout->addWidget(conditionEditors.last().widget);
    });
    auto* conditionScroll = new QScrollArea;
    conditionScroll->setWidgetResizable(true);
    conditionScroll->setWidget(conditionsHost);
    conditionScroll->setMinimumHeight(180);
    auto *relation = new QComboBox;
    relation->setObjectName(QStringLiteral("triggerConditionRelation"));
    for (const auto value : {TriggerConditionRelation::And, TriggerConditionRelation::Or}) {
        relation->addItem(triggerConditionRelationName(value), static_cast<int>(value));
    }
    relation->setCurrentIndex(relation->findData(static_cast<int>(initial.triggerConditionRelation)));
    relation->setToolTip(QStringLiteral("全部满足：各区域最新有效采样都成立；任一满足：至少一个条件成立。"));

    auto *feedback = new QComboBox;
    feedback->setObjectName(QStringLiteral("triggerFeedbackType"));
    feedback->addItem(triggerFeedbackName(TriggerFeedback::None),
                      QVariant::fromValue(static_cast<int>(TriggerFeedback::None)));
    feedback->addItem(triggerFeedbackName(TriggerFeedback::Popup),
                      QVariant::fromValue(static_cast<int>(TriggerFeedback::Popup)));
    feedback->addItem(triggerFeedbackName(TriggerFeedback::Audio),
                      QVariant::fromValue(static_cast<int>(TriggerFeedback::Audio)));
    feedback->setCurrentIndex(feedback->findData(static_cast<int>(initial.triggerFeedback)));
    auto *audioRow = new QWidget;
    auto *audioPath = new QLineEdit(initial.triggerFeedbackAudio);
    audioPath->setObjectName(QStringLiteral("triggerFeedbackAudio"));
    audioPath->setPlaceholderText(QStringLiteral("选择音频文件（wav / mp3）"));
    auto *browse = new QPushButton(QStringLiteral("浏览…"));
    auto *audioLayout = new QHBoxLayout(audioRow);
    audioLayout->setContentsMargins(0, 0, 0, 0);
    audioLayout->addWidget(audioPath);
    audioLayout->addWidget(browse);
    QObject::connect(browse, &QPushButton::clicked, &dialog, [&dialog, audioPath] {
        const QString file = EditorDialogs::getOpenFileName(
            &dialog, QStringLiteral("选择音频文件"), QString{},
            QStringLiteral("音频文件 (*.wav *.mp3 *.ogg);;所有文件 (*)"));
        if (!file.isEmpty()) {
            audioPath->setText(file);
        }
    });
    const auto refreshFeedbackState = [=] {
        const bool audio =
            feedback->currentData().toInt() == static_cast<int>(TriggerFeedback::Audio);
        audioRow->setVisible(audio);
    };
    QObject::connect(feedback, &QComboBox::currentIndexChanged, &dialog, refreshFeedbackState);
    refreshFeedbackState();

    auto *action = new QComboBox;
    action->setObjectName(QStringLiteral("postTriggerAction"));
    action->addItem(postTriggerActionName(PostTriggerAction::Loop),
                    QVariant::fromValue(static_cast<int>(PostTriggerAction::Loop)));
    action->addItem(postTriggerActionName(PostTriggerAction::ReverseWait),
                    QVariant::fromValue(static_cast<int>(PostTriggerAction::ReverseWait)));
    action->addItem(postTriggerActionName(PostTriggerAction::Jump),
                    QVariant::fromValue(static_cast<int>(PostTriggerAction::Jump)));
    action->setCurrentIndex(action->findData(static_cast<int>(initial.postTriggerAction)));

    form->addRow(QStringLiteral("备注"), note);
    form->addRow(QStringLiteral("区域条件槽"), conditionScroll);
    form->addRow(QString{}, addCondition);
    form->addRow(QStringLiteral("条件关系"), relation);
    form->addRow(QStringLiteral("触发反馈"), feedback);
    form->addRow(QString{}, audioRow);
    auto* switchPage = new QWidget;
    auto* switchLayout = new QVBoxLayout(switchPage);
    auto* switchDescription = new QLabel(QStringLiteral(
        "触发后效果在图色逻辑触发并完成反馈后执行。中键可直接强制切换，无需满足条件；两者共用下方关联的图色逻辑。"));
    switchDescription->setWordWrap(true);
    switchLayout->addWidget(switchDescription);
    auto* switchForm = new QFormLayout;
    switchForm->addRow(QStringLiteral("触发后效果"), action);
    auto* actionHelp = new QLabel;
    actionHelp->setWordWrap(true);
    const auto refreshActionHelp = [=] {
        switch (static_cast<PostTriggerAction>(action->currentData().toInt())) {
        case PostTriggerAction::Loop:
            actionHelp->setText(QStringLiteral("正向循环：条件持续满足时，在完成反馈后继续检测并触发。")); break;
        case PostTriggerAction::ReverseWait:
            actionHelp->setText(QStringLiteral("反向等待：本次反馈完成后停止闪烁，恢复常驻边框色；等待条件解除，再满足时重新触发。")); break;
        case PostTriggerAction::Jump:
            actionHelp->setText(QStringLiteral("切换至另一状态：触发并完成反馈后，停用当前逻辑，启用关联的图色逻辑。")); break;
        }
    };
    QObject::connect(action, &QComboBox::currentIndexChanged, &dialog, refreshActionHelp);
    refreshActionHelp();
    switchForm->addRow(actionHelp);
    auto* jumpTarget = new QComboBox;
    jumpTarget->setObjectName(QStringLiteral("triggeredSwitchRule"));
    jumpTarget->addItem(QStringLiteral("不关联"), 0);
    for (const auto& rule : rules) {
        if (rule.number != initial.number) {
            jumpTarget->addItem(QStringLiteral("#%1 %2").arg(rule.number).arg(rule.note), rule.number);
        }
    }
    int associatedRule = initial.jumpToRule;
    if (associatedRule <= 0) {
        for (const auto& setting : initial.windowSwitches) {
            if (setting.middleClickRule > 0) { associatedRule = setting.middleClickRule; break; }
        }
    }
    if (associatedRule > 0 && jumpTarget->findData(associatedRule) < 0) {
        jumpTarget->addItem(QStringLiteral("已删除逻辑 #%1").arg(associatedRule), associatedRule);
    }
    jumpTarget->setCurrentIndex(qMax(0, jumpTarget->findData(associatedRule)));
    jumpTarget->setToolTip(QStringLiteral("中键强制切换的目标；触发后效果为“切换至另一状态”时，也进入该图色逻辑。"));
    switchForm->addRow(QStringLiteral("关联的图色逻辑"), jumpTarget);
    auto* switchError = new QLabel;
    switchError->setObjectName(QStringLiteral("stateSwitchError"));
    switchError->setWordWrap(true);
    switchError->hide();
    switchForm->addRow(switchError);
    switchLayout->addLayout(switchForm);

    struct SwitchEditor {
        QString replicaId;
        QCheckBox* border{};
        QPushButton* color{};
    };
    QList<SwitchEditor> switchEditors;
    auto* switchHost = new QWidget;
    auto* switchRows = new QVBoxLayout(switchHost);
    switchRows->setAlignment(Qt::AlignTop);
    for (int index = 0; index < replicas.size(); ++index) {
        const auto& replica = replicas[index];
        ColorRuleWindowSwitch initialSwitch;
        for (const auto& setting : initial.windowSwitches) {
            if (setting.replicaId == replica.id) { initialSwitch = setting; break; }
        }
        auto* row = new QWidget;
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 4, 0, 4);
        auto* label = new QLabel(colorLogicReplicaLabel(replica, index, regionPresetNames));
        label->setMinimumWidth(150);
        rowLayout->addWidget(label);
        auto* border = new QCheckBox(QStringLiteral("当前逻辑启用时显示边框色"));
        border->setToolTip(QStringLiteral("当前逻辑启用时常驻显示该颜色，无需满足触发条件；触发时边框闪烁。"));
        border->setObjectName(QStringLiteral("switchBorderEnabled%1").arg(index + 1));
        border->setChecked(initialSwitch.borderColor.isValid());
        rowLayout->addWidget(border);
        auto* color = new QPushButton;
        color->setObjectName(QStringLiteral("switchBorderColor%1").arg(index + 1));
        color->setProperty("selectedColor", initialSwitch.borderColor.isValid()
            ? initialSwitch.borderColor : QColor(255, 80, 80));
        const auto refreshColor = [color, border] {
            color->setEnabled(border->isChecked());
            color->setText(color->property("selectedColor").value<QColor>().name());
        };
        QObject::connect(border, &QCheckBox::toggled, &dialog, refreshColor);
        QObject::connect(color, &QPushButton::clicked, &dialog, [color, &dialog, refreshColor] {
            const QColor selected = EditorDialogs::getColor(
                color->property("selectedColor").value<QColor>(), &dialog,
                QStringLiteral("选择边框颜色"));
            if (selected.isValid()) { color->setProperty("selectedColor", selected); refreshColor(); }
        });
        refreshColor();
        rowLayout->addWidget(color);
        rowLayout->addStretch(1);
        switchRows->addWidget(row);
        switchEditors.append({replica.id, border, color});
    }
    auto* switchScroll = new QScrollArea;
    switchScroll->setWidgetResizable(true);
    switchScroll->setWidget(switchHost);
    switchLayout->addWidget(switchScroll, 1);
    auto* borderDescription = new QLabel(QStringLiteral(
        "状态边框色用于标识当前逻辑（如绿色预警、蓝色反预警），触发时边框闪烁。条件槽及下方设置边框色的区域均可用中键切换。"));
    borderDescription->setWordWrap(true);
    switchLayout->insertWidget(2, borderDescription);
    tabs->addTab(switchPage, QStringLiteral("状态切换设置"));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("保存"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        int active = 0;
        for (const auto& editor : conditionEditors) {
            if (editor.widget->property("removed").toBool()) { continue; }
            const auto color = editor.color->target();
            if (!color || color->color.isEmpty()
                || (editor.lowerOp->currentData().toString().isEmpty()
                    && editor.upperOp->currentData().toString().isEmpty())
                || editor.window->currentData().toString().isEmpty()) {
                QMessageBox::warning(&dialog, QStringLiteral("条件未完成"),
                    QStringLiteral("请为每个条件槽选择区域预设、有效颜色和数量范围，或移除该条件槽。"));
                return;
            }
            ++active;
        }
        if (!active) {
            QMessageBox::warning(&dialog, QStringLiteral("逻辑未完成"),
                QStringLiteral("请配置至少一个区域条件槽。来源在“逻辑列表”中统一绑定。"));
            return;
        }
        const int associated = jumpTarget->currentData().toInt();
        bool targetExists = associated == 0;
        for (const auto& rule : rules) { targetExists = targetExists || rule.number == associated; }
        if (!targetExists || (action->currentData().toInt() == static_cast<int>(PostTriggerAction::Jump)
            && associated == 0)) {
            tabs->setCurrentWidget(switchPage);
            switchError->setText(QStringLiteral("请在“关联的图色逻辑”中选择有效的目标状态。"));
            switchError->show();
            jumpTarget->setFocus();
            return;
        }
        dialog.accept();
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return std::nullopt;
    }

    const auto readCondition = [](const ConditionEditor &editor) {
        const auto target = editor.color->target().value();
        const QString pixel = target.color;
        if (pixel.isEmpty()) {
            return ColorRuleCondition{};
        }
        TriggerCondition condition;
        condition.lowerOperator = editor.lowerOp->currentData().toString();
        condition.lowerValue = editor.lowerValue->value();
        condition.upperOperator = editor.upperOp->currentData().toString();
        condition.upperValue = editor.upperValue->value();
        condition.relation = static_cast<TriggerConditionRelation>(editor.boundRelation->currentData().toInt());
        QString expression = condition.toString();
        if (expression.isEmpty() && !editor.initialExpression.isEmpty()) {
            expression = editor.initialExpression;
        }
        return ColorRuleCondition{pixel, expression, -1,
            editor.window->currentData().toString()};
    };

    const auto feedbackType = static_cast<TriggerFeedback>(feedback->currentData().toInt());
    const auto actionType = static_cast<PostTriggerAction>(action->currentData().toInt());

    ColorRule result;
    result.number = initial.number;
    result.note = note->text().trimmed();
    for (const auto& editor : conditionEditors) {
        if (!editor.widget->property("removed").toBool()) { result.windowConditions.append(readCondition(editor)); }
    }
    result.enabled = initial.enabled;
    result.targetReplicaId = result.windowConditions.first().replicaId;
    result.jumpToRule = jumpTarget->currentData().toInt();
    QSet<QString> conditionRegions;
    for (const auto& condition : result.windowConditions) { conditionRegions.insert(condition.replicaId); }
    for (const auto& editor : switchEditors) {
        const QColor borderColor = editor.border->isChecked()
            ? editor.color->property("selectedColor").value<QColor>() : QColor{};
        const int middleRule = borderColor.isValid() || conditionRegions.contains(editor.replicaId)
            ? result.jumpToRule : 0;
        if (borderColor.isValid() || middleRule > 0) {
            result.windowSwitches.append({editor.replicaId, borderColor, middleRule});
        }
    }
    result.triggerConditionRelation = static_cast<TriggerConditionRelation>(relation->currentData().toInt());
    result.triggerFeedback = feedbackType;
    result.triggerFeedbackAudio = audioPath->text().trimmed();
    result.postTriggerAction = actionType;
    return result;
}
