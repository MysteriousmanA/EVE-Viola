#include "ui/pages/region_preset/region_preset_page.h"
#include "config/config_validation.h"

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
RegionPresetPage::RegionPresetPage(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    layout->addWidget(makeHeading(QStringLiteral("区域预设")));
    layout->addWidget(makeDescription(QStringLiteral(
        "可在当前最上方的 EVE 游戏画面中框选并保存区域；之后可从小窗右键菜单直接应用。")));

    auto *saveButton = new QPushButton(QStringLiteral("保存配置"));
    saveButton->setObjectName(QStringLiteral("saveRegionPresetConfiguration"));
    saveButton->setProperty("accent", true);
    auto *captureButton = new QPushButton(QStringLiteral("框选区域"));
    captureButton->setObjectName(QStringLiteral("captureRegionPreset"));
    captureButton->setToolTip(QStringLiteral("在当前最上方的 EVE 游戏画面中框选并保存区域预设"));
    auto *actions = new QHBoxLayout;
    actions->addStretch(1);
    actions->addWidget(captureButton);
    actions->addWidget(saveButton);
    layout->addLayout(actions);

    table_ = new QTableWidget(0, 7);
    table_->setObjectName(QStringLiteral("regionPresetTable"));
    table_->setHorizontalHeaderLabels(
        {QStringLiteral("预设名称"), QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("宽"),
         QStringLiteral("高"), QStringLiteral("像素统计间隔"), QStringLiteral("操作")});
    configureTable(table_);
    table_->verticalHeader()->setDefaultSectionSize(47);
    table_->horizontalHeader()->setStretchLastSection(false);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    for (int column = 1; column < 5; ++column) {
        table_->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    }
    table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    table_->horizontalHeader()->resizeSection(5, 120);
    table_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Fixed);
    table_->horizontalHeader()->resizeSection(6, 80);
    layout->addWidget(table_, 1);

    connect(saveButton, &QPushButton::clicked, this, &RegionPresetPage::saveRequested);
    connect(captureButton, &QPushButton::clicked, this, &RegionPresetPage::captureRequested);
    connect(table_, &QTableWidget::cellDoubleClicked, this,
            [this](int row, int) { emit renameRequested(row); });
}

void RegionPresetPage::setPresets(const QList<RegionPreset> &presets)
{
    table_->setRowCount(presets.size());
    for (int row = 0; row < presets.size(); ++row) {
        const auto &preset = presets[row];
        table_->setItem(row, 0, item(preset.name));
        table_->setItem(row, 1, item(QString::number(preset.region.x())));
        table_->setItem(row, 2, item(QString::number(preset.region.y())));
        table_->setItem(row, 3, item(QString::number(preset.region.width())));
        table_->setItem(row, 4, item(QString::number(preset.region.height())));

        auto *interval = new QComboBox;
        interval->setObjectName(QStringLiteral("regionPresetInterval"));
        for (int milliseconds = kStatisticsIntervalDisabledMs;
             milliseconds <= kStatisticsIntervalMaximumMs;
             milliseconds += kStatisticsIntervalStepMs) {
            interval->addItem(statisticsIntervalLabel(milliseconds), milliseconds);
        }
        interval->setCurrentIndex(interval->findData(preset.statisticsIntervalMs));
        table_->setCellWidget(row, 5, interval);

        auto *deleteButton = new QPushButton(QStringLiteral("删除"));
        deleteButton->setObjectName(QStringLiteral("deleteRegionPresetRow"));
        deleteButton->setToolTip(QStringLiteral("删除区域预设“%1”").arg(preset.name));
        table_->setCellWidget(row, 6, deleteButton);
        connect(deleteButton, &QPushButton::clicked, this,
                [this, row] { emit deleteRequested(row); });
        connect(interval, &QComboBox::currentIndexChanged, this, [this, row](int) {
            auto *combo = static_cast<QComboBox *>(table_->cellWidget(row, 5));
            if (combo != nullptr) {
                emit statisticsIntervalChanged(row, combo->currentData().toInt());
            }
        });
    }
}
