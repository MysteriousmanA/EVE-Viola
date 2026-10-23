#include "ui/replica_row.h"

#include <QComboBox>
#include <QLabel>
#include <QGridLayout>
#include <QPushButton>
#include "config/config_validation.h"
#include "features/replica/pixel_statistics.h"
#include "ui/replica_row/scroll_passthrough_controls.h"
#include "ui/pixel_color_editor.h"

using namespace ReplicaRowUi;

void ReplicaRow::initializeStatisticsControls()
{
    pixelStatisticsHost_ = new QWidget(this);
    new QGridLayout(pixelStatisticsHost_);
    statisticsColorsButton_ = new QPushButton(QStringLiteral("统计颜色…"), this);
    statisticsColorsButton_->setObjectName(QStringLiteral("statisticsColors"));
    setStatisticsColors(statisticsColors_);
    connect(statisticsColorsButton_, &QPushButton::clicked, this, [this] {
        const auto colors = promptStatisticsColors(this, statisticsColors_);
        if (colors && *colors != statisticsColors_) {
            setStatisticsColors(*colors);
            emit statisticsColorsChanged(statisticsColors_);
        }
    });

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
    setColorStatistics(PixelStatistics{0, redCount, whiteCount});
}

void ReplicaRow::setStatisticsColors(const QList<PixelColorTarget>& colors)
{
    statisticsColors_ = normalizedPixelColorTargets(colors);
    qDeleteAll(pixelStatisticsLabels_);
    pixelStatisticsLabels_.clear();
    auto* layout = static_cast<QGridLayout*>(pixelStatisticsHost_->layout());
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setHorizontalSpacing(12);
    for (qsizetype index = 0; index < statisticsColors_.size(); ++index) {
        const auto& target = statisticsColors_[index];
        auto* label = new QLabel(pixelStatisticsHost_);
        if (target.color == QStringLiteral("红色")) { label->setObjectName(QStringLiteral("redPixelPercentage")); }
        else if (target.color == QStringLiteral("白色")) { label->setObjectName(QStringLiteral("whitePixelPercentage")); }
        else if (target.color == QStringLiteral("蓝色")) { label->setObjectName(QStringLiteral("bluePixelPercentage")); }
        else { label->setObjectName(QStringLiteral("customPixelCount%1").arg(index)); }
        label->setToolTip(target.tolerance < 0 ? QStringLiteral("使用预设颜色判定范围") :
                          QStringLiteral("%1；RGB 每通道容差：%2").arg(target.color).arg(target.tolerance));
        layout->addWidget(label, static_cast<int>(index / 3), static_cast<int>(index % 3));
        pixelStatisticsLabels_.append(label);
    }
    clearPixelStatistics();
}

void ReplicaRow::setColorStatistics(const PixelStatistics& statistics)
{
    for (qsizetype index = 0; index < statisticsColors_.size(); ++index) {
        const auto& target = statisticsColors_[index];
        const auto count = statistics.count(target);
        pixelStatisticsLabels_[index]->setText(target.color + QStringLiteral("像素：") +
                                               (count < 0 ? QStringLiteral("--") : QString::number(count)));
    }
}

void ReplicaRow::clearPixelStatistics()
{
    for (qsizetype index = 0; index < statisticsColors_.size(); ++index) {
        pixelStatisticsLabels_[index]->setText(statisticsColors_[index].color + QStringLiteral("像素：--"));
    }
}

void ReplicaRow::setStatisticsInterval(int milliseconds)
{
    const int index = statisticsInterval_->findData(milliseconds);
    if (index >= 0 && statisticsInterval_->currentIndex() != index) {
        statisticsInterval_->setCurrentIndex(index);
    }
}
