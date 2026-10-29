#include <algorithm>
#include <cstdint>
#include <cwchar>
#include <type_traits>

#include <QtTest>

#include "platform/native_gateway.h"

namespace {

struct FakeState {
    std::uint32_t version{3};
    std::int32_t enumerateStatus{0};
    EveWindowInfo records[2]{};
    std::uint32_t recordCount{0};
    int freeCount{0};
    std::int32_t createStatus{0};
    std::uintptr_t createdHandle{707};
    std::int32_t sourceSizeStatus{0};
    std::int32_t width{1920};
    std::int32_t height{1080};
    std::int32_t updateStatus{0};
    EveRect lastRect{};
    EveRect lastSource{};
    std::uint8_t lastOpacity{0};
    std::int32_t lastVisible{0};
    std::int32_t destroyStatus{0};
    int destroyCount{0};
    std::uintptr_t destroyedHandle{0};
    std::int32_t activateStatus{0};
    int activateCount{0};
    std::uintptr_t activatedHwnd{0};
    std::uintptr_t observedForegroundHwnd{0xfeed};
    int captureCount{0};
    int pixelFreeCount{0};
    std::int32_t captureStatus{0};
    int frameCaptureCount{0};
    int frameReleaseCount{0};
    int frameStride{48};
};

FakeState* state = nullptr;

std::uint32_t fakeVersion()
{
    return state->version;
}

std::int32_t fakeEnumerate(EveWindowInfo** items, std::uint32_t* count)
{
    if (state->enumerateStatus == 0) {
        *items = state->recordCount == 0 ? nullptr : state->records;
        *count = state->recordCount;
    }
    return state->enumerateStatus;
}

void fakeFree(EveWindowInfo*)
{
    ++state->freeCount;
}

std::int32_t fakeCreate(
    std::uintptr_t,
    std::uintptr_t,
    std::uintptr_t* thumbnail)
{
    if (state->createStatus == 0) {
        *thumbnail = state->createdHandle;
    }
    return state->createStatus;
}

std::int32_t fakeSourceSize(
    std::uintptr_t,
    std::int32_t* width,
    std::int32_t* height)
{
    if (state->sourceSizeStatus == 0) {
        *width = state->width;
        *height = state->height;
    }
    return state->sourceSizeStatus;
}

std::int32_t fakeUpdate(
    std::uintptr_t,
    EveRect source,
    EveRect destination,
    std::uint8_t opacity,
    std::int32_t visible)
{
    state->lastSource = source;
    state->lastRect = destination;
    state->lastOpacity = opacity;
    state->lastVisible = visible;
    return state->updateStatus;
}

std::int32_t fakeDestroy(std::uintptr_t thumbnail)
{
    ++state->destroyCount;
    state->destroyedHandle = thumbnail;
    return state->destroyStatus;
}

std::int32_t fakeActivate(
    std::uintptr_t hwnd,
    std::uintptr_t* observedForegroundHwnd)
{
    ++state->activateCount;
    state->activatedHwnd = hwnd;
    *observedForegroundHwnd = state->observedForegroundHwnd;
    return state->activateStatus;
}

std::int32_t fakeCapture(
    std::uintptr_t,
    std::int32_t* width,
    std::int32_t* height,
    std::uint8_t** pixels)
{
    ++state->captureCount;
    constexpr std::int32_t w = 8;
    constexpr std::int32_t h = 6;
    auto* buffer = new std::uint8_t[static_cast<std::size_t>(w) * h * 4];
    std::fill(buffer, buffer + static_cast<std::size_t>(w) * h * 4, 0x11);
    auto* colors = reinterpret_cast<QRgb*>(buffer);
    colors[0] = qRgb(255, 0, 0);
    colors[1] = qRgb(255, 255, 255);
    *width = w;
    *height = h;
    *pixels = buffer;
    return state->captureStatus;
}

void fakeFreePixels(std::uint8_t* pixels)
{
    ++state->pixelFreeCount;
    delete[] pixels;
}

std::int32_t fakeCaptureFrame(std::uintptr_t, EveCaptureFrame* frame, EveCaptureMetrics*)
{
    ++state->frameCaptureCount;
    auto* colors = new QRgb[12 * 6];
    std::fill(colors, colors + 12 * 6, qRgb(255, 255, 255));
    for (int y = 0; y < 6; ++y) {
        std::fill(colors + 12 * y, colors + 12 * y + 8, qRgb(0, 0, 0));
    }
    colors[0] = qRgb(255, 0, 0);
    colors[1] = qRgb(255, 255, 255);
    *frame = EveCaptureFrame{8, 6, state->frameStride,
                             reinterpret_cast<const std::uint8_t*>(colors),
                             reinterpret_cast<std::uintptr_t>(colors)};
    return state->captureStatus;
}

void fakeReleaseFrame(EveCaptureFrame* frame)
{
    ++state->frameReleaseCount;
    delete[] reinterpret_cast<QRgb*>(frame->owner);
    *frame = {};
}

NativeFunctions fakeFunctions()
{
    return NativeFunctions{
        fakeVersion,
        fakeEnumerate,
        fakeFree,
        fakeCreate,
        fakeSourceSize,
        fakeUpdate,
        fakeDestroy,
        fakeActivate,
        fakeCapture,
        fakeFreePixels,
    };
}

void setTitle(EveWindowInfo& record, const wchar_t* title)
{
    const auto length = std::min<std::size_t>(std::wcslen(title), 511);
    std::wmemcpy(record.title, title, length);
    record.title[length] = L'\0';
}

} // namespace

class NativeGatewayTest final : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        state = &state_;
    }

    void cleanup()
    {
        state = nullptr;
        state_ = FakeState{};
    }

    void rejectsIncompatibleApiVersion()
    {
        state_.version = 9;

        try {
            NativeGateway gateway(fakeFunctions());
            Q_UNUSED(gateway);
            QFAIL("Expected NativeException");
        } catch (const NativeException& error) {
            QCOMPARE(error.operation(), QStringLiteral("API version"));
            QCOMPARE(error.status(), std::int32_t{9});
        }
    }

    void convertsFixedUtf16TitlesAndFreesList()
    {
        state_.recordCount = 2;
        state_.records[0].hwnd = 101;
        setTitle(state_.records[0], L"EVE Online - Alpha");
        state_.records[1].hwnd = 202;
        setTitle(state_.records[1], L"EVE Online - \u8d1d\u5854");
        NativeGateway gateway(fakeFunctions());

        const auto windows = gateway.listEveWindows();

        QCOMPARE(windows.size(), 2);
        QCOMPARE(windows[0].hwnd, std::uintptr_t{101});
        QCOMPARE(windows[0].title, QStringLiteral("EVE Online - Alpha"));
        QCOMPARE(windows[1].title, QStringLiteral("EVE Online - \u8d1d\u5854"));
        QCOMPARE(state_.freeCount, 1);
    }

    void supportsAnEmptyEnumeration()
    {
        NativeGateway gateway(fakeFunctions());

        QVERIFY(gateway.listEveWindows().isEmpty());
        QCOMPARE(state_.freeCount, 1);
    }

    void preservesSignedFailureStatus()
    {
        state_.enumerateStatus = -3;
        NativeGateway gateway(fakeFunctions());

        try {
            static_cast<void>(gateway.listEveWindows());
            QFAIL("Expected NativeException");
        } catch (const NativeException& error) {
            QCOMPARE(error.operation(), QStringLiteral("enumerate windows"));
            QCOMPARE(error.status(), std::int32_t{-3});
        }
    }

    void activatesTheRequestedWindowAndPreservesFailures()
    {
        NativeGateway gateway(fakeFunctions());

        QCOMPARE(gateway.activateWindow(0x1234), state_.observedForegroundHwnd);

        QCOMPARE(state_.activateCount, 1);
        QCOMPARE(state_.activatedHwnd, std::uintptr_t{0x1234});

        state_.activateStatus = -5;
        try {
            gateway.activateWindow(0x5678);
            QFAIL("Expected NativeException");
        } catch (const NativeException& error) {
            QCOMPARE(error.operation(), QStringLiteral("激活 EVE 窗口"));
            QCOMPARE(error.status(), std::int32_t{-5});
            QVERIFY(error.observedForegroundHwnd().has_value());
            QCOMPARE(*error.observedForegroundHwnd(), state_.observedForegroundHwnd);
        }
    }

    void throttlesActivationUntilCooldownExpires()
    {
        NativeGateway gateway(fakeFunctions());

        QVERIFY(gateway.activateWindowWithCooldown(0x1234, 100));
        QVERIFY(!gateway.activateWindowWithCooldown(0x5678, 100));
        QCOMPARE(state_.activateCount, 1);
        QCOMPARE(state_.activatedHwnd, std::uintptr_t{0x1234});

        QTest::qWait(120);

        QVERIFY(gateway.activateWindowWithCooldown(0x5678, 100));
        QCOMPARE(state_.activateCount, 2);
        QCOMPARE(state_.activatedHwnd, std::uintptr_t{0x5678});
    }

    void ownsThumbnailExactlyOnceAcrossMoves()
    {
        static_assert(!std::is_copy_constructible_v<ThumbnailHandle>);
        static_assert(!std::is_copy_assignable_v<ThumbnailHandle>);
        static_assert(std::is_move_constructible_v<ThumbnailHandle>);
        NativeGateway gateway(fakeFunctions());

        {
            auto first = gateway.createThumbnail(11, 22);
            auto second = std::move(first);
            QVERIFY(!first.isValid());
            QVERIFY(second.isValid());
            QCOMPARE(second.nativeHandle(), std::uintptr_t{707});
        }

        QCOMPARE(state_.destroyCount, 1);
        QCOMPARE(state_.destroyedHandle, std::uintptr_t{707});
    }

    void readsSizeUpdatesAndCleansUpAfterFailure()
    {
        NativeGateway gateway(fakeFunctions());

        {
            auto thumbnail = gateway.createThumbnail(11, 22);
            QCOMPARE(thumbnail.sourceSize(), QSize(1920, 1080));
            thumbnail.update(QRect(0, 0, 1920, 1080), QRect(3, 4, 640, 360), 201, true);
            QCOMPARE(state_.lastSource.left, 0);
            QCOMPARE(state_.lastSource.top, 0);
            QCOMPARE(state_.lastSource.right, 1920);
            QCOMPARE(state_.lastSource.bottom, 1080);
            QCOMPARE(state_.lastRect.left, 3);
            QCOMPARE(state_.lastRect.top, 4);
            QCOMPARE(state_.lastRect.right, 643);
            QCOMPARE(state_.lastRect.bottom, 364);
            QCOMPARE(state_.lastOpacity, std::uint8_t{201});
            QCOMPARE(state_.lastVisible, 1);
            state_.sourceSizeStatus = -7;
            QVERIFY_EXCEPTION_THROWN(
                static_cast<void>(thumbnail.sourceSize()), NativeException);
        }

        QCOMPARE(state_.destroyCount, 1);
    }

    void capturesWindowPixelsIntoImage()
    {
        NativeGateway gateway(fakeFunctions());

        const QImage image = gateway.captureWindow(123);

        QCOMPARE(image.size(), QSize(8, 6));
        QVERIFY(!image.isNull());
    }

    void capturesOnceForMultipleStatisticsRegionsAndFreesOnce()
    {
        NativeGateway gateway(fakeFunctions());

        const auto results = gateway.captureWindowStatistics(
            123,
            {QRect(0, 0, 1, 1), QRect(1, 0, 1, 1), std::nullopt});

        QCOMPARE(results.size(), 3);
        QCOMPARE(state_.captureCount, 1);
        QCOMPARE(state_.pixelFreeCount, 1);
        QVERIFY(results[0].has_value());
        QCOMPARE(results[0]->redCount, qsizetype{1});
        QCOMPARE(results[1]->whiteCount, qsizetype{1});
        QCOMPARE(results[2]->pixelCount, qsizetype{48});
    }

    void mergesTargetsOnlyForMatchingRegionsAndRefreshesCachedTolerance()
    {
        NativeGateway gateway(fakeFunctions());
        const PixelColorTarget red{QStringLiteral("#ff0000"), 0};
        const PixelColorTarget white{QStringLiteral("#ffffff"), 0};
        const PixelColorTarget black{QStringLiteral("#000000"), 0};
        const QRect first(0, 0, 1, 1), second(1, 0, 1, 1);
        const auto requests = QList<PixelStatisticsRequest>{
            {first, {red}}, {second, {white}}, {first, {black}}};
        for (int repeat = 0; repeat < 2; ++repeat) {
            const auto results = gateway.captureWindowStatisticsRequests(123, requests);
            QCOMPARE(results.size(), 3);
            QCOMPARE(results[0]->count(red), qsizetype{1});
            QCOMPARE(results[0]->count(black), qsizetype{0});
            QCOMPARE(results[0]->count(white), qsizetype{-1});
            QCOMPARE(results[1]->count(white), qsizetype{1});
            QCOMPARE(results[1]->count(red), qsizetype{-1});
            QCOMPARE(results[2]->count(red), qsizetype{1});
            QCOMPARE(results[2]->colors.size(), 2);
        }
        QCOMPARE(state_.captureCount, 2);
        QCOMPARE(state_.pixelFreeCount, 2);
        const PixelColorTarget broadRed{red.color, 255};
        const auto changed = gateway.captureWindowStatisticsRequests(123, {{second, {broadRed}}});
        QCOMPARE(changed[0]->count(broadRed), qsizetype{1});
        const auto restored = gateway.captureWindowStatisticsRequests(123, {{second, {red}}});
        QCOMPARE(restored[0]->count(red), qsizetype{0});
    }

    void freesUnexpectedPixelsWhenNativeCaptureReportsFailure()
    {
        state_.captureStatus = -4;
        NativeGateway gateway(fakeFunctions());

        QVERIFY_EXCEPTION_THROWN(
            static_cast<void>(gateway.captureWindowStatistics(123, {std::nullopt})),
            NativeException);

        QCOMPARE(state_.captureCount, 1);
        QCOMPARE(state_.pixelFreeCount, 1);
    }

    void scansBorrowedFramesUsingTheirStrideAndReleasesOnce()
    {
        auto functions = fakeFunctions();
        functions.captureFrame = fakeCaptureFrame;
        functions.releaseFrame = fakeReleaseFrame;
        NativeGateway gateway(functions);
        const auto results = gateway.captureWindowStatistics(123, {std::nullopt, QRect(0, 0, 2, 1)});
        QCOMPARE(state_.captureCount, 0);
        QCOMPARE(state_.frameCaptureCount, 1);
        QCOMPARE(state_.frameReleaseCount, 1);
        QCOMPARE(results[0]->pixelCount, qsizetype{48});
        QCOMPARE(results[0]->redCount, qsizetype{1});
        QCOMPARE(results[0]->whiteCount, qsizetype{1});
        QCOMPARE(results[1]->whiteCount, qsizetype{1});
    }

    void releasesBorrowedFrameOnNativeFailureAndInvalidStride()
    {
        auto functions = fakeFunctions();
        functions.captureFrame = fakeCaptureFrame;
        functions.releaseFrame = fakeReleaseFrame;
        NativeGateway gateway(functions);
        state_.captureStatus = -4;
        QVERIFY_EXCEPTION_THROWN(
            static_cast<void>(gateway.captureWindowStatistics(123, {std::nullopt})), NativeException);
        QCOMPARE(state_.frameReleaseCount, 1);
        state_.captureStatus = 0;
        state_.frameStride = 16;
        QVERIFY_EXCEPTION_THROWN(
            static_cast<void>(gateway.captureWindowStatistics(123, {std::nullopt})), NativeException);
        QCOMPARE(state_.frameReleaseCount, 2);
    }

private:
    FakeState state_{};
};

QTEST_GUILESS_MAIN(NativeGatewayTest)

#include "native_gateway_test.moc"
