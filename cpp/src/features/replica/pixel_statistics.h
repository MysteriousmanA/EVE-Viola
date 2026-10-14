#pragma once

#include "features/replica/pixel_statistics_calculator.h"

#include <QString>
#include <QStringList>

// Presentation metadata; existing includes keep access to the calculator API.
enum class PixelColorCategory { Red, White };

[[nodiscard]] QString pixelColorCategoryName(PixelColorCategory category);
[[nodiscard]] QStringList pixelColorCategoryNames();
