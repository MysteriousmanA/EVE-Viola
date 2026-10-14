#include "ui/expedition_timing_page.h"
#include "features/expedition_timing/expedition_timing_service.h"
#include "ui/common/page_widgets.h"
using PageWidgets::makeHeading;
using PageWidgets::makeDescription;

#include <array>

#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QDateTime>
#include <QDateTimeEdit>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

QRadioButton* makeChoice(
    const QString& text,
    const QString& objectName,
    QButtonGroup* group,
    QWidget* parent)
{
    auto* choice = new QRadioButton(text, parent);
    choice->setObjectName(objectName);
    choice->setProperty("value", text);
    group->addButton(choice);
    return choice;
}

QSpinBox* makeOffsetSpinBox(const QString& objectName, QWidget* parent)
{
    auto* spinBox = new QSpinBox(parent);
    spinBox->setObjectName(objectName);
    spinBox->setRange(-999, 999);
    spinBox->setAlignment(Qt::AlignCenter);
    spinBox->setButtonSymbols(QAbstractSpinBox::PlusMinus);
    spinBox->setMinimumWidth(86);
    return spinBox;
}

} // namespace

ExpeditionTimingPage::ExpeditionTimingPage(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    layout->addWidget(makeHeading(QStringLiteral("远征计时")));
    layout->addWidget(makeDescription(
        QStringLiteral("记录远征出现时间和有效期，生成可直接发送给舰队成员的到期文本。")));

    auto* optionsPanel = new QGroupBox(QStringLiteral("远征选项"), this);
    optionsPanel->setObjectName(QStringLiteral("expeditionOptionsPanel"));
    auto* optionsLayout = new QGridLayout(optionsPanel);
    optionsLayout->setContentsMargins(16, 20, 16, 14);
    optionsLayout->setHorizontalSpacing(12);
    optionsLayout->setVerticalSpacing(9);

    factionGroup_ = new QButtonGroup(this);
    factionGroup_->setObjectName(QStringLiteral("expeditionFactionGroup"));
    const std::array factions{
        QStringLiteral("兔子"), QStringLiteral("天蛇"), QStringLiteral("天使"),
        QStringLiteral("血袭"), QStringLiteral("萨沙"), QStringLiteral("无人机")};
    optionsLayout->addWidget(new QLabel(QStringLiteral("远征势力")), 0, 0);
    for (int index = 0; index < static_cast<int>(factions.size()); ++index) {
        auto* choice = makeChoice(
            factions[static_cast<std::size_t>(index)],
            QStringLiteral("expeditionFaction%1").arg(index),
            factionGroup_,
            optionsPanel);
        optionsLayout->addWidget(choice, 0, index + 1);
        if (index == 0) {
            choice->setChecked(true);
        }
    }
    optionsLayout->setColumnStretch(9, 1);

    typeGroup_ = new QButtonGroup(this);
    typeGroup_->setObjectName(QStringLiteral("expeditionTypeGroup"));
    const std::array topTypes{
        QStringLiteral("6-10"), QStringLiteral("10-10"),
        QStringLiteral("三无畏"), QStringLiteral("护盾母星")};
    const std::array bottomTypes{
        QStringLiteral("1-10"), QStringLiteral("2-10"), QStringLiteral("3-10"),
        QStringLiteral("4-10"), QStringLiteral("5-10"), QStringLiteral("7-10"),
        QStringLiteral("8-10"), QStringLiteral("9-10")};
    optionsLayout->addWidget(new QLabel(QStringLiteral("远征类型")), 1, 0);
    for (int index = 0; index < static_cast<int>(topTypes.size()); ++index) {
        const QString& type = topTypes[static_cast<std::size_t>(index)];
        QString objectName = QStringLiteral("expeditionTypeTop%1").arg(index);
        if (type == QStringLiteral("10-10")) {
            objectName = QStringLiteral("expeditionType10_10");
        } else if (type == QStringLiteral("三无畏")) {
            objectName = QStringLiteral("expeditionTypeCapitalDreadnought");
        } else if (type == QStringLiteral("护盾母星")) {
            objectName = QStringLiteral("expeditionTypeShieldMothership");
        }
        auto* choice = makeChoice(type, objectName, typeGroup_, optionsPanel);
        optionsLayout->addWidget(choice, 1, index + 1);
        if (type == QStringLiteral("10-10")) {
            choice->setChecked(true);
        }
        connect(choice, &QRadioButton::clicked, this, [this, type] {
            updateDefaultDuration(type);
        });
    }
    for (int index = 0; index < static_cast<int>(bottomTypes.size()); ++index) {
        const QString& type = bottomTypes[static_cast<std::size_t>(index)];
        auto* choice = makeChoice(
            type,
            QStringLiteral("expeditionTypeBottom%1").arg(index),
            typeGroup_,
            optionsPanel);
        optionsLayout->addWidget(choice, 2, index + 1);
        connect(choice, &QRadioButton::clicked, this, [this, type] {
            updateDefaultDuration(type);
        });
    }
    layout->addWidget(optionsPanel);

    auto* timePanel = new QGroupBox(QStringLiteral("远征时间"), this);
    timePanel->setObjectName(QStringLiteral("expeditionTimePanel"));
    auto* timeLayout = new QGridLayout(timePanel);
    timeLayout->setContentsMargins(16, 20, 16, 14);
    timeLayout->setHorizontalSpacing(10);
    timeLayout->setVerticalSpacing(10);

    startTime_ = new QDateTimeEdit(timePanel);
    startTime_->setObjectName(QStringLiteral("expeditionStartTime"));
    startTime_->setDisplayFormat(QStringLiteral("yyyy年M月d日  HH:mm"));
    startTime_->setCalendarPopup(true);
    startTime_->setMinimumWidth(210);
    auto* syncButton = new QPushButton(QStringLiteral("同步当前时间"), timePanel);
    syncButton->setObjectName(QStringLiteral("syncExpeditionTime"));
    connect(syncButton, &QPushButton::clicked, this, &ExpeditionTimingPage::syncCurrentTime);

    timeLayout->addWidget(new QLabel(QStringLiteral("出现时间")), 0, 0);
    timeLayout->addWidget(startTime_, 0, 1, 1, 3);
    timeLayout->addWidget(syncButton, 0, 4);
    timeLayout->setColumnStretch(5, 1);

    deltaDays_ = makeOffsetSpinBox(QStringLiteral("expeditionDeltaDays"), timePanel);
    deltaHours_ = makeOffsetSpinBox(QStringLiteral("expeditionDeltaHours"), timePanel);
    deltaMinutes_ = makeOffsetSpinBox(QStringLiteral("expeditionDeltaMinutes"), timePanel);
    deltaDays_->setSuffix(QStringLiteral(" 天"));
    deltaHours_->setSuffix(QStringLiteral(" 时"));
    deltaMinutes_->setSuffix(QStringLiteral(" 分"));
    auto* calculateButton = new QPushButton(QStringLiteral("计算到期时间"), timePanel);
    calculateButton->setObjectName(QStringLiteral("calculateExpeditionExpiry"));
    calculateButton->setProperty("accent", true);
    connect(calculateButton, &QPushButton::clicked, this, &ExpeditionTimingPage::calculateExpiry);

    timeLayout->addWidget(new QLabel(QStringLiteral("有效时长")), 1, 0);
    timeLayout->addWidget(deltaDays_, 1, 1);
    timeLayout->addWidget(deltaHours_, 1, 2);
    timeLayout->addWidget(deltaMinutes_, 1, 3);
    timeLayout->addWidget(calculateButton, 1, 4);
    layout->addWidget(timePanel);

    auto* resultCard = new QFrame(this);
    resultCard->setObjectName(QStringLiteral("expeditionResultCard"));
    auto* resultLayout = new QHBoxLayout(resultCard);
    resultLayout->setContentsMargins(18, 14, 16, 14);
    resultLayout->setSpacing(16);
    auto* resultText = new QVBoxLayout;
    resultText->setSpacing(4);
    auto* resultKicker = new QLabel(QStringLiteral("到期航标"), resultCard);
    resultKicker->setObjectName(QStringLiteral("expeditionResultKicker"));
    resultValue_ = new QLabel(QStringLiteral("设置远征信息后计算"), resultCard);
    resultValue_->setObjectName(QStringLiteral("expeditionResultValue"));
    resultValue_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    resultText->addWidget(resultKicker);
    resultText->addWidget(resultValue_);
    resultLayout->addLayout(resultText, 1);
    copyButton_ = new QPushButton(QStringLiteral("复制结果"), resultCard);
    copyButton_->setObjectName(QStringLiteral("copyExpeditionResult"));
    copyButton_->setEnabled(false);
    connect(copyButton_, &QPushButton::clicked, this, &ExpeditionTimingPage::copyResult);
    resultLayout->addWidget(copyButton_, 0, Qt::AlignVCenter);
    layout->addWidget(resultCard);
    layout->addStretch(1);

    syncCurrentTime();
    updateDefaultDuration(QStringLiteral("10-10"));
}

QString ExpeditionTimingPage::selectedValue(const QButtonGroup* group) const
{
    const auto* selected = group == nullptr ? nullptr : group->checkedButton();
    return selected == nullptr ? QString{} : selected->property("value").toString();
}

void ExpeditionTimingPage::syncCurrentTime()
{
    const QDateTime now = QDateTime::currentDateTime();
    startTime_->setDateTime(QDateTime(
        now.date(), QTime(now.time().hour(), now.time().minute()), now.timeZone()));
}

void ExpeditionTimingPage::updateDefaultDuration(const QString& expeditionType)
{
    const auto duration = ExpeditionTimingService::defaultDuration(expeditionType);
    deltaDays_->setValue(duration.days);
    deltaHours_->setValue(duration.hours);
    deltaMinutes_->setValue(duration.minutes);
}

void ExpeditionTimingPage::calculateExpiry()
{
    result_ = ExpeditionTimingService::expiryText(startTime_->dateTime(),
        {deltaDays_->value(), deltaHours_->value(), deltaMinutes_->value()},
        selectedValue(factionGroup_), selectedValue(typeGroup_));
    resultValue_->setText(result_);
    copyButton_->setEnabled(true);
}

void ExpeditionTimingPage::copyResult()
{
    if (!result_.isEmpty()) {
        QApplication::clipboard()->setText(result_);
    }
}
