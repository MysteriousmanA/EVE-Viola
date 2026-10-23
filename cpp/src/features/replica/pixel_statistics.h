#pragma once

#include "features/replica/pixel_statistics_calculator.h"

#include <QString>
#include <QStringList>

// Preset names; target normalization and RGB matching live behind the calculator API.
enum class PixelColorCategory { Red, White, Blue };

[[nodiscard]] QString pixelColorCategoryName(PixelColorCategory category);
[[nodiscard]] QStringList pixelColorCategoryNames();
