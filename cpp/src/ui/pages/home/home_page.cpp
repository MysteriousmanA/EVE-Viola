#include "ui/pages/home/home_page.h"
#include <QButtonGroup>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>

HomePage::HomePage(QWidget *parent) : QWidget(parent)
{
    auto *page = this;
    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);

    auto *scroll = new QScrollArea(page);
    scroll->setObjectName(QStringLiteral("homeScroll"));
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(18);
    layout->setSizeConstraint(QLayout::SetMinimumSize);

    auto *brand = new QLabel(QStringLiteral("EVE · VIOLA"));
    brand->setObjectName(QStringLiteral("brand"));
    auto *title = new QLabel(QStringLiteral("小窗管理"));
    title->setObjectName(QStringLiteral("title"));
    auto *description = new QLabel(QStringLiteral("创建置顶窗口，并在小窗中右键选择 EVE 客户端。"));
    description->setObjectName(QStringLiteral("muted"));
    description->setWordWrap(true);
    layout->addWidget(brand);
    layout->addWidget(title);
    layout->addWidget(description);

    auto *actionBar = new QHBoxLayout;
    auto *createButton = new QPushButton(QStringLiteral("＋ 生成小窗"));
    createButton->setObjectName(QStringLiteral("createButton"));
    createButton->setCursor(Qt::PointingHandCursor);
    connect(createButton, &QPushButton::clicked, this, [this] { emit createRequested(); });
    auto *closeAllButton = new QPushButton(QStringLiteral("关闭所有小窗"));
    closeAllButton->setObjectName(QStringLiteral("closeAllButton"));
    closeAllButton->setCursor(Qt::PointingHandCursor);
    connect(closeAllButton, &QPushButton::clicked, this, [this] { emit closeAllRequested(); });
    countLabel_ = new QLabel(QStringLiteral("0 个小窗"));
    countLabel_->setObjectName(QStringLiteral("count"));
    actionBar->addWidget(createButton);
    actionBar->addWidget(closeAllButton);
    actionBar->addStretch(1);
    actionBar->addWidget(countLabel_);
    layout->addLayout(actionBar);

    auto *managementDivider = new QFrame;
    managementDivider->setFrameShape(QFrame::HLine);
    managementDivider->setStyleSheet(QStringLiteral("color: #263D49;"));
    layout->addWidget(managementDivider);

    auto *softwareOptionsTitle = new QLabel(QStringLiteral("软件选项"));
    softwareOptionsTitle->setObjectName(QStringLiteral("softwareOptionsTitle"));
    auto *softwareOptionsDescription =
        new QLabel(QStringLiteral("统一管理软件行为；修改后立即生效并自动保存。"));
    softwareOptionsDescription->setObjectName(QStringLiteral("muted"));
    softwareOptionsDescription->setWordWrap(true);
    layout->addWidget(softwareOptionsTitle);
    layout->addWidget(softwareOptionsDescription);

    auto *softwareOptionsPanel = new QFrame;
    softwareOptionsPanel->setObjectName(QStringLiteral("softwareOptionsPanel"));
    auto *optionsGrid = new QGridLayout(softwareOptionsPanel);
    optionsGrid->setContentsMargins(16, 14, 16, 14);
    optionsGrid->setHorizontalSpacing(24);
    optionsGrid->setVerticalSpacing(12);

    activateSourceOnColorLogicTriggerCheckBox_ =
        new QCheckBox(QStringLiteral("图色逻辑触发时切换到源窗口"));
    activateSourceOnColorLogicTriggerCheckBox_->setObjectName(
        QStringLiteral("activateSourceOnColorLogicTrigger"));
    activateSourceOnColorLogicTriggerCheckBox_->setToolTip(
        QStringLiteral("关闭后仍执行图色反馈和状态切换，但不自动激活对应源窗口"));
    connect(activateSourceOnColorLogicTriggerCheckBox_, &QCheckBox::toggled, this,
            [this](bool checked) {
                automaticActivationCooldownSpinBox_->setEnabled(checked);
                automaticActivationMouseQuietPeriodSpinBox_->setEnabled(checked);
                emit activateSourceChanged(checked);
            });

    keepInTrayCheckBox_ = new QCheckBox(QStringLiteral("保留到托盘"));
    keepInTrayCheckBox_->setObjectName(QStringLiteral("keepInTray"));
    keepInTrayCheckBox_->setToolTip(QStringLiteral("关闭主窗口时最小化到系统托盘，小窗继续运行"));
    connect(keepInTrayCheckBox_, &QCheckBox::toggled, this,
            [this](bool checked) { emit keepInTrayChanged(checked); });
    restoreWindowsWithGameCheckBox_ = new QCheckBox(QStringLiteral("随游戏恢复窗口"));
    restoreWindowsWithGameCheckBox_->setObjectName(QStringLiteral("restoreWindowsWithGame"));
    restoreWindowsWithGameCheckBox_->setToolTip(
        QStringLiteral("游戏进程启动后，按勾选预设恢复因缺少源进程而未启用的小窗及其位置"));
    connect(restoreWindowsWithGameCheckBox_, &QCheckBox::toggled, this,
            [this](bool checked) { emit restoreWithGameChanged(checked); });
    raiseReplicaOnClickCheckBox_ = new QCheckBox(QStringLiteral("点击小窗时置于最前"));
    raiseReplicaOnClickCheckBox_->setObjectName(QStringLiteral("raiseReplicaOnClick"));
    raiseReplicaOnClickCheckBox_->setToolTip(QStringLiteral(
        "仅调整小窗之间的层级，不改变源进程窗口的置顶状态；关闭后点击仍会跳转源窗口"));
    connect(raiseReplicaOnClickCheckBox_, &QCheckBox::toggled, this,
            [this](bool checked) { emit raiseOnClickChanged(checked); });

    auto *activationTimingControl = new QWidget;
    auto *cooldownLayout = new QGridLayout(activationTimingControl);
    cooldownLayout->setContentsMargins(0, 0, 0, 0);
    cooldownLayout->setHorizontalSpacing(8);
    cooldownLayout->setVerticalSpacing(4);
    auto *cooldownLabel = new QLabel(QStringLiteral("自动切换冷却"));
    automaticActivationCooldownSpinBox_ = new QDoubleSpinBox;
    automaticActivationCooldownSpinBox_->setObjectName(
        QStringLiteral("automaticActivationCooldown"));
    automaticActivationCooldownSpinBox_->setRange(kAutomaticActivationCooldownMinimumMs / 1000.0,
                                                  kAutomaticActivationCooldownMaximumMs / 1000.0);
    automaticActivationCooldownSpinBox_->setSingleStep(kAutomaticActivationCooldownStepMs / 1000.0);
    automaticActivationCooldownSpinBox_->setDecimals(1);
    automaticActivationCooldownSpinBox_->setSuffix(QStringLiteral(" s"));
    automaticActivationCooldownSpinBox_->setValue(kDefaultAutomaticActivationCooldownMs / 1000.0);
    automaticActivationCooldownSpinBox_->setToolTip(
        QStringLiteral("图色逻辑触发后，自动切换到源窗口的最短间隔"));
    connect(automaticActivationCooldownSpinBox_, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double seconds) { emit cooldownChanged(seconds); });

    automaticActivationMouseQuietPeriodSpinBox_ = new QDoubleSpinBox;
    automaticActivationMouseQuietPeriodSpinBox_->setObjectName(
        QStringLiteral("automaticActivationMouseQuietPeriod"));
    automaticActivationMouseQuietPeriodSpinBox_->setRange(
        kAutomaticActivationMouseQuietPeriodMinimumMs / 1000.0,
        kAutomaticActivationMouseQuietPeriodMaximumMs / 1000.0);
    automaticActivationMouseQuietPeriodSpinBox_->setSingleStep(
        kAutomaticActivationMouseQuietPeriodStepMs / 1000.0);
    automaticActivationMouseQuietPeriodSpinBox_->setDecimals(1);
    automaticActivationMouseQuietPeriodSpinBox_->setSuffix(QStringLiteral(" s"));
    automaticActivationMouseQuietPeriodSpinBox_->setValue(
        kDefaultAutomaticActivationMouseQuietPeriodMs / 1000.0);
    automaticActivationMouseQuietPeriodSpinBox_->setToolTip(QStringLiteral(
        "任意位置的鼠标或键盘按键松开后，在此时间内不因图色触发而自动切换窗口；按住期间始终抑制，再次点击或按键会重新计时"));
    connect(automaticActivationMouseQuietPeriodSpinBox_,
            qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](double seconds) { emit mouseQuietPeriodChanged(seconds); });

    cooldownLayout->addWidget(cooldownLabel, 0, 0);
    cooldownLayout->addWidget(automaticActivationCooldownSpinBox_, 0, 1);
    cooldownLayout->addWidget(new QLabel(QStringLiteral("点击静默")), 1, 0);
    cooldownLayout->addWidget(automaticActivationMouseQuietPeriodSpinBox_, 1, 1);
    cooldownLayout->setColumnStretch(2, 1);

    auto *lifecycleOptions = new QWidget;
    auto *lifecycleLayout = new QHBoxLayout(lifecycleOptions);
    lifecycleLayout->setContentsMargins(0, 0, 0, 0);
    lifecycleLayout->setSpacing(18);
    lifecycleLayout->addWidget(restoreWindowsWithGameCheckBox_);
    lifecycleLayout->addWidget(keepInTrayCheckBox_);
    lifecycleLayout->addStretch(1);

    optionsGrid->addWidget(activateSourceOnColorLogicTriggerCheckBox_, 0, 0);
    optionsGrid->addWidget(activationTimingControl, 0, 1);
    optionsGrid->addWidget(raiseReplicaOnClickCheckBox_, 1, 0);
    optionsGrid->addWidget(lifecycleOptions, 1, 1);
    optionsGrid->setColumnStretch(0, 1);
    optionsGrid->setColumnStretch(1, 1);
    layout->addWidget(softwareOptionsPanel);

    auto *divider = new QFrame;
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet(QStringLiteral("color: #263D49;"));
    layout->addWidget(divider);

    auto *presetTitle = new QLabel(QStringLiteral("预设"));
    presetTitle->setObjectName(QStringLiteral("presetSectionTitle"));
    layout->addWidget(presetTitle);
    auto *presetDescription =
        new QLabel(QStringLiteral("保存/加载整套小窗配置；勾选一个预设作为默认启动。"));
    presetDescription->setObjectName(QStringLiteral("muted"));
    presetDescription->setWordWrap(true);
    layout->addWidget(presetDescription);

    presetButtonGroup_ = new QButtonGroup(this);
    presetButtonGroup_->setExclusive(true);
    auto *presetsContainer = new QWidget;
    auto *presetsLayout = new QVBoxLayout(presetsContainer);
    presetsLayout->setContentsMargins(0, 0, 0, 0);
    presetsLayout->setSpacing(6);
    for (int slot = 0; slot < kPresetCount; ++slot) {
        buildPresetBar(slot, presetsLayout);
    }
    layout->addWidget(presetsContainer);

    auto *hint = new QLabel(QStringLiteral("在“小窗列表”页可以分别控制小窗显示与像素统计。"));
    hint->setObjectName(QStringLiteral("muted"));
    hint->setWordWrap(true);
    layout->addStretch(1);
    layout->addWidget(hint);

    scroll->setWidget(content);
    pageLayout->addWidget(scroll);
}
void HomePage::buildPresetBar(int slot, QVBoxLayout *parent)
{
    auto *bar = new QHBoxLayout;
    bar->setSpacing(8);

    presetRadio_[slot] = new QRadioButton;
    presetRadio_[slot]->setObjectName(QStringLiteral("presetDefault%1").arg(slot + 1));
    presetRadio_[slot]->setToolTip(QStringLiteral("勾选后作为默认启动预设"));
    presetButtonGroup_->addButton(presetRadio_[slot]);

    presetNameLabel_[slot] = new QLabel(QStringLiteral("未命名"));
    presetNameLabel_[slot]->setObjectName(QStringLiteral("presetName"));

    auto *renameButton = new QPushButton(QStringLiteral("改名"));
    renameButton->setObjectName(QStringLiteral("renamePreset%1").arg(slot + 1));
    auto *saveButton = new QPushButton(QStringLiteral("保存"));
    saveButton->setObjectName(QStringLiteral("savePreset%1").arg(slot + 1));
    auto *loadButton = new QPushButton(QStringLiteral("加载"));
    loadButton->setObjectName(QStringLiteral("loadPreset%1").arg(slot + 1));
    auto *deleteButton = new QPushButton(QStringLiteral("删除"));
    deleteButton->setObjectName(QStringLiteral("deletePreset%1").arg(slot + 1));

    connect(renameButton, &QPushButton::clicked, this,
            [this, slot] { emit renamePresetRequested(slot); });
    connect(saveButton, &QPushButton::clicked, this,
            [this, slot] { emit savePresetRequested(slot); });
    connect(loadButton, &QPushButton::clicked, this,
            [this, slot] { emit loadPresetRequested(slot); });
    connect(deleteButton, &QPushButton::clicked, this,
            [this, slot] { emit deletePresetRequested(slot); });
    connect(presetRadio_[slot], &QRadioButton::toggled, this, [this, slot](bool checked) {
        if (checked) {
            emit defaultPresetRequested(slot);
        }
    });

    bar->addWidget(presetRadio_[slot]);
    bar->addWidget(presetNameLabel_[slot], 1);
    bar->addWidget(renameButton);
    bar->addWidget(saveButton);
    bar->addWidget(loadButton);
    bar->addWidget(deleteButton);
    parent->addLayout(bar);
}
void HomePage::setPresets(const AppConfig &config)
{
    for (int slot = 0; slot < kPresetCount; ++slot) {
        const PresetConfig &preset = config.presets[slot];
        presetNameLabel_[slot]->setText(preset.name.isEmpty() ? QStringLiteral("未命名")
                                                              : preset.name);
        const bool isDefault = config.defaultPresetSlot == slot + 1;
        const QSignalBlocker blocker(presetRadio_[slot]);
        presetRadio_[slot]->setChecked(isDefault);
    }
}
void HomePage::setConfig(const AppConfig &config)
{
    {
        const QSignalBlocker blocker(keepInTrayCheckBox_);
        keepInTrayCheckBox_->setChecked(config.keepInTray);
    }
    {
        const QSignalBlocker blocker(restoreWindowsWithGameCheckBox_);
        restoreWindowsWithGameCheckBox_->setChecked(config.restoreWindowsWithGame);
    }
    {
        const QSignalBlocker blocker(raiseReplicaOnClickCheckBox_);
        raiseReplicaOnClickCheckBox_->setChecked(config.raiseReplicaOnClick);
    }
    {
        const QSignalBlocker blocker(activateSourceOnColorLogicTriggerCheckBox_);
        activateSourceOnColorLogicTriggerCheckBox_->setChecked(
            config.activateSourceOnColorLogicTrigger);
    }
    {
        const QSignalBlocker blocker(automaticActivationCooldownSpinBox_);
        automaticActivationCooldownSpinBox_->setValue(config.automaticActivationCooldownMs /
                                                      1000.0);
    }
    {
        const QSignalBlocker blocker(automaticActivationMouseQuietPeriodSpinBox_);
        automaticActivationMouseQuietPeriodSpinBox_->setValue(
            config.automaticActivationMouseQuietPeriodMs / 1000.0);
    }
    automaticActivationCooldownSpinBox_->setEnabled(config.activateSourceOnColorLogicTrigger);
    automaticActivationMouseQuietPeriodSpinBox_->setEnabled(
        config.activateSourceOnColorLogicTrigger);
    setPresets(config);
}
void HomePage::setReplicaCount(int count)
{
    countLabel_->setText(QStringLiteral("%1 个小窗").arg(count));
}
