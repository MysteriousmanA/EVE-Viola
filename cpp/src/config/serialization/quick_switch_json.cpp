#include "config/serialization/quick_switch_json.h"
#include <algorithm>
#include <QSet>
#include "features/quick_switch/quick_switch_keys.h"

namespace ConfigSerialization
{
namespace
{
QStringList parseQuickSwitchKeys(const QJsonArray& values)
{
    QStringList keys;
    for (const auto& value : values) {
        if (!value.isString()) {
            continue;
        }
        const QString key = normalizeQuickSwitchKey(value.toString());
        if (!key.isEmpty() && !keys.contains(key)) {
            keys.append(key);
        }
    }
    return keys;
}
QJsonArray quickSwitchKeysToJson(const QStringList& keys)
{
    QJsonArray values;
    for (const QString& key : keys) {
        values.append(key);
    }
    return values;
}
} // namespace

QList<RadialAccountConfig> parseRadialAccounts(const QJsonArray& array)
{
    QList<RadialAccountConfig> result;
    QSet<QString> seenTitles;
    for (const QJsonValue& value : array) {
        const QJsonObject object = value.toObject();
        const QString title = object.value(QStringLiteral("windowTitle")).toString().trimmed();
        if (title.isEmpty() || seenTitles.contains(title)) {
            continue;
        }
        seenTitles.insert(title);
        result.append(RadialAccountConfig{
            title,
            object.value(QStringLiteral("nickname")).toString().trimmed(),
            qMax(1, object.value(QStringLiteral("position")).toInt(result.size() + 1)),
        });
    }
    std::stable_sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
        return left.position < right.position;
    });
    for (qsizetype index = 0; index < result.size(); ++index) {
        result[index].position = static_cast<int>(index + 1);
    }
    return result;
}

QJsonArray radialAccountsToJson(const QList<RadialAccountConfig>& configs)
{
    QList<RadialAccountConfig> normalized;
    QSet<QString> seenTitles;
    for (const auto& account : configs) {
        const QString title = account.windowTitle.trimmed();
        if (title.isEmpty() || seenTitles.contains(title)) {
            continue;
        }
        seenTitles.insert(title);
        normalized.append(RadialAccountConfig{
            title,
            account.nickname.trimmed(),
            qMax(1, account.position),
        });
    }
    std::stable_sort(normalized.begin(), normalized.end(), [](const auto& left, const auto& right) {
        return left.position < right.position;
    });

    QJsonArray result;
    for (qsizetype index = 0; index < normalized.size(); ++index) {
        const auto& account = normalized[index];
        result.append(QJsonObject{
            {QStringLiteral("windowTitle"), account.windowTitle},
            {QStringLiteral("nickname"), account.nickname},
            {QStringLiteral("position"), static_cast<int>(index + 1)},
        });
    }
    return result;
}
void parseQuickSwitchConfig(const QJsonObject& object, AppConfig& config)
{
    const QJsonValue previousClientKeysValue = object.value(QStringLiteral("previousClientKeys"));
    const QJsonValue nextClientKeysValue = object.value(QStringLiteral("nextClientKeys"));
    if (previousClientKeysValue.isArray() && nextClientKeysValue.isArray()) {
        config.previousClientKeys = parseQuickSwitchKeys(previousClientKeysValue.toArray());
        config.nextClientKeys = parseQuickSwitchKeys(nextClientKeysValue.toArray());
        config.nextClientKeys =
            normalizeQuickSwitchKeys(config.nextClientKeys, config.previousClientKeys);
    } else {
        const QString previousClientKey =
            object.value(QStringLiteral("previousClientKey")).toString();
        const QString nextClientKey = object.value(QStringLiteral("nextClientKey")).toString();
        if (isValidQuickSwitchKey(previousClientKey) && isValidQuickSwitchKey(nextClientKey) &&
            previousClientKey != nextClientKey) {
            config.previousClientKeys = {previousClientKey};
            config.nextClientKeys = {nextClientKey};
        }
    }
    const QString radialSwitchKey =
        normalizeQuickSwitchKey(object.value(QStringLiteral("radialSwitchKey")).toString());
    config.radialSwitchKey = quickSwitchKeyboardInput(radialSwitchKey).has_value()
                                 ? radialSwitchKey
                                 : kDefaultRadialSwitchKey;
    config.radialAccounts =
        parseRadialAccounts(object.value(QStringLiteral("radialAccounts")).toArray());
}
QJsonObject quickSwitchConfigToJson(const AppConfig& config)
{
    const QStringList previousClientKeys = normalizeQuickSwitchKeys(config.previousClientKeys);
    const QStringList nextClientKeys =
        normalizeQuickSwitchKeys(config.nextClientKeys, previousClientKeys);
    const QString normalizedRadialSwitchKey = normalizeQuickSwitchKey(config.radialSwitchKey);
    const QString radialSwitchKey = quickSwitchKeyboardInput(normalizedRadialSwitchKey).has_value()
                                        ? normalizedRadialSwitchKey
                                        : kDefaultRadialSwitchKey;
    return {
        {QStringLiteral("previousClientKeys"), quickSwitchKeysToJson(previousClientKeys)},
        {QStringLiteral("nextClientKeys"), quickSwitchKeysToJson(nextClientKeys)},
        {QStringLiteral("radialSwitchKey"), radialSwitchKey},
        {QStringLiteral("radialAccounts"), radialAccountsToJson(config.radialAccounts)},
    };
}
} // namespace ConfigSerialization
