#include "ui/replica_row.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QRegularExpression>
#include <QSignalBlocker>
#include "ui/replica_row/scroll_passthrough_controls.h"

using namespace ReplicaRowUi;

namespace {

std::optional<QPoint> parsePositionText(const QString& text)
{
    static const QRegularExpression positionFormat(
        QStringLiteral("^\\s*(-?\\d+)\\s*[,\\s]\\s*(-?\\d+)\\s*$"));
    const auto match = positionFormat.match(text.trimmed());
    if (!match.hasMatch()) {
        return std::nullopt;
    }
    return QPoint(match.captured(1).toInt(), match.captured(2).toInt());
}

QString formatPosition(const QPoint& pos)
{
    return QStringLiteral("%1,%2").arg(pos.x()).arg(pos.y());
}

} // namespace

void ReplicaRow::initializeGeometryEditor()
{
    positionEdit_ = new QLineEdit(QStringLiteral("0,0"));
    positionEdit_->setObjectName(QStringLiteral("windowPosition"));
    positionEdit_->setToolTip(QStringLiteral("小窗在屏幕上的当前位置，可输入 x,y 来移动"));
    connect(positionEdit_, &QLineEdit::editingFinished, this, [this] {
        const auto pos = parsePositionText(positionEdit_->text());
        if (pos.has_value()) {
            emit positionEditRequested(*pos);
        } else {
            positionEdit_->setText(formatPosition(lastPosition_));
        }
    });

    sizeEdit_ = new QLineEdit(QStringLiteral("360×240"));
    sizeEdit_->setObjectName(QStringLiteral("windowSize"));
    sizeEdit_->setReadOnly(true);
    sizeEdit_->setToolTip(QStringLiteral("小窗当前宽度和高度"));
    scaleEdit_ = new ScrollPassthroughDoubleSpinBox;
    scaleEdit_->setObjectName(QStringLiteral("windowScale"));
    scaleEdit_->setRange(0.0, 1.0e12);
    scaleEdit_->setDecimals(2);
    scaleEdit_->setSingleStep(5.0);
    scaleEdit_->setSuffix(QStringLiteral("%"));
    scaleEdit_->setSpecialValueText(QStringLiteral("--%"));
    scaleEdit_->setButtonSymbols(QAbstractSpinBox::NoButtons);
    scaleEdit_->setKeyboardTracking(false);
    scaleEdit_->setEnabled(false);
    scaleEdit_->setToolTip(
        QStringLiteral("输入百分比调整小窗尺寸；100% 表示截取区域的原始尺寸"));
    if (auto* scaleLineEdit = scaleEdit_->findChild<QLineEdit*>()) {
        connect(scaleLineEdit, &QLineEdit::textEdited, this, [this] {
            scaleEditHasUserInput_ = true;
        });
    }
    connect(scaleEdit_, &QDoubleSpinBox::editingFinished, this, [this] {
        if (!scaleEditHasUserInput_) {
            refreshWindowScale(true);
            return;
        }
        scaleEditHasUserInput_ = false;
        if (!currentRegion_.isValid() || currentRegion_.isEmpty()
            || !scaleEdit_->hasAcceptableInput() || scaleEdit_->value() <= 0.0) {
            refreshWindowScale(true);
            return;
        }

        const double currentScale = std::min(
            static_cast<double>(currentWindowSize_.width()) / currentRegion_.width(),
            static_cast<double>(currentWindowSize_.height()) / currentRegion_.height())
            * 100.0;
        const double requestedScale = scaleEdit_->value();
        if (std::abs(requestedScale - currentScale) >= 0.005) {
            emit scalePercentageEdited(requestedScale);
        }
        refreshWindowScale(true);
    });
}

void ReplicaRow::setWindowPosition(const QPoint& pos)
{
    lastPosition_ = pos;
    positionEdit_->setText(formatPosition(pos));
}

void ReplicaRow::setWindowSize(const QSize& size)
{
    currentWindowSize_ = size;
    sizeEdit_->setText(
        QStringLiteral("%1×%2").arg(size.width()).arg(size.height()));
    refreshWindowScale();
}

void ReplicaRow::refreshWindowScale(bool force)
{
    if (!currentRegion_.isValid() || currentRegion_.isEmpty()
        || !currentWindowSize_.isValid() || currentWindowSize_.isEmpty()) {
        scaleEditHasUserInput_ = false;
        const QSignalBlocker blocker(scaleEdit_);
        scaleEdit_->setSpecialValueText(QStringLiteral("--%"));
        scaleEdit_->setValue(0.0);
        scaleEdit_->setEnabled(false);
        return;
    }

    scaleEdit_->setEnabled(true);
    if (scaleEdit_->hasFocus() && !force) {
        return;
    }

    const QSignalBlocker blocker(scaleEdit_);
    scaleEdit_->setSpecialValueText(QString{});
    const double widthScale = static_cast<double>(currentWindowSize_.width())
        / currentRegion_.width();
    const double heightScale = static_cast<double>(currentWindowSize_.height())
        / currentRegion_.height();
    scaleEdit_->setValue(std::min(widthScale, heightScale) * 100.0);
}
