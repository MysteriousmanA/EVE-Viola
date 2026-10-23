#include "ui/desktop_pet_page.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

#include "features/desktop_pet/astrbot_notifier.h"
#include "ui/desktop_pet/floating_pet_window.h"

void DesktopPetPage::buildNotificationSettings(QVBoxLayout* layout, QWidget* parent)
{
    auto* panel = new QFrame(parent);
    auto* form = new QFormLayout(panel);
    form->setContentsMargins(18, 16, 18, 16);
    auto* title = new QLabel(QStringLiteral("提示与 AstrBot 推送"), panel);
    title->setObjectName(QStringLiteral("petSectionTitle"));
    form->addRow(title);
    modeCombo_ = new QComboBox(panel);
    modeCombo_->setObjectName(QStringLiteral("desktopPetMode"));
    for (auto mode : {DesktopPetMode::Resident, DesktopPetMode::Fish, DesktopPetMode::DoNotDisturb}) {
        modeCombo_->addItem(desktopPetModeName(mode), static_cast<int>(mode));
    }
    form->addRow(QStringLiteral("运行模式"), modeCombo_);
    modeDescription_ = new QLabel(panel);
    modeDescription_->setObjectName(QStringLiteral("desktopPetModeDescription"));
    modeDescription_->setWordWrap(true);
    form->addRow(modeDescription_);
    localMessagesEnabled_ = new QCheckBox(QStringLiteral("在桌宠下方显示提示"), panel);
    localMessagesEnabled_->setObjectName(QStringLiteral("desktopPetLocalMessagesEnabled"));
    localMessagesEnabled_->setChecked(true);
    form->addRow(localMessagesEnabled_);
    astrBotEnabled_ = new QCheckBox(QStringLiteral("通过 AstrBot 发送触发提示"), panel);
    astrBotEnabled_->setObjectName(QStringLiteral("desktopPetAstrBotEnabled"));
    form->addRow(astrBotEnabled_);
    astrBotServerUrl_ = new QLineEdit(QStringLiteral("http://127.0.0.1:6185"), panel);
    astrBotServerUrl_->setObjectName(QStringLiteral("desktopPetAstrBotServerUrl"));
    astrBotServerUrl_->setPlaceholderText(QStringLiteral("http://127.0.0.1:6185"));
    astrBotServerUrl_->setToolTip(QStringLiteral("填写 AstrBot 管理后台（WebUI）的地址，通常端口为 6185；此处不使用模型提供商的 Base URL。"));
    form->addRow(QStringLiteral("AstrBot 管理后台地址"), astrBotServerUrl_);
    astrBotApiKey_ = new QLineEdit(panel);
    astrBotApiKey_->setObjectName(QStringLiteral("desktopPetAstrBotApiKey"));
    astrBotApiKey_->setEchoMode(QLineEdit::Password);
    astrBotApiKey_->setPlaceholderText(QStringLiteral("AstrBot 设置 → OpenAPI 创建，授予 im 权限"));
    form->addRow(QStringLiteral("AstrBot OpenAPI 密钥"), astrBotApiKey_);
    astrBotUmo_ = new QLineEdit(panel);
    astrBotUmo_->setObjectName(QStringLiteral("desktopPetAstrBotUmo"));
    astrBotUmo_->setPlaceholderText(QStringLiteral("平台 ID:消息类型:会话 ID（发送 /sid 获取）"));
    form->addRow(QStringLiteral("接收会话 UMO"), astrBotUmo_);
    astrBotCooldown_ = new QSpinBox(panel);
    astrBotCooldown_->setObjectName(QStringLiteral("desktopPetAstrBotCooldown"));
    astrBotCooldown_->setRange(0, 600);
    astrBotCooldown_->setValue(30);
    astrBotCooldown_->setSuffix(QStringLiteral(" 秒"));
    astrBotCooldown_->setToolTip(QStringLiteral("同一账号同一规则的最短推送间隔；0 表示不限制。失败请求也参与冷却，避免连续请求。"));
    form->addRow(QStringLiteral("重复推送冷却"), astrBotCooldown_);
    auto* help = new QLabel(QStringLiteral(
        "需要 AstrBot 4.18.0 或更新版本。填写 AstrBot 管理后台地址（通常端口为 6185），不要填写模型提供商的 Base URL。"
        "在 AstrBot 的“设置 → OpenAPI”创建具有 im 权限的 API Key，此处不使用模型服务密钥。"
        "在目标私聊或群聊发送 /sid 后粘贴 UMO。设置自动保存，API Key 随配置保存在本机。"), panel);
    help->setObjectName(QStringLiteral("muted"));
    help->setWordWrap(true);
    form->addRow(help);
    astrBotTestButton_ = new QPushButton(QStringLiteral("发送测试消息"), panel);
    astrBotTestButton_->setObjectName(QStringLiteral("desktopPetAstrBotTest"));
    form->addRow(astrBotTestButton_);
    astrBotStatus_ = new QLabel(QStringLiteral("AstrBot 推送仅在摸鱼模式可用，切换后可发送测试消息。"), panel);
    astrBotStatus_->setObjectName(QStringLiteral("desktopPetAstrBotStatus"));
    astrBotStatus_->setTextFormat(Qt::PlainText);
    astrBotStatus_->setWordWrap(true);
    form->addRow(astrBotStatus_);
    layout->addWidget(panel);
}

AstrBotSettings DesktopPetPage::astrBotSettings() const
{
    AstrBotSettings settings;
    settings.enabled = astrBotEnabled_->isChecked();
    settings.serverUrl = astrBotServerUrl_->text().trimmed();
    settings.apiKey = astrBotApiKey_->text().trimmed();
    settings.umo = astrBotUmo_->text().trimmed();
    settings.cooldownSeconds = astrBotCooldown_->value();
    return settings;
}

void DesktopPetPage::connectNotificationSignals()
{
    connect(modeCombo_, &QComboBox::currentIndexChanged, this, [this] {
        setMode(static_cast<DesktopPetMode>(modeCombo_->currentData().toInt()));
    });
    const auto changed = [this] { messagePersistTimer_->start(); };
    connect(localMessagesEnabled_, &QCheckBox::toggled, this, changed);
    connect(astrBotEnabled_, &QCheckBox::toggled, this, changed);
    for (auto* edit : {astrBotServerUrl_, astrBotApiKey_, astrBotUmo_}) {
        connect(edit, &QLineEdit::textChanged, this, changed);
    }
    connect(astrBotCooldown_, &QSpinBox::valueChanged, this, changed);
    connect(astrBotNotifier_, &AstrBotNotifier::deliveryFinished, this,
            [this](bool, const QString& detail) { astrBotStatus_->setText(detail); });
    connect(astrBotTestButton_, &QPushButton::clicked, this, [this] {
        if (mode_ != DesktopPetMode::Fish) { return; }
        astrBotStatus_->setText(QStringLiteral("正在发送 AstrBot 测试消息…"));
        astrBotNotifier_->sendMessage(astrBotSettings(),
            QStringLiteral("EVE-Viola 摸鱼桌宠：AstrBot 测试消息，推送配置已连通。"));
    });
    updateNotificationMode();
}

void DesktopPetPage::setMode(DesktopPetMode mode)
{
    if (!isValidDesktopPetMode(mode) || mode_ == mode) { return; }
    mode_ = mode;
    petWindow_->setMode(mode);
    if (mode != DesktopPetMode::Fish) { astrBotNotifier_->cancelPending(); }
    if (mode == DesktopPetMode::DoNotDisturb) {
        petWindow_->clearMessage();
        activeTriggerMessages_.clear();
        activeTriggerAccounts_.clear();
    }
    updateNotificationMode();
    saveSettings();
    emit modeChanged(mode);
}

void DesktopPetPage::updateNotificationMode()
{
    const QSignalBlocker blocker(modeCombo_);
    modeCombo_->setCurrentIndex(modeCombo_->findData(static_cast<int>(mode_)));
    localMessagesEnabled_->setEnabled(mode_ != DesktopPetMode::DoNotDisturb);
    astrBotTestButton_->setEnabled(mode_ == DesktopPetMode::Fish);
    switch (mode_) {
    case DesktopPetMode::Resident:
        modeDescription_->setText(QStringLiteral("常驻：允许小窗、划线和图色逻辑，允许桌宠文字；禁止 AstrBot 推送。"));
        break;
    case DesktopPetMode::Fish:
        modeDescription_->setText(QStringLiteral("摸鱼：隐藏小窗与划线，暂停图色动作，仅保留后台检测用于文字和 AstrBot 提醒。"));
        break;
    case DesktopPetMode::DoNotDisturb:
        modeDescription_->setText(QStringLiteral("勿扰：隐藏小窗与划线，暂停图色逻辑，禁止文字和 AstrBot 提醒。"));
        break;
    }
}
