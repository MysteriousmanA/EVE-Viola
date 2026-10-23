#include "ui/pages/advanced_logic/advanced_logic_page.h"

#include <algorithm>
#include <optional>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QUuid>
#include <QVBoxLayout>
#include "ui/common/page_widgets.h"

namespace {
QString ruleLabel(const ColorRule& rule)
{
    return QStringLiteral("#%1 %2").arg(rule.number).arg(rule.note);
}

std::optional<WhitelistSequence> promptSequence(
    QWidget* parent, WhitelistSequence sequence, const QList<ColorRule>& rules)
{
    QDialog dialog(parent);
    dialog.setObjectName(QStringLiteral("whitelistSequenceDialog"));
    dialog.setWindowTitle(sequence.id.isEmpty() ? QStringLiteral("新增白名单序列") : QStringLiteral("编辑白名单序列"));
    dialog.resize(880, 580);
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout;
    auto* name = new QLineEdit(sequence.name);
    name->setObjectName(QStringLiteral("sequenceName"));
    name->setPlaceholderText(QStringLiteral("例如：规避后的正常加速"));
    auto* enabled = new QCheckBox(QStringLiteral("启用这条白名单"));
    enabled->setChecked(sequence.enabled);
    auto* timeout = new QDoubleSpinBox;
    timeout->setObjectName(QStringLiteral("sequenceTimeout"));
    timeout->setRange(0.1, 3600.0);
    timeout->setDecimals(1);
    timeout->setSuffix(QStringLiteral(" 秒"));
    timeout->setValue(sequence.timeoutMs / 1000.0);
    auto* scope = new QComboBox;
    scope->setObjectName(QStringLiteral("sequenceScope"));
    scope->addItems({QStringLiteral("同一源窗口 / 账号的多个小窗"), QStringLiteral("允许跨源窗口 / 账号")});
    scope->setCurrentIndex(sequence.sameSource ? 0 : 1);
    auto* ignore = new QCheckBox(QStringLiteral("允许序列中间出现其他事件"));
    ignore->setChecked(sequence.ignoreUnrelated);
    form->addRow(QStringLiteral("名称"), name);
    form->addRow(QStringLiteral("状态"), enabled);
    form->addRow(QStringLiteral("整串完成时限"), timeout);
    form->addRow(QStringLiteral("匹配范围"), scope);
    form->addRow(QStringLiteral("事件顺序"), ignore);
    layout->addLayout(form);
    layout->addWidget(PageWidgets::makeDescription(QStringLiteral(
        "从上到下按顺序匹配。匹配成功后，各节点分别保留或消除效果；最高优先级始终立即执行，且仍参与匹配。超时未匹配的普通事件恢复原效果。")));
    auto* nodes = new QTableWidget(0, 4);
    nodes->setObjectName(QStringLiteral("whitelistNodes"));
    nodes->setHorizontalHeaderLabels({QStringLiteral("顺序"), QStringLiteral("图色事件"),
        QStringLiteral("匹配后的效果"), QStringLiteral("限定源窗口标题（可留空）")});
    PageWidgets::configureTable(nodes);
    nodes->setSelectionBehavior(QAbstractItemView::SelectRows);
    nodes->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    nodes->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    nodes->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    nodes->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    nodes->setMinimumHeight(200);
    const auto fillNodes = [&] {
        nodes->setRowCount(0);
        for (int index = 0; index < sequence.nodes.size(); ++index) {
            const auto& node = sequence.nodes[index];
            nodes->insertRow(index);
            nodes->setItem(index, 0, PageWidgets::item(QString::number(index + 1)));
            auto* rule = new QComboBox;
            for (const auto& available : rules) {
                rule->addItem(ruleLabel(available) + (available.priority == ColorRulePriority::Immediate
                    ? QStringLiteral(" · 最高，立即执行") : QString{}), available.number);
            }
            if (rule->findData(node.ruleNumber) < 0) {
                rule->addItem(QStringLiteral("#%1 已删除，请更换").arg(node.ruleNumber), node.ruleNumber);
            }
            rule->setCurrentIndex(rule->findData(node.ruleNumber));
            nodes->setCellWidget(index, 1, rule);
            auto* effect = new QComboBox;
            effect->addItems({QStringLiteral("保留效果"), QStringLiteral("消除效果")});
            effect->setCurrentIndex(node.effect == WhitelistNodeEffect::Keep ? 0 : 1);
            effect->setToolTip(QStringLiteral("只影响完整匹配的事件；最高优先级不受消除设置限制。"));
            nodes->setCellWidget(index, 2, effect);
            auto* source = new QLineEdit(node.sourceTitle);
            source->setObjectName(QStringLiteral("whitelistSourceTitle"));
            source->setPlaceholderText(QStringLiteral("任意；填写时须与完整标题一致"));
            nodes->setCellWidget(index, 3, source);
        }
    };
    const auto readNodes = [&] {
        sequence.nodes.clear();
        for (int row = 0; row < nodes->rowCount(); ++row) {
            sequence.nodes.append({
                qobject_cast<QComboBox*>(nodes->cellWidget(row, 1))->currentData().toInt(),
                qobject_cast<QComboBox*>(nodes->cellWidget(row, 2))->currentIndex() == 0
                    ? WhitelistNodeEffect::Keep : WhitelistNodeEffect::Suppress,
                qobject_cast<QLineEdit*>(nodes->cellWidget(row, 3))->text().trimmed()});
        }
    };
    fillNodes();
    layout->addWidget(nodes, 1);
    auto* actions = new QHBoxLayout;
    auto* add = new QPushButton(QStringLiteral("＋ 添加节点"));
    add->setObjectName(QStringLiteral("addWhitelistNode"));
    auto* remove = new QPushButton(QStringLiteral("删除节点"));
    auto* up = new QPushButton(QStringLiteral("上移"));
    auto* down = new QPushButton(QStringLiteral("下移"));
    for (auto* button : {add, remove, up, down}) { actions->addWidget(button); }
    actions->addStretch();
    layout->addLayout(actions);
    QObject::connect(add, &QPushButton::clicked, &dialog, [&] {
        readNodes();
        sequence.nodes.append({rules.first().number, WhitelistNodeEffect::Keep, {}});
        fillNodes();
        nodes->selectRow(nodes->rowCount() - 1);
    });
    QObject::connect(remove, &QPushButton::clicked, &dialog, [&] {
        const int row = nodes->currentRow();
        if (row < 0) { return; }
        readNodes();
        sequence.nodes.removeAt(row);
        fillNodes();
        if (nodes->rowCount() > 0) { nodes->selectRow(qMin(row, nodes->rowCount() - 1)); }
    });
    const auto move = [&](int direction) {
        const int row = nodes->currentRow();
        if (row < 0 || row + direction < 0 || row + direction >= nodes->rowCount()) { return; }
        readNodes();
        sequence.nodes.swapItemsAt(row, row + direction);
        fillNodes();
        nodes->selectRow(row + direction);
    };
    QObject::connect(up, &QPushButton::clicked, &dialog, [&] { move(-1); });
    QObject::connect(down, &QPushButton::clicked, &dialog, [&] { move(1); });
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("保存序列"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        readNodes();
        if (name->text().trimmed().isEmpty() || sequence.nodes.isEmpty()) {
            QMessageBox::warning(&dialog, QStringLiteral("序列未完成"), QStringLiteral("请填写名称，并添加至少一个事件节点。"));
            return;
        }
        for (const auto& node : sequence.nodes) {
            if (std::none_of(rules.cbegin(), rules.cend(), [&](const auto& rule) { return node.ruleNumber == rule.number; })) {
                QMessageBox::warning(&dialog, QStringLiteral("规则已删除"), QStringLiteral("请为已删除的规则选择有效的图色规则。"));
                return;
            }
        }
        dialog.accept();
    });
    if (dialog.exec() != QDialog::Accepted) { return std::nullopt; }
    if (sequence.id.isEmpty()) { sequence.id = QUuid::createUuid().toString(QUuid::WithoutBraces); }
    sequence.name = name->text().trimmed();
    sequence.enabled = enabled->isChecked();
    sequence.timeoutMs = qRound(timeout->value() * 1000);
    sequence.sameSource = scope->currentIndex() == 0;
    sequence.ignoreUnrelated = ignore->isChecked();
    return sequence;
}
}

AdvancedLogicPage::AdvancedLogicPage(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("advancedLogicPage"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    layout->addWidget(PageWidgets::makeHeading(QStringLiteral("高级逻辑")));
    layout->addWidget(PageWidgets::makeDescription(QStringLiteral(
        "用事件序列识别正常操作：匹配白名单后，逐节点决定保留或消除图色效果。")));
    enabled_ = new QCheckBox(QStringLiteral("启用事件序列白名单"));
    enabled_->setObjectName(QStringLiteral("advancedLogicEnabled"));
    layout->addWidget(enabled_);
    auto* actions = new QHBoxLayout;
    add_ = new QPushButton(QStringLiteral("＋ 新增白名单"));
    add_->setObjectName(QStringLiteral("addWhitelistSequence"));
    add_->setProperty("accent", true);
    edit_ = new QPushButton(QStringLiteral("编辑"));
    edit_->setObjectName(QStringLiteral("editWhitelistSequence"));
    remove_ = new QPushButton(QStringLiteral("删除"));
    up_ = new QPushButton(QStringLiteral("上移"));
    down_ = new QPushButton(QStringLiteral("下移"));
    actions->addWidget(add_);
    actions->addStretch();
    for (auto* button : {edit_, remove_, up_, down_}) { actions->addWidget(button); }
    layout->addLayout(actions);
    sequences_ = new QTableWidget(0, 5);
    sequences_->setObjectName(QStringLiteral("whitelistSequenceTable"));
    sequences_->setHorizontalHeaderLabels({QStringLiteral("状态"), QStringLiteral("名称"),
        QStringLiteral("范围"), QStringLiteral("时限"), QStringLiteral("事件序列 → 节点效果")});
    PageWidgets::configureTable(sequences_);
    for (int col = 0; col < 4; ++col) { sequences_->horizontalHeader()->setSectionResizeMode(col, QHeaderView::ResizeToContents); }
    sequences_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    sequences_->setMinimumHeight(180);
    layout->addWidget(sequences_, 1);
    layout->addWidget(PageWidgets::makeDescription(QStringLiteral(
        "最先完成的序列生效；多条同时完成时，上方优先。事件匹配后只消费一次。引用已删除规则的整条序列暂停匹配。")));
    layout->addWidget(PageWidgets::makeHeading(QStringLiteral("图色规则优先级")));
    layout->addWidget(PageWidgets::makeDescription(QStringLiteral(
        "最高：立即执行，不等待、不消除，仍参与序列。较高：待判定后优先执行。普通：按序列判定。消除涵盖提醒、切屏、闪烁、桌宠文案及触发后操作。")));
    priorities_ = new QTableWidget(0, 2);
    priorities_->setObjectName(QStringLiteral("colorRulePriorityTable"));
    priorities_->setHorizontalHeaderLabels({QStringLiteral("图色规则"), QStringLiteral("优先级")});
    PageWidgets::configureTable(priorities_);
    priorities_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    priorities_->setMinimumHeight(150);
    layout->addWidget(priorities_);
    status_ = PageWidgets::makeDescription(QStringLiteral("先添加图色规则，再创建白名单。未匹配的事件正常执行。"));
    status_->setObjectName(QStringLiteral("advancedLogicStatus"));
    layout->addWidget(status_);
    connect(enabled_, &QCheckBox::toggled, this, [this](bool enabled) {
        config_.enabled = enabled;
        emit configurationChanged();
    });
    connect(add_, &QPushButton::clicked, this, [this] { editSequence(true); });
    connect(edit_, &QPushButton::clicked, this, [this] { editSequence(false); });
    connect(sequences_, &QTableWidget::cellDoubleClicked, this, [this] { editSequence(false); });
    connect(sequences_, &QTableWidget::itemSelectionChanged, this, &AdvancedLogicPage::refreshActions);
    connect(remove_, &QPushButton::clicked, this, [this] {
        const int row = sequences_->currentRow();
        if (row < 0) { return; }
        config_.sequences.removeAt(row);
        refreshSequences();
        emit configurationChanged();
    });
    connect(up_, &QPushButton::clicked, this, [this] { moveSequence(-1); });
    connect(down_, &QPushButton::clicked, this, [this] { moveSequence(1); });
    refreshActions();
}

void AdvancedLogicPage::setConfiguration(const AdvancedLogicConfig& config, const QList<ColorRule>& rules)
{
    config_ = config;
    rules_ = rules;
    const QSignalBlocker blocker(enabled_);
    enabled_->setChecked(config.enabled);
    refreshSequences();
    priorities_->setRowCount(rules.size());
    for (int row = 0; row < rules.size(); ++row) {
        const auto& rule = rules[row];
        priorities_->setItem(row, 0, PageWidgets::item(ruleLabel(rule)));
        auto* priority = new QComboBox;
        priority->addItems({QStringLiteral("普通"), QStringLiteral("较高"), QStringLiteral("最高 · 立即执行")});
        priority->setCurrentIndex(static_cast<int>(rule.priority));
        priorities_->setCellWidget(row, 1, priority);
        connect(priority, &QComboBox::currentIndexChanged, this, [this, number = rule.number](int index) {
            emit priorityChanged(number, static_cast<ColorRulePriority>(index));
        });
    }
}

void AdvancedLogicPage::refreshSequences()
{
    const int selected = sequences_->currentRow();
    sequences_->setRowCount(config_.sequences.size());
    for (int row = 0; row < config_.sequences.size(); ++row) {
        const auto& sequence = config_.sequences[row];
        bool valid = !sequence.nodes.isEmpty();
        QStringList nodes;
        for (const auto& node : sequence.nodes) {
            const auto found = std::find_if(rules_.cbegin(), rules_.cend(), [&](const auto& rule) { return rule.number == node.ruleNumber; });
            const bool exists = found != rules_.cend();
            valid = valid && exists;
            QString policy = node.effect == WhitelistNodeEffect::Keep ? QStringLiteral("保留") : QStringLiteral("消除");
            if (exists && found->priority == ColorRulePriority::Immediate) { policy += QStringLiteral(" / 最高立即执行"); }
            nodes.append(QStringLiteral("%1【%2】").arg(exists ? ruleLabel(*found) : QStringLiteral("#%1 已删除").arg(node.ruleNumber), policy));
        }
        const QStringList columns{!valid ? QStringLiteral("规则缺失") : sequence.enabled ? QStringLiteral("启用") : QStringLiteral("停用"),
            sequence.name, sequence.sameSource ? QStringLiteral("同一账号") : QStringLiteral("跨账号"),
            QStringLiteral("%1 秒").arg(sequence.timeoutMs / 1000.0), nodes.join(QStringLiteral(" → "))};
        for (int col = 0; col < columns.size(); ++col) {
            auto* cell = PageWidgets::item(columns[col]);
            cell->setToolTip(columns[col]);
            sequences_->setItem(row, col, cell);
        }
    }
    if (selected >= 0 && selected < sequences_->rowCount()) { sequences_->selectRow(selected); }
    refreshActions();
}

void AdvancedLogicPage::refreshActions()
{
    const int row = sequences_->currentRow();
    const bool selected = row >= 0 && row < config_.sequences.size();
    add_->setEnabled(!rules_.isEmpty());
    edit_->setEnabled(selected && !rules_.isEmpty());
    remove_->setEnabled(selected);
    up_->setEnabled(selected && row > 0);
    down_->setEnabled(selected && row + 1 < config_.sequences.size());
}

void AdvancedLogicPage::editSequence(bool create)
{
    if (rules_.isEmpty()) { return; }
    const int row = sequences_->currentRow();
    if (!create && (row < 0 || row >= config_.sequences.size())) { return; }
    const auto result = promptSequence(this, create ? WhitelistSequence{} : config_.sequences[row], rules_);
    if (!result) { return; }
    if (create) { config_.sequences.append(*result); }
    else { config_.sequences[row] = *result; }
    refreshSequences();
    sequences_->selectRow(create ? sequences_->rowCount() - 1 : row);
    emit configurationChanged();
}

void AdvancedLogicPage::moveSequence(int direction)
{
    const int row = sequences_->currentRow();
    if (row < 0 || row + direction < 0 || row + direction >= config_.sequences.size()) { return; }
    config_.sequences.swapItemsAt(row, row + direction);
    refreshSequences();
    sequences_->selectRow(row + direction);
    emit configurationChanged();
}

void AdvancedLogicPage::showDecision(int ruleNumber, bool execute)
{
    status_->setText(QStringLiteral("最近判定：图色规则 #%1 · %2").arg(ruleNumber)
        .arg(execute ? QStringLiteral("执行效果") : QStringLiteral("白名单匹配，消除效果")));
}
