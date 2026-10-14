#pragma once

#include <optional>

#include <QImage>
#include <QRect>

// Pixel computation boundary: input pixels/region, output counts.
// Keep capture scheduling, rule evaluation and UI labels outside this module.
struct PixelStatistics {
    qsizetype pixelCount{};
    qsizetype redCount{};
    qsizetype whiteCount{};
};

// The borrowed-buffer overload accepts RGB32/ARGB32 rows with caller-owned storage.
// The QImage overload also converts other formats before scanning.
[[nodiscard]] bool isRedPixelRgb(QRgb pixel) noexcept;

[[nodiscard]] PixelStatistics calculatePixelStatistics(
    const QImage& image,
    std::optional<QRect> region = std::nullopt);

[[nodiscard]] PixelStatistics calculatePixelStatistics(
    const uchar* pixels,
    int width,
    int height,
    qsizetype bytesPerLine,
    std::optional<QRect> region = std::nullopt);
