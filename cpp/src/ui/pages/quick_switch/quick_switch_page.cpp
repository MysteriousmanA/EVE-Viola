#include "ui/pages/quick_switch/quick_switch_page.h"
#include "features/quick_switch/quick_switch_keys.h"
#include "features/quick_switch/radial_account_order.h"

#include <algorithm>

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSet>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include "ui/common/page_widgets.h"
using namespace PageWidgets;
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace
{
const QStringList &quickSwitchKeyOptions()
{
    static const QStringList options{
        QStringLiteral("PageUp"),
        QStringLiteral("PageDown"),
        QStringLiteral("Mouse4"),
        QStringLiteral("Mouse5"),
    };
    return options;
}

QString quickSwitchKeyLabel(const QString &key)
{
    const auto input = quickSwitchKeyboardInput(key);
    if (!input.has_value()) {
        return key;
    }

#ifdef Q_OS_WIN
    const UINT scanCode = MapVirtualKeyW(static_cast<UINT>(input->virtualKey), MAPVK_VK_TO_VSC);
    if (scanCode != 0) {
        LONG keyNameParameter = static_cast<LONG>((scanCode & 0xFF) << 16);
        if (input->extended) {
            keyNameParameter |= 1L << 24;
        }
        std::array<wchar_t, 128> buffer{};
        const int length =
            GetKeyNameTextW(keyNameParameter, buffer.data(), static_cast<int>(buffer.size()));
        if (length > 0) {
            return QString::fromWCharArray(buffer.data(), length);
        }
    }
#endif

    return QStringLiteral("VK 0x%1").arg(input->virtualKey, 2, 16, QLatin1Char('0')).toUpper();
}

} // namespace
QuickSwitchPage::QuickSwitchPage(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    layout->addWidget(makeHeading(QStringLiteral("快捷切屏")));
    layout->addWidget(makeDescription(
        QStringLiteral("EVE 客户端处于前台时，使用所选按键或鼠标侧键切换客户端。") +
        QStringLiteral(
            "点击“监听以选择”后按下任意单个键盘键或鼠标侧键；再次点击或切离页面可取消。") +
        QStringLiteral("长按圆盘按键 300ms "
                       "呼出账号圆盘，移动鼠标高亮后松开切换；短按会在松开时回放原按键。")));

    auto *previousGroup = new QGroupBox(QStringLiteral("上一个客户端"), this);
    auto *previousLayout = new QGridLayout(previousGroup);
    auto *nextGroup = new QGroupBox(QStringLiteral("下一个客户端"), this);
    auto *nextLayout = new QGridLayout(nextGroup);

    auto *previousCustomKeys = new QWidget(previousGroup);
    previousCustomKeysLayout_ = new QHBoxLayout(previousCustomKeys);
    previousCustomKeysLayout_->setContentsMargins(0, 0, 0, 0);
    previousCustomKeysLayout_->setSpacing(4);
    previousLayout->addWidget(previousCustomKeys, 3, 0, 1, 2);

    auto *nextCustomKeys = new QWidget(nextGroup);
    nextCustomKeysLayout_ = new QHBoxLayout(nextCustomKeys);
    nextCustomKeysLayout_->setContentsMargins(0, 0, 0, 0);
    nextCustomKeysLayout_->setSpacing(4);
    nextLayout->addWidget(nextCustomKeys, 3, 0, 1, 2);

    const QStringList &options = quickSwitchKeyOptions();
    for (int index = 0; index < options.size(); ++index) {
        const QString &key = options[index];
        auto *previousCheck = new QCheckBox(key, previousGroup);
        previousCheck->setObjectName(QStringLiteral("previousClientKey%1").arg(key));
        previousKeyChecks_[static_cast<std::size_t>(index)] = previousCheck;
        previousLayout->addWidget(previousCheck, index / 2, index % 2);
        connect(previousCheck, &QCheckBox::toggled, this,
                [this, index](bool checked) { bindingChanged(true, index, checked); });

        auto *nextCheck = new QCheckBox(key, nextGroup);
        nextCheck->setObjectName(QStringLiteral("nextClientKey%1").arg(key));
        nextKeyChecks_[static_cast<std::size_t>(index)] = nextCheck;
        nextLayout->addWidget(nextCheck, index / 2, index % 2);
        connect(nextCheck, &QCheckBox::toggled, this,
                [this, index](bool checked) { bindingChanged(false, index, checked); });
    }

    previousListenButton_ = new QPushButton(QStringLiteral("监听以选择"), previousGroup);
    previousListenButton_->setObjectName(QStringLiteral("listenPreviousClientKey"));
    previousListenButton_->setCheckable(true);
    previousListenButton_->setToolTip(
        QStringLiteral("监听任意单个键盘键或鼠标侧键；再次点击此按钮可取消"));
    previousLayout->addWidget(previousListenButton_, 2, 0, 1, 2);
    connect(previousListenButton_, &QPushButton::clicked, this, [this](bool checked) {
        if (checked) {
            beginListening(true);
        } else if (activeListenButton_ == previousListenButton_) {
            finishListening();
        }
    });

    nextListenButton_ = new QPushButton(QStringLiteral("监听以选择"), nextGroup);
    nextListenButton_->setObjectName(QStringLiteral("listenNextClientKey"));
    nextListenButton_->setCheckable(true);
    nextListenButton_->setToolTip(
        QStringLiteral("监听任意单个键盘键或鼠标侧键；再次点击此按钮可取消"));
    nextLayout->addWidget(nextListenButton_, 2, 0, 1, 2);
    connect(nextListenButton_, &QPushButton::clicked, this, [this](bool checked) {
        if (checked) {
            beginListening(false);
        } else if (activeListenButton_ == nextListenButton_) {
            finishListening();
        }
    });

    layout->addWidget(previousGroup);
    layout->addWidget(nextGroup);

    auto *radialGroup = new QGroupBox(QStringLiteral("账号圆盘切换"), this);
    auto *radialLayout = new QHBoxLayout(radialGroup);
    radialKeyLabel_ = new QLabel(radialGroup);
    radialKeyLabel_->setObjectName(QStringLiteral("radialSwitchKeyValue"));
    radialKeyLabel_->setMinimumWidth(90);
    radialLayout->addWidget(new QLabel(QStringLiteral("长按按键："), radialGroup));
    radialLayout->addWidget(radialKeyLabel_);
    radialLayout->addStretch(1);
    radialListenButton_ = new QPushButton(QStringLiteral("监听以选择"), radialGroup);
    radialListenButton_->setObjectName(QStringLiteral("listenRadialSwitchKey"));
    radialListenButton_->setCheckable(true);
    radialListenButton_->setToolTip(
        QStringLiteral("监听单个键盘键；短按会在松开时回放原按键，按住 300ms 呼出账号圆盘"));
    radialLayout->addWidget(radialListenButton_);
    connect(radialListenButton_, &QPushButton::clicked, this, [this](bool checked) {
        if (checked) {
            beginListeningRadialKey();
        } else if (activeListenButton_ == radialListenButton_) {
            finishListening();
        }
    });
    layout->addWidget(radialGroup);

    auto *accountsGroup = new QGroupBox(QStringLiteral("账号昵称与圆盘位置"), this);
    auto *accountsLayout = new QVBoxLayout(accountsGroup);
    auto *accountsToolbar = new QHBoxLayout;
    accountsToolbar->addWidget(makeDescription(
        QStringLiteral("账号按 EVE "
                       "窗口标题记忆；先启动需要配置的账号并刷新列表。位置按在线账号排序，圆盘按在"
                       "线数量均分；预览中移动鼠标查看昵称，点击扇区可调整位置。")));
    accountsToolbar->addStretch(1);
    auto *refreshAccountsButton = new QPushButton(QStringLiteral("刷新账号"), accountsGroup);
    refreshAccountsButton->setObjectName(QStringLiteral("refreshRadialAccounts"));
    accountsToolbar->addWidget(refreshAccountsButton);
    accountsLayout->addLayout(accountsToolbar);
    connect(refreshAccountsButton, &QPushButton::clicked, this,
            [this] { emit radialAccountsRefreshRequested(); });

    radialAccountsSummary_ = new QLabel(accountsGroup);
    radialAccountsSummary_->setObjectName(QStringLiteral("radialAccountsSummary"));
    accountsLayout->addWidget(radialAccountsSummary_);
    radialAccountsTable_ = new QTableWidget(accountsGroup);
    radialAccountsTable_->setObjectName(QStringLiteral("radialAccountsTable"));
    radialAccountsTable_->setColumnCount(4);
    radialAccountsTable_->setHorizontalHeaderLabels({
        QStringLiteral("账号窗口"),
        QStringLiteral("圆盘昵称"),
        QStringLiteral("位置"),
        QStringLiteral("预览"),
    });
    radialAccountsTable_->verticalHeader()->hide();
    radialAccountsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    radialAccountsTable_->setSelectionMode(QAbstractItemView::NoSelection);
    radialAccountsTable_->horizontalHeader()->setStretchLastSection(false);
    radialAccountsTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    radialAccountsTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    radialAccountsTable_->horizontalHeader()->setSectionResizeMode(2,
                                                                   QHeaderView::ResizeToContents);
    radialAccountsTable_->horizontalHeader()->setSectionResizeMode(3,
                                                                   QHeaderView::ResizeToContents);
    radialAccountsTable_->setMinimumHeight(112);
    accountsLayout->addWidget(radialAccountsTable_);
    auto *accountsScrollArea = new QScrollArea(this);
    accountsScrollArea->setObjectName(QStringLiteral("radialAccountsScrollArea"));
    accountsScrollArea->setFrameShape(QFrame::NoFrame);
    accountsScrollArea->setWidgetResizable(true);
    accountsScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    accountsScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    accountsScrollArea->setMinimumHeight(150);
    accountsScrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    accountsScrollArea->setWidget(accountsGroup);
    layout->addWidget(accountsScrollArea, 1);

    layout->addStretch(1);
    setKeys(previousKeys_, nextKeys_);
    setRadialSwitchKey(radialSwitchKey_);
}

QuickSwitchPage::~QuickSwitchPage()
{
    finishListening();
}

void QuickSwitchPage::beginListening(bool previousDirection)
{
    if (activeListenButton_ != nullptr) {
        finishListening();
    }

    listeningPreviousDirection_ = previousDirection;
    listeningRadialKey_ = false;
    activeListenButton_ = previousDirection ? previousListenButton_ : nextListenButton_;
    activeListenButton_->setText(QStringLiteral("正在监听…（点击取消）"));
    activeListenButton_->setToolTip(
        QStringLiteral("监听任意单个键盘键或鼠标侧键；再次点击此按钮可取消"));
    if (auto *application = QApplication::instance()) {
        application->installEventFilter(this);
    }
    emit captureRequested(true);
}

void QuickSwitchPage::beginListeningRadialKey()
{
    if (activeListenButton_ != nullptr) {
        finishListening();
    }

    listeningRadialKey_ = true;
    activeListenButton_ = radialListenButton_;
    activeListenButton_->setText(QStringLiteral("正在监听…（点击取消）"));
    activeListenButton_->setToolTip(QStringLiteral("请按下一个键盘键；鼠标侧键不用于长按圆盘"));
    if (auto *application = QApplication::instance()) {
        application->installEventFilter(this);
    }
    emit captureRequested(true);
}

void QuickSwitchPage::finishListening()
{
    if (activeListenButton_ == nullptr) {
        return;
    }

    QPushButton *button = activeListenButton_;
    const bool wasListeningRadialKey = listeningRadialKey_;
    activeListenButton_ = nullptr;
    listeningRadialKey_ = false;
    button->setText(QStringLiteral("监听以选择"));
    button->setToolTip(
        wasListeningRadialKey
            ? QStringLiteral("监听单个键盘键；短按会在松开时回放原按键，按住 300ms 呼出账号圆盘")
            : QStringLiteral("监听任意单个键盘键或鼠标侧键；再次点击此按钮可取消"));
    const QSignalBlocker blocker(button);
    button->setChecked(false);
    if (auto *application = QApplication::instance()) {
        application->removeEventFilter(this);
    }
    emit captureRequested(false);
}

bool QuickSwitchPage::eventFilter(QObject *watched, QEvent *event)
{
    if (activeListenButton_ == nullptr) {
        return QWidget::eventFilter(watched, event);
    }

    if (event->type() == QEvent::ApplicationDeactivate ||
        (watched == window() && event->type() == QEvent::WindowDeactivate) ||
        (watched == this && event->type() == QEvent::Hide)) {
        finishListening();
        return QWidget::eventFilter(watched, event);
    }
    return QWidget::eventFilter(watched, event);
}

void QuickSwitchPage::captureInput(const QString &key)
{
    if (activeListenButton_ == nullptr) {
        return;
    }

    const QString normalized = normalizeQuickSwitchKey(key);
    if (normalized.isEmpty()) {
        activeListenButton_->setText(QStringLiteral("无法识别，继续监听…"));
        return;
    }

    if (listeningRadialKey_) {
        if (!quickSwitchKeyboardInput(normalized).has_value()) {
            activeListenButton_->setText(QStringLiteral("请按键盘键，继续监听…"));
            emit captureRequested(true);
            return;
        }
        setRadialSwitchKey(normalized);
        emit radialSwitchKeyChanged(radialSwitchKey_);
        finishListening();
        return;
    }

    const int optionIndex = quickSwitchKeyOptions().indexOf(normalized);
    if (optionIndex >= 0) {
        auto &checks = listeningPreviousDirection_ ? previousKeyChecks_ : nextKeyChecks_;
        QCheckBox *check = checks[static_cast<std::size_t>(optionIndex)];
        if (!check->isChecked()) {
            check->setChecked(true);
        }
    } else {
        bindKey(listeningPreviousDirection_, normalized);
    }
    finishListening();
}

void QuickSwitchPage::setKeys(const QStringList &previousKeys, const QStringList &nextKeys)
{
    const auto normalize = [](const QStringList &values) {
        QStringList result;
        for (const QString &key : values) {
            const QString normalized = normalizeQuickSwitchKey(key);
            if (!normalized.isEmpty() && !result.contains(normalized)) {
                result.append(normalized);
            }
        }
        return result;
    };
    previousKeys_ = normalize(previousKeys);
    nextKeys_ = normalize(nextKeys);
    for (const QString &key : previousKeys_) {
        nextKeys_.removeAll(key);
    }

    const QStringList &options = quickSwitchKeyOptions();
    for (int index = 0; index < options.size(); ++index) {
        const auto offset = static_cast<std::size_t>(index);
        const QSignalBlocker previousBlocker(previousKeyChecks_[offset]);
        const QSignalBlocker nextBlocker(nextKeyChecks_[offset]);
        previousKeyChecks_[offset]->setChecked(previousKeys_.contains(options[index]));
        nextKeyChecks_[offset]->setChecked(nextKeys_.contains(options[index]));
    }
    refreshCustomKeys();
}

void QuickSwitchPage::setRadialSwitchKey(const QString &key)
{
    const QString normalized = normalizeQuickSwitchKey(key);
    radialSwitchKey_ =
        quickSwitchKeyboardInput(normalized).has_value() ? normalized : kDefaultRadialSwitchKey;
    if (radialKeyLabel_ != nullptr) {
        radialKeyLabel_->setText(quickSwitchKeyLabel(radialSwitchKey_));
    }
}

void QuickSwitchPage::setRadialAccounts(const QStringList &liveWindowTitles,
                                        const QList<RadialAccountConfig> &accountConfigs)
{
    liveRadialAccountTitles_.clear();
    for (const QString &title : liveWindowTitles) {
        if (!title.trimmed().isEmpty() && !liveRadialAccountTitles_.contains(title)) {
            liveRadialAccountTitles_.append(title);
        }
    }
    radialAccountConfigs_ = accountConfigs;
    rebuildRadialAccountTable();
}

void QuickSwitchPage::setRadialAccountPosition(const QString &windowTitle, int targetIndex)
{
    QStringList visibleOrder =
        orderedRadialAccountTitles(liveRadialAccountTitles_, radialAccountConfigs_);
    const int currentIndex = visibleOrder.indexOf(windowTitle);
    if (currentIndex < 0 || visibleOrder.isEmpty()) {
        return;
    }
    const int boundedIndex = qBound(0, targetIndex, visibleOrder.size() - 1);
    if (currentIndex == boundedIndex) {
        return;
    }
    visibleOrder.removeAt(currentIndex);
    visibleOrder.insert(boundedIndex, windowTitle);
    commitRadialAccountOrder(visibleOrder);
}

void QuickSwitchPage::rebuildRadialAccountTable()
{
    const QStringList orderedTitles =
        orderedRadialAccountTitles(liveRadialAccountTitles_, radialAccountConfigs_);
    radialAccountsTable_->setRowCount(orderedTitles.size());
    radialAccountsSummary_->setText(
        orderedTitles.isEmpty()
            ? QStringLiteral("未检测到运行中的 EVE 账号。启动账号后点击“刷新账号”。")
            : QStringLiteral("当前检测到 %1 个在线账号。昵称留空时使用窗口标题。")
                  .arg(orderedTitles.size()));
    for (int row = 0; row < orderedTitles.size(); ++row) {
        const QString title = orderedTitles.at(row);
        const auto configIt = std::find_if(
            radialAccountConfigs_.cbegin(), radialAccountConfigs_.cend(),
            [&title](const RadialAccountConfig &account) { return account.windowTitle == title; });
        const QString nickname =
            configIt == radialAccountConfigs_.cend() ? QString{} : configIt->nickname;

        auto *titleLabel = new QLabel(title, radialAccountsTable_);
        titleLabel->setWordWrap(true);
        titleLabel->setToolTip(title);
        radialAccountsTable_->setCellWidget(row, 0, titleLabel);

        auto *nicknameEdit = new QLineEdit(nickname, radialAccountsTable_);
        nicknameEdit->setObjectName(QStringLiteral("radialAccountNickname%1").arg(row));
        nicknameEdit->setPlaceholderText(title);
        nicknameEdit->setMaxLength(32);
        radialAccountsTable_->setCellWidget(row, 1, nicknameEdit);
        connect(nicknameEdit, &QLineEdit::editingFinished, this, [this, title, nicknameEdit] {
            updateRadialAccountNickname(title, nicknameEdit->text());
        });

        auto *positionCombo = new QComboBox(radialAccountsTable_);
        positionCombo->setObjectName(QStringLiteral("radialAccountPosition%1").arg(row));
        for (int position = 0; position < orderedTitles.size(); ++position) {
            positionCombo->addItem(QString::number(position + 1), position);
        }
        positionCombo->setCurrentIndex(row);
        radialAccountsTable_->setCellWidget(row, 2, positionCombo);
        connect(positionCombo, &QComboBox::currentIndexChanged, this,
                [this, title, positionCombo](int) {
                    setRadialAccountPosition(title, positionCombo->currentData().toInt());
                });

        auto *previewButton = new QPushButton(QStringLiteral("预览并选位置"), radialAccountsTable_);
        previewButton->setObjectName(QStringLiteral("previewRadialAccount%1").arg(row));
        previewButton->setToolTip(
            QStringLiteral("打开模拟圆盘；点击一个扇区，将此账号移动到该位置"));
        radialAccountsTable_->setCellWidget(row, 3, previewButton);
        connect(previewButton, &QPushButton::clicked, this,
                [this, title] { emit radialAccountPreviewRequested(title); });
    }
    radialAccountsTable_->resizeRowsToContents();
}

void QuickSwitchPage::updateRadialAccountNickname(const QString &windowTitle,
                                                  const QString &nickname)
{
    const QString trimmedNickname = nickname.trimmed();
    bool changed = false;
    for (auto &account : radialAccountConfigs_) {
        if (account.windowTitle == windowTitle) {
            if (account.nickname != trimmedNickname) {
                account.nickname = trimmedNickname;
                changed = true;
            }
            break;
        }
    }
    if (!changed && std::none_of(radialAccountConfigs_.cbegin(), radialAccountConfigs_.cend(),
                                 [&windowTitle](const RadialAccountConfig &account) {
                                     return account.windowTitle == windowTitle;
                                 })) {
        radialAccountConfigs_.append(RadialAccountConfig{
            windowTitle,
            trimmedNickname,
            static_cast<int>(radialAccountConfigs_.size() + 1),
        });
        changed = true;
    }
    if (changed) {
        emit radialAccountConfigsChanged(radialAccountConfigs_);
    }
}

void QuickSwitchPage::commitRadialAccountOrder(const QStringList &visibleOrder)
{
    QList<RadialAccountConfig> orderedConfigs;
    QSet<QString> includedTitles;
    for (const QString &title : visibleOrder) {
        auto configIt = std::find_if(
            radialAccountConfigs_.begin(), radialAccountConfigs_.end(),
            [&title](const RadialAccountConfig &account) { return account.windowTitle == title; });
        if (configIt == radialAccountConfigs_.end()) {
            orderedConfigs.append(RadialAccountConfig{title, {}, 0});
        } else {
            orderedConfigs.append(*configIt);
        }
        includedTitles.insert(title);
    }

    QList<RadialAccountConfig> offlineConfigs;
    for (const auto &account : radialAccountConfigs_) {
        if (!includedTitles.contains(account.windowTitle)) {
            offlineConfigs.append(account);
        }
    }
    std::stable_sort(
        offlineConfigs.begin(), offlineConfigs.end(),
        [](const auto &left, const auto &right) { return left.position < right.position; });
    orderedConfigs.append(offlineConfigs);
    for (int index = 0; index < orderedConfigs.size(); ++index) {
        orderedConfigs[index].position = index + 1;
    }
    if (orderedConfigs == radialAccountConfigs_) {
        return;
    }
    radialAccountConfigs_ = orderedConfigs;
    rebuildRadialAccountTable();
    emit radialAccountConfigsChanged(radialAccountConfigs_);
}

void QuickSwitchPage::bindKey(bool previousDirection, const QString &key)
{
    QStringList &selectedKeys = previousDirection ? previousKeys_ : nextKeys_;
    QStringList &otherKeys = previousDirection ? nextKeys_ : previousKeys_;
    otherKeys.removeAll(key);
    if (!selectedKeys.contains(key)) {
        selectedKeys.append(key);
    }
    setKeys(previousKeys_, nextKeys_);
    emit keysChanged(previousKeys_, nextKeys_);
}

void QuickSwitchPage::removeCustomKey(bool previousDirection, const QString &key)
{
    if (quickSwitchKeyOptions().contains(key) || !quickSwitchKeyboardInput(key).has_value()) {
        return;
    }

    (previousDirection ? previousKeys_ : nextKeys_).removeAll(key);
    setKeys(previousKeys_, nextKeys_);
    emit keysChanged(previousKeys_, nextKeys_);
}

void QuickSwitchPage::refreshCustomKeys()
{
    const auto refresh = [this](QHBoxLayout *layout, const QStringList &values,
                                bool previousDirection) {
        while (QLayoutItem *item = layout->takeAt(0)) {
            if (QWidget *widget = item->widget()) {
                widget->deleteLater();
            }
            delete item;
        }

        QStringList customKeys;
        for (const QString &key : values) {
            if (!quickSwitchKeyOptions().contains(key) &&
                quickSwitchKeyboardInput(key).has_value()) {
                customKeys.append(key);
            }
        }
        layout->parentWidget()->setVisible(!customKeys.isEmpty());
        if (customKeys.isEmpty()) {
            return;
        }

        auto *label = new QLabel(QStringLiteral("自定义键："), layout->parentWidget());
        label->setObjectName(QStringLiteral("muted"));
        layout->addWidget(label);
        for (const QString &key : customKeys) {
            auto *removeButton = new QToolButton(layout->parentWidget());
            removeButton->setAutoRaise(true);
            removeButton->setText(QStringLiteral("%1 ×").arg(quickSwitchKeyLabel(key)));
            removeButton->setToolTip(QStringLiteral("移除按键 %1").arg(quickSwitchKeyLabel(key)));
            removeButton->setObjectName(previousDirection
                                            ? QStringLiteral("removePreviousCustomKey")
                                            : QStringLiteral("removeNextCustomKey"));
            connect(removeButton, &QToolButton::clicked, this,
                    [this, previousDirection, key] { removeCustomKey(previousDirection, key); });
            layout->addWidget(removeButton);
        }
        layout->addStretch(1);
    };

    refresh(previousCustomKeysLayout_, previousKeys_, true);
    refresh(nextCustomKeysLayout_, nextKeys_, false);
}

void QuickSwitchPage::bindingChanged(bool previousDirection, int keyIndex, bool checked)
{
    if (keyIndex < 0 || keyIndex >= static_cast<int>(quickSwitchKeyOptions().size())) {
        return;
    }
    const QString &key = quickSwitchKeyOptions()[keyIndex];
    QStringList &selectedKeys = previousDirection ? previousKeys_ : nextKeys_;
    QStringList &otherKeys = previousDirection ? nextKeys_ : previousKeys_;
    if (checked) {
        otherKeys.removeAll(key);
        if (!selectedKeys.contains(key)) {
            selectedKeys.append(key);
        }
    } else {
        selectedKeys.removeAll(key);
    }
    setKeys(previousKeys_, nextKeys_);
    emit keysChanged(previousKeys_, nextKeys_);
}
