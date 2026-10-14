#include "ui/replica_row.h"

#include <limits>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include "ui/replica_row/scroll_passthrough_controls.h"

using namespace ReplicaRowUi;

namespace {

QRect parseRegionText(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    static const QRegularExpression displayFormat(
        QStringLiteral("^(\\d+)\\s*[xX×*]\\s*(\\d+)\\s*@\\s*\\((-?\\d+)\\s*,\\s*(-?\\d+)\\)$"));
    const auto displayMatch = displayFormat.match(trimmed);
    if (displayMatch.hasMatch()) {
        return QRect(
            displayMatch.captured(3).toInt(),
            displayMatch.captured(4).toInt(),
            displayMatch.captured(1).toInt(),
            displayMatch.captured(2).toInt());
    }

    static const QRegularExpression plainFormat(
        QStringLiteral("^\\s*(-?\\d+)\\s*[,\\s]\\s*(-?\\d+)\\s*[,\\s]\\s*(\\d+)\\s*[,\\s]\\s*(\\d+)\\s*$"));
    const auto plainMatch = plainFormat.match(trimmed);
    if (plainMatch.hasMatch()) {
        return QRect(
            plainMatch.captured(1).toInt(),
            plainMatch.captured(2).toInt(),
            plainMatch.captured(3).toInt(),
            plainMatch.captured(4).toInt());
    }
    return {};
}

inline constexpr int kPresetMatchTolerance = 2;

bool rectsNearlyEqual(const QRect& a, const QRect& b, int tolerance)
{
    return qAbs(a.x() - b.x()) <= tolerance
        && qAbs(a.y() - b.y()) <= tolerance
        && qAbs(a.width() - b.width()) <= tolerance
        && qAbs(a.height() - b.height()) <= tolerance;
}

} // namespace

void ReplicaRow::initializeRegionEditor()
{
    regionEdit_ = new QLineEdit(QStringLiteral("未截取"));
    regionEdit_->setObjectName(QStringLiteral("capturedRegion"));
    regionEdit_->setToolTip(QStringLiteral("截取区域范围，可输入 x,y,w,h 或 宽×高 @ (x,y)"));
    connect(regionEdit_, &QLineEdit::editingFinished, this, [this] {
        const QRect region = parseRegionText(regionEdit_->text());
        if (region.isValid() && !region.isEmpty()) {
            emit regionEditRequested(region);
        } else {
            regionEdit_->setText(lastRegionText_);
        }
    });

    regionRecaptureButton_ = new QPushButton(QStringLiteral("重截区域"));
    regionRecaptureButton_->setObjectName(QStringLiteral("recaptureRegion"));
    regionRecaptureButton_->setToolTip(
        QStringLiteral("在绑定的游戏窗口上重新框选截取区域"));
    connect(regionRecaptureButton_, &QPushButton::clicked,
        this, &ReplicaRow::regionRecaptureRequested);
}

void ReplicaRow::initializeRegionPresetControls()
{
    regionPresetCombo_ = new ScrollPassthroughCombo;
    regionPresetCombo_->setObjectName(QStringLiteral("currentRegionPreset"));
    regionPresetCombo_->setMinimumWidth(130);
    regionPresetCombo_->addItem(QStringLiteral("自定义"));
    regionPresetCombo_->setToolTip(QStringLiteral("当前区域预设：自定义或从区域预设列表选择"));
    connect(regionPresetCombo_, &QComboBox::activated, this, [this](int index) {
        if (index == 0) {
            setCurrentRegionPresetName(QString{});
            return;
        }
        if (index > 0 && index <= regionPresets_.size()) {
            setCurrentRegionPresetName(regionPresets_[index - 1].name);
            emit regionPresetSelected(regionPresets_[index - 1]);
        }
    });

    savePresetButton_ = new QPushButton(QStringLiteral("保存为新区域预设"));
    savePresetButton_->setObjectName(QStringLiteral("saveRegionPreset"));
    savePresetButton_->setEnabled(false);
    savePresetButton_->setToolTip(QStringLiteral("将当前框选区域保存为可复用预设"));
    connect(savePresetButton_, &QPushButton::clicked, this, &ReplicaRow::saveRegionPresetRequested);
}

void ReplicaRow::setCaptureRegion(const QString& description)
{
    const bool hasRegion = !description.isEmpty()
        && description != QStringLiteral("未截取")
        && description != QStringLiteral("未发现区域，已重置");
    lastRegionText_ = description.isEmpty() ? QStringLiteral("未截取") : description;
    regionEdit_->setText(lastRegionText_);
    savePresetButton_->setEnabled(hasRegion);
    if (!hasRegion) {
        setCurrentRegion(QRect{});
    }
}

void ReplicaRow::setRegionPresets(const QList<RegionPreset>& presets)
{
    const QString preferredName = currentRegionPresetName_;
    regionPresets_ = presets;
    syncRegionPresetCombo(preferredName);
}

void ReplicaRow::setCurrentRegion(const QRect& region)
{
    currentRegion_ = region;
    syncRegionPresetCombo();
    refreshWindowScale();
}

QString ReplicaRow::currentRegionPresetName() const noexcept
{
    return currentRegionPresetName_;
}

QString ReplicaRow::currentRegionFilterName() const
{
    if (!currentRegionPresetName_.isEmpty()) {
        return currentRegionPresetName_;
    }
    return currentRegion_.isValid() && !currentRegion_.isEmpty()
        ? QStringLiteral("自定义")
        : QStringLiteral("未截取");
}

void ReplicaRow::setCurrentRegionPresetName(const QString& name)
{
    const QSignalBlocker blocker(regionPresetCombo_);
    for (int index = 0; index < regionPresets_.size(); ++index) {
        if (regionPresets_[index].name.compare(name, Qt::CaseInsensitive) == 0) {
            currentRegionPresetName_ = regionPresets_[index].name;
            regionPresetCombo_->setCurrentIndex(index + 1);
            return;
        }
    }
    currentRegionPresetName_.clear();
    regionPresetCombo_->setCurrentIndex(0);
}

void ReplicaRow::syncRegionPresetCombo(const QString& preferredName)
{
    const QSignalBlocker blocker(regionPresetCombo_);
    regionPresetCombo_->clear();
    regionPresetCombo_->addItem(QStringLiteral("自定义"));
    for (const auto& preset : regionPresets_) {
        regionPresetCombo_->addItem(preset.name);
    }
    if (!preferredName.isEmpty()) {
        for (int index = 0; index < regionPresets_.size(); ++index) {
            if (regionPresets_[index].name.compare(
                    preferredName, Qt::CaseInsensitive) == 0) {
                currentRegionPresetName_ = regionPresets_[index].name;
                regionPresetCombo_->setCurrentIndex(index + 1);
                return;
            }
        }
    }
    if (currentRegion_.isValid() && !currentRegion_.isEmpty()) {
        int bestIndex = -1;
        int bestDistance = std::numeric_limits<int>::max();
        for (int index = 0; index < regionPresets_.size(); ++index) {
            const QRect& preset = regionPresets_[index].region;
            if (rectsNearlyEqual(preset, currentRegion_, 0)) {
                currentRegionPresetName_ = regionPresets_[index].name;
                regionPresetCombo_->setCurrentIndex(index + 1);
                return;
            }
            const int distance = qAbs(preset.x() - currentRegion_.x())
                + qAbs(preset.y() - currentRegion_.y())
                + qAbs(preset.width() - currentRegion_.width())
                + qAbs(preset.height() - currentRegion_.height());
            if (distance < bestDistance) {
                bestDistance = distance;
                bestIndex = index;
            }
        }
        if (bestIndex >= 0
            && rectsNearlyEqual(
                regionPresets_[bestIndex].region, currentRegion_, kPresetMatchTolerance)) {
            currentRegionPresetName_ = regionPresets_[bestIndex].name;
            regionPresetCombo_->setCurrentIndex(bestIndex + 1);
            return;
        }
    }
    currentRegionPresetName_.clear();
    regionPresetCombo_->setCurrentIndex(0);
}
