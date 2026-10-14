#include "ui/replica_row.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QPushButton>
#include <QSignalBlocker>
#include "ui/replica_row/scroll_passthrough_controls.h"

using namespace ReplicaRowUi;

namespace {

QString colorRuleSlotLabel(const ColorRule& rule)
{
    QString note = rule.note.trimmed();
    if (note.isEmpty()) {
        note = QStringLiteral("未命名");
    }
    return QStringLiteral("#%1 %2")
        .arg(rule.number)
        .arg(note);
}

} // namespace

void ReplicaRow::initializeColorLogicControls()
{
    colorLogicSlotA_ = new ScrollPassthroughCombo;
    colorLogicSlotA_->setObjectName(QStringLiteral("colorLogicSlotA"));
    colorLogicSlotA_->setMinimumWidth(75);
    colorLogicSlotA_->addItem(QStringLiteral("无"), 0);
    colorLogicSlotA_->setCurrentIndex(0);
    colorLogicSlotA_->setToolTip(QStringLiteral("图色逻辑槽位 1，两个槽位不能选同一条规则"));

    colorLogicSlotB_ = new ScrollPassthroughCombo;
    colorLogicSlotB_->setObjectName(QStringLiteral("colorLogicSlotB"));
    colorLogicSlotB_->setMinimumWidth(75);
    colorLogicSlotB_->addItem(QStringLiteral("无"), 0);
    colorLogicSlotB_->setCurrentIndex(0);
    colorLogicSlotB_->setToolTip(QStringLiteral("图色逻辑槽位 2，两个槽位不能选同一条规则"));

    connect(colorLogicSlotA_, &QComboBox::currentIndexChanged, this, [this](int) {
        const int selected = colorLogicSlotA_->currentData().toInt();
        rebuildColorLogicSlot(colorLogicSlotB_, selected);
        emit colorLogicSlotChanged(1, selected);
    });
    connect(colorLogicSlotB_, &QComboBox::currentIndexChanged, this, [this](int) {
        const int selected = colorLogicSlotB_->currentData().toInt();
        rebuildColorLogicSlot(colorLogicSlotA_, selected);
        emit colorLogicSlotChanged(2, selected);
    });

    colorLogicSlotColorA_ = new QPushButton;
    colorLogicSlotColorA_->setObjectName(QStringLiteral("colorLogicSlotColorA"));
    colorLogicSlotColorA_->setFixedSize(26, 26);
    colorLogicSlotColorA_->setToolTip(QStringLiteral("启用槽位 1 时小窗的边框颜色"));
    applyColorButton(colorLogicSlotColorA_, slotColorA_);
    connect(colorLogicSlotColorA_, &QPushButton::clicked, this, [this] {
        const QColor color = QColorDialog::getColor(
            slotColorA_, this, QStringLiteral("槽位 1 边框颜色"));
        if (color.isValid()) {
            slotColorA_ = color;
            applyColorButton(colorLogicSlotColorA_, slotColorA_);
            emit colorLogicSlotColorChanged(1, color);
        }
    });

    colorLogicSlotColorB_ = new QPushButton;
    colorLogicSlotColorB_->setObjectName(QStringLiteral("colorLogicSlotColorB"));
    colorLogicSlotColorB_->setFixedSize(26, 26);
    colorLogicSlotColorB_->setToolTip(QStringLiteral("启用槽位 2 时小窗的边框颜色"));
    applyColorButton(colorLogicSlotColorB_, slotColorB_);
    connect(colorLogicSlotColorB_, &QPushButton::clicked, this, [this] {
        const QColor color = QColorDialog::getColor(
            slotColorB_, this, QStringLiteral("槽位 2 边框颜色"));
        if (color.isValid()) {
            slotColorB_ = color;
            applyColorButton(colorLogicSlotColorB_, slotColorB_);
            emit colorLogicSlotColorChanged(2, color);
        }
    });

    colorLogicEnabledCheck_ = new QCheckBox(QStringLiteral("启用图色逻辑"));
    colorLogicEnabledCheck_->setObjectName(QStringLiteral("colorLogicEnabled"));
    colorLogicEnabledCheck_->setToolTip(QStringLiteral("开启后执行当前槽位的图色逻辑"));
    colorLogicEnabledCheck_->setChecked(false);
    connect(colorLogicEnabledCheck_, &QCheckBox::toggled, this, [this](bool enabled) {
        emit colorLogicEnabledChanged(enabled);
    });

    activeSlotCombo_ = new ScrollPassthroughCombo;
    activeSlotCombo_->setObjectName(QStringLiteral("colorLogicActiveSlot"));
    activeSlotCombo_->setToolTip(QStringLiteral("执行图色逻辑时使用的槽位"));
    activeSlotCombo_->addItem(QStringLiteral("槽位1"), 1);
    activeSlotCombo_->addItem(QStringLiteral("槽位2"), 2);
    activeSlotCombo_->setCurrentIndex(0);
    connect(activeSlotCombo_, &QComboBox::currentIndexChanged, this, [this](int) {
        emit activeColorLogicSlotChanged(activeSlotCombo_->currentData().toInt());
    });
}

int ReplicaRow::colorLogicSlotRule(int slotIndex) const noexcept
{
    auto* combo = slotIndex == 1 ? colorLogicSlotA_ : colorLogicSlotB_;
    return combo->currentData().toInt();
}

void ReplicaRow::setColorLogicSlotColor(int slotIndex, const QColor& color)
{
    if (slotIndex < 1 || slotIndex > 2 || !color.isValid()) {
        return;
    }
    QColor& target = slotIndex == 1 ? slotColorA_ : slotColorB_;
    QPushButton* button = slotIndex == 1 ? colorLogicSlotColorA_ : colorLogicSlotColorB_;
    if (target == color) {
        return;
    }
    target = color;
    applyColorButton(button, target);
    emit colorLogicSlotColorChanged(slotIndex, color);
}

void ReplicaRow::setColorRules(const QList<ColorRule>& rules)
{
    colorRules_ = rules;
    const int selectedA = colorLogicSlotA_->currentData().toInt();
    const int selectedB = colorLogicSlotB_->currentData().toInt();
    rebuildColorLogicSlot(colorLogicSlotA_, selectedB);
    rebuildColorLogicSlot(colorLogicSlotB_, selectedA);
}

void ReplicaRow::setColorLogicSlot(int slotIndex, int ruleNumber)
{
    auto* combo = slotIndex == 1 ? colorLogicSlotA_ : colorLogicSlotB_;
    const int index = combo->findData(ruleNumber);
    if (index >= 0) {
        if (combo->currentIndex() != index) {
            combo->setCurrentIndex(index);
        }
    } else if (combo->currentData().toInt() != 0) {
        combo->setCurrentIndex(combo->findData(0));
    }
}

void ReplicaRow::setColorLogicEnabled(bool enabled)
{
    if (colorLogicEnabledCheck_->isChecked() != enabled) {
        colorLogicEnabledCheck_->setChecked(enabled);
    }
}

void ReplicaRow::setActiveColorLogicSlot(int slotIndex)
{
    const int index = activeSlotCombo_->findData(slotIndex);
    if (index >= 0 && activeSlotCombo_->currentIndex() != index) {
        activeSlotCombo_->setCurrentIndex(index);
    }
}

QColor ReplicaRow::colorLogicSlotColor(int slotIndex) const noexcept
{
    return slotIndex == 1 ? slotColorA_ : slotColorB_;
}

void ReplicaRow::applyColorButton(QPushButton* button, const QColor& color)
{
    button->setStyleSheet(
        QStringLiteral("background-color: %1; border: 1px solid #4D6C7A;").arg(color.name()));
}

void ReplicaRow::rebuildColorLogicSlot(QComboBox* combo, int excludedNumber)
{
    const int current = combo->currentData().toInt();
    const QSignalBlocker blocker(combo);
    combo->clear();
    combo->addItem(QStringLiteral("无"), 0);
    for (const auto& rule : colorRules_) {
        if (rule.number == excludedNumber) {
            continue;
        }
        combo->addItem(colorRuleSlotLabel(rule), rule.number);
    }
    const int index = combo->findData(current);
    combo->setCurrentIndex(index >= 0 ? index : 0);
}
