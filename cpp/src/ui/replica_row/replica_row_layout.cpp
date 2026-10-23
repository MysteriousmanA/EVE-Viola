#include "ui/replica_row.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include "ui/replica_row/replica_row_constants.h"

using namespace ReplicaRowUi;

void ReplicaRow::buildContentLayout()
{
    auto* switchLayout = new QVBoxLayout;
    switchLayout->setSpacing(2);
    switchLayout->addWidget(sourceNameVisibilityCheckBox_);

    auto* metaLayout = new QVBoxLayout;
    metaLayout->setSpacing(2);
    metaLayout->addWidget(new QLabel(QStringLiteral("截取区域")));
    auto* regionEditLayout = new QHBoxLayout;
    regionEditLayout->setSpacing(5);
    regionEditLayout->addWidget(regionRecaptureButton_);
    regionEditLayout->addWidget(regionEdit_, 1);
    metaLayout->addLayout(regionEditLayout);

    auto* presetComboLayout = new QVBoxLayout;
    presetComboLayout->setSpacing(2);
    presetComboLayout->addWidget(new QLabel(QStringLiteral("当前区域预设")));
    presetComboLayout->addWidget(regionPresetCombo_);

    auto* positionGroupLayout = new QVBoxLayout;
    positionGroupLayout->setSpacing(2);
    positionGroupLayout->addWidget(new QLabel(QStringLiteral("小窗当前位置")));
    positionGroupLayout->addWidget(positionEdit_);

    auto* sizeGroupLayout = new QVBoxLayout;
    sizeGroupLayout->setSpacing(2);
    sizeGroupLayout->addWidget(new QLabel(QStringLiteral("小窗当前尺寸")));
    sizeGroupLayout->addWidget(sizeEdit_);

    auto* scaleGroupLayout = new QVBoxLayout;
    scaleGroupLayout->setSpacing(2);
    scaleGroupLayout->addWidget(new QLabel(QStringLiteral("缩放系数")));
    scaleGroupLayout->addWidget(scaleEdit_);

    auto* intervalLayout = new QVBoxLayout;
    intervalLayout->setSpacing(2);
    intervalLayout->addWidget(new QLabel(QStringLiteral("统计间隔")));
    intervalLayout->addWidget(statisticsInterval_);

    auto* regionRowLayout = new QHBoxLayout;
    regionRowLayout->setSpacing(12);
    regionRowLayout->addLayout(metaLayout, 1);
    regionRowLayout->addLayout(presetComboLayout, 1);
    regionRowLayout->addWidget(savePresetButton_, 0, Qt::AlignBottom);

    auto* positionLayout = new QHBoxLayout;
    positionLayout->setSpacing(12);
    positionLayout->addLayout(positionGroupLayout, 1);
    positionLayout->addWidget(visibilityCheckBox_, 0, Qt::AlignBottom);
    positionLayout->addStretch(2);

    auto* sizeLayout = new QHBoxLayout;
    sizeLayout->setSpacing(12);
    sizeLayout->addLayout(sizeGroupLayout, 3);
    sizeLayout->addLayout(scaleGroupLayout, 2);
    sizeLayout->addStretch(7);

    auto* slotRowA = new QHBoxLayout;
    slotRowA->setSpacing(4);
    slotRowA->addWidget(colorLogicSlotA_, 1);
    slotRowA->addWidget(colorLogicSlotColorA_);

    auto* slotLayoutA = new QVBoxLayout;
    slotLayoutA->setSpacing(2);
    slotLayoutA->addWidget(new QLabel(QStringLiteral("图色逻辑槽位 1")));
    slotLayoutA->addLayout(slotRowA);

    auto* slotRowB = new QHBoxLayout;
    slotRowB->setSpacing(4);
    slotRowB->addWidget(colorLogicSlotB_, 1);
    slotRowB->addWidget(colorLogicSlotColorB_);

    auto* slotLayoutB = new QVBoxLayout;
    slotLayoutB->setSpacing(2);
    slotLayoutB->addWidget(new QLabel(QStringLiteral("图色逻辑槽位 2")));
    slotLayoutB->addLayout(slotRowB);

    auto* activeSlotLayout = new QVBoxLayout;
    activeSlotLayout->setSpacing(2);
    activeSlotLayout->addWidget(new QLabel(QStringLiteral("当前启用槽位")));
    activeSlotLayout->addWidget(activeSlotCombo_);

    auto* enabledLayout = new QVBoxLayout;
    enabledLayout->setSpacing(2);
    enabledLayout->addWidget(colorLogicEnabledCheck_);
    enabledLayout->addStretch(1);

    auto* colorLogicLayout = new QHBoxLayout;
    colorLogicLayout->setSpacing(12);
    colorLogicLayout->addLayout(activeSlotLayout);
    colorLogicLayout->addLayout(slotLayoutA, 1);
    colorLogicLayout->addLayout(slotLayoutB, 1);
    colorLogicLayout->addLayout(enabledLayout);

    auto* pixelLayout = new QHBoxLayout;
    pixelLayout->setSpacing(12);
    pixelLayout->addWidget(pixelStatisticsHost_, 1);
    pixelLayout->addWidget(statisticsColorsButton_);

    auto* measurementsLayout = new QHBoxLayout;
    measurementsLayout->setSpacing(12);
    measurementsLayout->addLayout(pixelLayout, 1);
    measurementsLayout->addLayout(intervalLayout);

    contentHost_ = new QWidget;
    auto* contentLayout = new QVBoxLayout(contentHost_);
    contentLayout->setSpacing(7);
    contentLayout->addLayout(switchLayout);
    contentLayout->addLayout(regionRowLayout);
    contentLayout->addLayout(positionLayout);
    contentLayout->addLayout(sizeLayout);
    contentLayout->addLayout(colorLogicLayout);
    contentLayout->addLayout(measurementsLayout);
}

void ReplicaRow::buildRowLayout()
{
    sourceCell_ = new QWidget;
    sourceCell_->setObjectName(QStringLiteral("sourceWindowCell"));
    sourceCell_->setFixedWidth(kSourceCellWidth);
    sourceDetails_ = new QWidget(sourceCell_);
    sourceDetails_->setObjectName(QStringLiteral("sourceWindowDetails"));
    auto* sourceDetailsLayout = new QVBoxLayout(sourceDetails_);
    sourceDetailsLayout->setContentsMargins(8, 4, 8, 4);
    sourceDetailsLayout->setSpacing(5);
    auto* sourceLabel = new QLabel(QStringLiteral("源窗口"));
    sourceLabel->setObjectName(QStringLiteral("sourceWindowLabel"));
    sourceDetailsLayout->addWidget(sourceLabel);
    sourceDetailsLayout->addWidget(sourceCombo_);
    groupCountLabel_ = new QLabel;
    groupCountLabel_->setObjectName(QStringLiteral("sourceGroupCount"));
    groupCountLabel_->setAlignment(Qt::AlignCenter);
    groupCountLabel_->hide();
    sourceDetailsLayout->addWidget(groupCountLabel_);
    sourceDetailsLayout->addWidget(collapseButton_);

    auto* sourceCellLayout = new QVBoxLayout(sourceCell_);
    sourceCellLayout->setContentsMargins(7, 0, 7, 0);
    sourceCellLayout->addStretch(1);
    sourceCellLayout->addWidget(sourceDetails_);
    sourceCellLayout->addStretch(1);

    auto* actionsLayout = new QVBoxLayout;
    actionsLayout->setSpacing(7);
    actionsLayout->addWidget(closeButton_);
    actionsLayout->addWidget(copyButton_);
    actionsLayout->addWidget(syncActionRow_);
    actionsLayout->addStretch(1);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 9, 10, 9);
    layout->setSpacing(12);
    layout->addWidget(sourceCell_);
    layout->addWidget(badge_, 0, Qt::AlignTop);
    layout->addWidget(contentHost_, 1);
    layout->addLayout(actionsLayout);
}
