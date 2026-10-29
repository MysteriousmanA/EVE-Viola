#include "features/replica/pixel_statistics_calculator.h"

#include <algorithm>

// All pixel classification thresholds, region clipping and counting live here.
// Changing the algorithm does not require changing the capture pipeline or UI.
namespace {

bool isWhitePixel(int red, int green, int blue)
{
    const int brightest = std::max({red, green, blue});
    const int darkest = std::min({red, green, blue});
    return darkest >= 180 && brightest - darkest <= 40;
}

} // namespace

bool isRedPixelRgb(QRgb pixel) noexcept
{
    const int red = qRed(pixel);
    const int green = qGreen(pixel);
    const int blue = qBlue(pixel);
    const int maximum = std::max({red, green, blue});
    const int minimum = std::min({red, green, blue});
    const int delta = maximum - minimum;
    if (red != maximum || maximum < 64) {
        return false;
    }

    const int saturationLeft = 100 * delta;
    const int saturationRight = 45 * maximum;
    if (saturationLeft < saturationRight) {
        return false;
    }
    if (saturationLeft == saturationRight) {
        const double r = red / 255.0;
        const double minimumValue = minimum / 255.0;
        if ((r - minimumValue) / r < 0.45) {
            return false;
        }
    }

    const int hueLeft = 4 * std::abs(green - blue);
    if (hueLeft > delta) {
        return false;
    }
    if (hueLeft == delta) {
        const double r = red / 255.0;
        const double g = green / 255.0;
        const double b = blue / 255.0;
        const double range = r - minimum / 255.0;
        double hue = 60.0 * ((g - b) / range);
        if (hue < 0.0) {
            hue += 360.0;
        }
        return hue <= 15.0 || hue >= 345.0;
    }
    return true;
}

PixelStatistics calculatePixelStatistics(
    const QImage& image,
    std::optional<QRect> region,
    const QList<PixelColorTarget>& targets)
{
    if (image.isNull()) {
        return {};
    }

    const QRect bounds(QPoint(0, 0), image.size());
    const QRect sampleRegion = region.has_value()
        ? region->normalized().intersected(bounds)
        : bounds;
    if (sampleRegion.isEmpty()) {
        return {};
    }

    QImage pixels = image;
    if (pixels.format() != QImage::Format_RGB32
        && pixels.format() != QImage::Format_ARGB32) {
        pixels = pixels.convertToFormat(QImage::Format_RGB32);
    }

    return calculatePixelStatistics(
        pixels.constBits(),
        pixels.width(),
        pixels.height(),
        pixels.bytesPerLine(),
        region,
        targets);
}

PixelStatistics calculatePixelStatistics(
    const uchar* pixels,
    int width,
    int height,
    qsizetype bytesPerLine,
    std::optional<QRect> region,
    const QList<PixelColorTarget>& targets)
{
    if (pixels == nullptr || width <= 0 || height <= 0
        || bytesPerLine < static_cast<qsizetype>(width) * 4) {
        return {};
    }
    return calculatePlannedPixelStatistics(
        pixels, width, height, bytesPerLine, region, compilePixelStatisticsPlan(targets));
}

PixelStatisticsPlan compilePixelStatisticsPlan(const QList<PixelColorTarget>& targets)
{
    PixelStatisticsPlan plan;
    for (const auto& target : normalizedPixelColorTargets(targets)) {
        if (target.tolerance < 0) { continue; }
        const auto value = pixelColorValue(target.color);
        const int tolerance = target.tolerance;
        plan.matchers.append({target,
            value.red() - tolerance, value.red() + tolerance,
            value.green() - tolerance, value.green() + tolerance,
            value.blue() - tolerance, value.blue() + tolerance});
    }
    return plan;
}

PixelStatistics calculatePlannedPixelStatistics(
    const uchar* pixels, int width, int height, qsizetype bytesPerLine,
    std::optional<QRect> region, const PixelStatisticsPlan& plan)
{
    if (pixels == nullptr || width <= 0 || height <= 0
        || bytesPerLine < static_cast<qsizetype>(width) * 4) {
        return {};
    }

    const QRect bounds(0, 0, width, height);
    const QRect sampleRegion = region.has_value()
        ? region->normalized().intersected(bounds)
        : bounds;
    if (sampleRegion.isEmpty()) {
        return {};
    }

    qsizetype redCount = 0;
    qsizetype whiteCount = 0;
    qsizetype blueCount = 0;
    constexpr int eveBlueRed = 48;
    constexpr int eveBlueGreen = 79;
    constexpr int eveBlueBlue = 91;
    // Keep the muted EVE blue distinct from neutral gray and pure RGB blue.
    constexpr int eveBlueTolerance = 16;
    const auto& matchers = plan.matchers;
    QList<PixelColorCount> colors;
    colors.reserve(matchers.size());
    for (const auto& matcher : matchers) {
        colors.append({matcher.target, 0});
    }

    for (int y = sampleRegion.top(); y <= sampleRegion.bottom(); ++y) {
        const auto* row = reinterpret_cast<const QRgb*>(
            pixels + static_cast<qsizetype>(y) * bytesPerLine);
        for (int x = sampleRegion.left(); x <= sampleRegion.right(); ++x) {
            const int red = qRed(row[x]);
            const int green = qGreen(row[x]);
            const int blue = qBlue(row[x]);
            if (isRedPixelRgb(row[x])) {
                ++redCount;
            }
            if (isWhitePixel(red, green, blue)) {
                ++whiteCount;
            }
            if (std::abs(red - eveBlueRed) <= eveBlueTolerance
                && std::abs(green - eveBlueGreen) <= eveBlueTolerance
                && std::abs(blue - eveBlueBlue) <= eveBlueTolerance) {
                ++blueCount;
            }
            for (qsizetype index = 0; index < matchers.size(); ++index) {
                const auto& match = matchers[index];
                if (red >= match.redMin && red <= match.redMax
                    && green >= match.greenMin && green <= match.greenMax
                    && blue >= match.blueMin && blue <= match.blueMax) {
                    ++colors[index].count;
                }
            }
        }
    }

    const qsizetype pixelCount =
        static_cast<qsizetype>(sampleRegion.width()) * sampleRegion.height();
    return PixelStatistics{
        pixelCount,
        redCount,
        whiteCount,
        blueCount,
        std::move(colors),
    };
}
