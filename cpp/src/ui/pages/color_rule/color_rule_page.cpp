#include "ui/pages/color_rule/color_rule_page.h"
#include "ui/pages/color_rule/replica_choice_label.h"
#include "features/color_rule/color_rule_config.h"

#include <algorithm>

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSet>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include "ui/common/page_widgets.h"
using namespace PageWidgets;
ColorRulePage::ColorRulePage(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    layout->addWidget(makeHeading(QStringLiteral("图色逻辑")));
    layout->addWidget(makeDescription(
        QStringLiteral("条件槽直接选择区域预设，无需创建无主小窗；在“逻辑列表”添加实例，再统一选择源窗口。")));

    auto *addButton = new QPushButton(QStringLiteral("＋ 新增规则"));
    addButton->setObjectName(QStringLiteral("addColorRule"));
    addButton->setProperty("accent", true);
    editButton_ = new QPushButton(QStringLiteral("编辑"));
    editButton_->setObjectName(QStringLiteral("editColorRule"));
    deleteButton_ = new QPushButton(QStringLiteral("删除"));
    deleteButton_->setObjectName(QStringLiteral("deleteColorRule"));
    editButton_->setEnabled(false);
    deleteButton_->setEnabled(false);
    auto *actions = new QHBoxLayout;
    actions->addWidget(addButton);
    actions->addStretch(1);
    actions->addWidget(editButton_);
    auto* tolerances = new QPushButton(QStringLiteral("颜色容差设置"));
    tolerances->setObjectName(QStringLiteral("colorToleranceSettings"));
    actions->addWidget(tolerances);
    connect(tolerances, &QPushButton::clicked, this, &ColorRulePage::colorTolerancesRequested);
    actions->addWidget(deleteButton_);
    layout->addLayout(actions);

    table_ = new QTableWidget(0, 7);
    table_->setObjectName(QStringLiteral("colorRuleTable"));
    table_->setHorizontalHeaderLabels({QStringLiteral("编号"), QStringLiteral("备注"),
        QStringLiteral("区域条件槽"), QStringLiteral("条件关系"), QStringLiteral("状态切换设置"),
        QStringLiteral("触发反馈"), QStringLiteral("触发后效果")});
    configureTable(table_);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    for (int column = 1; column < table_->columnCount(); ++column) {
        table_->horizontalHeader()->setSectionResizeMode(column, QHeaderView::Stretch);
    }
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    layout->addWidget(table_, 1);

    connect(addButton, &QPushButton::clicked, this, &ColorRulePage::addRequested);
    connect(table_, &QTableWidget::itemSelectionChanged, this, &ColorRulePage::refreshActionState);
    connect(editButton_, &QPushButton::clicked, this, [this] {
        if (const int number = selectedRuleNumber(); number > 0) {
            emit editRequested(number);
        }
    });
    connect(deleteButton_, &QPushButton::clicked, this, [this] {
        if (const int number = selectedRuleNumber(); number > 0) {
            emit deleteRequested(number);
        }
    });
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int, int) {
        if (const int number = selectedRuleNumber(); number > 0) {
            emit editRequested(number);
        }
    });
}

void ColorRulePage::setRules(const QList<ColorRule> &rules)
{
    table_->setRowCount(rules.size());
    for (int row = 0; row < rules.size(); ++row) {
        const auto &rule = rules[row];
        table_->setItem(row, 0, item(QString::number(rule.number)));
        table_->setItem(row, 1, item(rule.note));
        const auto windowName = [this](const QString& id) {
            for (int i = 0; i < replicas_.size(); ++i) {
                if (!id.isEmpty() && replicas_[i].id == id) {
                    return colorLogicReplicaLabel(replicas_[i], i, presetNames_);
                }
            }
            return id.isEmpty() ? QStringLiteral("未绑定")
                : QStringLiteral("缺失区域预设 · %1").arg(id.left(8));
        };
        QStringList conditions;
        for (const auto& condition : rule.windowConditions) {
            conditions.append(QStringLiteral("%1：%2 %3").arg(windowName(condition.replicaId), condition.pixel, condition.expression));
        }
        table_->setItem(row, 2, item(conditions.join(QStringLiteral("\n"))));
        table_->setItem(row, 3, item(triggerConditionRelationName(rule.triggerConditionRelation)));
        QStringList switches;
        QSet<int> associatedRules;
        if (rule.jumpToRule > 0) { associatedRules.insert(rule.jumpToRule); }
        for (const auto& setting : rule.windowSwitches) {
            QStringList details;
            if (setting.borderColor.isValid()) {
                details.append(QStringLiteral("状态边框 %1").arg(setting.borderColor.name()));
            }
            if (setting.middleClickRule > 0) {
                associatedRules.insert(setting.middleClickRule);
            }
            if (!details.isEmpty()) {
                switches.append(QStringLiteral("%1：%2").arg(
                    windowName(setting.replicaId), details.join(QStringLiteral("、"))));
            }
        }
        auto associated = associatedRules.values();
        std::sort(associated.begin(), associated.end());
        QStringList targets;
        for (int number : associated) { targets.append(QStringLiteral("#%1").arg(number)); }
        if (!targets.isEmpty()) { switches.prepend(QStringLiteral("关联逻辑：%1").arg(targets.join(QStringLiteral("、")))); }
        table_->setItem(row, 4, item(switches.isEmpty()
            ? QStringLiteral("无") : switches.join(QStringLiteral("\n"))));
        QString feedbackText = triggerFeedbackName(rule.triggerFeedback);
        if (rule.triggerFeedback == TriggerFeedback::Audio &&
            !rule.triggerFeedbackAudio.isEmpty()) {
            feedbackText = QStringLiteral("播放音频：%1").arg(rule.triggerFeedbackAudio);
        }
        table_->setItem(row, 5, item(feedbackText));

        table_->setItem(row, 6, item(rule.postTriggerAction == PostTriggerAction::Jump
            ? QStringLiteral("切换至 #%1").arg(rule.jumpToRule) : postTriggerActionName(rule.postTriggerAction)));
    }
    table_->resizeRowsToContents();
    refreshActionState();
}

int ColorRulePage::selectedRuleNumber() const
{
    const int row = table_->currentRow();
    const auto *number = row >= 0 ? table_->item(row, 0) : nullptr;
    return number == nullptr ? 0 : number->text().toInt();
}

void ColorRulePage::refreshActionState()
{
    const bool selected = selectedRuleNumber() > 0;
    editButton_->setEnabled(selected);
    deleteButton_->setEnabled(selected);
}
