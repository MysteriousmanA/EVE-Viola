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

    // Legacy controls are retained only for loading older configs and API compatibility.
    for (QWidget* control : QList<QWidget*>{colorLogicSlotA_, colorLogicSlotB_,
        colorLogicSlotColorA_, colorLogicSlotColorB_, activeSlotCombo_, colorLogicEnabledCheck_}) {
        control->setParent(this);
        control->hide();
    }
    colorLogicSummary_ = new QLabel;
    colorLogicSummary_->setObjectName(QStringLiteral("colorLogicSummary"));
    colorLogicSummary_->setWordWrap(true);
    colorLogicSummary_->hide();
    auto* pixelLayout = new QHBoxLayout;
    pixelLayout->setSpacing(12);
    pixelLayout->addWidget(pixelStatisticsHost_, 1);

    auto* measurementsLayout = new QHBoxLayout;
    measurementsLayout->setSpacing(12);
    measurementsLayout->addLayout(pixelLayout, 1);
    measurementsLayout->addLayout(intervalLayout);

    contentHost_ = new QWidget;
    auto* contentLayout = new QVBoxLayout(contentHost_);
    contentLayout->setSpacing(7);
    contentLayout->addLayout(regionRowLayout);
    contentLayout->addLayout(positionLayout);
    contentLayout->addLayout(sizeLayout);
    contentLayout->addWidget(colorLogicSummary_);
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
    collapsedPixelStatistics_ = new QLabel;
    collapsedPixelStatistics_->setObjectName(QStringLiteral("collapsedPixelStatistics"));
    collapsedPixelStatistics_->setWordWrap(true);
    collapsedPixelStatistics_->setAlignment(Qt::AlignCenter);
    collapsedPixelStatistics_->installEventFilter(this);
    collapsedPixelStatistics_->hide();
    layout->addWidget(collapsedPixelStatistics_, 1);
    layout->addLayout(actionsLayout);
}

void ReplicaRow::setColorLogicSummary(const QString& text)
{
    const QString summary = text == QStringLiteral("未被图色逻辑引用") ? QString{} : text;
    colorLogicSummary_->setText(summary);
    colorLogicSummary_->setVisible(!summary.isEmpty());
    colorLogicSummary_->setToolTip(QStringLiteral("在“图色逻辑”页中配置小窗条件槽和执行小窗。"));
}
