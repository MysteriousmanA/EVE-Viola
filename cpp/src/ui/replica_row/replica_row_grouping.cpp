#include "ui/replica_row.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include "ui/replica_row/replica_row_constants.h"

using namespace ReplicaRowUi;

namespace {

QString formatEstimatedPixelRate(double pixelsPerSecond)
{
    if (pixelsPerSecond < 0.0) {
        return QStringLiteral("待获取尺寸");
    }
    if (pixelsPerSecond >= 1'000'000.0) {
        return QStringLiteral("%1 MP/s").arg(pixelsPerSecond / 1'000'000.0, 0, 'f', 2);
    }
    if (pixelsPerSecond >= 1000.0) {
        return QStringLiteral("%1 kP/s").arg(pixelsPerSecond / 1000.0, 0, 'f', 1);
    }
    return QStringLiteral("%1 P/s").arg(pixelsPerSecond, 0, 'f', 1);
}

} // namespace

void ReplicaRow::initializeActionControls()
{
    closeButton_ = new QPushButton(QStringLiteral("关闭"));
    closeButton_->setObjectName(QStringLiteral("closeButton"));
    connect(closeButton_, &QPushButton::clicked, this, &ReplicaRow::closeRequested);

    copyButton_ = new QPushButton(QStringLiteral("复制"));
    copyButton_->setObjectName(QStringLiteral("copyReplica"));
    copyButton_->setToolTip(QStringLiteral("复制该小窗及其区域、图色逻辑配置"));
    connect(copyButton_, &QPushButton::clicked, this, &ReplicaRow::copyRequested);

    collapseButton_ = new QPushButton(QStringLiteral("折叠"));
    collapseButton_->setObjectName(QStringLiteral("collapseReplica"));
    collapseButton_->setCheckable(true);
    collapseButton_->setToolTip(QStringLiteral("折叠/展开同一来源的连续小窗组"));
    connect(collapseButton_, &QPushButton::toggled, this, &ReplicaRow::setCollapsed);

    syncToButton_ = new QPushButton(QStringLiteral("同步到"));
    syncToButton_->setObjectName(QStringLiteral("syncReplicaGroupTo"));
    syncToButton_->setToolTip(QStringLiteral("将基于此源窗口的所有小窗切换到其他源窗口"));
    syncToButton_->hide();
    connect(syncToButton_, &QPushButton::clicked, this, &ReplicaRow::syncToRequested);

    performanceEstimateLabel_ = new QLabel;
    performanceEstimateLabel_->setObjectName(QStringLiteral("replicaPerformanceEstimate"));
    performanceEstimateLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    performanceEstimateLabel_->setToolTip(
        QStringLiteral("捕获：源窗口整帧像素数 × 每秒捕获次数；扫描：统计区域像素数 × 每秒扫描次数。\n"
                       "按当前卡片的统计间隔估算，同来源同一时刻合并捕获，相同区域合并扫描。\n"
                       "P/s 为像素/秒，kP/s 为千像素/秒，MP/s 为百万像素/秒；不是 CPU 占用率。"));
    performanceEstimateLabel_->hide();
    syncActionRow_ = new QWidget;
    auto* syncActionLayout = new QHBoxLayout(syncActionRow_);
    syncActionLayout->setContentsMargins(0, 0, 0, 0);
    syncActionLayout->setSpacing(8);
    syncActionLayout->addWidget(performanceEstimateLabel_, 1);
    syncActionLayout->addWidget(syncToButton_);
    syncActionRow_->hide();
}

bool ReplicaRow::collapsed() const noexcept
{
    return collapsed_;
}

void ReplicaRow::setCollapsed(bool collapsed)
{
    if (collapsed_ == collapsed) {
        return;
    }
    collapsed_ = collapsed;
    collapseButton_->setText(collapsed ? QStringLiteral("展开") : QStringLiteral("折叠"));
    collapseButton_->setChecked(collapsed);
    badge_->setVisible(!collapsed);
    contentHost_->setVisible(!collapsed);
    closeButton_->setVisible(!collapsed);
    copyButton_->setVisible(!collapsed);
    syncActionRow_->setVisible(collapsed);
    syncToButton_->setVisible(collapsed);
    performanceEstimateLabel_->hide();
    emit collapseChanged(collapsed);
    updateGeometry();
}

void ReplicaRow::setGroupPresentation(
    int memberIndex,
    int memberCount,
    bool representative,
    double estimatedCapturePixelsPerSecond,
    double estimatedScanPixelsPerSecond)
{
    const int safeCount = qMax(1, memberCount);
    sourceDetails_->setVisible(representative);
    groupCountLabel_->setText(QStringLiteral("%1 个小窗").arg(safeCount));
    groupCountLabel_->setVisible(representative && safeCount > 1);
    sourceCell_->setProperty("grouped", safeCount > 1);
    sourceCell_->setProperty("groupPosition",
        memberIndex == 0 ? QStringLiteral("first")
        : memberIndex == safeCount - 1 ? QStringLiteral("last")
                                       : QStringLiteral("middle"));

    const bool collapsedGroupSummary = collapsed_ && safeCount > 1 && representative;
    syncActionRow_->setVisible(collapsed_ && representative);
    syncToButton_->setVisible(collapsed_ && representative);
    performanceEstimateLabel_->setVisible(collapsed_ && representative);
    performanceEstimateLabel_->setText(
        estimatedCapturePixelsPerSecond == 0.0 && estimatedScanPixelsPerSecond == 0.0
            ? QStringLiteral("性能预估\n统计未运行")
            : QStringLiteral("性能预估\n捕获 %1 · 扫描 %2")
                  .arg(formatEstimatedPixelRate(estimatedCapturePixelsPerSecond),
                       formatEstimatedPixelRate(estimatedScanPixelsPerSecond)));
    if (collapsedGroupSummary) {
        sourceCell_->setMinimumWidth(0);
        sourceCell_->setMaximumWidth(QWIDGETSIZE_MAX);
        sourceCell_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        sourceDetails_->setFixedWidth(kCollapsedGroupSummaryWidth);
    } else {
        sourceCell_->setFixedWidth(kSourceCellWidth);
        sourceCell_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
        sourceDetails_->setMinimumWidth(0);
        sourceDetails_->setMaximumWidth(QWIDGETSIZE_MAX);
    }

    if (collapsed_ && representative) {
        sourceCell_->show();
        sourceDetails_->show();
        sourceCombo_->show();
        collapseButton_->show();
    }
    sourceCell_->style()->unpolish(sourceCell_);
    sourceCell_->style()->polish(sourceCell_);
    updateGeometry();
}
