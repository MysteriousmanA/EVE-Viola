#pragma once
#include <QWidget>
#include <QHash>
#include "features/replica/replica_manager.h"
class ReplicaRow;
class ReplicaWindow;
class QLabel;
class QComboBox;
class QCheckBox;
class QFrame;
class QScrollArea;
class QVBoxLayout;
class ReplicaListPage final : public QWidget
{
    Q_OBJECT
  public:
    explicit ReplicaListPage(QWidget *parent = nullptr);
    void setEntries(const QList<ReplicaId> &order, const QHash<ReplicaId, ReplicaRow *> &rows,
                    const QHash<ReplicaId, ReplicaWindow *> &windows);
    void addRow(ReplicaRow *row);
    void ensureRowVisible(ReplicaRow *row);
    void setSizeLocked(bool locked);
    void refreshReplicaGroups();
    void refreshReplicaFilters();
    void refreshReplicaGroupBars();
    void refreshReplicaListSize();
    void synchronizeReplicaGroupCollapse(ReplicaId id, bool collapsed);
  signals:
    void sizeLockChanged(bool locked);
    void persistRequested();

  private:
    QList<QList<ReplicaRow *>> replicaGroups() const;
    QList<ReplicaRow *> orderedRows() const;
    QList<ReplicaId> orderedIds_;
    QHash<ReplicaId, ReplicaRow *> rows_;
    QHash<ReplicaId, ReplicaWindow *> windows_;
    QVBoxLayout *listLayout_{};
    QScrollArea *listScroll_{};
    QLabel *emptyState_{};
    QComboBox *sourceFilterCombo_{};
    QComboBox *regionFilterCombo_{};
    QCheckBox *lockReplicaSizeCheckBox_{};
    QList<QFrame *> replicaGroupBars_;
    bool synchronizingReplicaGroupCollapse_{};
};
