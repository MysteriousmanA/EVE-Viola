#include "features/replica/pixel_statistics.h"

QString pixelColorCategoryName(PixelColorCategory category)
{
    switch (category) {
    case PixelColorCategory::Red:
        return QStringLiteral("红色");
    case PixelColorCategory::White:
        return QStringLiteral("白色");
    }
    return {};
}

QStringList pixelColorCategoryNames()
{
    return {
        pixelColorCategoryName(PixelColorCategory::Red),
        pixelColorCategoryName(PixelColorCategory::White),
    };
}
