#include "features/color_rule/trigger_condition.h"
#include <QRegularExpression>

bool TriggerCondition::isEmpty() const noexcept
{
    return lowerOperator.isEmpty() && upperOperator.isEmpty();
}

QString TriggerCondition::toString() const
{
    QString text;
    if (!lowerOperator.isEmpty()) {
        text += lowerOperator + QString::number(lowerValue);
    }
    if (!upperOperator.isEmpty()) {
        if (!text.isEmpty()) {
            text += QStringLiteral("&");
        }
        text += upperOperator + QString::number(upperValue);
    }
    return text;
}

std::optional<TriggerCondition> parseTriggerCondition(const QString& text)
{
    QString normalized = text;
    normalized.remove(QLatin1Char(' '));
    normalized.replace(QStringLiteral("≥"), QStringLiteral(">="));
    normalized.replace(QStringLiteral("≤"), QStringLiteral("<="));
    normalized.replace(QStringLiteral("＞"), QStringLiteral(">"));
    normalized.replace(QStringLiteral("＜"), QStringLiteral("<"));
    normalized.replace(QStringLiteral("并且"), QStringLiteral("&"));
    normalized.replace(QStringLiteral("且"), QStringLiteral("&"));
    if (normalized.isEmpty()) {
        return TriggerCondition{};
    }

    const auto parseBound = [](const QString& part) -> std::optional<std::pair<QString, double>> {
        static const QRegularExpression pattern(
            QStringLiteral("^([><]=?)([0-9]+(?:\\.[0-9]+)?)%?$"));
        const auto match = pattern.match(part);
        if (!match.hasMatch()) {
            return std::nullopt;
        }
        const double value = match.captured(2).toDouble();
        if (value < kColorRulePixelValueMinimum || value > kColorRulePixelValueMaximum) {
            return std::nullopt;
        }
        return std::make_pair(match.captured(1), value);
    };

    const QStringList parts = normalized.split(QLatin1Char('&'));
    if (parts.size() > 2) {
        return std::nullopt;
    }
    if (parts.size() == 1) {
        const auto bound = parseBound(parts[0]);
        if (!bound.has_value()) {
            return std::nullopt;
        }
        TriggerCondition condition;
        if (bound->first == QStringLiteral(">") || bound->first == QStringLiteral(">=")) {
            condition.lowerOperator = bound->first;
            condition.lowerValue = bound->second;
        } else {
            condition.upperOperator = bound->first;
            condition.upperValue = bound->second;
        }
        return condition;
    }

    const auto lower = parseBound(parts[0]);
    const auto upper = parseBound(parts[1]);
    if (!lower.has_value() || !upper.has_value() ||
        (lower->first != QStringLiteral(">") && lower->first != QStringLiteral(">=")) ||
        (upper->first != QStringLiteral("<") && upper->first != QStringLiteral("<="))) {
        return std::nullopt;
    }
    TriggerCondition condition;
    condition.lowerOperator = lower->first;
    condition.lowerValue = lower->second;
    condition.upperOperator = upper->first;
    condition.upperValue = upper->second;
    return condition;
}

bool evaluateTriggerCondition(const TriggerCondition& condition, double value)
{
    if (condition.isEmpty()) {
        return false;
    }
    const bool lowerOk =
        condition.lowerOperator.isEmpty() ||
        (condition.lowerOperator == QStringLiteral(">=") ? value >= condition.lowerValue
                                                         : value > condition.lowerValue);
    const bool upperOk =
        condition.upperOperator.isEmpty() ||
        (condition.upperOperator == QStringLiteral("<=") ? value <= condition.upperValue
                                                         : value < condition.upperValue);
    return lowerOk && upperOk;
}
