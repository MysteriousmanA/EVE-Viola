#include "ui/pages/replica_list/replica_list_page.h"
#include "ui/replica_row.h"
#include "ui/replica_window.h"
#include "features/replica_list/statistics_workload_estimator.h"
#include <algorithm>
#include <numeric>
#include <optional>
#include <utility>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QFrame>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QMap>
ReplicaListPage::ReplicaListPage(QWidget *parent) : QWidget(parent)
{
    auto *page = this;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    emptyState_ = new QLabel(QStringLiteral("还没有小窗\n点击首页“生成小窗”开始"));
    emptyState_->setObjectName(QStringLiteral("muted"));
    emptyState_->setAlignment(Qt::AlignCenter);
    emptyState_->setMinimumHeight(150);

    auto *filterLayout = new QHBoxLayout;
    filterLayout->setContentsMargins(0, 0, 0, 0);
    filterLayout->setSpacing(8);
    filterLayout->addWidget(new QLabel(QStringLiteral("源窗口"), page));
    sourceFilterCombo_ = new QComboBox(page);
    sourceFilterCombo_->setObjectName(QStringLiteral("replicaSourceFilter"));
    sourceFilterCombo_->setMinimumWidth(150);
    filterLayout->addWidget(sourceFilterCombo_);
    filterLayout->addWidget(new QLabel(QStringLiteral("区域名称"), page));
    regionFilterCombo_ = new QComboBox(page);
    regionFilterCombo_->setObjectName(QStringLiteral("replicaRegionFilter"));
    regionFilterCombo_->setMinimumWidth(130);
    filterLayout->addWidget(regionFilterCombo_);
    lockReplicaSizeCheckBox_ = new QCheckBox(QStringLiteral("锁定小窗尺寸"), page);
    lockReplicaSizeCheckBox_->setObjectName(QStringLiteral("lockReplicaSize"));
    lockReplicaSizeCheckBox_->setToolTip(
        QStringLiteral("选中后所有小窗都不能使用鼠标滚轮缩放；仍可在列表中输入缩放系数"));
    filterLayout->addWidget(lockReplicaSizeCheckBox_);
    auto *collapseAllButton = new QPushButton(QStringLiteral("折叠所有窗口"), page);
    collapseAllButton->setObjectName(QStringLiteral("collapseAllReplicas"));
    collapseAllButton->setToolTip(QStringLiteral("将全部小窗列表卡片按源窗口分组折叠"));
    filterLayout->addWidget(collapseAllButton);
    filterLayout->addStretch(1);
    layout->addLayout(filterLayout);

    connect(collapseAllButton, &QPushButton::clicked, this, [this] {
        for (auto *row : rows_) {
            const QSignalBlocker blocker(row);
            row->setCollapsed(true);
        }
        refreshReplicaGroups();
        emit persistRequested();
    });

    connect(lockReplicaSizeCheckBox_, &QCheckBox::toggled, this, &ReplicaListPage::sizeLockChanged);

    connect(sourceFilterCombo_, &QComboBox::currentIndexChanged, this, [this] {
        refreshReplicaFilters();
        refreshReplicaGroups();
    });
    connect(regionFilterCombo_, &QComboBox::currentIndexChanged, this,
            &ReplicaListPage::refreshReplicaGroups);

    auto *scroll = new QScrollArea;
    scroll->setObjectName(QStringLiteral("replicaListScroll"));
    scroll->setWidgetResizable(true);
    listScroll_ = scroll;
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *listHost = new QWidget;
    listLayout_ = new QVBoxLayout(listHost);
    listLayout_->setContentsMargins(0, 0, 0, 0);
    listLayout_->setSpacing(9);
    listLayout_->setSizeConstraint(QLayout::SetMinAndMaxSize);
    listLayout_->addWidget(emptyState_);
    listLayout_->addStretch(1);
    scroll->setWidget(listHost);
    layout->addWidget(scroll);
    refreshReplicaFilters();
}
QList<QList<ReplicaRow *>> ReplicaListPage::replicaGroups() const
{
    QList<QList<ReplicaRow *>> groups;
    QHash<QString, qsizetype> groupIndices;
    for (const ReplicaId id : orderedIds_) {
        const auto *window = windows_.value(id);
        QString key;
        if (window != nullptr && window->source().has_value()) {
            key = QStringLiteral("source:%1").arg(static_cast<qulonglong>(window->source()->hwnd));
        } else {
            key = QStringLiteral("unbound:%1").arg(id);
        }

        auto *row = rows_.value(id);
        if (row == nullptr) {
            continue;
        }
        const auto group = groupIndices.constFind(key);
        if (group == groupIndices.cend()) {
            groupIndices.insert(key, groups.size());
            groups.append(QList<ReplicaRow *>{row});
        } else {
            groups[group.value()].append(row);
        }
    }
    std::stable_sort(groups.begin(), groups.end(), [](const auto &left, const auto &right) {
        return QString::compare(left.front()->sourceCombo()->currentText(),
                                right.front()->sourceCombo()->currentText(),
                                Qt::CaseInsensitive) < 0;
    });
    return groups;
}
void ReplicaListPage::refreshReplicaGroups()
{
    if (listLayout_ == nullptr) {
        return;
    }
    const QString sourceFilter =
        sourceFilterCombo_ != nullptr && sourceFilterCombo_->currentIndex() > 0
            ? sourceFilterCombo_->currentData().toString()
            : QString();
    const QString regionFilter =
        regionFilterCombo_ != nullptr && regionFilterCombo_->currentIndex() > 0
            ? regionFilterCombo_->currentData().toString()
            : QString();
    int matchingRowCount = 0;
    const auto groups = replicaGroups();
    int layoutIndex = 1;
    for (const auto &group : groups) {
        for (auto *row : group) {
            listLayout_->removeWidget(row);
            listLayout_->insertWidget(layoutIndex++, row);
        }
    }
    synchronizingReplicaGroupCollapse_ = true;
    for (const auto &group : groups) {
        const bool collapsed = std::any_of(group.cbegin(), group.cend(),
                                           [](const ReplicaRow *row) { return row->collapsed(); });
        for (auto *row : group) {
            if (row->collapsed() != collapsed) {
                row->setCollapsed(collapsed);
            }
        }
        QList<ReplicaRow *> matchingRows;
        for (auto *row : group) {
            const bool matchesSource =
                sourceFilter.isEmpty() ||
                row->sourceCombo()->currentText().compare(sourceFilter, Qt::CaseInsensitive) == 0;
            const bool matchesRegion =
                regionFilter.isEmpty() ||
                row->currentRegionFilterName().compare(regionFilter, Qt::CaseInsensitive) == 0;
            if (matchesSource && matchesRegion) {
                matchingRows.append(row);
            } else {
                row->setVisible(false);
            }
        }
        matchingRowCount += matchingRows.size();
        if (matchingRows.isEmpty()) {
            continue;
        }
        StatisticsWorkloadEstimate workload;
        if (collapsed) {
            QList<StatisticsWorkloadInput> matchingWindows;
            matchingWindows.reserve(matchingRows.size());
            for (const auto *row : matchingRows) {
                const auto *window = windows_.value(row->replicaId(), nullptr);
                if (window && window->source().has_value()) {
                    matchingWindows.append({window->source()->hwnd, window->sourceFrameSize(),
                                            window->capturedRegion(),
                                            window->statisticsIntervalMs()});
                }
            }
            workload = estimateStatisticsWorkload(matchingWindows);
        }
        const int representativeIndex = collapsed ? 0 : matchingRows.size() / 2;
        for (int index = 0; index < matchingRows.size(); ++index) {
            auto *row = matchingRows[index];
            const bool representative = index == representativeIndex;
            row->setVisible(!collapsed || representative);
            row->setGroupPresentation(index, matchingRows.size(), representative,
                                      workload.capturePixelsPerSecond,
                                      workload.scanPixelsPerSecond);
        }
    }
    synchronizingReplicaGroupCollapse_ = false;
    const bool noReplicas = rows_.isEmpty();
    emptyState_->setText(noReplicas ? QStringLiteral("还没有小窗\n点击首页“生成小窗”开始")
                                    : QStringLiteral("没有符合筛选条件的小窗"));
    emptyState_->setVisible(noReplicas || matchingRowCount == 0);
    refreshReplicaListSize();
    QTimer::singleShot(0, this, &ReplicaListPage::refreshReplicaGroupBars);
}
void ReplicaListPage::refreshReplicaFilters()
{
    if (sourceFilterCombo_ == nullptr || regionFilterCombo_ == nullptr) {
        return;
    }

    const QString selectedSource = sourceFilterCombo_->currentIndex() > 0
                                       ? sourceFilterCombo_->currentData().toString()
                                       : QString();
    const QString selectedRegion = regionFilterCombo_->currentIndex() > 0
                                       ? regionFilterCombo_->currentData().toString()
                                       : QString();
    const QSignalBlocker sourceBlocker(sourceFilterCombo_);
    const QSignalBlocker regionBlocker(regionFilterCombo_);

    QStringList sourceNames;
    for (auto *row : orderedRows()) {
        const QString sourceName = row->sourceCombo()->currentText().trimmed();
        if (!sourceName.isEmpty() && !sourceNames.contains(sourceName, Qt::CaseInsensitive)) {
            sourceNames.append(sourceName);
        }
    }
    std::sort(sourceNames.begin(), sourceNames.end(),
              [](const QString &left, const QString &right) {
                  return QString::compare(left, right, Qt::CaseInsensitive) < 0;
              });
    sourceFilterCombo_->clear();
    sourceFilterCombo_->addItem(QStringLiteral("全部源窗口"), QString());
    for (const auto &sourceName : sourceNames) {
        sourceFilterCombo_->addItem(sourceName, sourceName);
    }
    const int sourceIndex =
        selectedSource.isEmpty() ? 0 : sourceFilterCombo_->findData(selectedSource);
    sourceFilterCombo_->setCurrentIndex(sourceIndex >= 0 ? sourceIndex : 0);

    const QString activeSource = sourceFilterCombo_->currentIndex() > 0
                                     ? sourceFilterCombo_->currentData().toString()
                                     : QString();
    QStringList regionNames;
    for (auto *row : orderedRows()) {
        if (!activeSource.isEmpty() &&
            row->sourceCombo()->currentText().compare(activeSource, Qt::CaseInsensitive) != 0) {
            continue;
        }
        const QString regionName = row->currentRegionFilterName();
        if (!regionNames.contains(regionName, Qt::CaseInsensitive)) {
            regionNames.append(regionName);
        }
    }
    std::sort(regionNames.begin(), regionNames.end(),
              [](const QString &left, const QString &right) {
                  return QString::compare(left, right, Qt::CaseInsensitive) < 0;
              });
    regionFilterCombo_->clear();
    regionFilterCombo_->addItem(QStringLiteral("全部区域"), QString());
    for (const auto &regionName : regionNames) {
        regionFilterCombo_->addItem(regionName, regionName);
    }
    const int regionIndex =
        selectedRegion.isEmpty() ? 0 : regionFilterCombo_->findData(selectedRegion);
    regionFilterCombo_->setCurrentIndex(regionIndex >= 0 ? regionIndex : 0);
}
void ReplicaListPage::refreshReplicaGroupBars()
{
    qDeleteAll(replicaGroupBars_);
    replicaGroupBars_.clear();
    if (listScroll_ == nullptr || listScroll_->widget() == nullptr) {
        return;
    }
    auto *host = listScroll_->widget();
    for (const auto &group : replicaGroups()) {
        if (group.size() < 2) {
            continue;
        }
        ReplicaRow *firstVisible = nullptr;
        ReplicaRow *lastVisible = nullptr;
        for (auto *row : group) {
            if (!row->isHidden()) {
                if (firstVisible == nullptr) {
                    firstVisible = row;
                }
                lastVisible = row;
            }
        }
        if (firstVisible == nullptr || lastVisible == nullptr) {
            continue;
        }
        auto *bar = new QFrame(host);
        bar->setObjectName(QStringLiteral("sourceGroupBar"));
        bar->setAttribute(Qt::WA_TransparentForMouseEvents);
        const QRect firstGeometry = firstVisible->geometry();
        const QRect lastGeometry = lastVisible->geometry();
        bar->setGeometry(firstGeometry.left() + 15, firstGeometry.top(), 4,
                         lastGeometry.bottom() - firstGeometry.top() + 1);
        bar->show();
        bar->raise();
        replicaGroupBars_.append(bar);
    }
}
void ReplicaListPage::synchronizeReplicaGroupCollapse(ReplicaId id, bool collapsed)
{
    if (synchronizingReplicaGroupCollapse_) {
        return;
    }
    for (const auto &group : replicaGroups()) {
        const bool contains =
            std::any_of(group.cbegin(), group.cend(),
                        [id](const ReplicaRow *row) { return row->replicaId() == id; });
        if (!contains) {
            continue;
        }
        synchronizingReplicaGroupCollapse_ = true;
        for (auto *row : group) {
            row->setCollapsed(collapsed);
        }
        synchronizingReplicaGroupCollapse_ = false;
        refreshReplicaGroups();
        emit persistRequested();
        return;
    }
}
void ReplicaListPage::refreshReplicaListSize()
{
    if (listScroll_ == nullptr) {
        return;
    }
    QWidget *host = listScroll_->widget();
    if (host == nullptr) {
        return;
    }
    host->setMinimumHeight(host->sizeHint().height());
    host->adjustSize();
    listScroll_->verticalScrollBar()->setRange(
        0, qMax(0, host->height() - listScroll_->viewport()->height()));
}

void ReplicaListPage::setEntries(const QList<ReplicaId> &order,
                                 const QHash<ReplicaId, ReplicaRow *> &rows,
                                 const QHash<ReplicaId, ReplicaWindow *> &windows)
{
    orderedIds_ = order;
    rows_ = rows;
    windows_ = windows;
}
QList<ReplicaRow *> ReplicaListPage::orderedRows() const
{
    QList<ReplicaRow *> result;
    for (const auto id : orderedIds_) {
        if (auto *row = rows_.value(id)) {
            result.append(row);
        }
    }
    return result;
}
void ReplicaListPage::addRow(ReplicaRow *row)
{
    listLayout_->insertWidget(listLayout_->count() - 1, row);
}
void ReplicaListPage::ensureRowVisible(ReplicaRow *row)
{
    if (row) {
        listScroll_->ensureWidgetVisible(row, 0, 0);
    }
}
void ReplicaListPage::setSizeLocked(bool locked)
{
    const QSignalBlocker blocker(lockReplicaSizeCheckBox_);
    lockReplicaSizeCheckBox_->setChecked(locked);
}
