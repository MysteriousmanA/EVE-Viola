#include "ui/pixel_color_editor.h"

#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

PixelColorEditor::PixelColorEditor(const PixelColorTarget& initial, bool allowNone, QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("pixelColorEditor"));
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    combo_ = new QComboBox(this);
    combo_->setEditable(true);
    combo_->setInsertPolicy(QComboBox::NoInsert);
    combo_->setMinimumContentsLength(8);
    combo_->setMinimumWidth(110);
    if (allowNone) { combo_->addItem(QStringLiteral("无"), QString{}); }
    for (const auto& name : pixelColorCategoryNames()) { combo_->addItem(name, name); }
    combo_->lineEdit()->setPlaceholderText(QStringLiteral("#RRGGBB"));
    combo_->setToolTip(QStringLiteral(
        "选择红/白/蓝预设，或输入任意 #RRGGBB 色值。蓝色为 EVE 蓝 #304F5B，预设 RGB 容差为 16。"));
    auto* choose = new QPushButton(QStringLiteral("选色…"), this);
    choose->setObjectName(QStringLiteral("choosePixelColor"));
    tolerance_ = new QSpinBox(this);
    tolerance_->setObjectName(QStringLiteral("pixelColorTolerance"));
    tolerance_->setRange(-1, 255);
    tolerance_->setMinimumWidth(84);
    tolerance_->setSpecialValueText(QStringLiteral("预设范围"));
    tolerance_->setToolTip(QStringLiteral(
        "预设范围保留内置颜色判定。数值 0–255 表示每个 RGB 通道允许的最大差值；"
        "0 为完全一致，数值越大匹配越宽。不同颜色的统计可重叠。"));
    const auto updateState = [this, choose] {
        const QString color = combo_->currentText().trimmed();
        const bool none = color.isEmpty() || color == QStringLiteral("无");
        const bool preset = pixelColorCategoryNames().contains(color);
        const bool wasPreset = tolerance_->minimum() < 0;
        tolerance_->setMinimum(preset || none ? -1 : 0);
        tolerance_->setSpecialValueText(preset || none ? QStringLiteral("预设范围") : QString{});
        if (!preset && !none && wasPreset && tolerance_->value() == 0) {
            tolerance_->setValue(32);
        }
        tolerance_->setEnabled(!none);
        choose->setEnabled(true);
    };
    connect(combo_, &QComboBox::currentTextChanged, this, updateState);
    connect(choose, &QPushButton::clicked, this, [this] {
        const auto initialColor = pixelColorValue(combo_->currentText().trimmed());
        const auto color = QColorDialog::getColor(
            initialColor.isValid() ? initialColor : QColor(Qt::green), this,
            QStringLiteral("选择统计颜色"));
        if (color.isValid()) { combo_->setCurrentText(color.name()); }
    });
    combo_->setCurrentText(initial.color.isEmpty() && allowNone ? QStringLiteral("无") : initial.color);
    updateState();
    tolerance_->setValue(initial.tolerance < 0 && tolerance_->minimum() == 0 ? 32 : initial.tolerance);
    layout->addWidget(combo_, 1);
    layout->addWidget(choose);
    layout->addWidget(new QLabel(QStringLiteral("容差"), this));
    layout->addWidget(tolerance_);
}

std::optional<PixelColorTarget> PixelColorEditor::target() const
{
    const QString color = combo_->currentText().trimmed();
    if (color.isEmpty() || color == QStringLiteral("无")) { return PixelColorTarget{}; }
    if (!pixelColorValue(color).isValid()) { return std::nullopt; }
    return normalizedPixelColorTargets({{color, tolerance_->value()}}).first();
}

std::optional<QList<PixelColorTarget>> promptStatisticsColors(
    QWidget* parent, const QList<PixelColorTarget>& targets)
{
    QDialog dialog(parent);
    dialog.setObjectName(QStringLiteral("statisticsColorsDialog"));
    dialog.setWindowTitle(QStringLiteral("设置统计颜色"));
    dialog.resize(610, 330);
    auto* layout = new QVBoxLayout(&dialog);
    auto* description = new QLabel(QStringLiteral(
        "默认统计红、白、蓝。可添加任意颜色，并分别调整 RGB 容差（0 为完全一致）。"));
    description->setWordWrap(true);
    layout->addWidget(description);
    auto* table = new QTableWidget(0, 1, &dialog);
    table->setObjectName(QStringLiteral("statisticsColorsTable"));
    table->setHorizontalHeaderLabels({QStringLiteral("颜色与匹配范围")});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    const auto append = [table](const PixelColorTarget& target) {
        const int row = table->rowCount();
        table->insertRow(row);
        table->setCellWidget(row, 0, new PixelColorEditor(target, false, table));
        table->setRowHeight(row, 40);
        table->setCurrentCell(row, 0);
    };
    for (const auto& target : targets) { append(target); }
    layout->addWidget(table, 1);
    auto* actions = new QHBoxLayout;
    auto* add = new QPushButton(QStringLiteral("添加颜色"), &dialog);
    add->setObjectName(QStringLiteral("addStatisticsColor"));
    auto* remove = new QPushButton(QStringLiteral("删除选中"), &dialog);
    auto* reset = new QPushButton(QStringLiteral("恢复红/白/蓝"), &dialog);
    QObject::connect(add, &QPushButton::clicked, &dialog, [append] { append({QStringLiteral("#00ff00"), 32}); });
    QObject::connect(remove, &QPushButton::clicked, &dialog, [table] {
        if (table->currentRow() >= 0) { table->removeRow(table->currentRow()); }
    });
    QObject::connect(reset, &QPushButton::clicked, &dialog, [table, append] {
        table->setRowCount(0);
        for (const auto& target : defaultPixelColorTargets()) { append(target); }
    });
    actions->addWidget(add);
    actions->addWidget(remove);
    actions->addStretch();
    actions->addWidget(reset);
    layout->addLayout(actions);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("保存"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    QList<PixelColorTarget> result;
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        result.clear();
        for (int row = 0; row < table->rowCount(); ++row) {
            const auto target = static_cast<PixelColorEditor*>(table->cellWidget(row, 0))->target();
            if (!target || target->color.isEmpty()) {
                QMessageBox::warning(&dialog, QStringLiteral("颜色无效"),
                                     QStringLiteral("请选择预设颜色或输入有效的 #RRGGBB 色值。"));
                return;
            }
            result.append(*target);
        }
        if (result.isEmpty()) {
            QMessageBox::warning(&dialog, QStringLiteral("请选择颜色"), QStringLiteral("至少保留一种统计颜色。"));
            return;
        }
        dialog.accept();
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) { return std::nullopt; }
    return normalizedPixelColorTargets(result);
}
