#include "ui/replica_row.h"

ReplicaRow::ReplicaRow(ReplicaId replicaId, QWidget* parent)
    : QFrame(parent)
    , replicaId_(replicaId)
{
    setObjectName(QStringLiteral("replicaRow"));
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setAcceptDrops(true);

    initializeDragHandle();
    initializeSourceSelection();
    initializeRegionEditor();
    initializeGeometryEditor();
    initializeRegionPresetControls();
    initializeStatisticsControls();
    initializeVisibilityControls();
    initializeColorLogicControls();
    buildContentLayout();
    initializeActionControls();
    buildRowLayout();
}

ReplicaId ReplicaRow::replicaId() const noexcept
{
    return replicaId_;
}
