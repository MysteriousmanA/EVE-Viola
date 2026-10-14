#include "ui/replica_row.h"

#include <QComboBox>
#include <QLabel>
#include "config/config_validation.h"
#include "features/replica/pixel_statistics.h"
#include "ui/replica_row/scroll_passthrough_controls.h"

using namespace ReplicaRowUi;

void ReplicaRow::initializeStatisticsControls()
{
    redPercentageLabel_ = new QLabel(
        pixelColorCategoryName(PixelColorCategory::Red) + QStringLiteral("像素：--"));
    redPercentageLabel_->setObjectName(QStringLiteral("redPixelPercentage"));
    whitePercentageLabel_ = new QLabel(
        pixelColorCategoryName(PixelColorCategory::White) + QStringLiteral("像素：--"));
    whitePercentageLabel_->setObjectName(QStringLiteral("whitePixelPercentage"));

    statisticsInterval_ = new ScrollPassthroughCombo;
    statisticsInterval_->setObjectName(QStringLiteral("statisticsInterval"));
    statisticsInterval_->setToolTip(QStringLiteral("像素统计刷新间隔，0 表示不统计"));
    for (int milliseconds = kStatisticsIntervalDisabledMs;
         milliseconds <= kStatisticsIntervalMaximumMs;
         milliseconds += kStatisticsIntervalStepMs) {
        statisticsInterval_->addItem(
            statisticsIntervalLabel(milliseconds), milliseconds);
    }
    statisticsInterval_->setCurrentIndex(
        statisticsInterval_->findData(kDefaultStatisticsIntervalMs));
    connect(
        statisticsInterval_,
        &QComboBox::currentIndexChanged,
        this,
        [this](int) {
            emit statisticsIntervalChanged(statisticsInterval_->currentData().toInt());
        });
}

void ReplicaRow::setPixelStatistics(qint64 redCount, qint64 whiteCount)
{
    redPercentageLabel_->setText(
        pixelColorCategoryName(PixelColorCategory::Red)
        + QStringLiteral("像素：%1").arg(redCount));
    whitePercentageLabel_->setText(
        pixelColorCategoryName(PixelColorCategory::White)
        + QStringLiteral("像素：%1").arg(whiteCount));
}

void ReplicaRow::clearPixelStatistics()
{
    redPercentageLabel_->setText(
        pixelColorCategoryName(PixelColorCategory::Red) + QStringLiteral("像素：--"));
    whitePercentageLabel_->setText(
        pixelColorCategoryName(PixelColorCategory::White) + QStringLiteral("像素：--"));
}

void ReplicaRow::setStatisticsInterval(int milliseconds)
{
    const int index = statisticsInterval_->findData(milliseconds);
    if (index >= 0 && statisticsInterval_->currentIndex() != index) {
        statisticsInterval_->setCurrentIndex(index);
    }
}
