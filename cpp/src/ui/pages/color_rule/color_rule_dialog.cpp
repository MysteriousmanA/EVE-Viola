#include "ui/pages/color_rule/color_rule_dialog.h"
#include "features/color_rule/trigger_condition.h"
#include "features/color_rule/color_rule_config.h"
#include "features/replica/pixel_statistics.h"
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

std::optional<ColorRule> promptColorRule(QWidget *parent, const ColorRule &initial)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(initial.number > 0 ? QStringLiteral("编辑图色规则")
                                             : QStringLiteral("新增图色规则"));
    dialog.setMinimumWidth(540);

    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout;
    auto *note = new QLineEdit(initial.note);
    note->setPlaceholderText(QStringLiteral("例如：护盾告警"));

    const QStringList pixelCategories = pixelColorCategoryNames();
    struct ConditionEditor {
        QWidget *widget{};
        QComboBox *pixel{};
        QComboBox *lowerOp{};
        QSpinBox *lowerValue{};
        QComboBox *upperOp{};
        QSpinBox *upperValue{};
        QString initialExpression;
    };
    const auto createConditionEditor = [&](const ColorRuleCondition &initialCondition, int index) {
        ConditionEditor editor;
        editor.widget = new QWidget;
        editor.pixel = new QComboBox;
        editor.pixel->setObjectName(QStringLiteral("triggerCondition%1Pixel").arg(index));
        editor.pixel->addItem(QStringLiteral("无"), QString{});
        for (const auto &category : pixelCategories) {
            editor.pixel->addItem(category, category);
        }
        const int pixelIndex = editor.pixel->findData(initialCondition.pixel);
        editor.pixel->setCurrentIndex(pixelIndex >= 0 ? pixelIndex : 0);

        editor.lowerOp = new QComboBox;
        editor.lowerOp->setObjectName(QStringLiteral("triggerCondition%1LowerOperator").arg(index));
        editor.lowerOp->addItem(QStringLiteral("无"), QString{});
        editor.lowerOp->addItem(QStringLiteral("≥"), QStringLiteral(">="));
        editor.lowerOp->addItem(QStringLiteral(">"), QStringLiteral(">"));
        editor.lowerValue = new QSpinBox;
        editor.lowerValue->setRange(kColorRulePixelValueMinimum, kColorRulePixelValueMaximum);
        editor.upperOp = new QComboBox;
        editor.upperOp->setObjectName(QStringLiteral("triggerCondition%1UpperOperator").arg(index));
        editor.upperOp->addItem(QStringLiteral("无"), QString{});
        editor.upperOp->addItem(QStringLiteral("≤"), QStringLiteral("<="));
        editor.upperOp->addItem(QStringLiteral("<"), QStringLiteral("<"));
        editor.upperValue = new QSpinBox;
        editor.upperValue->setRange(kColorRulePixelValueMinimum, kColorRulePixelValueMaximum);
        editor.initialExpression = initialCondition.expression.trimmed();

        const auto parsedCondition = parseTriggerCondition(editor.initialExpression);
        if (parsedCondition.has_value()) {
            const auto &condition = *parsedCondition;
            if (!condition.lowerOperator.isEmpty()) {
                editor.lowerOp->setCurrentIndex(editor.lowerOp->findData(condition.lowerOperator));
                editor.lowerValue->setValue(static_cast<int>(condition.lowerValue));
            }
            if (!condition.upperOperator.isEmpty()) {
                editor.upperOp->setCurrentIndex(editor.upperOp->findData(condition.upperOperator));
                editor.upperValue->setValue(static_cast<int>(condition.upperValue));
            }
        }

        auto *conditionLayout = new QHBoxLayout(editor.widget);
        conditionLayout->setContentsMargins(0, 0, 0, 0);
        conditionLayout->addWidget(new QLabel(QStringLiteral("像素")));
        conditionLayout->addWidget(editor.pixel);
        conditionLayout->addWidget(new QLabel(QStringLiteral("下界")));
        conditionLayout->addWidget(editor.lowerOp);
        conditionLayout->addWidget(editor.lowerValue);
        conditionLayout->addWidget(new QLabel(QStringLiteral("且上界")));
        conditionLayout->addWidget(editor.upperOp);
        conditionLayout->addWidget(editor.upperValue);
        conditionLayout->addStretch(1);

        const auto refreshState = [editor] {
            const bool active = !editor.pixel->currentData().toString().isEmpty();
            editor.lowerOp->setEnabled(active);
            editor.upperOp->setEnabled(active);
            editor.lowerValue->setEnabled(active &&
                                          !editor.lowerOp->currentData().toString().isEmpty());
            editor.upperValue->setEnabled(active &&
                                          !editor.upperOp->currentData().toString().isEmpty());
        };
        QObject::connect(editor.pixel, &QComboBox::currentIndexChanged, &dialog, refreshState);
        QObject::connect(editor.lowerOp, &QComboBox::currentIndexChanged, &dialog, refreshState);
        QObject::connect(editor.upperOp, &QComboBox::currentIndexChanged, &dialog, refreshState);
        refreshState();
        return editor;
    };
    const ConditionEditor condition1 = createConditionEditor(initial.triggerCondition1, 1);
    const ConditionEditor condition2 = createConditionEditor(initial.triggerCondition2, 2);

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
        const QString file = QFileDialog::getOpenFileName(
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
    action->addItem(postTriggerActionName(PostTriggerAction::Jump),
                    QVariant::fromValue(static_cast<int>(PostTriggerAction::Jump)));
    action->addItem(postTriggerActionName(PostTriggerAction::ReverseWait),
                    QVariant::fromValue(static_cast<int>(PostTriggerAction::ReverseWait)));
    action->setCurrentIndex(action->findData(static_cast<int>(initial.postTriggerAction)));

    form->addRow(QStringLiteral("备注"), note);
    form->addRow(QStringLiteral("触发条件 1"), condition1.widget);
    form->addRow(QStringLiteral("触发条件 2"), condition2.widget);
    form->addRow(QStringLiteral("触发反馈"), feedback);
    form->addRow(QString{}, audioRow);
    form->addRow(QStringLiteral("触发后操作"), action);
    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("保存"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return std::nullopt;
    }

    const auto readCondition = [](const ConditionEditor &editor) {
        const QString pixel = editor.pixel->currentData().toString();
        if (pixel.isEmpty()) {
            return ColorRuleCondition{};
        }
        TriggerCondition condition;
        condition.lowerOperator = editor.lowerOp->currentData().toString();
        condition.lowerValue = editor.lowerValue->value();
        condition.upperOperator = editor.upperOp->currentData().toString();
        condition.upperValue = editor.upperValue->value();
        QString expression = condition.toString();
        if (expression.isEmpty() && !editor.initialExpression.isEmpty()) {
            expression = editor.initialExpression;
        }
        return ColorRuleCondition{pixel, expression};
    };

    const auto feedbackType = static_cast<TriggerFeedback>(feedback->currentData().toInt());
    const auto actionType = static_cast<PostTriggerAction>(action->currentData().toInt());

    ColorRule result;
    result.number = initial.number;
    result.note = note->text().trimmed();
    result.triggerCondition1 = readCondition(condition1);
    result.triggerCondition2 = readCondition(condition2);
    result.triggerFeedback = feedbackType;
    result.triggerFeedbackAudio = audioPath->text().trimmed();
    result.postTriggerAction = actionType;
    return result;
}
