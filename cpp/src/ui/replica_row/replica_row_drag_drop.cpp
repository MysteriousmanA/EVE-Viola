#include "ui/replica_row.h"

#include <QApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QLabel>
#include <QMimeData>
#include <QMouseEvent>

namespace {

constexpr auto kReplicaRowMimeType = "application/x-eve-viola-replica-row";

} // namespace

void ReplicaRow::initializeDragHandle()
{
    badge_ = new QLabel(QStringLiteral("#%1").arg(replicaId_, 2, 10, QLatin1Char('0')));
    badge_->setObjectName(QStringLiteral("replicaBadge"));
    badge_->setCursor(Qt::OpenHandCursor);
    badge_->setToolTip(QStringLiteral("拖拽调整小窗顺序"));
}

void ReplicaRow::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        dragStartPosition_ = event->position().toPoint();
    }
    QFrame::mousePressEvent(event);
}

void ReplicaRow::mouseMoveEvent(QMouseEvent* event)
{
    if (!event->buttons().testFlag(Qt::LeftButton)
        || (event->position().toPoint() - dragStartPosition_).manhattanLength()
            < QApplication::startDragDistance()) {
        QFrame::mouseMoveEvent(event);
        return;
    }
    auto* drag = new QDrag(this);
    auto* mimeData = new QMimeData;
    mimeData->setData(
        kReplicaRowMimeType, QByteArray::number(static_cast<qulonglong>(replicaId_)));
    drag->setMimeData(mimeData);
    drag->exec(Qt::MoveAction);
}

void ReplicaRow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasFormat(kReplicaRowMimeType)) {
        event->acceptProposedAction();
        return;
    }
    QFrame::dragEnterEvent(event);
}

void ReplicaRow::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData()->hasFormat(kReplicaRowMimeType)) {
        event->acceptProposedAction();
        return;
    }
    QFrame::dragMoveEvent(event);
}

void ReplicaRow::dropEvent(QDropEvent* event)
{
    if (!event->mimeData()->hasFormat(kReplicaRowMimeType)) {
        QFrame::dropEvent(event);
        return;
    }
    bool valid = false;
    const auto sourceId = static_cast<ReplicaId>(
        event->mimeData()->data(kReplicaRowMimeType).toULongLong(&valid));
    if (valid && sourceId != replicaId_) {
        emit reorderRequested(
            sourceId, replicaId_, event->position().y() >= height() / 2.0);
        event->acceptProposedAction();
    }
}
