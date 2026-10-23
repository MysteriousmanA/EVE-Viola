#include <QtTest>

#include <algorithm>
#include <cmath>

#include <QImage>
#include <QElapsedTimer>
#include <QRandomGenerator>

#include "features/replica/pixel_statistics.h"

namespace {

bool referenceIsRed(QRgb pixel)
{
    const double red = qRed(pixel) / 255.0;
    const double green = qGreen(pixel) / 255.0;
    const double blue = qBlue(pixel) / 255.0;
    const double maximum = std::max({red, green, blue});
    const double minimum = std::min({red, green, blue});
    const double delta = maximum - minimum;

    double hue = 0.0;
    if (delta > 0.0) {
        if (maximum == red) {
            hue = 60.0 * std::fmod((green - blue) / delta, 6.0);
        } else if (maximum == green) {
            hue = 60.0 * ((blue - red) / delta + 2.0);
        } else {
            hue = 60.0 * ((red - green) / delta + 4.0);
        }
        if (hue < 0.0) {
            hue += 360.0;
        }
    }

    const double saturation = maximum > 0.0 ? delta / maximum : 0.0;
    return maximum >= 0.25
        && saturation >= 0.45
        && (hue <= 15.0 || hue >= 345.0);
}

} // namespace

class PixelStatisticsTest final : public QObject {
    Q_OBJECT

private slots:
    void customColorsRespectInclusiveRgbToleranceAndIndependentCounts()
    {
        QImage image(5, 1, QImage::Format_RGB32);
        const QRgb pixels[]{qRgb(20, 120, 220), qRgb(30, 110, 230),
                            qRgb(31, 120, 220), qRgb(0, 0, 0), qRgb(255, 255, 255)};
        for (int x = 0; x < image.width(); ++x) { image.setPixel(x, 0, pixels[x]); }
        const QList<PixelColorTarget> targets{{QStringLiteral("#1478dc"), 10},
            {QStringLiteral("#1478dc"), 0}, {QStringLiteral("#000000"), 0},
            {QStringLiteral("#ffffff"), 255}};
        const auto result = calculatePixelStatistics(image, std::nullopt, targets);
        QCOMPARE(result.count(targets[0]), qsizetype{2});
        QCOMPARE(result.count(targets[1]), qsizetype{1});
        QCOMPARE(result.count(targets[2]), qsizetype{1});
        QCOMPARE(result.count(targets[3]), qsizetype{5});
        QCOMPARE(result.count({QStringLiteral("#ffffff"), 0}), qsizetype{-1});
        QCOMPARE(result.count({QStringLiteral("invalid"), 0}), qsizetype{-1});
    }

    void bluePresetUsesEveReferenceAndInclusiveRgbTolerance()
    {
        QImage image(13, 1, QImage::Format_RGB32);
        const QRgb pixels[]{qRgb(48, 79, 91), qRgb(49, 80, 92),
            qRgb(32, 63, 75), qRgb(64, 95, 107), // inclusive tolerance boundaries
            qRgb(31, 79, 91), qRgb(65, 79, 91), qRgb(48, 62, 91),
            qRgb(48, 96, 91), qRgb(48, 79, 74), qRgb(48, 79, 108),
            qRgb(0, 0, 255), qRgb(80, 80, 80), qRgb(0, 0, 0)};
        for (int x = 0; x < image.width(); ++x) { image.setPixel(x, 0, pixels[x]); }
        const auto result = calculatePixelStatistics(image, std::nullopt,
            {{QStringLiteral("蓝色"), 0}, {QStringLiteral("蓝色"), 16}});
        QCOMPARE(pixelColorValue(QStringLiteral("蓝色")), QColor(48, 79, 91));
        QCOMPARE(result.blueCount, qsizetype{4});
        QCOMPARE(result.count({QStringLiteral("蓝色")}), qsizetype{4});
        QCOMPARE(result.count({QStringLiteral("蓝色"), 16}), qsizetype{4});
        QCOMPARE(result.count({QStringLiteral("蓝色"), 0}), qsizetype{1});
    }

    void eveBluePresetIncludesEveryShadeInTheReferenceSwatch()
    {
        QImage image(6, 1, QImage::Format_RGB32);
        const QRgb pixels[]{qRgb(48, 79, 91), qRgb(49, 80, 92), qRgb(48, 78, 90),
                            qRgb(47, 78, 90), qRgb(47, 77, 89), qRgb(48, 79, 90)};
        for (int x = 0; x < image.width(); ++x) { image.setPixel(x, 0, pixels[x]); }
        const auto owned = calculatePixelStatistics(image);
        const auto borrowed = calculatePixelStatistics(image.constBits(), image.width(),
            image.height(), image.bytesPerLine());
        QCOMPARE(owned.blueCount, qsizetype{6});
        QCOMPARE(borrowed.blueCount, owned.blueCount);
    }

    void customColorsWorkWithClippingConversionAndPaddedRows()
    {
        QImage image(3, 2, QImage::Format_RGB888);
        image.fill(QColor(QStringLiteral("#12ab34")));
        image.setPixelColor(1, 0, Qt::black);
        const PixelColorTarget target{QStringLiteral("#12AB34"), 0};
        const auto result = calculatePixelStatistics(image, QRect(-1, -1, 3, 3), {target});
        QCOMPARE(result.pixelCount, qsizetype{4});
        QCOMPARE(result.count(target), qsizetype{3});
        const auto rgb32 = image.convertToFormat(QImage::Format_RGB32);
        const auto borrowed = calculatePixelStatistics(rgb32.constBits(), rgb32.width(),
            rgb32.height(), rgb32.bytesPerLine(), QRect(-1, -1, 3, 3), {target});
        QCOMPARE(borrowed.count(target), result.count(target));
        const QList<PixelColorTarget> invalid{{QStringLiteral("bad"), -99},
            {QStringLiteral("#12AB34"), 999}, {QStringLiteral("#12ab34"), 255}};
        const auto normalized = normalizedPixelColorTargets(invalid);
        QCOMPARE(normalized.size(), 1);
        QCOMPARE(normalized.first().color, QStringLiteral("#12ab34"));
        QCOMPARE(normalized.first().tolerance, 255);
    }

    void reportsRedPredicatePerformance()
    {
        QList<QRgb> pixels;
        pixels.reserve(262144);
        QRandomGenerator random(0x0051A7E5u);
        for (int index = 0; index < 262144; ++index) {
            pixels.append(0xff000000u | (random.generate() & 0x00ffffffu));
        }

        quint64 referenceCount = 0;
        QElapsedTimer timer;
        timer.start();
        for (int repeat = 0; repeat < 8; ++repeat) {
            for (const QRgb pixel : std::as_const(pixels)) {
                referenceCount += referenceIsRed(pixel);
            }
        }
        const qint64 referenceNanoseconds = timer.nsecsElapsed();

        quint64 integerCount = 0;
        timer.restart();
        for (int repeat = 0; repeat < 8; ++repeat) {
            for (const QRgb pixel : std::as_const(pixels)) {
                integerCount += isRedPixelRgb(pixel);
            }
        }
        const qint64 integerNanoseconds = timer.nsecsElapsed();

        QCOMPARE(integerCount, referenceCount);
        qInfo().noquote() << QStringLiteral(
            "RED_PREDICATE_REFERENCE_NS=%1 INTEGER_NS=%2 SPEEDUP=%3x")
            .arg(referenceNanoseconds)
            .arg(integerNanoseconds)
            .arg(static_cast<double>(referenceNanoseconds)
                     / std::max<qint64>(1, integerNanoseconds),
                 0,
                 'f',
                 2);
    }

    void integerRedPredicateMatchesReferenceHsv()
    {
        const QList<QRgb> boundaries = {
            qRgb(63, 0, 0),
            qRgb(64, 0, 0),
            qRgb(100, 55, 55),
            qRgb(100, 54, 54),
            qRgb(255, 64, 0),
            qRgb(255, 65, 0),
            qRgb(255, 0, 64),
            qRgb(255, 0, 65),
            qRgb(255, 255, 255),
        };
        for (const QRgb pixel : boundaries) {
            const QByteArray message = QByteArray::number(pixel, 16);
            QVERIFY2(isRedPixelRgb(pixel) == referenceIsRed(pixel), message.constData());
        }

        QRandomGenerator random(0x0E7E2026u);
        for (int index = 0; index < 100000; ++index) {
            const QRgb pixel = 0xff000000u | (random.generate() & 0x00ffffffu);
            const QByteArray message = QByteArray::number(pixel, 16);
            QVERIFY2(isRedPixelRgb(pixel) == referenceIsRed(pixel), message.constData());
        }
    }

    void borrowedRgb32BufferMatchesQImageOverload()
    {
        QImage image(4, 2, QImage::Format_RGB32);
        image.fill(qRgb(0, 0, 0));
        image.setPixel(0, 0, qRgb(255, 0, 0));
        image.setPixel(1, 0, qRgb(255, 255, 255));

        const QRect region(0, 0, 2, 1);
        const auto owned = calculatePixelStatistics(image, region);
        const auto borrowed = calculatePixelStatistics(
            image.constBits(),
            image.width(),
            image.height(),
            image.bytesPerLine(),
            region);

        QCOMPARE(borrowed.pixelCount, owned.pixelCount);
        QCOMPARE(borrowed.redCount, owned.redCount);
        QCOMPARE(borrowed.whiteCount, owned.whiteCount);
    }

    void classifiesNearRedAndNearWhitePixels()
    {
        QImage image(10, 1, QImage::Format_RGB32);
        const QRgb pixels[] = {
            qRgb(255, 0, 0),
            qRgb(210, 80, 40),
            qRgb(200, 90, 90),
            qRgb(199, 20, 20),
            qRgb(255, 255, 255),
            qRgb(230, 225, 220),
            qRgb(219, 219, 219),
            qRgb(220, 220, 190),
            qRgb(0, 0, 0),
            qRgb(20, 120, 220),
        };
        for (int x = 0; x < image.width(); ++x) {
            image.setPixel(x, 0, pixels[x]);
        }

        const auto statistics = calculatePixelStatistics(image);

        QCOMPARE(statistics.pixelCount, qsizetype{10});
        QCOMPARE(statistics.redCount, qsizetype{4});
        QCOMPARE(statistics.whiteCount, qsizetype{4});
    }

    void classifiesRedByHueAcrossBrightnessRanges()
    {
        QImage image(8, 1, QImage::Format_RGB32);
        const QRgb pixels[] = {
            qRgb(199, 20, 20),   // dark red, still detected by hue
            qRgb(120, 25, 25),   // even darker red
            qRgb(255, 0, 255),   // magenta, excluded
            qRgb(255, 150, 0),   // orange, excluded
            qRgb(80, 50, 30),    // brown, excluded
            qRgb(240, 150, 150), // washed-out pink, excluded
            qRgb(219, 219, 219), // light gray, now white
            qRgb(0, 0, 0),       // black
        };
        for (int x = 0; x < image.width(); ++x) {
            image.setPixel(x, 0, pixels[x]);
        }

        const auto statistics = calculatePixelStatistics(image);

        QCOMPARE(statistics.pixelCount, qsizetype{8});
        QCOMPARE(statistics.redCount, qsizetype{2});
        QCOMPARE(statistics.whiteCount, qsizetype{1});
    }

    void limitsStatisticsToRequestedRegion()
    {
        QImage image(4, 2, QImage::Format_RGB32);
        image.fill(qRgb(0, 0, 0));
        image.setPixel(1, 0, qRgb(255, 0, 0));
        image.setPixel(2, 0, qRgb(255, 255, 255));
        image.setPixel(1, 1, qRgb(255, 255, 255));
        image.setPixel(2, 1, qRgb(255, 0, 0));

        const auto statistics = calculatePixelStatistics(image, QRect(1, 0, 2, 2));

        QCOMPARE(statistics.pixelCount, qsizetype{4});
        QCOMPARE(statistics.redCount, qsizetype{2});
        QCOMPARE(statistics.whiteCount, qsizetype{2});
    }

    void emptyIntersectionProducesNoSample()
    {
        QImage image(2, 2, QImage::Format_RGB32);
        image.fill(Qt::red);

        const auto statistics = calculatePixelStatistics(image, QRect(10, 10, 2, 2));

        QCOMPARE(statistics.pixelCount, qsizetype{0});
        QCOMPARE(statistics.redCount, qsizetype{0});
        QCOMPARE(statistics.whiteCount, qsizetype{0});
    }

    void exposesOnlyThePixelCategoriesTheReplicaCanReport()
    {
        const QStringList categories = pixelColorCategoryNames();
        QCOMPARE(categories, QStringList({QStringLiteral("红色"), QStringLiteral("白色"), QStringLiteral("蓝色")}));
        QCOMPARE(pixelColorCategoryName(PixelColorCategory::Red), QStringLiteral("红色"));
        QCOMPARE(pixelColorCategoryName(PixelColorCategory::White), QStringLiteral("白色"));
    }
};

QTEST_GUILESS_MAIN(PixelStatisticsTest)

#include "pixel_statistics_test.moc"
