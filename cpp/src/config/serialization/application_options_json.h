#pragma once
#include "config/config_types.h"
#include <QJsonObject>
namespace ConfigSerialization
{
void parseApplicationOptions(const QJsonObject& object, AppConfig& config);
QJsonObject applicationOptionsToJson(const AppConfig& config);
} // namespace ConfigSerialization
