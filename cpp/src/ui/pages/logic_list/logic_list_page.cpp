#include "ui/common/editor_dialogs.h"
#include "ui/pages/logic_list/logic_list_page.h"
#include "ui/logic_binding_source.h"
#include "ui/common/page_widgets.h"
#include "ui/replica_row/scroll_passthrough_controls.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace {
QString sourceDisplayName(const QString& title)
{
    return title.startsWith(QStringLiteral("EVE - "), Qt::CaseInsensitive)
        ? title.mid(6) : title;
}
}

LogicListPage::LogicListPage(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("logicListPage"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(PageWidgets::makeHeading(QStringLiteral("逻辑列表")));
    layout->addWidget(PageWidgets::makeDescription(QStringLiteral(
        "单击添加逻辑后选择源窗口，或长按添加逻辑批量选择账号。该实例的所有区域条件槽及跳转逻辑共用此来源；同一逻辑可添加多份。")));
    auto* actions = new QHBoxLayout;
    rules_ = new QComboBox;
    rules_->setObjectName(QStringLiteral("logicDefinitionChoice"));
    auto* add = new QPushButton(QStringLiteral("＋ 添加逻辑"));
    add->setObjectName(QStringLiteral("addLogicBinding"));
    add->setToolTip(QStringLiteral("单击添加一条逻辑；长按选择要添加的账号"));
    auto* refresh = new QPushButton(QStringLiteral("刷新源窗口"));
    refresh->setObjectName(QStringLiteral("refreshLogicSources"));
    actions->addWidget(rules_, 1); actions->addWidget(add); actions->addWidget(refresh);
    layout->addLayout(actions);
    auto* filters = new QHBoxLayout;
    sourceFilter_ = new QComboBox;
    sourceFilter_->setObjectName(QStringLiteral("logicSourceFilter"));
    sourceFilter_->addItem(QStringLiteral("全部源窗口"));
    logicFilter_ = new QComboBox;
    logicFilter_->setObjectName(QStringLiteral("logicNameFilter"));
    logicFilter_->addItem(QStringLiteral("全部逻辑"), 0);
    auto* resetFilters = new QPushButton(QStringLiteral("重置筛选"));
    resetFilters->setObjectName(QStringLiteral("resetLogicFilters"));
    filters->addWidget(new QLabel(QStringLiteral("源窗口")));
    filters->addWidget(sourceFilter_, 1);
    filters->addWidget(new QLabel(QStringLiteral("逻辑名")));
    filters->addWidget(logicFilter_, 1);
    filters->addWidget(resetFilters);
    layout->addLayout(filters);
    connect(sourceFilter_, &QComboBox::currentIndexChanged, this, &LogicListPage::applyFilters);
    connect(logicFilter_, &QComboBox::currentIndexChanged, this, &LogicListPage::applyFilters);
    connect(resetFilters, &QPushButton::clicked, this, [this] {
        sourceFilter_->setCurrentIndex(0);
        logicFilter_->setCurrentIndex(0);
    });
    table_ = new QTableWidget(0, 5);
    table_->setObjectName(QStringLiteral("logicBindingTable"));
    table_->setHorizontalHeaderLabels({QStringLiteral("启用"), QStringLiteral("图色逻辑"),
        QStringLiteral("源窗口（全部条件槽共用）"), QStringLiteral("状态"), QStringLiteral("操作")});
    PageWidgets::configureTable(table_);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    layout->addWidget(table_, 1);
    auto* holdTimer = new QTimer(add);
    holdTimer->setSingleShot(true);
    holdTimer->setInterval(600);
    connect(add, &QPushButton::pressed, holdTimer, qOverload<>(&QTimer::start));
    connect(add, &QPushButton::released, holdTimer, &QTimer::stop);
    connect(holdTimer, &QTimer::timeout, this, [this, add] {
        if (!add->isDown() || !add->isEnabled() || rules_->currentData().toInt() <= 0) { return; }
        // Consume the held press so releasing it cannot also add an unbound row.
        add->setDown(false);
        chooseSourcesForAddition();
    });
    connect(add, &QPushButton::clicked, this, [this, holdTimer] {
        holdTimer->stop();
        if (rules_->currentData().toInt() > 0) { emit addRequested(rules_->currentData().toInt()); }
    });
    connect(refresh, &QPushButton::clicked, this, &LogicListPage::refreshRequested);
}

void LogicListPage::chooseSourcesForAddition()
{
    const int number = rules_->currentData().toInt();
    EditorDialogs::Dialog dialog(this);
    dialog.setObjectName(QStringLiteral("logicSourceSelectionDialog"));
    dialog.setWindowTitle(QStringLiteral("选择要添加逻辑的账号"));
    dialog.resize(420, 360);
    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(QStringLiteral("为勾选的账号添加：%1").arg(rules_->currentText())));
    auto* choices = new QListWidget;
    choices->setObjectName(QStringLiteral("logicSourceChoices"));
    for (const auto& source : lastSources_) {
        auto* item = new QListWidgetItem(sourceDisplayName(source.title), choices);
        item->setData(Qt::UserRole, QVariant::fromValue(source));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
    }
    if (choices->count() == 0) {
        layout->addWidget(new QLabel(QStringLiteral("未找到可用账号，请关闭后刷新源窗口。")));
    }
    layout->addWidget(choices, 1);
    auto* selectAll = new QCheckBox(QStringLiteral("全选"));
    selectAll->setObjectName(QStringLiteral("selectAllLogicSources"));
    selectAll->setEnabled(choices->count() > 0);
    layout->addWidget(selectAll);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("添加"));
    buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    layout->addWidget(buttons);
    connect(selectAll, &QCheckBox::toggled, choices, [choices](bool checked) {
        for (int i = 0; i < choices->count(); ++i) {
            choices->item(i)->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
        }
    });
    connect(choices, &QListWidget::itemChanged, &dialog, [choices, selectAll, buttons] {
        int checked = 0;
        for (int i = 0; i < choices->count(); ++i) {
            if (choices->item(i)->checkState() == Qt::Checked) { ++checked; }
        }
        const QSignalBlocker blocker(selectAll);
        selectAll->setChecked(checked > 0 && checked == choices->count());
        buttons->button(QDialogButtonBox::Ok)->setEnabled(checked > 0);
    });
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) { return; }
    QList<WindowInfo> selected;
    for (int i = 0; i < choices->count(); ++i) {
        const auto* item = choices->item(i);
        if (item->checkState() == Qt::Checked) { selected.append(item->data(Qt::UserRole).value<WindowInfo>()); }
    }
    if (!selected.isEmpty()) { emit addForSourcesRequested(number, selected); }
}
void LogicListPage::setConfiguration(const QList<ColorRule>& rules,
    const QList<LogicBindingConfig>& bindings, const QList<WindowInfo>& sources,
    const QHash<QString, QString>& statuses)
{
    if (lastRules_ == rules && lastBindings_ == bindings && lastSources_ == sources && lastStatuses_ == statuses) { return; }
    lastRules_ = rules; lastBindings_ = bindings; lastSources_ = sources; lastStatuses_ = statuses;
    const int selected = rules_->currentData().toInt();
    const QSignalBlocker blocker(rules_);
    rules_->clear();
    QHash<int, QString> labels;
    for (const auto& rule : rules) {
        const auto label = QStringLiteral("#%1 %2").arg(rule.number).arg(rule.note);
        labels.insert(rule.number, label); rules_->addItem(label, rule.number);
    }
    rules_->setCurrentIndex(qMax(0, rules_->findData(selected)));
    const auto selectedSourceFilter = sourceFilter_->currentData();
    const int selectedLogicFilter = logicFilter_->currentData().toInt();
    const QSignalBlocker sourceFilterBlocker(sourceFilter_);
    const QSignalBlocker logicFilterBlocker(logicFilter_);
    sourceFilter_->clear();
    sourceFilter_->addItem(QStringLiteral("全部源窗口"));
    logicFilter_->clear();
    logicFilter_->addItem(QStringLiteral("全部逻辑"), 0);
    QSet<QString> sourceTitles;
    QSet<int> ruleNumbers;
    for (const auto& binding : bindings) {
        if (!sourceTitles.contains(binding.sourceTitle)) {
            sourceTitles.insert(binding.sourceTitle);
            sourceFilter_->addItem(binding.sourceTitle.isEmpty() ? QStringLiteral("未选择源窗口")
                : sourceDisplayName(binding.sourceTitle), binding.sourceTitle);
        }
        if (!ruleNumbers.contains(binding.ruleNumber)) {
            ruleNumbers.insert(binding.ruleNumber);
            logicFilter_->addItem(labels.value(binding.ruleNumber,
                QStringLiteral("已删除逻辑 #%1").arg(binding.ruleNumber)), binding.ruleNumber);
        }
    }
    sourceFilter_->setCurrentIndex(qMax(0, sourceFilter_->findData(selectedSourceFilter)));
    logicFilter_->setCurrentIndex(qMax(0, logicFilter_->findData(selectedLogicFilter)));
    table_->setRowCount(0);
    table_->setRowCount(bindings.size());
    for (int row = 0; row < bindings.size(); ++row) {
        const auto binding = bindings[row];
        auto* enabled = new QCheckBox;
        enabled->setObjectName(QStringLiteral("logicBindingEnabled_%1").arg(binding.id));
        enabled->setChecked(binding.enabled);
        table_->setCellWidget(row, 0, enabled);
        table_->setItem(row, 1, PageWidgets::item(labels.value(binding.ruleNumber,
            QStringLiteral("已删除逻辑 #%1").arg(binding.ruleNumber))));
        auto* source = new ReplicaRowUi::ScrollPassthroughCombo;
        source->setObjectName(QStringLiteral("logicBindingSource_%1").arg(binding.id));
        source->addItem(QStringLiteral("选择源窗口…"), QVariant::fromValue(WindowInfo{}));
        int selectedSource = 0;
        const auto* resolved = resolveLogicBindingSource(binding, sources);
        for (const auto& window : sources) {
            source->addItem(sourceDisplayName(window.title),
                QVariant::fromValue(window));
            if (&window == resolved) {
                selectedSource = source->count() - 1;
            }
        }
        if (!selectedSource && !binding.sourceTitle.isEmpty()) {
            source->addItem(QStringLiteral("暂未找到 · %1").arg(sourceDisplayName(binding.sourceTitle)),
                QVariant::fromValue(WindowInfo{0, binding.sourceTitle}));
            selectedSource = source->count() - 1;
        }
        source->setCurrentIndex(selectedSource);
        table_->setCellWidget(row, 2, source);
        table_->setItem(row, 3, PageWidgets::item(statuses.value(binding.id, QStringLiteral("待绑定"))));
        auto* remove = new QPushButton(QStringLiteral("移除"));
        table_->setCellWidget(row, 4, remove);
        connect(enabled, &QCheckBox::toggled, this, [this, id = binding.id](bool value) { emit enabledChanged(id, value); });
        connect(source, &QComboBox::activated, this, [this, source, id = binding.id] {
            emit sourceChanged(id, source->currentData().value<WindowInfo>());
        });
        connect(remove, &QPushButton::clicked, this, [this, id = binding.id] { emit removeRequested(id); });
    }
    table_->resizeRowsToContents();
    applyFilters();
}

void LogicListPage::applyFilters()
{
    const auto source = sourceFilter_->currentData();
    const int ruleNumber = logicFilter_->currentData().toInt();
    for (int row = 0; row < lastBindings_.size(); ++row) {
        const auto& binding = lastBindings_[row];
        table_->setRowHidden(row, (source.isValid() && binding.sourceTitle != source.toString())
            || (ruleNumber > 0 && binding.ruleNumber != ruleNumber));
    }
}
