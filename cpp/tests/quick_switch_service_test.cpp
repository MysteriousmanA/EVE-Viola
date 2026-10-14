#include <QtTest>

#include "features/quick_switch/quick_switch_keys.h"
#include "features/quick_switch/quick_switch_service_impl.h"
#include "platform/native_gateway.h"

class QuickSwitchServiceTest final : public QObject {
    Q_OBJECT

private slots:
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
