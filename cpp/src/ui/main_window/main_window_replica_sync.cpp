#include "ui/main_window.h"

#include <algorithm>
#include <utility>
#include <QAbstractItemView>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include "platform/window_catalog.h"
#include "ui/replica_window.h"

void MainWindow::showSyncReplicasDialog(ReplicaId id)
{
    const ReplicaWindow* selectedReplica = windows_.value(id);
    if (selectedReplica == nullptr || !selectedReplica->source().has_value()) {
        return;
    }

    const std::uintptr_t sourceHwnd = selectedReplica->source()->hwnd;
    const QString sourceTitle = selectedReplica->source()->title;
    QList<WindowInfo> availableWindows;
    try {
        availableWindows = windowCatalog_->windows();
    } catch (const NativeException& error) {
        QMessageBox::warning(
            this,
            QStringLiteral("同步到"),
            QStringLiteral("无法获取窗口列表：%1").arg(QString::fromUtf8(error.what())));
        return;
    }

    QList<WindowInfo> targetWindows;
    for (const auto& candidate : availableWindows) {
        const bool alreadyAdded = std::any_of(
            targetWindows.cbegin(), targetWindows.cend(),
            [&candidate](const WindowInfo& existing) {
                return existing.hwnd == candidate.hwnd;
            });
        if (candidate.hwnd != sourceHwnd && !alreadyAdded) {
            targetWindows.append(candidate);
        }
    }
    if (targetWindows.isEmpty()) {
        QMessageBox::information(
            this,
            QStringLiteral("同步到"),
            QStringLiteral("没有可同步到的其他窗口。"));
        return;
    }

    const auto countReplicasForSource = [this](std::uintptr_t hwnd) {
        int count = 0;
        for (const ReplicaWindow* replica : windows_) {
            if (replica != nullptr && replica->source().has_value()
                && replica->source()->hwnd == hwnd) {
                ++count;
            }
        }
        return count;
    };
    const int replicaCount = countReplicasForSource(sourceHwnd);
    if (replicaCount == 0) {
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("同步小窗来源"));
    dialog.setMinimumSize(430, 320);
    auto* layout = new QVBoxLayout(&dialog);
    auto* description = new QLabel(
        QStringLiteral("选择目标窗口后，会保留源窗口“%1”的 %2 个小窗，关闭目标下已有的小窗，再按源配置生成副本。")
            .arg(sourceTitle, QString::number(replicaCount)),
        &dialog);
    description->setWordWrap(true);
    layout->addWidget(description);

    auto* windowList = new QListWidget(&dialog);
    windowList->setObjectName(QStringLiteral("syncTargetWindowList"));
    windowList->setSelectionMode(QAbstractItemView::SingleSelection);
    QHash<QString, int> titleCounts;
    for (const auto& candidate : targetWindows) {
        ++titleCounts[candidate.title.trimmed()];
    }
    for (const auto& candidate : targetWindows) {
        QString displayTitle = candidate.title.trimmed();
        if (displayTitle.isEmpty()) {
            displayTitle = QStringLiteral("未命名窗口");
        }
        if (titleCounts.value(candidate.title.trimmed()) > 1) {
            displayTitle += QStringLiteral(" [HWND 0x%1]")
                .arg(static_cast<qulonglong>(candidate.hwnd), 0, 16)
                .toUpper();
        }
        const int existingCount = countReplicasForSource(candidate.hwnd);
        displayTitle += QStringLiteral("（当前 %1 个小窗）").arg(existingCount);
        auto* item = new QListWidgetItem(displayTitle, windowList);
        item->setData(Qt::UserRole, QVariant::fromValue(candidate));
        item->setToolTip(QStringLiteral("HWND 0x%1")
            .arg(static_cast<qulonglong>(candidate.hwnd), 0, 16)
            .toUpper());
    }
    auto* targetSummary = new QLabel(&dialog);
    targetSummary->setWordWrap(true);
    layout->addWidget(targetSummary);
    windowList->setCurrentRow(0);
    layout->addWidget(windowList, 1);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("同步到"));
    buttons->button(QDialogButtonBox::Ok)->setEnabled(windowList->currentItem() != nullptr);
    connect(windowList, &QListWidget::currentRowChanged, this,
        [buttons, windowList, targetSummary, sourceTitle, replicaCount,
            countReplicasForSource](int row) {
            buttons->button(QDialogButtonBox::Ok)->setEnabled(row >= 0);
            if (row < 0 || windowList->currentItem() == nullptr) {
                targetSummary->clear();
                return;
            }
            const auto target = windowList->currentItem()->data(Qt::UserRole)
                .value<WindowInfo>();
            const int existingCount = countReplicasForSource(target.hwnd);
            targetSummary->setText(
                QStringLiteral("将保留源窗口“%1”的 %2 个小窗；先关闭目标窗口下已有的 %3 个小窗，再生成 %2 个副本。")
                    .arg(sourceTitle)
                    .arg(replicaCount)
                    .arg(existingCount));
        });
    if (windowList->currentRow() >= 0) {
        const auto initialTarget = windowList->currentItem()->data(Qt::UserRole)
            .value<WindowInfo>();
        targetSummary->setText(
            QStringLiteral("将保留源窗口“%1”的 %2 个小窗；先关闭目标窗口下已有的 %3 个小窗，再生成 %2 个副本。")
                .arg(sourceTitle)
                .arg(replicaCount)
                .arg(countReplicasForSource(initialTarget.hwnd)));
    }
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(windowList, &QListWidget::itemDoubleClicked, &dialog,
        [&dialog](QListWidgetItem*) { dialog.accept(); });
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted || windowList->currentItem() == nullptr) {
        return;
    }
    const WindowInfo target = windowList->currentItem()->data(Qt::UserRole).value<WindowInfo>();
    if (target.hwnd == 0 || target.hwnd == sourceHwnd) {
        return;
    }

    try {
        availableWindows = windowCatalog_->windows();
    } catch (const NativeException& error) {
        QMessageBox::warning(
            this,
            QStringLiteral("同步到"),
            QStringLiteral("无法获取窗口列表：%1").arg(QString::fromUtf8(error.what())));
        return;
    }
    const auto currentTarget = std::find_if(
        availableWindows.cbegin(), availableWindows.cend(),
        [&target](const WindowInfo& candidate) { return candidate.hwnd == target.hwnd; });
    if (currentTarget == availableWindows.cend()) {
        QMessageBox::information(
            this,
            QStringLiteral("同步到"),
            QStringLiteral("目标窗口已关闭，请重新选择。"));
        return;
    }

    QList<ReplicaConfig> replicasToCreate;
    QList<ReplicaWindow*> targetReplicas;
    for (const ReplicaId replicaId : replicaController_.ids()) {
        ReplicaWindow* replica = windows_.value(replicaId);
        if (replica == nullptr || !replica->source().has_value()) {
            continue;
        }
        if (replica->source()->hwnd == sourceHwnd) {
            auto config = replicaConfigFor(replicaId, replicaController_.desiredConfig(replicaId));
            config.sourceTitle = currentTarget->title;
            config.capturedRegion = replica->capturedRegion().value_or(QRect{});
            replicasToCreate.append(std::move(config));
        } else if (replica->source()->hwnd == target.hwnd) {
            targetReplicas.append(replica);
        }
    }
    if (replicasToCreate.isEmpty()) {
        return;
    }

    for (ReplicaWindow* replica : targetReplicas) {
        replica->close();
    }
    for (const auto& config : replicasToCreate) {
        restoreReplica(config, availableWindows, &*currentTarget, true);
    }
    refreshReplicaFilters();
    refreshReplicaGroups();
    schedulePersistReplicas();
}
