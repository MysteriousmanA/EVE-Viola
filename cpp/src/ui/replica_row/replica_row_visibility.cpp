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

    });
}

bool ReplicaRow::visibilityChecked() const noexcept
{
    return visibilityCheckBox_->isChecked();
}

void ReplicaRow::setVisibilityChecked(bool visible)
{
    visibilityCheckBox_->setChecked(visible);
}
