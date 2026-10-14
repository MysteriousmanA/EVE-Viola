#include "features/quick_switch/quick_switch_keys.h"

QString quickSwitchKeyboardKey(int virtualKey, bool extended)
{
    if (virtualKey <= 0 || virtualKey >= 0xFF) {
        return {};
    }
    if (virtualKey == 0x21 && extended) {
        return QStringLiteral("PageUp");
    }
    if (virtualKey == 0x22 && extended) {
        return QStringLiteral("PageDown");
    }
    return QStringLiteral("KeyboardVK:%1:%2")
        .arg(virtualKey, 2, 16, QLatin1Char('0'))
        .arg(extended ? 1 : 0);
}

std::optional<QuickSwitchKeyboardInput> quickSwitchKeyboardInput(const QString& key)
{
    if (key == QStringLiteral("PageUp")) {
        return QuickSwitchKeyboardInput{0x21, true};
    }
    if (key == QStringLiteral("PageDown")) {
        return QuickSwitchKeyboardInput{0x22, true};
    }
    if (!key.startsWith(QStringLiteral("KeyboardVK:"), Qt::CaseInsensitive)) {
        return std::nullopt;
    }

    const QStringList parts = key.split(QLatin1Char(':'));
    if (parts.size() != 3) {
        return std::nullopt;
    }
    bool virtualKeyValid = false;
    bool extendedValid = false;
    const int virtualKey = parts[1].toInt(&virtualKeyValid, 16);
    const int extended = parts[2].toInt(&extendedValid);
    if (!virtualKeyValid || !extendedValid || virtualKey <= 0 || virtualKey >= 0xFF ||
        (extended != 0 && extended != 1)) {
        return std::nullopt;
    }
    return QuickSwitchKeyboardInput{virtualKey, extended != 0};
}

QString normalizeQuickSwitchKey(const QString& key)
{
    if (key == QStringLiteral("Mouse4") || key == QStringLiteral("Mouse5")) {
        return key;
    }
    const auto input = quickSwitchKeyboardInput(key);
    return input.has_value() ? quickSwitchKeyboardKey(input->virtualKey, input->extended)
                             : QString{};
}

bool isValidQuickSwitchKey(const QString& key)
{
    return !normalizeQuickSwitchKey(key).isEmpty();
}

QStringList normalizeQuickSwitchKeys(const QStringList& values, const QStringList& excluded)
{
    QStringList keys;
    for (const QString& key : values) {
        const QString normalized = normalizeQuickSwitchKey(key);
        if (!normalized.isEmpty() && !keys.contains(normalized) && !excluded.contains(normalized)) {
            keys.append(normalized);
        }
    }
    return keys;
}
