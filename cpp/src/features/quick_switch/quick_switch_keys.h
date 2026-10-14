#pragma once

#include "config/config_types.h"

[[nodiscard]] QString quickSwitchKeyboardKey(int virtualKey, bool extended);
[[nodiscard]] std::optional<QuickSwitchKeyboardInput> quickSwitchKeyboardInput(const QString& key);
[[nodiscard]] QString normalizeQuickSwitchKey(const QString& key);
[[nodiscard]] bool isValidQuickSwitchKey(const QString& key);
[[nodiscard]] QStringList normalizeQuickSwitchKeys(const QStringList& values,
                                                   const QStringList& excluded = {});
