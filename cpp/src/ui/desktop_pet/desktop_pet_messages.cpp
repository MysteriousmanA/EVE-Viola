#include "ui/desktop_pet_page.h"

#include <utility>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpression>
#include <QTableWidget>
#include <QTimer>
#include "features/desktop_pet/astrbot_notifier.h"
#include "ui/desktop_pet/floating_pet_window.h"

#include "ui/desktop_pet/desktop_pet_constants.h"

using namespace DesktopPetUi;

namespace {

QString accountNameFromWindowTitle(const QString& sourceWindowTitle)
{
    QString accountName = sourceWindowTitle.trimmed();
    static const QRegularExpression eveTitlePrefix(
        QStringLiteral("^EVE\\s*-\\s*"),
        QRegularExpression::CaseInsensitiveOption);
    accountName.remove(eveTitlePrefix);
    return accountName.trimmed();
}

} // namespace

void DesktopPetPage::initializeMessagePersistence()
{
    messagePersistTimer_ = new QTimer(this);
    messagePersistTimer_->setSingleShot(true);
    messagePersistTimer_->setInterval(350);
    connect(messagePersistTimer_, &QTimer::timeout, this, [this] { saveSettings(); });
}

void DesktopPetPage::setColorRules(const QList<ColorRule>& rules)
{
    if (colorRules_ == rules) {
        return;
    }
    QHash<int, QString> retainedMessages;
    retainedMessages.reserve(rules.size());
    for (const ColorRule& rule : rules) {
        if (ruleMessages_.contains(rule.number)) {
            retainedMessages.insert(rule.number, ruleMessages_.value(rule.number));
        }
    }
    const bool messagesPruned = retainedMessages.size() != ruleMessages_.size();
    ruleMessages_ = std::move(retainedMessages);
    colorRules_ = rules;
    rulesTable_->setRowCount(colorRules_.size());
    for (int row = 0; row < colorRules_.size(); ++row) {
        const ColorRule& rule = colorRules_[row];
        auto* ruleItem = new QTableWidgetItem(
            rule.note.trimmed().isEmpty()
                ? QStringLiteral("规则 #%1").arg(rule.number)
                : QStringLiteral("#%1  %2").arg(rule.number).arg(rule.note.trimmed()));
        ruleItem->setData(Qt::UserRole, rule.number);
        ruleItem->setToolTip(QStringLiteral("图色逻辑规则 #%1").arg(rule.number));
        rulesTable_->setItem(row, 0, ruleItem);

        auto* message = new QLineEdit(ruleMessages_.value(rule.number), rulesTable_);
        message->setObjectName(QStringLiteral("desktopPetRuleMessage_%1").arg(rule.number));
        message->setMaxLength(120);
        message->setPlaceholderText(QStringLiteral("例如：注意，条件已经满足"));
        message->setToolTip(QStringLiteral("该规则触发时显示和推送的文案，最多 120 字；留空时不提示也不推送"));
        rulesTable_->setCellWidget(row, 1, message);
        rulesTable_->setRowHeight(row, 46);
        connect(message, &QLineEdit::textChanged, this, [this, number = rule.number](const QString& text) {
            if (text.trimmed().isEmpty()) {
                ruleMessages_.remove(number);
            } else {
                ruleMessages_.insert(number, text);
            }
            messagePersistTimer_->start();
        });
    }
    rulesEmptyState_->setVisible(colorRules_.isEmpty());
    rulesTable_->setVisible(!colorRules_.isEmpty());
    if (messagesPruned) {
        saveSettings();
    }
}

void DesktopPetPage::showRuleMessage(int ruleNumber, const QString& sourceWindowTitle)
{
    const QString message = ruleMessages_.value(ruleNumber);
    if (message.trimmed().isEmpty() || mode_ == DesktopPetMode::DoNotDisturb) {
        return;
    }
    const QString accountName = accountNameFromWindowTitle(sourceWindowTitle);
    const auto astrBot = astrBotSettings();
    if (mode_ == DesktopPetMode::Fish && astrBot.enabled) {
        const QString text = accountName.isEmpty()
            ? message : QStringLiteral("%1：%2").arg(accountName, message);
        astrBotNotifier_->sendMessage(astrBot, text,
            QString::number(ruleNumber) + QChar(0x1f) + accountName);
    }
    if (!localMessagesEnabled_->isChecked() || sourceImage_.isNull()) {
        return;
    }
    if (!activeTriggerMessages_.contains(message)) {
        activeTriggerMessages_.append(message);
    }
    if (!accountName.isEmpty()) {
        QStringList& accountNames = activeTriggerAccounts_[message];
        if (!accountNames.contains(accountName)) {
            accountNames.append(accountName);
        }
    }
    QStringList displayMessages;
    displayMessages.reserve(activeTriggerMessages_.size());
    for (const QString& activeMessage : activeTriggerMessages_) {
        const QStringList accountNames = activeTriggerAccounts_.value(activeMessage);
        if (accountNames.isEmpty()) {
            displayMessages.append(activeMessage);
        } else {
            displayMessages.append(QStringLiteral("%1：%2")
                                       .arg(accountNames.join(QStringLiteral("、")),
                                            activeMessage));
        }
    }
    if (!petPositionInitialized_) {
        setDefaultPetPosition();
    }
    refreshPet();
    petWindow_->showMessage(displayMessages.join(QLatin1Char('\n')));
    updatePetButton();
    statusLabel_->setText(QStringLiteral("图色逻辑 #%1 触发，桌宠正在显示提示。")
                              .arg(ruleNumber));
}
