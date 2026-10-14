#include "ui/replica_row.h"

#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>

void ReplicaRow::initializeVisibilityControls()
{
    visibilityCheckBox_ = new QCheckBox(QStringLiteral("显示小窗"));
    visibilityCheckBox_->setObjectName(QStringLiteral("replicaVisibility"));
    visibilityCheckBox_->setChecked(true);
    visibilityCheckBox_->setToolTip(QStringLiteral("关闭后隐藏小窗，但不删除小窗配置"));
    connect(visibilityCheckBox_, &QCheckBox::toggled, this, [this](bool visible) {
        emit replicaVisibilityChanged(visible);
        colorLogicSlotB_->setEnabled(visible);
        colorLogicSlotColorB_->setEnabled(visible);
        const int slotTwoIndex = activeSlotCombo_->findData(2);
        activeSlotCombo_->setItemData(slotTwoIndex, visible, Qt::UserRole - 1);

    });

    sourceNameVisibilityCheckBox_ = new QCheckBox(QStringLiteral("显示角色名(不显示画面)"));
    sourceNameVisibilityCheckBox_->setObjectName(QStringLiteral("sourceNameVisibility"));
    sourceNameVisibilityCheckBox_->setChecked(false);
    sourceNameVisibilityCheckBox_->setToolTip(QStringLiteral("隐藏 DWM 画面，在小窗中央显示来源角色名"));
    connect(
        sourceNameVisibilityCheckBox_,
        &QCheckBox::toggled,
        this,
        &ReplicaRow::sourceNameVisibilityChanged);
}

bool ReplicaRow::sourceNameVisible() const noexcept
{
    return sourceNameVisibilityCheckBox_->isChecked();
}

bool ReplicaRow::visibilityChecked() const noexcept
{
    return visibilityCheckBox_->isChecked();
}

void ReplicaRow::setSourceNameVisible(bool visible)
{
    if (sourceNameVisibilityCheckBox_->isChecked() != visible) {
        sourceNameVisibilityCheckBox_->setChecked(visible);
    }
}

void ReplicaRow::setVisibilityChecked(bool visible)
{
    visibilityCheckBox_->setChecked(visible);
}
