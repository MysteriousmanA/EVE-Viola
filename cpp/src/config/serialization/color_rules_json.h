#pragma once

#include "config/config_types.h"
#include <QJsonArray>
#include <QJsonObject>

namespace ConfigSerialization
{
QJsonObject colorRuleToJson(const ColorRule& rule);
[[nodiscard]] QList<ColorRule> parseColorRules(const QJsonArray& array, int* nextNumber);
QJsonObject colorRulesSectionToJson(const QList<ColorRule>& rules, int nextNumber);
} // namespace ConfigSerialization
