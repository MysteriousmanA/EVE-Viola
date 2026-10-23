#include "features/replica/pixel_statistics.h"

QString pixelColorCategoryName(PixelColorCategory category)
{
    switch (category) {
    case PixelColorCategory::Red:
        return QStringLiteral("红色");
    case PixelColorCategory::White:
        return QStringLiteral("白色");
    case PixelColorCategory::Blue:
        return QStringLiteral("蓝色");
    }
    return {};
}

QStringList pixelColorCategoryNames()
{
    return {
        pixelColorCategoryName(PixelColorCategory::Red),
        pixelColorCategoryName(PixelColorCategory::White),
        pixelColorCategoryName(PixelColorCategory::Blue),
    };
}

QList<PixelColorTarget> defaultPixelColorTargets()
{
    return {{QStringLiteral("红色")}, {QStringLiteral("白色")}, {QStringLiteral("蓝色")}};
}

QColor pixelColorValue(const QString& color)
{
    if (color == QStringLiteral("红色")) { return QColor(Qt::red); }
    if (color == QStringLiteral("白色")) { return QColor(Qt::white); }
    // Reference sampled from the EVE UI swatch supplied by the user.
    if (color == QStringLiteral("蓝色")) { return QColor(48, 79, 91); }
    // Persist custom colors as RGB only; alpha is not part of pixel matching.
    if (color.size() != 7 || !color.startsWith(QLatin1Char('#'))) { return {}; }
    return QColor(color);
}

QList<PixelColorTarget> normalizedPixelColorTargets(const QList<PixelColorTarget>& targets)
{
    QList<PixelColorTarget> result;
    const auto presets = pixelColorCategoryNames();
    for (auto target : targets) {
        target.color = target.color.trimmed();
        const auto value = pixelColorValue(target.color);
        if (!value.isValid()) { continue; }
        target.tolerance = qBound(-1, target.tolerance, 255);
        if (!presets.contains(target.color)) {
            target.color = value.name();
            if (target.tolerance < 0) { target.tolerance = 32; }
        }
        if (!result.contains(target)) { result.append(target); }
    }
    return result.isEmpty() ? defaultPixelColorTargets() : result;
}

qsizetype PixelStatistics::count(const PixelColorTarget& target) const
{
    if (target.tolerance < 0) {
        if (target.color == QStringLiteral("红色")) { return redCount; }
        if (target.color == QStringLiteral("白色")) { return whiteCount; }
        if (target.color == QStringLiteral("蓝色")) { return blueCount; }
    }
    const auto normalized = normalizedPixelColorTargets({target});
    if (!pixelColorValue(target.color.trimmed()).isValid()) { return -1; }
    for (const auto& entry : colors) {
        if (entry.target == normalized.first()) { return entry.count; }
    }
    return -1;
}
