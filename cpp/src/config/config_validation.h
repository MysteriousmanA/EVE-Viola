#pragma once

#include "config/config_types.h"

[[nodiscard]] bool isValidAutomaticActivationCooldownMs(int milliseconds);
[[nodiscard]] bool isValidAutomaticActivationMouseQuietPeriodMs(int milliseconds);
[[nodiscard]] bool isValidStatisticsIntervalMs(int milliseconds);
[[nodiscard]] QString statisticsIntervalLabel(int milliseconds);
