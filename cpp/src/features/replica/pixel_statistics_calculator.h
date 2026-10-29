#pragma once

#include <optional>

#include <QColor>
#include <QImage>
#include <QList>
#include <QMetaType>
#include <QRect>
#include <QString>

struct PixelColorTarget {
    QString color;
    // -1 keeps the built-in red/white/blue classification; custom colors use 32.
    // 0..255 is the maximum absolute difference in each RGB channel.
    int tolerance{-1};

    bool operator==(const PixelColorTarget&) const = default;
};

[[nodiscard]] QList<PixelColorTarget> defaultPixelColorTargets();
[[nodiscard]] QColor pixelColorValue(const QString& color);
[[nodiscard]] QList<PixelColorTarget> normalizedPixelColorTargets(
    const QList<PixelColorTarget>& targets);

struct PixelColorCount {
    PixelColorTarget target;
    qsizetype count{};
};

// Pixel computation boundary: input pixels/region, output counts.
// Keep capture scheduling, rule evaluation and UI labels outside this module.
struct PixelStatistics {
    qsizetype pixelCount{};
    qsizetype redCount{};
    qsizetype whiteCount{};
    qsizetype blueCount{};
    QList<PixelColorCount> colors;

    [[nodiscard]] qsizetype count(const PixelColorTarget& target) const;
};

Q_DECLARE_METATYPE(PixelStatistics)

// Immutable once compiled; safe to share between capture workers and scans.
struct PixelStatisticsPlan {
    struct Matcher {
        PixelColorTarget target;
        int redMin, redMax, greenMin, greenMax, blueMin, blueMax;
    };
    QList<Matcher> matchers;
};

[[nodiscard]] PixelStatisticsPlan compilePixelStatisticsPlan(
    const QList<PixelColorTarget>& targets);

[[nodiscard]] PixelStatistics calculatePlannedPixelStatistics(
    const uchar* pixels, int width, int height, qsizetype bytesPerLine,
    std::optional<QRect> region, const PixelStatisticsPlan& plan);

// The borrowed-buffer overload accepts RGB32/ARGB32 rows with caller-owned storage.
// The QImage overload also converts other formats before scanning.
[[nodiscard]] bool isRedPixelRgb(QRgb pixel) noexcept;

[[nodiscard]] PixelStatistics calculatePixelStatistics(
    const QImage& image,
    std::optional<QRect> region = std::nullopt,
    const QList<PixelColorTarget>& targets = {});

[[nodiscard]] PixelStatistics calculatePixelStatistics(
    const uchar* pixels,
    int width,
    int height,
    qsizetype bytesPerLine,
    std::optional<QRect> region = std::nullopt,
    const QList<PixelColorTarget>& targets = {});
