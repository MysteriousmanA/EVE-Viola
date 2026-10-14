#include "config/config_validation.h"

bool isValidAutomaticActivationCooldownMs(int milliseconds)
{
    return milliseconds >= kAutomaticActivationCooldownMinimumMs &&
           milliseconds <= kAutomaticActivationCooldownMaximumMs &&
           milliseconds % kAutomaticActivationCooldownStepMs == 0;
}

bool isValidAutomaticActivationMouseQuietPeriodMs(int milliseconds)
{
    return milliseconds >= kAutomaticActivationMouseQuietPeriodMinimumMs &&
           milliseconds <= kAutomaticActivationMouseQuietPeriodMaximumMs &&
           milliseconds % kAutomaticActivationMouseQuietPeriodStepMs == 0;
}

bool isValidStatisticsIntervalMs(int milliseconds)
{
    return milliseconds == kStatisticsIntervalDisabledMs ||
           (milliseconds >= kStatisticsIntervalMinimumMs &&
            milliseconds <= kStatisticsIntervalMaximumMs &&
            milliseconds % kStatisticsIntervalStepMs == 0);
}

QString statisticsIntervalLabel(int milliseconds)
{
    if (milliseconds <= 0) {
        return QStringLiteral("不统计");
    }
    return QStringLiteral("%1 s").arg(milliseconds / 1000.0, 0, 'f', 1);
}
