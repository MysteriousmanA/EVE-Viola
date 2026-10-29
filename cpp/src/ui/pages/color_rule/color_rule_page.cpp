#include "ui/pages/color_rule/color_rule_page.h"
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
        QStringLiteral("使用红/白/蓝预设或任意自定义颜色的像素数量设置触发条件，可调整颜色匹配容差。")));

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
    actions->addWidget(deleteButton_);
    layout->addLayout(actions);

    table_ = new QTableWidget(0, 7);
    table_->setObjectName(QStringLiteral("colorRuleTable"));
    table_->setHorizontalHeaderLabels({QStringLiteral("编号"), QStringLiteral("备注"),
                                       QStringLiteral("触发条件 1"), QStringLiteral("条件关系"),
                                       QStringLiteral("触发条件 2"),
                                       QStringLiteral("触发反馈"), QStringLiteral("触发后操作")});
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
        table_->setItem(row, 2, item(rule.triggerCondition1.displayText()));
        table_->setItem(row, 3, item(triggerConditionRelationName(rule.triggerConditionRelation)));
        table_->setItem(row, 4, item(rule.triggerCondition2.displayText()));

        QString feedbackText = triggerFeedbackName(rule.triggerFeedback);
        if (rule.triggerFeedback == TriggerFeedback::Audio &&
            !rule.triggerFeedbackAudio.isEmpty()) {
            feedbackText = QStringLiteral("播放音频：%1").arg(rule.triggerFeedbackAudio);
        }
        table_->setItem(row, 5, item(feedbackText));

        table_->setItem(row, 6, item(postTriggerActionName(rule.postTriggerAction)));
    }
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
