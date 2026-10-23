#include <limits>
#include <QtTest>

#include "features/quick_switch/quick_switch_keys.h"
#include "features/quick_switch/quick_switch_service_impl.h"
#include "platform/native_gateway.h"

class QuickSwitchServiceTest final : public QObject {
    Q_OBJECT

private slots:
    void mouseClicksAnywhereSuppressAutomaticActivation_data()
    {
#ifdef Q_OS_WIN
        QTest::addColumn<int>("downMessage");
        QTest::addColumn<int>("upMessage");
        QTest::addColumn<int>("buttonIndex");
        QTest::addColumn<quint32>("mouseData");
        QTest::newRow("left") << int(WM_LBUTTONDOWN) << int(WM_LBUTTONUP)
                              << 0 << quint32{0};
        QTest::newRow("right") << int(WM_RBUTTONDOWN) << int(WM_RBUTTONUP)
                               << 1 << quint32{0};
        QTest::newRow("middle") << int(WM_MBUTTONDOWN) << int(WM_MBUTTONUP)
                                << 2 << quint32{0};
        QTest::newRow("mouse4") << int(WM_XBUTTONDOWN) << int(WM_XBUTTONUP)
                                << 3 << quint32{XBUTTON1 << 16};
        QTest::newRow("mouse5") << int(WM_XBUTTONDOWN) << int(WM_XBUTTONUP)
                                << 4 << quint32{XBUTTON2 << 16};
#endif
    }

    void mouseClicksAnywhereSuppressAutomaticActivation()
    {
#ifdef Q_OS_WIN
        QFETCH(int, downMessage);
        QFETCH(int, upMessage);
        QFETCH(int, buttonIndex);
        QFETCH(quint32, mouseData);
        static int activations = 0;
        activations = 0;
        auto functions = NativeFunctions::linked();
        functions.activateWindow = [](std::uintptr_t hwnd, std::uintptr_t* foreground) {
            ++activations;
            *foreground = hwnd;
            return std::int32_t{0};
        };
        NativeGateway gateway(functions);
        QuickSwitchService service(gateway);
        auto& impl = *service.impl_;
        const auto flushActivation = [&gateway] {
            gateway.queueAutomaticActivation(1, 123, 0);
            gateway.flushAutomaticActivations();
        };
        MSLLHOOKSTRUCT mouse{};
        // Outside any window, so no EVE target can qualify this click.
        mouse.pt = {std::numeric_limits<LONG>::min(), std::numeric_limits<LONG>::min()};
        mouse.mouseData = mouseData;
        const auto parameter = reinterpret_cast<LPARAM>(&mouse);

        impl.handleMouseEvent(WM_MOUSEMOVE, parameter);
        flushActivation();
        QCOMPARE(activations, 1);

        impl.handleMouseEvent(downMessage, parameter);
        QVERIFY(impl.mouseButtonStates[static_cast<std::size_t>(buttonIndex)]);
        flushActivation();
        QCOMPARE(activations, 1);

        impl.handleMouseEvent(upMessage, parameter);
        QVERIFY(!impl.mouseButtonStates[static_cast<std::size_t>(buttonIndex)]);
        flushActivation();
        QCOMPARE(activations, 1);
#else
        QSKIP("Windows input implementation");
#endif
    }

    void keyboardInputAnywhereSuppressesAutomaticActivation_data()
    {
#ifdef Q_OS_WIN
        QTest::addColumn<int>("downMessage");
        QTest::addColumn<int>("upMessage");
        QTest::addColumn<quint32>("virtualKey");
        QTest::addColumn<quint32>("scanCode");
        QTest::addColumn<quint32>("flags");
        QTest::addColumn<int>("keyIndex");
        QTest::newRow("typing") << int(WM_KEYDOWN) << int(WM_KEYUP)
                               << quint32{'A'} << quint32{0x1E} << quint32{0} << 'A' * 2;
        QTest::newRow("left-control") << int(WM_KEYDOWN) << int(WM_KEYUP)
                                     << quint32{VK_CONTROL} << quint32{0x1D}
                                     << quint32{0} << VK_LCONTROL * 2;
        QTest::newRow("right-alt-system-key") << int(WM_SYSKEYDOWN) << int(WM_SYSKEYUP)
                                            << quint32{VK_MENU} << quint32{0x38}
                                            << quint32{LLKHF_EXTENDED} << VK_RMENU * 2 + 1;
        QTest::newRow("windows-key") << int(WM_KEYDOWN) << int(WM_KEYUP)
                                    << quint32{VK_LWIN} << quint32{0x5B}
                                    << quint32{LLKHF_EXTENDED} << VK_LWIN * 2 + 1;
        QTest::newRow("function-key") << int(WM_KEYDOWN) << int(WM_KEYUP)
                                     << quint32{VK_F12} << quint32{0x58} << quint32{0}
                                     << VK_F12 * 2;
        QTest::newRow("enter") << int(WM_KEYDOWN) << int(WM_KEYUP)
                              << quint32{VK_RETURN} << quint32{0x1C} << quint32{0}
                              << VK_RETURN * 2;
        QTest::newRow("numpad-enter") << int(WM_KEYDOWN) << int(WM_KEYUP)
                                     << quint32{VK_RETURN} << quint32{0x1C}
                                     << quint32{LLKHF_EXTENDED} << VK_RETURN * 2 + 1;
        QTest::newRow("unicode-input") << int(WM_KEYDOWN) << int(WM_KEYUP)
                                      << quint32{VK_PACKET} << quint32{0} << quint32{0}
                                      << VK_PACKET * 2;
        QTest::newRow("injected-input") << int(WM_KEYDOWN) << int(WM_KEYUP)
                                       << quint32{'A'} << quint32{0x1E}
                                       << quint32{LLKHF_INJECTED} << 'A' * 2;
#endif
    }

    void keyboardInputAnywhereSuppressesAutomaticActivation()
    {
#ifdef Q_OS_WIN
        QFETCH(int, downMessage);
        QFETCH(int, upMessage);
        QFETCH(quint32, virtualKey);
        QFETCH(quint32, scanCode);
        QFETCH(quint32, flags);
        QFETCH(int, keyIndex);
        static int activations = 0;
        activations = 0;
        auto functions = NativeFunctions::linked();
        functions.activateWindow = [](std::uintptr_t hwnd, std::uintptr_t* foreground) {
            ++activations;
            *foreground = hwnd;
            return std::int32_t{0};
        };
        NativeGateway gateway(functions);
        gateway.setAutomaticActivationMouseQuietPeriodMs(100);
        QuickSwitchService service(gateway);
        auto& impl = *service.impl_;
        const auto flushActivation = [&gateway] {
            gateway.queueAutomaticActivation(1, 123, 0);
            gateway.flushAutomaticActivations();
        };
        KBDLLHOOKSTRUCT key{};
        key.vkCode = virtualKey;
        key.scanCode = scanCode;
        key.flags = flags;
        const auto parameter = reinterpret_cast<LPARAM>(&key);

        flushActivation();
        QCOMPARE(activations, 1);
        // No switch keys are configured: ordinary input must still be tracked.
        impl.handleKeyboardEvent(downMessage, parameter);
        impl.handleKeyboardEvent(downMessage, parameter);
        QVERIFY(impl.keyboardPressedStates[static_cast<std::size_t>(keyIndex)]);
        QTest::qWait(150);
        flushActivation();
        QCOMPARE(activations, 1);

        impl.handleKeyboardEvent(upMessage, parameter);
        QVERIFY(!impl.keyboardPressedStates[static_cast<std::size_t>(keyIndex)]);
        flushActivation();
        QCOMPARE(activations, 1);
        QTest::qWait(150);
        flushActivation();
        QCOMPARE(activations, 2);
#else
        QSKIP("Windows input implementation");
#endif
    }

    void keyboardAndMouseShareQuietPeriodAndCleanupReleasesHeldKeys()
    {
#ifdef Q_OS_WIN
        static int activations = 0;
        activations = 0;
        auto functions = NativeFunctions::linked();
        functions.activateWindow = [](std::uintptr_t hwnd, std::uintptr_t* foreground) {
            ++activations;
            *foreground = hwnd;
            return std::int32_t{0};
        };
        NativeGateway gateway(functions);
        gateway.setAutomaticActivationMouseQuietPeriodMs(100);
        QuickSwitchService service(gateway);
        auto& impl = *service.impl_;
        const auto flushActivation = [&gateway] {
            gateway.queueAutomaticActivation(1, 123, 0);
            gateway.flushAutomaticActivations();
        };
        KBDLLHOOKSTRUCT key{};
        const auto keyboardParameter = reinterpret_cast<LPARAM>(&key);
        MSLLHOOKSTRUCT mouse{};
        const auto mouseParameter = reinterpret_cast<LPARAM>(&mouse);

        key.vkCode = 'A';
        impl.handleKeyboardEvent(WM_KEYDOWN, keyboardParameter);
        key.vkCode = 'B';
        impl.handleKeyboardEvent(WM_KEYDOWN, keyboardParameter);
        impl.handleMouseEvent(WM_LBUTTONDOWN, mouseParameter);
        key.vkCode = 'A';
        impl.handleKeyboardEvent(WM_KEYUP, keyboardParameter);
        QTest::qWait(150);
        flushActivation();
        QCOMPARE(activations, 0);

        key.vkCode = 'B';
        impl.handleKeyboardEvent(WM_KEYUP, keyboardParameter);
        QTest::qWait(150);
        flushActivation();
        QCOMPARE(activations, 0);
        impl.handleMouseEvent(WM_LBUTTONUP, mouseParameter);
        flushActivation();
        QCOMPARE(activations, 0);
        QTest::qWait(150);
        flushActivation();
        QCOMPARE(activations, 1);

        impl.handleKeyboardEvent(WM_KEYDOWN, keyboardParameter);
        impl.releaseTrackedKeyboardKeys();
        impl.releaseTrackedKeyboardKeys();
        QVERIFY(!impl.keyboardPressedStates['B' * 2]);
        flushActivation();
        QCOMPARE(activations, 1);
        QTest::qWait(150);
        flushActivation();
        QCOMPARE(activations, 2);
#else
        QSKIP("Windows input implementation");
#endif
    }

    void syntheticRadialInputDoesNotReleasePhysicallyHeldKeys()
    {
#ifdef Q_OS_WIN
        NativeGateway gateway;
        QuickSwitchService service(gateway);
        auto& impl = *service.impl_;
        KBDLLHOOKSTRUCT key{};
        key.vkCode = 'A';
        const auto parameter = reinterpret_cast<LPARAM>(&key);
        impl.handleKeyboardEvent(WM_KEYDOWN, parameter);
        QVERIFY(impl.keyboardPressedStates['A' * 2]);
        key.flags = LLKHF_INJECTED;
        key.dwExtraInfo = impl.kRadialSyntheticInputTag;
        impl.handleKeyboardEvent(WM_KEYUP, parameter);
        QVERIFY(impl.keyboardPressedStates['A' * 2]);
        key.flags = 0;
        key.dwExtraInfo = 0;
        impl.handleKeyboardEvent(WM_KEYUP, parameter);
        QVERIFY(!impl.keyboardPressedStates['A' * 2]);
#else
        QSKIP("Windows input implementation");
#endif
    }

    void mappingsKeepKeyboardIdentitiesSeparateAndPreviousWinsConflicts()
    {
#ifdef Q_OS_WIN
        NativeGateway gateway;
        QuickSwitchService service(gateway);
        auto& impl = *service.impl_;
        const QString normalEnter = quickSwitchKeyboardKey(VK_RETURN, false);
        const QString extendedEnter = quickSwitchKeyboardKey(VK_RETURN, true);
        service.updateKeys({normalEnter, QStringLiteral("Mouse4")},
                           {normalEnter, extendedEnter, QStringLiteral("Mouse4"),
                            QStringLiteral("Mouse5")});
        QCOMPARE(impl.keyboardDirection(VK_RETURN * 2), int(impl.kPreviousDirection));
        QCOMPARE(impl.keyboardDirection(VK_RETURN * 2 + 1), int(impl.kNextDirection));
        QCOMPARE(impl.mouseDirection(0), int(impl.kPreviousDirection));
        QCOMPARE(impl.mouseDirection(1), int(impl.kNextDirection));
        impl.switchMappingVersion.fetch_add(1);
        QCOMPARE(impl.keyboardDirection(VK_RETURN * 2), int(impl.kNoDirection));
        QCOMPARE(impl.mouseDirection(0), int(impl.kNoDirection));
        impl.switchMappingVersion.fetch_add(1);
        service.updateKeys({}, {});
        QCOMPARE(impl.keyboardDirection(VK_RETURN * 2), int(impl.kNoDirection));
        QCOMPARE(impl.mouseDirection(0), int(impl.kNoDirection));
        service.updateRadialSwitchKey(QStringLiteral("Mouse4"));
        QCOMPARE(impl.radialSwitchKeyIndex.load(), VK_SPACE * 2);
        service.updateRadialSwitchKey(extendedEnter);
        QCOMPARE(impl.radialSwitchKeyIndex.load(), VK_RETURN * 2 + 1);
#else
        QSKIP("Windows input implementation");
#endif
    }

    void capturedKeyIsConsumedOnceAndBothIdentitiesAreClearedOnRelease()
    {
#ifdef Q_OS_WIN
        NativeGateway gateway;
        QuickSwitchService service(gateway);
        auto& impl = *service.impl_;
        QSignalSpy captured(&service, &QuickSwitchService::inputCaptured);
        auto state = QuickSwitchService::Impl::KeyState::Up;
        impl.captureArmed.store(true);
        QCOMPARE(impl.processInputEvent(impl.kNoDirection, state, true, false,
                                       nullptr, WM_KEYDOWN, 0, VK_PRIOR, true), LRESULT{1});
        QVERIFY(state == QuickSwitchService::Impl::KeyState::Captured);
        QVERIFY(!impl.captureArmed.load());
        QCOMPARE(impl.processInputEvent(impl.kNoDirection, state, true, false,
                                       nullptr, WM_KEYDOWN, 0, VK_PRIOR, true), LRESULT{1});
        impl.captureIgnoreUntilUp[VK_PRIOR * 2].store(true);
        impl.captureIgnoreUntilUp[VK_PRIOR * 2 + 1].store(true);
        QCOMPARE(impl.processInputEvent(impl.kNoDirection, state, false, true,
                                       nullptr, WM_KEYUP, 0, VK_PRIOR, true), LRESULT{1});
        QVERIFY(state == QuickSwitchService::Impl::KeyState::Up);
        QVERIFY(!impl.captureIgnoreUntilUp[VK_PRIOR * 2].load());
        QVERIFY(!impl.captureIgnoreUntilUp[VK_PRIOR * 2 + 1].load());
        QTRY_COMPARE(captured.count(), 1);
        QCOMPARE(captured.first().first().toString(), QStringLiteral("PageUp"));
#else
        QSKIP("Windows input implementation");
#endif
    }

    void injectedCaptureIsSkippedAndMouseCaptureUsesTheSameNotificationPath()
    {
#ifdef Q_OS_WIN
        NativeGateway gateway;
        QuickSwitchService service(gateway);
        auto& impl = *service.impl_;
        QSignalSpy captured(&service, &QuickSwitchService::inputCaptured);
        auto state = QuickSwitchService::Impl::KeyState::Up;
        impl.captureArmed.store(true);
        QVERIFY(!impl.captureKeyDown(state, nullptr, WM_KEYDOWN, 0,
                                     VK_SPACE, false, -1, false).has_value());
        QVERIFY(impl.captureArmed.load());
        QVERIFY(state == QuickSwitchService::Impl::KeyState::Up);
        const auto result = impl.captureKeyDown(state, nullptr, WM_XBUTTONDOWN, 0,
                                               -1, false, impl.kMouse4Token, true);
        QVERIFY(result.has_value());
        QCOMPARE(*result, LRESULT{1});
        QTRY_COMPARE(captured.count(), 1);
        QCOMPARE(captured.first().first().toString(), QStringLiteral("Mouse4"));
#else
        QSKIP("Windows input implementation");
#endif
    }

    void cancelledRadialReleaseClearsStateAndReportsItsSession()
    {
#ifdef Q_OS_WIN
        NativeGateway gateway;
        QuickSwitchService service(gateway);
        auto& impl = *service.impl_;
        const std::size_t index = VK_SPACE * 2;
        impl.keyStates[index] = QuickSwitchService::Impl::KeyState::RadialPending;
        impl.radialSessionIds[index] = 42;
        impl.radialForegrounds[index] = nullptr;
        impl.radialStartedAt[index] = GetTickCount64() - 1000;
        impl.radialStartDirections[index] = impl.kPreviousDirection;
        impl.captureIgnoreUntilUp[index].store(true);
        impl.captureIgnoreUntilUp[index + 1].store(true);
        impl.radialCancelRequested.store(true);
        QSignalSpy ended(&service, &QuickSwitchService::radialSwitchHoldEnded);
        QSignalSpy switched(&service, &QuickSwitchService::switchRequested);
        const auto repeated = impl.processRadialKeyboardEvent(
            VK_SPACE, false, index, true, false, false, WM_KEYDOWN, 0);
        QVERIFY(repeated.has_value());
        QCOMPARE(*repeated, LRESULT{1});
        QVERIFY(impl.radialCancelRequested.load());
        const auto released = impl.processRadialKeyboardEvent(
            VK_SPACE, false, index, false, true, false, WM_KEYUP, 0);
        QVERIFY(released.has_value());
        QCOMPARE(*released, LRESULT{1});
        QVERIFY(impl.keyStates[index] == QuickSwitchService::Impl::KeyState::Up);
        QVERIFY(!impl.captureIgnoreUntilUp[index].load());
        QVERIFY(!impl.captureIgnoreUntilUp[index + 1].load());
        QVERIFY(!impl.radialCancelRequested.load());
        QTRY_COMPARE(ended.count(), 1);
        QCOMPARE(ended.first()[0].toULongLong(), quint64{42});
        QCOMPARE(ended.first()[1].toBool(), true);
        QCOMPARE(ended.first()[2].toBool(), false);
        QCOMPARE(ended.first()[3].toBool(), true);
        QCOMPARE(ended.first()[4].value<quintptr>(), quintptr{0});
        QCOMPARE(switched.count(), 0);
#else
        QSKIP("Windows input implementation");
#endif
    }

    void queuedSignalsDoNotOutliveTheService()
    {
#ifdef Q_OS_WIN
        NativeGateway gateway;
        auto service = std::make_unique<QuickSwitchService>(gateway);
        QSignalSpy captured(service.get(), &QuickSwitchService::inputCaptured);
        service->impl_->postInputCaptured(QStringLiteral("Mouse4"));
        QCOMPARE(captured.count(), 0);
        service.reset();
        QCoreApplication::processEvents();
        QCOMPARE(captured.count(), 0);
#else
        QSKIP("Windows input implementation");
#endif
    }
};

QTEST_GUILESS_MAIN(QuickSwitchServiceTest)
#include "quick_switch_service_test.moc"
