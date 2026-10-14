#include "config/serialization/application_options_json.h"
#include "config/config_validation.h"
namespace ConfigSerialization
{
void parseApplicationOptions(const QJsonObject& object, AppConfig& config)
{
    config.defaultPresetSlot =
        qBound(0, object.value(QStringLiteral("defaultPresetSlot")).toInt(0), kPresetCount);
    config.restoreWindowsWithGame =
        object.value(QStringLiteral("restoreWindowsWithGame")).toBool(false);
    config.keepInTray = object.value(QStringLiteral("keepInTray")).toBool(false);
    config.raiseReplicaOnClick = object.value(QStringLiteral("raiseReplicaOnClick")).toBool(true);
    config.lockReplicaSize = object.value(QStringLiteral("lockReplicaSize")).toBool(false);
    config.activateSourceOnColorLogicTrigger =
        object.value(QStringLiteral("activateSourceOnColorLogicTrigger")).toBool(true);
    config.automaticActivationCooldownMs =
        object.value(QStringLiteral("automaticActivationCooldownMs"))
            .toInt(kDefaultAutomaticActivationCooldownMs);
    if (!isValidAutomaticActivationCooldownMs(config.automaticActivationCooldownMs)) {
        config.automaticActivationCooldownMs = kDefaultAutomaticActivationCooldownMs;
    }
    config.automaticActivationMouseQuietPeriodMs =
        object.value(QStringLiteral("automaticActivationMouseQuietPeriodMs"))
            .toInt(kDefaultAutomaticActivationMouseQuietPeriodMs);
    if (!isValidAutomaticActivationMouseQuietPeriodMs(
            config.automaticActivationMouseQuietPeriodMs)) {
        config.automaticActivationMouseQuietPeriodMs =
            kDefaultAutomaticActivationMouseQuietPeriodMs;
    }
}
QJsonObject applicationOptionsToJson(const AppConfig& config)
{
    return {
        {QStringLiteral("defaultPresetSlot"), config.defaultPresetSlot},
        {QStringLiteral("restoreWindowsWithGame"), config.restoreWindowsWithGame},
        {QStringLiteral("keepInTray"), config.keepInTray},
        {QStringLiteral("raiseReplicaOnClick"), config.raiseReplicaOnClick},
        {QStringLiteral("lockReplicaSize"), config.lockReplicaSize},
        {QStringLiteral("activateSourceOnColorLogicTrigger"),
         config.activateSourceOnColorLogicTrigger},
        {QStringLiteral("automaticActivationCooldownMs"),
         isValidAutomaticActivationCooldownMs(config.automaticActivationCooldownMs)
             ? config.automaticActivationCooldownMs
             : kDefaultAutomaticActivationCooldownMs},
        {QStringLiteral("automaticActivationMouseQuietPeriodMs"),
         isValidAutomaticActivationMouseQuietPeriodMs(config.automaticActivationMouseQuietPeriodMs)
             ? config.automaticActivationMouseQuietPeriodMs
             : kDefaultAutomaticActivationMouseQuietPeriodMs},
    };
}
} // namespace ConfigSerialization
