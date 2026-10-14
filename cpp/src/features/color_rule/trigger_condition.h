#pragma once

#include "config/config_types.h"

[[nodiscard]] std::optional<TriggerCondition> parseTriggerCondition(const QString& text);
[[nodiscard]] bool evaluateTriggerCondition(const TriggerCondition& condition, double value);
