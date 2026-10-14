#include "ui/replica_row.h"

#include <QComboBox>
#include <QHash>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVariant>
#include "ui/replica_row/scroll_passthrough_controls.h"

using namespace ReplicaRowUi;

namespace {

QString sourceTitleForDisplay(const QString& title)
{
    const QString trimmed = title.trimmed();
    constexpr auto titleSeparator = " - ";
    const QLatin1StringView separator(titleSeparator);
    const qsizetype separatorIndex = trimmed.lastIndexOf(separator);
    if (separatorIndex >= 0) {
        return trimmed.sliced(separatorIndex + separator.size()).trimmed();
    }
    return trimmed;
}

int findSourceTitleIndex(const QComboBox* combo, const QString& sourceTitle)
{
    const QString displayTitle = sourceTitleForDisplay(sourceTitle);
    int match = -1;
    for (int index = 1; index < combo->count(); ++index) {
        const QVariant data = combo->itemData(index);
        if (data.isValid() && data.value<WindowInfo>().title.trimmed() == sourceTitle) {
            if (match >= 0) {
                return -1;
            }
            match = index;
        }
    }
    if (match >= 0) {
        return match;
    }
    for (int index = 1; index < combo->count(); ++index) {
        if (!combo->itemData(index).isValid()
            && combo->itemText(index) == displayTitle) {
            return index;
        }
    }
    return -1;
}

} // namespace

void ReplicaRow::initializeSourceSelection()
{
    sourceCombo_ = new ScrollPassthroughCombo;
    sourceCombo_->setObjectName(QStringLiteral("sourceCombo"));
    sourceCombo_->setMinimumWidth(150);
    sourceCombo_->addItem(QStringLiteral("尚未选择窗口"));
    sourceCombo_->setToolTip(QStringLiteral("选择该小窗绑定的 EVE 窗口"));
    connect(sourceCombo_, &QComboBox::activated, this, [this](int index) {
        const auto info = sourceCombo_->itemData(index).value<WindowInfo>();
        if (info.hwnd != 0) {
            emit sourceSelectionRequested(info);
        }
    });
    static_cast<ScrollPassthroughCombo*>(sourceCombo_)->onShowPopup = [this] {
        emit sourceOptionsRequested();
    };
}

QComboBox* ReplicaRow::sourceCombo() const noexcept
{
    return sourceCombo_;
}

void ReplicaRow::setSourceTitle(const QString& title)
{
    sourceTitle_ = title.trimmed();
    syncToButton_->setEnabled(!sourceTitle_.isEmpty());
    const auto currentInfo = sourceCombo_->currentData().value<WindowInfo>();
    if (currentInfo.hwnd != 0 && currentInfo.title.trimmed() == sourceTitle_) {
        return;
    }
    const QSignalBlocker blocker(sourceCombo_);
    if (sourceTitle_.isEmpty()) {
        sourceCombo_->setCurrentIndex(0);
        return;
    }
    const int index = findSourceTitleIndex(sourceCombo_, sourceTitle_);
    if (index >= 0) {
        sourceCombo_->setCurrentIndex(index);
    } else {
        sourceCombo_->addItem(sourceTitleForDisplay(sourceTitle_));
        sourceCombo_->setCurrentIndex(sourceCombo_->count() - 1);
    }
}

void ReplicaRow::setSourceWindow(const WindowInfo& window)
{
    sourceTitle_ = window.title.trimmed();
    syncToButton_->setEnabled(window.hwnd != 0);
    const QSignalBlocker blocker(sourceCombo_);
    for (int index = 1; index < sourceCombo_->count(); ++index) {
        const QVariant data = sourceCombo_->itemData(index);
        if (data.isValid() && data.value<WindowInfo>().hwnd == window.hwnd) {
            sourceCombo_->setCurrentIndex(index);
            return;
        }
    }

    QString displayTitle = sourceTitleForDisplay(window.title);
    for (int index = 1; index < sourceCombo_->count(); ++index) {
        if (sourceCombo_->itemText(index) == displayTitle) {
            displayTitle += QStringLiteral(" [HWND 0x%1]")
                .arg(static_cast<qulonglong>(window.hwnd), 0, 16)
                .toUpper();
            break;
        }
    }
    sourceCombo_->addItem(displayTitle, QVariant::fromValue(window));
    sourceCombo_->setCurrentIndex(sourceCombo_->count() - 1);
}

void ReplicaRow::setSourceOptions(const QList<WindowInfo>& windows)
{
    const auto currentInfo = sourceCombo_->currentIndex() >= 0
        ? sourceCombo_->currentData().value<WindowInfo>()
        : WindowInfo{};
    const std::uintptr_t currentHwnd = currentInfo.hwnd;
    const QSignalBlocker blocker(sourceCombo_);
    sourceCombo_->clear();
    sourceCombo_->addItem(QStringLiteral("尚未选择窗口"));
    QHash<QString, int> displayTitleCounts;
    for (const auto& window : windows) {
        ++displayTitleCounts[sourceTitleForDisplay(window.title)];
    }
    for (const auto& window : windows) {
        QString displayTitle = sourceTitleForDisplay(window.title);
        if (displayTitleCounts.value(displayTitle) > 1) {
            displayTitle += QStringLiteral(" [HWND 0x%1]")
                .arg(static_cast<qulonglong>(window.hwnd), 0, 16)
                .toUpper();
        }
        sourceCombo_->addItem(displayTitle, QVariant::fromValue(window));
    }
    if (currentHwnd != 0) {
        for (int index = 1; index < sourceCombo_->count(); ++index) {
            if (sourceCombo_->itemData(index).value<WindowInfo>().hwnd == currentHwnd) {
                sourceCombo_->setCurrentIndex(index);
                return;
            }
        }
    }
    if (!sourceTitle_.isEmpty()) {
        int index = findSourceTitleIndex(sourceCombo_, sourceTitle_);
        if (index < 0) {
            sourceCombo_->addItem(sourceTitleForDisplay(sourceTitle_));
            index = sourceCombo_->count() - 1;
        }
        sourceCombo_->setCurrentIndex(index);
        return;
    }
    sourceCombo_->setCurrentIndex(0);
}
