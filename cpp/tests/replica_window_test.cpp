#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <vector>

#include <QtTest>

#include <QApplication>
#include <QLabel>
#include <QMenu>
#include <QTimer>
#include <QWheelEvent>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

#include "ui/replica_window.h"
#include "features/advanced_logic/advanced_logic_service.h"

namespace {

struct WindowFakeState {
    EveWindowInfo record{};
    std::uint32_t count{0};
    int freeCount{0};
    std::uintptr_t handle{909};
    int createCount{0};
    std::int32_t createStatus{0};
    std::uintptr_t rejectedThumbnail{0};
    std::uintptr_t stalledThumbnail{0};
    std::uintptr_t lastDestroyedThumbnail{0};
    int sourceRevision{1};
    int displayedRevision{1};
    std::int32_t width{1920};
    std::int32_t height{1080};
    std::int32_t sourceStatus{0};
    std::int32_t updateStatus{0};
    std::int32_t captureStatus{0};
    int captureWidth{8};
    int captureHeight{6};
    bool captureCalled{false};
    int captureCount{0};
    std::uintptr_t lastCapturedHwnd{0};
    int freePixelCount{0};
    std::vector<QRgb> captureColors;
    EveRect lastRect{};
    EveRect lastSource{};
    int lastVisible{1};
    int updateCount{0};
    int destroyCount{0};
    int activateCount{0};
    int resumeCount{0};
    bool sourceMinimized{false};
    std::int32_t resumeStatus{0};
    std::uintptr_t activatedHwnd{0};
};

WindowFakeState* fake = nullptr;

std::uint32_t version() { return 3; }
std::int32_t enumerate(EveWindowInfo** items, std::uint32_t* count)
{
    *items = fake->count == 0 ? nullptr : &fake->record;
    *count = fake->count;
    return 0;
}
void freeList(EveWindowInfo*) { ++fake->freeCount; }
std::int32_t create(std::uintptr_t destination, std::uintptr_t source, std::uintptr_t* handle)
{
    if (destination == 0 || source == 0) {
        return -2;
    }
    ++fake->createCount;
    if (fake->createStatus != 0) { return fake->createStatus; }
    *handle = fake->handle + fake->createCount - 1;
    return 0;
}
std::int32_t sourceSize(std::uintptr_t, std::int32_t* width, std::int32_t* height)
{
    if (fake->sourceStatus == 0) {
        *width = fake->width;
        *height = fake->height;
    }
    return fake->sourceStatus;
}
std::int32_t update(
    std::uintptr_t handle,
    EveRect source,
    EveRect rect,
    std::uint8_t,
    std::int32_t visible)
{
    ++fake->updateCount;
    fake->lastSource = source;
    fake->lastRect = rect;
    fake->lastVisible = visible;
    if (handle == fake->rejectedThumbnail) { return -9; }
    if (fake->updateStatus == 0 && visible && !fake->sourceMinimized
        && handle != fake->stalledThumbnail) {
        fake->displayedRevision = fake->sourceRevision;
    }
    return fake->updateStatus;
}
std::int32_t destroy(std::uintptr_t handle)
{
    ++fake->destroyCount;
    fake->lastDestroyedThumbnail = handle;
    return 0;
}
std::int32_t activate(std::uintptr_t hwnd, std::uintptr_t* observedForegroundHwnd)
{
    ++fake->activateCount;
    fake->activatedHwnd = hwnd;
    *observedForegroundHwnd = hwnd;
    return 0;
}
std::int32_t capture(
    std::uintptr_t hwnd,
    std::int32_t* width,
    std::int32_t* height,
    std::uint8_t** pixels)
{
    fake->captureCalled = true;
    ++fake->captureCount;
    fake->lastCapturedHwnd = hwnd;
    if (fake->captureStatus != 0) {
        return fake->captureStatus;
    }
    *width = fake->captureWidth;
    *height = fake->captureHeight;
    const auto rowBytes = static_cast<std::size_t>(fake->captureWidth) * 4;
    auto* buffer = new std::uint8_t[
        rowBytes * static_cast<std::size_t>(fake->captureHeight)];
    if (fake->captureColors.empty()) {
        std::fill(buffer, buffer + rowBytes * fake->captureHeight, 0xFF);
    } else {
        const auto pixelCount = static_cast<std::size_t>(fake->captureWidth)
            * static_cast<std::size_t>(fake->captureHeight);
        Q_ASSERT(fake->captureColors.size() == pixelCount);
        std::memcpy(buffer, fake->captureColors.data(), rowBytes * fake->captureHeight);
    }
    *pixels = buffer;
    return 0;
}
void freePixels(std::uint8_t* pixels)
{
    ++fake->freePixelCount;
    delete[] pixels;
}

std::int32_t resumeSource(std::uintptr_t hwnd)
{
    if (hwnd == 0) { return -1; }
    ++fake->resumeCount;
    if (fake->resumeStatus == 0) { fake->sourceMinimized = false; }
    return fake->resumeStatus;
}

NativeFunctions functions()
{
    NativeFunctions result{
        version, enumerate, freeList, create, sourceSize, update, destroy, activate,
        capture, freePixels,
    };
    result.resumeThumbnailSource = resumeSource;
    return result;
}

void setRecord(WindowFakeState& state)
{
    state.count = 1;
    state.record.hwnd = 123;
    const wchar_t* title = L"EVE Online - Alpha";
    std::wmemcpy(state.record.title, title, std::wcslen(title) + 1);
}

} // namespace

class ReplicaWindowTest final : public QObject {
    Q_OBJECT

private slots:
    void petModesPauseRuleActionsButFishStillRequestsNotifications()
    {
        ColorRule rule;
        rule.number = 1;
        rule.triggerCondition1 = {QStringLiteral("红色"), QStringLiteral(">=1")};
        rule.triggerFeedback = TriggerFeedback::Popup;
        rule.postTriggerAction = PostTriggerAction::Jump;
        ReplicaWindow window(81, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        window.setStatisticsInterval(0);
        window.selectSource({123, QStringLiteral("EVE - Alpha")});
        window.setColorRules({rule});
        window.setColorLogicSlot(1, 1);
        window.setColorLogicEnabled(true);
        window.setAutomaticActivationEnabled(true);
        QSignalSpy effects(&window, &ReplicaWindow::colorLogicTriggered);
        QSignalSpy notifications(&window, &ReplicaWindow::colorLogicNotificationRequested);
        window.setDesktopPetMode(DesktopPetMode::Fish);
        window.show();
        QVERIFY(!window.isVisible());
        window.evaluateColorLogic({0, 1, 0});
        window.evaluateColorLogic({0, 1, 0});
        QCOMPARE(notifications.count(), 1);
        QCOMPARE(effects.count(), 0);
        QCOMPARE(window.activeColorLogicSlot(), 1);
        QCOMPARE(window.colorLogicTriggerCount(1), 0);
        QCOMPARE(state_.activateCount, 0);
        QVERIFY(!window.isFlashActive());
        window.evaluateColorLogic({0, 0, 0});
        window.evaluateColorLogic({0, 1, 0});
        QCOMPARE(notifications.count(), 2);
        window.setDesktopPetMode(DesktopPetMode::DoNotDisturb);
        window.evaluateColorLogic({0, 0, 0});
        window.evaluateColorLogic({0, 1, 0});
        QCOMPARE(notifications.count(), 2);
        QCOMPARE(window.colorLogicTriggerCount(1), 0);
        window.setDesktopPetMode(DesktopPetMode::Resident);
        window.show();
        window.evaluateColorLogic({0, 1, 0});
        QCOMPARE(effects.count(), 1);
        QCOMPARE(window.colorLogicTriggerCount(1), 1);
        QCOMPARE(window.activeColorLogicSlot(), 2);
    }

    void switchingModeCancelsDeferredAdvancedEffects()
    {
        AdvancedLogicService service;
        ColorRule rule;
        rule.number = 1;
        rule.triggerCondition1 = {QStringLiteral("红色"), QStringLiteral(">=1")};
        rule.postTriggerAction = PostTriggerAction::Jump;
        ColorRule second = rule;
        second.number = 2;
        WhitelistSequence sequence;
        sequence.timeoutMs = 100;
        sequence.nodes = {{1, WhitelistNodeEffect::Suppress, {}}, {2, WhitelistNodeEffect::Suppress, {}}};
        service.setConfiguration({true, {sequence}}, {rule, second});
        ReplicaWindow window(82, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        window.setStatisticsInterval(0);
        window.setColorRules({rule, second});
        window.setColorLogicSlot(1, 1);
        window.setColorLogicSlot(2, 2);
        window.setColorLogicEnabled(true);
        window.setAdvancedLogicService(&service);
        QSignalSpy effects(&window, &ReplicaWindow::colorLogicTriggered);
        window.evaluateColorLogic({0, 1, 0});
        QCOMPARE(effects.count(), 0);
        window.setDesktopPetMode(DesktopPetMode::Fish);
        QTest::qWait(130);
        QCOMPARE(effects.count(), 0);
        QCOMPARE(window.activeColorLogicSlot(), 1);
        QVERIFY(!window.isFlashActive());
    }

    void advancedSequenceCanObserveRepeatedEdgesFromOneWindowWhileWaiting()
    {
        AdvancedLogicService service;
        ColorRule rule;
        rule.number = 1;
        rule.triggerCondition1 = {QStringLiteral("红色"), QStringLiteral(">=1")};
        rule.postTriggerAction = PostTriggerAction::Loop;
        WhitelistSequence sequence;
        sequence.nodes = {{1, WhitelistNodeEffect::Suppress, {}}, {1, WhitelistNodeEffect::Suppress, {}}};
        service.setConfiguration({true, {sequence}}, {rule});
        ReplicaWindow window(75, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        window.setStatisticsInterval(0);
        window.setColorRules({rule});
        window.setColorLogicSlot(1, 1);
        window.setColorLogicEnabled(true);
        window.setAdvancedLogicService(&service);
        window.setAutomaticActivationEnabled(false);
        QSignalSpy effects(&window, &ReplicaWindow::colorLogicTriggered);
        QSignalSpy decisions(&service, &AdvancedLogicService::eventResolved);
        window.evaluateColorLogic({0, 1, 0});
        QCOMPARE(effects.size(), 0);
        window.evaluateColorLogic({0, 0, 0});
        window.evaluateColorLogic({0, 1, 0});
        QCOMPARE(window.colorLogicTriggerCount(1), 2);
        QCOMPARE(decisions.size(), 2);
        QCOMPARE(decisions[0][1].toBool(), false);
        QCOMPARE(decisions[1][1].toBool(), false);
        QCOMPARE(effects.size(), 0);
        QVERIFY(!window.isFlashActive());
    }

    void advancedWhitelistImmediatelyExecutesWarningAndSuppressesAccelerationEffects()
    {
        AdvancedLogicService service;
        ColorRule warning;
        warning.number = 1;
        warning.triggerCondition1 = {QStringLiteral("红色"), QStringLiteral(">=1")};
        warning.priority = ColorRulePriority::Immediate;
        warning.postTriggerAction = PostTriggerAction::ReverseWait;
        ColorRule speed = warning;
        speed.number = 2;
        speed.priority = ColorRulePriority::Normal;
        speed.postTriggerAction = PostTriggerAction::Jump;
        const QList<ColorRule> rules{warning, speed};
        WhitelistSequence sequence;
        sequence.nodes = {{1, WhitelistNodeEffect::Suppress, {}}, {2, WhitelistNodeEffect::Suppress, {}}};
        service.setConfiguration({true, {sequence}}, rules);
        ReplicaWindow local(71, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        ReplicaWindow acceleration(72, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        for (auto* window : {&local, &acceleration}) {
            window->setStatisticsInterval(0);
            window->setColorRules(rules);
            window->setAdvancedLogicService(&service);
            window->selectSource({123, QStringLiteral("EVE - Alpha")});
            window->setAutomaticActivationEnabled(false);
            window->setColorLogicEnabled(true);
        }
        local.setColorLogicSlot(1, 1);
        acceleration.setColorLogicSlot(1, 2);
        acceleration.setColorLogicSlot(2, 1);
        QSignalSpy warningEffects(&local, &ReplicaWindow::colorLogicTriggered);
        QSignalSpy speedEffects(&acceleration, &ReplicaWindow::colorLogicTriggered);
        local.evaluateColorLogic({0, 1, 0});
        QCOMPARE(warningEffects.size(), 1);
        acceleration.evaluateColorLogic({0, 1, 0});
        QCOMPARE(speedEffects.size(), 0);
        QVERIFY(!acceleration.isFlashActive());
        QCOMPARE(acceleration.activeColorLogicSlot(), 1);
        QCOMPARE(warningEffects.size(), 1);
        // Sustained pixels do not replay a suppressed loop; a new, isolated edge does execute.
        acceleration.evaluateColorLogic({0, 1, 0});
        QCOMPARE(speedEffects.size(), 0);
        acceleration.evaluateColorLogic({0, 0, 0});
        acceleration.evaluateColorLogic({0, 1, 0});
        QCOMPARE(speedEffects.size(), 1);
        QCOMPARE(acceleration.activeColorLogicSlot(), 2);
    }

    void disablingPendingAdvancedRuleRejectsLateTimeout()
    {
        AdvancedLogicService service;
        ColorRule first;
        first.number = 1;
        first.triggerCondition1 = {QStringLiteral("红色"), QStringLiteral(">=1")};
        first.postTriggerAction = PostTriggerAction::Jump;
        ColorRule second = first;
        second.number = 2;
        WhitelistSequence sequence;
        sequence.timeoutMs = 100;
        sequence.nodes = {{1, WhitelistNodeEffect::Suppress, {}}, {2, WhitelistNodeEffect::Suppress, {}}};
        service.setConfiguration({true, {sequence}}, {first, second});
        ReplicaWindow window(73, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        window.setStatisticsInterval(0);
        window.setColorRules({first, second});
        window.setColorLogicSlot(1, 1);
        window.setColorLogicSlot(2, 2);
        window.setColorLogicEnabled(true);
        window.setAdvancedLogicService(&service);
        window.setAutomaticActivationEnabled(false);
        QSignalSpy effects(&window, &ReplicaWindow::colorLogicTriggered);
        window.evaluateColorLogic({0, 1, 0});
        QCOMPARE(effects.size(), 0);
        QVERIFY(!window.isFlashActive());
        window.setColorLogicEnabled(false);
        QTest::qWait(130);
        QCOMPARE(effects.size(), 0);
        QCOMPARE(window.activeColorLogicSlot(), 1);
    }

    void unmatchedAdvancedPrefixReleasesItsOriginalEffectAfterTimeout()
    {
        AdvancedLogicService service;
        ColorRule first;
        first.number = 1;
        first.triggerCondition1 = {QStringLiteral("红色"), QStringLiteral(">=1")};
        first.postTriggerAction = PostTriggerAction::Jump;
        ColorRule second = first;
        second.number = 2;
        WhitelistSequence sequence;
        sequence.timeoutMs = 100;
        sequence.nodes = {{1, WhitelistNodeEffect::Suppress, {}}, {2, WhitelistNodeEffect::Suppress, {}}};
        service.setConfiguration({true, {sequence}}, {first, second});
        ReplicaWindow window(74, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        window.setStatisticsInterval(0);
        window.setColorRules({first, second});
        window.setColorLogicSlot(1, 1);
        window.setColorLogicSlot(2, 2);
        window.setColorLogicEnabled(true);
        window.setAdvancedLogicService(&service);
        window.setAutomaticActivationEnabled(false);
        QSignalSpy effects(&window, &ReplicaWindow::colorLogicTriggered);
        window.evaluateColorLogic({0, 1, 0});
        QCOMPARE(effects.size(), 0);
        QCOMPARE(window.activeColorLogicSlot(), 1);
        QTRY_COMPARE_WITH_TIMEOUT(effects.size(), 1, 500);
        QCOMPARE(window.activeColorLogicSlot(), 2);
    }

    void customRuleColorIsCapturedEvenWhenItIsNotInTheDisplayColors()
    {
        setRecord(state_);
        state_.captureWidth = 3;
        state_.captureHeight = 1;
        state_.captureColors = {qRgb(0, 255, 0), qRgb(16, 240, 16), qRgb(48, 79, 91)};
        ReplicaWindow window(77, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        ColorRule rule;
        rule.number = 1;
        rule.triggerCondition1 = {QStringLiteral("#00ff00"), QStringLiteral(">=2"), 16};
        rule.triggerCondition2 = {QStringLiteral("蓝色"), QStringLiteral(">=1")};
        rule.postTriggerAction = PostTriggerAction::ReverseWait;
        window.setColorRules({rule});
        window.setColorLogicSlot(1, 1);
        window.setColorLogicEnabled(true);
        window.setStatisticsInterval(500);
        QSignalSpy statistics(&window, &ReplicaWindow::colorStatisticsChanged);
        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});
        QTRY_COMPARE_WITH_TIMEOUT(window.colorLogicTriggerCount(1), 1, 1500);
        QVERIFY(window.statisticsColors() == defaultPixelColorTargets());
        QVERIFY(!statistics.isEmpty());
        const auto result = qvariant_cast<PixelStatistics>(statistics.first()[0]);
        QCOMPARE(result.blueCount, qsizetype{1});
        QCOMPARE(result.count({QStringLiteral("#00ff00"), 16}), qsizetype{2});
    }

    void viewportEditRejectsAnOlderPendingFreeze()
    {
        std::vector<std::function<void()>> tasks;
        StatisticsCoordinator coordinator(
            [] { return qint64{0}; },
            [](std::uintptr_t, const QList<std::optional<QRect>>&) {
                return QList<std::optional<PixelStatistics>>{};
            },
            [&tasks](std::function<void()> task) { tasks.push_back(std::move(task)); },
            [](std::uintptr_t) {
                QImage image(8, 4, QImage::Format_RGB32);
                image.fill(Qt::black);
                return image;
            }, nullptr);
        ReplicaWindow window(99, *gateway_, coordinator, *audioFeedbackManager_);
        window.selectSource({123, QStringLiteral("EVE - Alpha")});
        window.setStatisticsInterval(0);
        window.show();
        window.beginRegionCapture();
        QCOMPARE(tasks.size(), std::size_t{1});
        QVERIFY(!window.isCapturingRegion());
        const int createsWhilePending = state_.createCount;
        window.renewThumbnail();
        QCOMPARE(state_.createCount, createsWhilePending);
        window.setCapturedRegion(QRect(100, 100, 80, 40));
        tasks.front()();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
        QVERIFY(coordinator.isIdle());
        QVERIFY(!window.isCapturingRegion());
        QCOMPARE(window.capturedRegion(), std::optional<QRect>(QRect(100, 100, 80, 40)));
        window.beginRegionCapture();
        QCOMPARE(tasks.size(), std::size_t{2});
        tasks.back()();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
        QVERIFY(window.isCapturingRegion());
        window.cancelRegionCapture();
    }
    void init()
    {
        fake = &state_;
        gateway_ = std::make_unique<NativeGateway>(functions());
        statisticsCoordinator_ = std::make_unique<StatisticsCoordinator>(*gateway_);
        audioFeedbackManager_ = std::make_unique<AudioFeedbackManager>();
    }

    void cleanup()
    {
        audioFeedbackManager_.reset();
        statisticsCoordinator_.reset();
        gateway_.reset();
        fake = nullptr;
        state_ = WindowFakeState{};
    }

    void aspectFitCentersWidescreenContent()
    {
        QCOMPARE(
            ReplicaWindow::aspectFit(QSize(1920, 1080), QSize(800, 600)),
            QRect(0, 75, 800, 450));
        QCOMPARE(
            ReplicaWindow::aspectFit(QSize(0, 1080), QSize(800, 600)),
            QRect());
    }

    void deviceAspectFitScalesDestinationByDevicePixelRatio()
    {
        QCOMPARE(
            ReplicaWindow::deviceAspectFit(QSize(1920, 1080), QSize(800, 600), 1.0),
            QRect(0, 75, 800, 450));

        const QRect fitted = ReplicaWindow::deviceAspectFit(
            QSize(1920, 1080), QSize(800, 600), 1.5);
        QCOMPARE(fitted.width(), 1200);
        QCOMPARE(fitted.height(), 675);
        QCOMPARE(
            fitted,
            ReplicaWindow::aspectFit(QSize(1920, 1080), QSize(1200, 900)));

        QCOMPARE(
            ReplicaWindow::deviceAspectFit(QSize(1920, 1080), QSize(800, 600), 0.0),
            ReplicaWindow::aspectFit(QSize(1920, 1080), QSize(800, 600)));
    }

    void startsFramelessTopmostWithInstruction()
    {
        ReplicaWindow window(1, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);

        QVERIFY(window.windowFlags().testFlag(Qt::FramelessWindowHint));
        QVERIFY(window.windowFlags().testFlag(Qt::WindowStaysOnTopHint));
        QCOMPARE(window.emptyLabel()->text(), QStringLiteral("右键选择完整窗口"));
        QVERIFY(!window.source().has_value());
    }

    void automaticActivationCooldownUsesFiveSecondDefaultAndHalfSecondSteps()
    {
        ReplicaWindow window(1, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);

        QCOMPARE(window.property("automaticActivationCooldownMs").toInt(), 5000);
        window.setProperty("automaticActivationCooldownMs", 1000);
        QCOMPARE(window.property("automaticActivationCooldownMs").toInt(), 1000);
        window.setProperty("automaticActivationCooldownMs", 1250);
        QCOMPARE(window.property("automaticActivationCooldownMs").toInt(), 1000);
        window.setProperty("automaticActivationCooldownMs", 10000);
        QCOMPARE(window.property("automaticActivationCooldownMs").toInt(), 10000);
    }

    void emptyEnumerationCreatesDisabledMenuEntry()
    {
        ReplicaWindow window(1, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        const std::unique_ptr<QMenu> menu(window.buildContextMenu());
        auto* sourceMenu = menu->actions().first()->menu();

        QVERIFY(sourceMenu != nullptr);
        QCOMPARE(sourceMenu->actions().first()->text(), QStringLiteral("未找到 EVE 窗口"));
        QVERIFY(!sourceMenu->actions().first()->isEnabled());
    }

    void selectingSourceUpdatesCenteredDestinationAndCleansOnce()
    {
        setRecord(state_);
        int closedCount = 0;
        ReplicaWindow window(7, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_, [&closedCount](ReplicaId) {
            ++closedCount;
        });
        window.resize(800, 600);
        window.show();
        QCoreApplication::processEvents();

        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});

        QVERIFY(window.source().has_value());
        QCOMPARE(window.size(), QSize(800, 450));
        QCOMPARE(state_.lastRect.left, 5);
        QCOMPARE(state_.lastRect.top, 3);
        QCOMPARE(state_.lastRect.right, 794);
        QCOMPARE(state_.lastRect.bottom, 447);
        QVERIFY(state_.updateCount > 0);
        window.close();
        window.close();
        QCoreApplication::processEvents();
        QCOMPARE(state_.destroyCount, 1);
        QCOMPARE(closedCount, 1);
    }

    void inactiveThumbnailRecoversWhenPropertyUpdatesKeepAnOldFrame()
    {
        ReplicaWindow window(7, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        window.setStatisticsInterval(0);
        window.show();
        window.selectSource({123, QStringLiteral("EVE - Alpha")});
        window.setCapturedRegion(QRect(100, 100, 200, 150));
        const QRect geometry = window.geometry();
        const auto originalHandle = window.thumbnail_->nativeHandle();
        state_.stalledThumbnail = originalHandle;
        state_.sourceRevision = 2;
        window.updateThumbnail();
        QCOMPARE(state_.displayedRevision, 1); // Successful update, stale frame.

        QWidget otherWindow;
        otherWindow.show();
        QApplication::setActiveWindow(&otherWindow);
        QCoreApplication::processEvents();
        QVERIFY(!window.isActiveWindow());

        QCOMPARE(window.sourceCheckTimer_->interval(), 2000);
        window.sourceCheckTimer_->setInterval(30);
        QTRY_COMPARE_WITH_TIMEOUT(state_.displayedRevision, 2, 500);
        QVERIFY(window.thumbnail_->nativeHandle() != originalHandle);
        QCOMPARE(state_.lastDestroyedThumbnail, originalHandle);
        QCOMPARE(window.capturedRegion(), std::optional<QRect>(QRect(100, 100, 200, 150)));
        QCOMPARE(window.geometry(), geometry);
        QCOMPARE(state_.lastVisible, 1);
        QCOMPARE(state_.captureCount, 0);
        QCOMPARE(state_.activateCount, 0);
        QVERIFY(!window.isActiveWindow());

        window.close();
        const int updatesAfterClose = state_.updateCount;
        QTest::qWait(120);
        QCOMPARE(state_.updateCount, updatesAfterClose);
    }

    void thumbnailRenewalSkipsHiddenMinimizedAndSourceNameViews()
    {
        ReplicaWindow window(7, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        window.setStatisticsInterval(0);
        window.selectSource({123, QStringLiteral("EVE - Alpha")});
        const int resumesBefore = state_.resumeCount;
        window.renewThumbnail();
        QCOMPARE(state_.createCount, 1);
        QCOMPARE(state_.resumeCount, resumesBefore);
        window.show();
        int createsBefore = state_.createCount;
        window.hide();
        window.renewThumbnail();
        QCOMPARE(state_.createCount, createsBefore);

        window.show();
        window.setSourceNameVisible(true);
        createsBefore = state_.createCount;
        window.renewThumbnail();
        QCOMPARE(state_.createCount, createsBefore);
        QCOMPARE(state_.lastVisible, 0);

        window.setSourceNameVisible(false);
        window.showMinimized();
        QCoreApplication::processEvents();
        QVERIFY(window.isMinimized());
        window.renewThumbnail();
        QCOMPARE(state_.createCount, createsBefore);
        window.showNormal();
        window.renewThumbnail();
        QVERIFY(state_.createCount > createsBefore);
        QCOMPARE(state_.lastVisible, 1);
    }

    void minimizedSourceResumesWithoutClickOrActivation()
    {
        ReplicaWindow window(7, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        window.setStatisticsInterval(0);
        window.show();
        window.selectSource({123, QStringLiteral("EVE - Alpha")});
        QWidget otherWindow;
        otherWindow.show();
        QApplication::setActiveWindow(&otherWindow);
        QCoreApplication::processEvents();
        QVERIFY(!window.isActiveWindow());

        state_.sourceMinimized = true; // Source minimized by Win+D.
        state_.sourceRevision = 2;
        window.updateThumbnail();
        QCOMPARE(state_.displayedRevision, 1);
        const int resumesBefore = state_.resumeCount;
        window.sourceCheckTimer_->setInterval(30);
        QTRY_COMPARE_WITH_TIMEOUT(state_.displayedRevision, 2, 500);
        QVERIFY(state_.resumeCount > resumesBefore);
        QVERIFY(!state_.sourceMinimized);
        QCOMPARE(state_.activateCount, 0);
        QVERIFY(!window.isActiveWindow());
    }

    void failedThumbnailRenewalKeepsExistingRelationship_data()
    {
        QTest::addColumn<int>("failureStage");
        QTest::newRow("registration") << 0;
        QTest::newRow("source-size") << 1;
        QTest::newRow("configuration") << 2;
        QTest::newRow("background-resume") << 3;
    }

    void failedThumbnailRenewalKeepsExistingRelationship()
    {
        QFETCH(int, failureStage);
        ReplicaWindow window(7, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        window.setStatisticsInterval(0);
        window.show();
        window.selectSource({123, QStringLiteral("EVE - Alpha")});
        const auto originalHandle = window.thumbnail_->nativeHandle();
        QSignalSpy unavailable(&window, &ReplicaWindow::sourceUnavailable);
        if (failureStage == 0) { state_.createStatus = -3; }
        if (failureStage == 1) { state_.sourceStatus = -7; }
        if (failureStage == 2) { state_.rejectedThumbnail = originalHandle + 1; }
        if (failureStage == 3) { state_.resumeStatus = -5; }

        window.renewThumbnail();

        QVERIFY(window.isVisible());
        QVERIFY(window.source().has_value());
        QCOMPARE(window.thumbnail_->nativeHandle(), originalHandle);
        QCOMPARE(unavailable.count(), 0);
        QCOMPARE(state_.destroyCount, failureStage == 0 || failureStage == 3 ? 0 : 1);
        QVERIFY(state_.lastDestroyedThumbnail != originalHandle);

        state_.createStatus = 0;
        state_.sourceStatus = 0;
        state_.rejectedThumbnail = 0;
        state_.resumeStatus = 0;
        window.renewThumbnail();
        QVERIFY(window.thumbnail_->nativeHandle() != originalHandle);
        QCOMPARE(state_.lastDestroyedThumbnail, originalHandle);
        QCOMPARE(state_.captureCount, 0);
        QCOMPARE(state_.activateCount, 0);
    }

    void shortLeftClickActivatesSelectedSource()
    {
        ReplicaWindow window(1, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        window.show();
        window.selectSource(WindowInfo{123, QStringLiteral("EVE - Alpha")});
        window.setSourceNameVisible(true);

        QTest::mouseClick(
            &window,
            Qt::LeftButton,
            Qt::NoModifier,
            QPoint(100, 100));

        QCOMPARE(state_.activateCount, 1);
        QCOMPARE(state_.activatedHwnd, std::uintptr_t{123});
    }

    void disabledClickRaiseStillActivatesSelectedSource()
    {
        ReplicaWindow window(1, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        window.show();
        window.selectSource(WindowInfo{123, QStringLiteral("EVE - Alpha")});
        window.setRaiseOnClickEnabled(false);

        QVERIFY(!window.raiseOnClickEnabled());
        QTest::mouseClick(
            &window,
            Qt::LeftButton,
            Qt::NoModifier,
            QPoint(100, 100));

        QCOMPARE(state_.activateCount, 1);
        QCOMPARE(state_.activatedHwnd, std::uintptr_t{123});
    }

    void movementBeforeLongPressThresholdDoesNotMoveWindow()
    {
        ReplicaWindow window(1, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        window.move(100, 100);
        window.show();
        window.selectSource(WindowInfo{123, QStringLiteral("EVE - Alpha")});
        const QPoint originalPosition = window.pos();

        QTest::mousePress(
            &window,
            Qt::LeftButton,
            Qt::NoModifier,
            QPoint(40, 40));
        QMouseEvent moveEvent(
            QEvent::MouseMove,
            QPointF(80, 80),
            QPointF(window.mapToGlobal(QPoint(80, 80))),
            Qt::NoButton,
            Qt::LeftButton,
            Qt::NoModifier);
        QApplication::sendEvent(&window, &moveEvent);

        QCOMPARE(window.pos(), originalPosition);
        QTest::mouseRelease(
            &window,
            Qt::LeftButton,
            Qt::NoModifier,
            QPoint(80, 80));
        QCOMPARE(state_.activateCount, 1);
    }

    void longLeftPressEnablesDraggingWithoutActivation()
    {
        ReplicaWindow window(1, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        window.move(100, 100);
        window.show();
        window.selectSource(WindowInfo{123, QStringLiteral("EVE - Alpha")});
        const QPoint originalPosition = window.pos();

        QTest::mousePress(
            &window,
            Qt::LeftButton,
            Qt::NoModifier,
            QPoint(40, 40));
        QTest::qWait(220);
        QMouseEvent moveEvent(
            QEvent::MouseMove,
            QPointF(80, 80),
            QPointF(window.mapToGlobal(QPoint(80, 80))),
            Qt::NoButton,
            Qt::LeftButton,
            Qt::NoModifier);
        QApplication::sendEvent(&window, &moveEvent);
        QTest::mouseRelease(
            &window,
            Qt::LeftButton,
            Qt::NoModifier,
            QPoint(80, 80));

        QVERIFY(window.pos() != originalPosition);
        QCOMPARE(state_.activateCount, 0);
    }

    void sourceNameModePaintsBlackBackdrop()
    {
        ReplicaWindow window(1, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        window.resize(360, 203);
        window.show();
        window.selectSource(WindowInfo{123, QStringLiteral("EVE - MysteriousmanA")});
        window.setSourceNameVisible(true);
        QCoreApplication::processEvents();

        const QImage snapshot = window.grab().toImage();
        QCOMPARE(snapshot.pixelColor(20, window.height() / 2), QColor(5, 9, 12));
    }

    void sourceSizeFailureDestroysNewThumbnail()
    {
        state_.sourceStatus = -7;
        ReplicaWindow window(3, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);

        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});

        QCOMPARE(state_.destroyCount, 1);
        QVERIFY(!window.source().has_value());
        QVERIFY(window.emptyLabel()->text().startsWith(QStringLiteral("无法显示窗口")));
    }

    void updateFailureClosesReplicaAfterReportingUnavailableSource()
    {
        state_.updateStatus = -9;
        int closedCount = 0;
        ReplicaWindow window(4, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_, [&closedCount](ReplicaId) {
            ++closedCount;
        });
        QSignalSpy unavailable(&window, &ReplicaWindow::sourceUnavailable);
        window.setSourceNameVisible(true);

        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});

        QCOMPARE(state_.destroyCount, 1);
        QVERIFY(!window.source().has_value());
        QCOMPARE(closedCount, 1);
        QCOMPARE(unavailable.count(), 1);
        QCOMPARE(unavailable.first().first().toString(), QStringLiteral("EVE Online - Alpha"));
    }

    void sourceDisappearanceClosesStationaryWindow()
    {
        setRecord(state_);
        ReplicaWindow window(80, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        QSignalSpy unavailable(&window, &ReplicaWindow::sourceUnavailable);
        window.show();
        QCoreApplication::processEvents();
        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});
        QVERIFY(window.isVisible());

        state_.updateStatus = -9;
        QTRY_VERIFY_WITH_TIMEOUT(!window.isVisible(), 6000);
        QVERIFY(!window.source().has_value());
        QCOMPARE(unavailable.count(), 1);
    }

    void wheelZoomScalesWindowKeepingAspectAndThumbnailFilled()
    {
        setRecord(state_);
        ReplicaWindow window(5, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        window.resize(800, 600);
        window.show();
        QCoreApplication::processEvents();
        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});

        QCOMPARE(window.size(), QSize(800, 450));
        QCOMPARE(state_.lastRect.left, 5);
        QCOMPARE(state_.lastRect.top, 3);
        QCOMPARE(state_.lastRect.right, 794);
        QCOMPARE(state_.lastRect.bottom, 447);
        const int updatesBefore = state_.updateCount;

        const QPointF pos(400, 300);
        const QPointF globalPos(500, 400);
        QWheelEvent zoomIn(
            pos, globalPos, QPoint(0, 0), QPoint(0, 120),
            Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(&window, &zoomIn);
        QCoreApplication::processEvents();

        QCOMPARE(window.size().width(), 920);
        QCOMPARE(window.size().height(), 518);
        QVERIFY(state_.updateCount > updatesBefore);
        QCOMPARE(state_.lastRect.left, 5);
        QCOMPARE(state_.lastRect.top, 3);
        QCOMPARE(state_.lastRect.right, 915);
        QCOMPARE(state_.lastRect.bottom, 515);

        QWheelEvent zoomOut(
            pos, globalPos, QPoint(0, 0), QPoint(0, -120),
            Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(&window, &zoomOut);
        QCoreApplication::processEvents();

        QCOMPARE(window.size(), QSize(800, 450));
        QCOMPARE(state_.lastRect.left, 5);
        QCOMPARE(state_.lastRect.top, 3);
        QCOMPARE(state_.lastRect.right, 794);
        QCOMPARE(state_.lastRect.bottom, 447);
    }

    void wheelZoomHonorsMinimumSize()
    {
        ReplicaWindow window(6, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        window.resize(200, 130);
        window.show();
        QCoreApplication::processEvents();

        const QPointF pos(100, 65);
        const QPointF globalPos(200, 300);
        for (int i = 0; i < 10; ++i) {
            QWheelEvent zoomOut(
                pos, globalPos, QPoint(0, 0), QPoint(0, -120),
                Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            QApplication::sendEvent(&window, &zoomOut);
        }
        QCoreApplication::processEvents();

        QVERIFY(window.width() >= window.minimumWidth());
        QVERIFY(window.height() >= window.minimumHeight());
    }

    void minimumSizeTracksContentAspectWithThirtyPixelShortestEdge()
    {
        ReplicaWindow window(7, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        QCOMPARE(window.minimumSize(), QSize(30, 30));

        setRecord(state_);
        window.show();
        QCoreApplication::processEvents();
        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});

        QCOMPARE(window.minimumSize(), QSize(54, 30));
        QCOMPARE(window.size(), QSize(360, 203));

        const QPointF pos(100, 100);
        const QPointF globalPos(200, 200);
        for (int i = 0; i < 20; ++i) {
            QWheelEvent zoomOut(
                pos, globalPos, QPoint(0, 0), QPoint(0, -120),
                Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            QApplication::sendEvent(&window, &zoomOut);
        }
        QCoreApplication::processEvents();
        QCOMPARE(window.size(), QSize(54, 30));

        window.applyRegionPreset({QStringLiteral("竖屏"), QRect(0, 0, 9, 16), 0});
        QCOMPARE(window.minimumSize(), QSize(30, 54));
    }

    void contextMenuContainsCaptureRegionBetweenActions()
    {
        setRecord(state_);
        ReplicaWindow window(1, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});

        const std::unique_ptr<QMenu> menu(window.buildContextMenu());
        const auto actions = menu->actions();
        QCOMPARE(actions.size(), 5);
        QCOMPARE(actions[0]->text(), QStringLiteral("选择完整窗口"));
        QCOMPARE(actions[1]->text(), QStringLiteral("截取区域"));
        QVERIFY(actions[1]->isEnabled());
        QCOMPARE(actions[2]->text(), QStringLiteral("应用区域预设"));
        QVERIFY(!actions[2]->isEnabled());
        QCOMPARE(actions[3]->isSeparator(), true);
        QCOMPARE(actions[4]->text(), QStringLiteral("关闭小窗"));
    }

    void contextMenuAppliesConfiguredRegionPreset()
    {
        setRecord(state_);
        ReplicaWindow window(1, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});
        window.setRegionPresets({
            {QStringLiteral("总览"), QRect(120, 80, 640, 480), 1500},
            {QStringLiteral("无人机"), QRect(1400, 700, 400, 300), 0},
        });
        QSignalSpy captured(&window, &ReplicaWindow::regionCaptured);

        const std::unique_ptr<QMenu> menu(window.buildContextMenu());
        auto* presetsMenu = menu->actions()[2]->menu();
        QVERIFY(presetsMenu != nullptr);
        QCOMPARE(presetsMenu->actions().size(), 2);
        QCOMPARE(presetsMenu->actions().first()->text(), QStringLiteral("总览"));
        presetsMenu->actions().first()->trigger();

        QCOMPARE(captured.count(), 1);
        QVERIFY(window.capturedRegion().has_value());
        QCOMPARE(*window.capturedRegion(), QRect(120, 80, 640, 480));
        QCOMPARE(window.statisticsIntervalMs(), 1500);
        QCOMPARE(state_.lastSource.left, 120);
        QCOMPARE(state_.lastSource.top, 80);
        QCOMPARE(state_.lastSource.right, 760);
        QCOMPARE(state_.lastSource.bottom, 560);
    }

    void captureRegionFreezesFrameAndRestoresOnEnter()
    {
        setRecord(state_);
        int capturedRegions = 0;
        QRect capturedRect;
        ReplicaWindow window(2, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        window.resize(800, 600);
        window.show();
        QCoreApplication::processEvents();
        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});
        window.setStatisticsInterval(0);
        connect(&window, &ReplicaWindow::regionCaptured, [&](const QRect& region) {
            ++capturedRegions;
            capturedRect = region;
        });

        window.beginRegionCapture();
        QTRY_VERIFY(window.isCapturingRegion());
        QVERIFY(window.isCapturingRegion());
        QVERIFY(state_.captureCalled);
        QCOMPARE(state_.lastVisible, 0);

        QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, QPoint(100, 100));
        QTest::mouseMove(&window, QPoint(300, 250));
        QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, QPoint(300, 250));

        QVERIFY(window.selectedRegion().has_value());
        QTest::keyClick(&window, Qt::Key_Return);

        QVERIFY(!window.isCapturingRegion());
        QCOMPARE(capturedRegions, 1);
        const auto expected = QRect(231, 236, 488, 366);
        QCOMPARE(capturedRect, expected);
        QVERIFY(window.capturedRegion().has_value());
        QCOMPARE(*window.capturedRegion(), expected);
        QCOMPARE(state_.lastVisible, 1);
        QCOMPARE(state_.lastSource.left, expected.left());
        QCOMPARE(state_.lastSource.top, expected.top());
        QCOMPARE(state_.lastSource.right, expected.right() + 1);
        QCOMPARE(state_.lastSource.bottom, expected.bottom() + 1);
        QCOMPARE(state_.freePixelCount, 1);
    }

    void captureRegionCancelRestoresSyncWithoutSignal()
    {
        setRecord(state_);
        int capturedRegions = 0;
        ReplicaWindow window(3, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        window.resize(800, 600);
        window.show();
        QCoreApplication::processEvents();
        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});
        window.setStatisticsInterval(0);
        connect(&window, &ReplicaWindow::regionCaptured, [&](const QRect&) {
            ++capturedRegions;
        });

        window.beginRegionCapture();
        QTRY_VERIFY(window.isCapturingRegion());
        QVERIFY(window.isCapturingRegion());
        QTest::keyClick(&window, Qt::Key_Escape);

        QVERIFY(!window.isCapturingRegion());
        QCOMPARE(capturedRegions, 0);
        QCOMPARE(state_.lastVisible, 1);
        QCOMPARE(state_.freePixelCount, 1);
    }

    void captureRegionKeepsThumbnailHiddenDuringRefresh()
    {
        setRecord(state_);
        ReplicaWindow window(31, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        window.resize(800, 600);
        window.show();
        QCoreApplication::processEvents();
        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});
        window.setStatisticsInterval(0);

        window.beginRegionCapture();
        QTRY_VERIFY(window.isCapturingRegion());
        QVERIFY(window.isCapturingRegion());
        QCOMPARE(state_.lastVisible, 0);

        // Resizing invokes the normal thumbnail refresh path. It must not make
        // the live DWM thumbnail cover the frozen selection frame again.
        window.resize(801, 601);
        QCoreApplication::processEvents();

        QCOMPARE(state_.lastVisible, 0);
        const int createsWhileFrozen = state_.createCount;
        window.renewThumbnail();
        QCOMPARE(state_.createCount, createsWhileFrozen);
        QCOMPARE(state_.lastVisible, 0);
        QTest::keyClick(&window, Qt::Key_Escape);
        QCOMPARE(state_.lastVisible, 1);
        window.renewThumbnail();
        QCOMPARE(state_.createCount, createsWhileFrozen + 1);
    }

    void captureRegionWithoutSelectionEnterDoesNothing()
    {
        setRecord(state_);
        int capturedRegions = 0;
        ReplicaWindow window(4, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        window.resize(800, 600);
        window.show();
        QCoreApplication::processEvents();
        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});
        window.setStatisticsInterval(0);
        connect(&window, &ReplicaWindow::regionCaptured, [&](const QRect&) {
            ++capturedRegions;
        });

        window.beginRegionCapture();
        QTRY_VERIFY(window.isCapturingRegion());
        QVERIFY(window.isCapturingRegion());
        QTest::keyClick(&window, Qt::Key_Return);

        QVERIFY(!window.isCapturingRegion());
        QCOMPARE(capturedRegions, 0);
        QCOMPARE(state_.lastVisible, 1);
    }

    void secondCaptureFreezesCurrentRegionNotFullFrame()
    {
        setRecord(state_);
        state_.captureWidth = 1920;
        state_.captureHeight = 1080;
        ReplicaWindow window(5, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        window.resize(800, 600);
        window.show();
        QCoreApplication::processEvents();
        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});
        window.setStatisticsInterval(0);

        window.beginRegionCapture();
        QTRY_VERIFY(window.isCapturingRegion());
        QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, QPoint(100, 100));
        QTest::mouseMove(&window, QPoint(300, 250));
        QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, QPoint(300, 250));
        QTest::keyClick(&window, Qt::Key_Return);
        QVERIFY(window.capturedRegion().has_value());
        const auto expectedRegion = QRect(231, 236, 488, 366);
        QCOMPARE(*window.capturedRegion(), expectedRegion);

        const int capturesBefore = state_.freePixelCount;
        state_.lastCapturedHwnd = 0;
        window.beginRegionCapture();
        QTRY_VERIFY(window.isCapturingRegion());
        QVERIFY(window.isCapturingRegion());
        QVERIFY(state_.freePixelCount > capturesBefore);
        QCOMPARE(state_.lastCapturedHwnd, window.winId());
        QTest::keyClick(&window, Qt::Key_Escape);
    }

    void sourceProcessNameAndRegionDescriptionAreExposed()
    {
        setRecord(state_);
        QString regionDescription;
        ReplicaWindow window(6, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        connect(&window, &ReplicaWindow::regionCaptureChanged, [&](const QString& description) {
            regionDescription = description;
        });
        window.resize(800, 600);
        window.show();
        QCoreApplication::processEvents();
        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});
        window.setStatisticsInterval(0);

        QCOMPARE(regionDescription, QStringLiteral("未截取"));

        window.beginRegionCapture();
        QTRY_VERIFY(window.isCapturingRegion());
        QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, QPoint(100, 100));
        QTest::mouseMove(&window, QPoint(300, 250));
        QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, QPoint(300, 250));
        QTest::keyClick(&window, Qt::Key_Return);
        QCOMPARE(regionDescription, QStringLiteral("488×366 @ (231,236)"));
    }

    void switchingSourcePreservesUsableRegionAndClips()
    {
        setRecord(state_);
        ReplicaWindow window(40, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        window.resize(800, 600);
        window.show();
        QCoreApplication::processEvents();
        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});
        window.applyRegionPreset({QStringLiteral("总览"), QRect(100, 100, 800, 600), 0});
        QVERIFY(window.capturedRegion().has_value());
        QCOMPARE(*window.capturedRegion(), QRect(100, 100, 800, 600));

        state_.width = 300;
        state_.height = 200;
        window.selectSource(WindowInfo{456, QStringLiteral("EVE Online - Beta")});

        QVERIFY(window.capturedRegion().has_value());
        QCOMPARE(*window.capturedRegion(), QRect(100, 100, 200, 100));
    }

    void switchingSourceDiscardsUnusableRegionWithMessage()
    {
        setRecord(state_);
        QString regionDescription;
        ReplicaWindow window(41, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        connect(&window, &ReplicaWindow::regionCaptureChanged, [&](const QString& description) {
            regionDescription = description;
        });
        window.resize(800, 600);
        window.show();
        QCoreApplication::processEvents();
        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});
        window.applyRegionPreset({QStringLiteral("角落"), QRect(1500, 500, 300, 400), 0});
        QVERIFY(window.capturedRegion().has_value());

        state_.width = 300;
        state_.height = 200;
        window.selectSource(WindowInfo{456, QStringLiteral("EVE Online - Beta")});

        QVERIFY(!window.capturedRegion().has_value());
        QCOMPARE(regionDescription, QStringLiteral("未发现区域，已重置"));
    }

    void applyingNonFittingPresetReportsResetMessage()
    {
        setRecord(state_);
        state_.width = 300;
        state_.height = 200;
        QString regionDescription;
        ReplicaWindow window(42, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        connect(&window, &ReplicaWindow::regionCaptureChanged, [&](const QString& description) {
            regionDescription = description;
        });
        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});
        window.applyRegionPreset({QStringLiteral("大区域"), QRect(500, 500, 400, 300), 0});

        QVERIFY(!window.capturedRegion().has_value());
        QCOMPARE(regionDescription, QStringLiteral("未发现区域，已重置"));
    }

    void selectingFullWindowClearsCapturedRegion()
    {
        setRecord(state_);
        ReplicaWindow window(50, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        window.resize(800, 600);
        window.show();
        QCoreApplication::processEvents();
        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});
        window.applyRegionPreset({QStringLiteral("总览"), QRect(100, 100, 800, 600), 0});
        QVERIFY(window.capturedRegion().has_value());

        const std::unique_ptr<QMenu> menu(window.buildContextMenu());
        auto* sourceMenu = menu->actions()[0]->menu();
        QVERIFY(sourceMenu != nullptr);
        sourceMenu->actions().first()->trigger();

        QVERIFY(!window.capturedRegion().has_value());
        QVERIFY(window.source().has_value());
    }

    void manualRegionInputSetsCapturedRegion()
    {
        setRecord(state_);
        int capturedRegions = 0;
        QRect capturedRect;
        ReplicaWindow window(60, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        connect(&window, &ReplicaWindow::regionCaptured, [&](const QRect& region) {
            ++capturedRegions;
            capturedRect = region;
        });
        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});

        window.setCapturedRegion(QRect(120, 80, 640, 480));

        QVERIFY(window.capturedRegion().has_value());
        QCOMPARE(*window.capturedRegion(), QRect(120, 80, 640, 480));
        QCOMPARE(capturedRegions, 1);
        QCOMPARE(capturedRect, QRect(120, 80, 640, 480));
        QCOMPARE(state_.lastSource.left, 120);
        QCOMPARE(state_.lastSource.right, 760);
    }

    void manualRegionInputBeyondBoundsResetsRegion()
    {
        setRecord(state_);
        state_.width = 300;
        state_.height = 200;
        QString regionDescription;
        ReplicaWindow window(61, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        connect(&window, &ReplicaWindow::regionCaptureChanged, [&](const QString& description) {
            regionDescription = description;
        });
        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});

        window.setCapturedRegion(QRect(500, 500, 400, 300));

        QVERIFY(!window.capturedRegion().has_value());
        QCOMPARE(regionDescription, QStringLiteral("未发现区域，已重置"));
    }

    void colorLogicTriggerFlashesBorderAndStopsWhenConditionDrops()
    {
        setRecord(state_);
        state_.captureWidth = 2;
        state_.captureHeight = 1;
        state_.captureColors = {qRgb(255, 0, 0), qRgb(255, 0, 0)};
        ReplicaWindow window(70, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        QSignalSpy flashChanged(&window, &ReplicaWindow::flashStateChanged);
        window.setStatisticsInterval(500);
        window.setColorRules({
            {1, QStringLiteral("红态"), QStringLiteral("红色"), QStringLiteral(">=2"),
             TriggerFeedback::None, QString{}, PostTriggerAction::Loop},
        });
        window.setColorLogicSlot(1, 1);
        window.setColorLogicSlotColor(1, QColor(0, 255, 0));
        window.setColorLogicEnabled(true);
        window.setActiveColorLogicSlot(1);
        window.resize(200, 120);
        window.show();
        QCoreApplication::processEvents();
        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});

        QTRY_VERIFY_WITH_TIMEOUT(window.isFlashActive(), 2000);
        QVERIFY(flashChanged.count() >= 1);
        QCOMPARE(window.grab().toImage().pixelColor(1, 1), QColor(0, 255, 0));

        state_.captureColors = {qRgb(0, 0, 0), qRgb(0, 0, 0)};
        QTRY_VERIFY_WITH_TIMEOUT(!window.isFlashActive(), 2000);
    }

    void colorLogicTriggerActivatesSourceWindow()
    {
        setRecord(state_);
        state_.captureWidth = 2;
        state_.captureHeight = 1;
        state_.captureColors = {qRgb(255, 0, 0), qRgb(255, 0, 0)};
        ReplicaWindow window(73, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        window.setStatisticsInterval(500);
        window.setColorRules({
            {1, QStringLiteral("红态"), QStringLiteral("红色"), QStringLiteral(">=2"),
             TriggerFeedback::None, QString{}, PostTriggerAction::Loop},
        });
        window.setColorLogicSlot(1, 1);
        window.setColorLogicEnabled(true);
        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});

        QTRY_COMPARE_WITH_TIMEOUT(window.colorLogicTriggerCount(1), 1, 1500);

        // Activation runs in a queued callback after the trigger is observed.
        QTRY_COMPARE_WITH_TIMEOUT(state_.activateCount, 1, 1500);
        QCOMPARE(state_.activatedHwnd, std::uintptr_t{123});
    }

    void disabledAutomaticActivationLeavesColorLogicRunning()
    {
        setRecord(state_);
        state_.captureWidth = 2;
        state_.captureHeight = 1;
        state_.captureColors = {qRgb(255, 0, 0), qRgb(255, 0, 0)};
        ReplicaWindow window(74, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        window.setStatisticsInterval(500);
        window.setColorRules({
            {1, QStringLiteral("红态"), QStringLiteral("红色"), QStringLiteral(">=2"),
             TriggerFeedback::None, QString{}, PostTriggerAction::Loop},
        });
        window.setColorLogicSlot(1, 1);
        window.setColorLogicEnabled(true);
        window.setAutomaticActivationEnabled(false);
        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});

        QTRY_COMPARE_WITH_TIMEOUT(window.colorLogicTriggerCount(1), 1, 1500);
        QVERIFY(!window.automaticActivationEnabled());
        QCOMPARE(state_.activateCount, 0);
    }

    void colorLogicRequiresAllNonEmptyTriggerConditions()
    {
        setRecord(state_);
        state_.captureWidth = 3;
        state_.captureHeight = 1;
        state_.captureColors = {
            qRgb(255, 0, 0), qRgb(255, 0, 0), qRgb(0, 0, 0)};
        ReplicaWindow window(71, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        ColorRule rule;
        rule.number = 1;
        rule.note = QStringLiteral("红白同时满足");
        rule.triggerCondition1 = {
            QStringLiteral("红色"), QStringLiteral(">=2")};
        rule.triggerCondition2 = {
            QStringLiteral("白色"), QStringLiteral(">=1")};
        window.setStatisticsInterval(500);
        window.setColorRules({rule});
        window.setColorLogicSlot(1, 1);
        window.setColorLogicEnabled(true);
        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});

        QTest::qWait(700);
        QCOMPARE(window.colorLogicTriggerCount(1), 0);

        state_.captureColors = {
            qRgb(255, 0, 0), qRgb(255, 0, 0), qRgb(255, 255, 255)};
        QTRY_COMPARE_WITH_TIMEOUT(window.colorLogicTriggerCount(1), 1, 1500);
    }

    void colorLogicIgnoresOneEmptyConditionAndRejectsTwoEmptyConditions()
    {
        setRecord(state_);
        state_.captureWidth = 2;
        state_.captureHeight = 1;
        state_.captureColors = {qRgb(255, 0, 0), qRgb(255, 0, 0)};
        ReplicaWindow window(72, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        ColorRule oneCondition;
        oneCondition.number = 1;
        oneCondition.triggerCondition1 = {
            QStringLiteral("红色"), QStringLiteral(">=2")};
        ColorRule noConditions;
        noConditions.number = 2;
        window.setStatisticsInterval(500);
        window.setColorRules({oneCondition, noConditions});
        window.setColorLogicSlot(1, 1);
        window.setColorLogicSlot(2, 2);
        window.setColorLogicEnabled(true);
        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});

        QTRY_COMPARE_WITH_TIMEOUT(window.colorLogicTriggerCount(1), 1, 1500);
        window.setActiveColorLogicSlot(2);
        QTest::qWait(700);
        QCOMPARE(window.colorLogicTriggerCount(2), 0);
    }

    void hiddenWindowContinuesAsynchronousPixelStatistics()
    {
        setRecord(state_);
        state_.captureWidth = 4;
        state_.captureHeight = 1;
        state_.captureColors = {
            qRgb(255, 0, 0),
            qRgb(210, 80, 40),
            qRgb(255, 255, 255),
            qRgb(0, 0, 0),
        };
        ReplicaWindow window(7, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        QSignalSpy statistics(&window, &ReplicaWindow::pixelStatisticsChanged);
        QSignalSpy unavailable(&window, &ReplicaWindow::pixelStatisticsUnavailable);

        window.setStatisticsInterval(500);
        window.show();
        window.hide();
        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});

        QTRY_COMPARE_WITH_TIMEOUT(statistics.count(), 1, 1500);
        QCOMPARE(unavailable.count(), 1);
        QCOMPARE(state_.lastCapturedHwnd, std::uintptr_t{123});
        QCOMPARE(statistics.first()[0].toLongLong(), 2);
        QCOMPARE(statistics.first()[1].toLongLong(), 1);
    }

    void disablingPixelStatisticsStopsAndReenablingResumes()
    {
        setRecord(state_);
        state_.captureWidth = 2;
        state_.captureHeight = 1;
        state_.captureColors = {qRgb(255, 0, 0), qRgb(255, 255, 255)};
        ReplicaWindow window(8, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        QSignalSpy statistics(&window, &ReplicaWindow::pixelStatisticsChanged);
        QSignalSpy unavailable(&window, &ReplicaWindow::pixelStatisticsUnavailable);

        window.setStatisticsInterval(500);
        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});
        QTRY_COMPARE_WITH_TIMEOUT(statistics.count(), 1, 1500);
        const int capturesBeforeDisable = state_.captureCount;

        window.setStatisticsInterval(0);
        QCOMPARE(unavailable.count(), 2);
        QTest::qWait(700);
        QCOMPARE(state_.captureCount, capturesBeforeDisable);
        QCOMPARE(statistics.count(), 1);

        window.setStatisticsInterval(500);
        QTRY_COMPARE_WITH_TIMEOUT(statistics.count(), 2, 1500);
        QVERIFY(state_.captureCount > capturesBeforeDisable);
    }

    void colorLogicSlotFeedbackNoneJumpsToOtherEnableState()
    {
        setRecord(state_);
        state_.captureWidth = 4;
        state_.captureHeight = 1;
        state_.captureColors = {
            qRgb(255, 0, 0), qRgb(255, 0, 0), qRgb(255, 0, 0), qRgb(0, 0, 0),
        };
        ReplicaWindow window(9, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        QSignalSpy slotChanged(&window, &ReplicaWindow::activeColorLogicSlotChanged);
        window.setStatisticsInterval(500);
        window.setColorRules({
            {1, QStringLiteral("红色告警"), QStringLiteral("红色"), QStringLiteral(">=3"),
             TriggerFeedback::None, QString{}, PostTriggerAction::Jump},
            {2, QStringLiteral("目标"), QStringLiteral("白色"), QStringLiteral(">=1"),
             TriggerFeedback::None, QString{}, PostTriggerAction::Loop},
        });
        window.setColorLogicSlot(1, 1);
        window.setColorLogicSlot(2, 2);
        window.setColorLogicEnabled(true);
        window.setActiveColorLogicSlot(1);
        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});

        QTRY_COMPARE_WITH_TIMEOUT(slotChanged.count(), 1, 1500);
        QCOMPARE(slotChanged.first().first().toInt(), 2);
        QCOMPARE(window.activeColorLogicSlot(), 2);
    }

    void colorLogicSlotIsEdgeTriggeredAcrossEnableStates()
    {
        setRecord(state_);
        state_.captureWidth = 2;
        state_.captureHeight = 1;
        state_.captureColors = {qRgb(255, 0, 0), qRgb(255, 0, 0)};
        ReplicaWindow window(11, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        QSignalSpy slotChanged(&window, &ReplicaWindow::activeColorLogicSlotChanged);
        window.setStatisticsInterval(500);
        window.setColorRules({
            {1, QStringLiteral("红态"), QStringLiteral("红色"), QStringLiteral(">=2"),
             TriggerFeedback::None, QString{}, PostTriggerAction::Jump},
            {2, QStringLiteral("白态"), QStringLiteral("白色"), QStringLiteral(">=2"),
             TriggerFeedback::None, QString{}, PostTriggerAction::Jump},
        });
        window.setColorLogicSlot(1, 1);
        window.setColorLogicSlot(2, 2);
        window.setColorLogicEnabled(true);
        window.setActiveColorLogicSlot(1);
        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});

        QTRY_COMPARE_WITH_TIMEOUT(slotChanged.count(), 1, 1500);
        QCOMPARE(slotChanged.last().first().toInt(), 2);
        QTest::qWait(700);
        QCOMPARE(slotChanged.count(), 1);

        state_.captureColors = {qRgb(255, 255, 255), qRgb(255, 255, 255)};
        QTRY_COMPARE_WITH_TIMEOUT(slotChanged.count(), 2, 1500);
        QCOMPARE(slotChanged.last().first().toInt(), 1);
        QTest::qWait(700);
        QCOMPARE(slotChanged.count(), 2);

        state_.captureColors = {qRgb(255, 0, 0), qRgb(255, 0, 0)};
        QTRY_COMPARE_WITH_TIMEOUT(slotChanged.count(), 3, 1500);
        QCOMPARE(slotChanged.last().first().toInt(), 2);
    }

    void colorLogicSlotWithEmptyAudioCompletesImmediately()
    {
        setRecord(state_);
        state_.captureWidth = 2;
        state_.captureHeight = 1;
        state_.captureColors = {qRgb(255, 0, 0), qRgb(255, 0, 0)};
        ReplicaWindow window(10, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        QSignalSpy slotChanged(&window, &ReplicaWindow::activeColorLogicSlotChanged);
        window.setStatisticsInterval(500);
        window.setColorRules({
            {1, QStringLiteral("音频"), QStringLiteral("红色"), QStringLiteral(">=2"),
             TriggerFeedback::Audio, QString{}, PostTriggerAction::Jump},
            {2, QStringLiteral("目标"), QStringLiteral("白色"), QStringLiteral(">=2"),
             TriggerFeedback::None, QString{}, PostTriggerAction::Loop},
        });
        window.setColorLogicSlot(1, 1);
        window.setColorLogicSlot(2, 2);
        window.setColorLogicEnabled(true);
        window.setActiveColorLogicSlot(1);
        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});

        QTRY_COMPARE_WITH_TIMEOUT(slotChanged.count(), 1, 1500);
        QCOMPARE(slotChanged.first().first().toInt(), 2);
    }

    void colorLogicSlotWithMissingAudioDoesNotRunPostAction()
    {
        setRecord(state_);
        state_.captureWidth = 2;
        state_.captureHeight = 1;
        state_.captureColors = {qRgb(255, 0, 0), qRgb(255, 0, 0)};
        ReplicaWindow window(10, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        QSignalSpy slotChanged(&window, &ReplicaWindow::activeColorLogicSlotChanged);
        window.setStatisticsInterval(500);
        window.setColorRules({
            {1, QStringLiteral("音频"), QStringLiteral("红色"), QStringLiteral(">=2"),
             TriggerFeedback::Audio,
             QStringLiteral("C:/__eve_viola_missing_feedback__.wav"),
             PostTriggerAction::Jump},
            {2, QStringLiteral("目标"), QStringLiteral("白色"), QStringLiteral(">=2"),
             TriggerFeedback::None, QString{}, PostTriggerAction::Loop},
        });
        window.setColorLogicSlot(1, 1);
        window.setColorLogicSlot(2, 2);
        window.setColorLogicEnabled(true);
        window.setActiveColorLogicSlot(1);
        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});

        QTest::qWait(700);
        QCOMPARE(slotChanged.count(), 0);
        QCOMPARE(window.colorLogicTriggerCount(1), 1);
    }

    void enabledSlotColorsTheWindowBorder()
    {
        ReplicaWindow window(1, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        QCOMPARE(window.borderColor(), QColor(255, 255, 255));

        window.setColorLogicSlotColor(2, QColor(255, 0, 0));
        window.setColorLogicEnabled(true);
        window.setActiveColorLogicSlot(2);
        QCOMPARE(window.borderColor(), QColor(255, 0, 0));

        window.setColorLogicEnabled(false);
        QCOMPARE(window.borderColor(), QColor(255, 255, 255));

        window.setColorLogicEnabled(true);
        window.setActiveColorLogicSlot(1);
        QCOMPARE(window.borderColor(), QColor(46, 204, 64));
    }

    void middleClickTogglesOnlyActiveColorLogicSlot()
    {
        ReplicaWindow window(1, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        QSignalSpy slotChanged(&window, &ReplicaWindow::activeColorLogicSlotChanged);

        QTest::mouseClick(&window, Qt::MiddleButton);
        QCOMPARE(window.activeColorLogicSlot(), 2);
        QCOMPARE(slotChanged.count(), 1);
        QVERIFY(!window.colorLogicEnabled());

        QTest::mouseClick(&window, Qt::MiddleButton);
        QCOMPARE(window.activeColorLogicSlot(), 1);
        QCOMPARE(slotChanged.count(), 2);
        QVERIFY(!window.colorLogicEnabled());

        QTest::mouseClick(&window, Qt::MiddleButton);
        QCOMPARE(window.activeColorLogicSlot(), 2);
        QCOMPARE(slotChanged.count(), 3);
    }

    void middleClickDoesNotActivateOrRaiseReplica()
    {
#ifdef Q_OS_WIN
        ReplicaWindow window(1, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        MSG message{};
        message.message = WM_MOUSEACTIVATE;
        message.lParam = MAKELPARAM(HTCLIENT, WM_MBUTTONDOWN);
        qintptr result = 0;

        QVERIFY(window.nativeEvent(QByteArrayLiteral("windows_generic_MSG"), &message, &result));
        QCOMPARE(result, static_cast<qintptr>(MA_NOACTIVATE));
#else
        QSKIP("WM_MOUSEACTIVATE is specific to Windows");
#endif
    }

    void middleLongPressTogglesColorLogicEnabled()
    {
        ReplicaWindow window(1, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        QSignalSpy enabledChanged(&window, &ReplicaWindow::colorLogicEnabledChanged);
        QSignalSpy slotChanged(&window, &ReplicaWindow::activeColorLogicSlotChanged);

        QTest::mouseClick(&window, Qt::MiddleButton);
        QCOMPARE(window.activeColorLogicSlot(), 2);
        QVERIFY(!window.colorLogicEnabled());

        QTest::mousePress(&window, Qt::MiddleButton);
        QTest::qWait(1200);
        QTest::mouseRelease(&window, Qt::MiddleButton);
        QVERIFY(window.colorLogicEnabled());
        QCOMPARE(enabledChanged.last().first().toBool(), true);
        QCOMPARE(window.activeColorLogicSlot(), 2);
        QCOMPARE(slotChanged.count(), 1);

        QTest::mousePress(&window, Qt::MiddleButton);
        QTest::qWait(1200);
        QTest::mouseRelease(&window, Qt::MiddleButton);
        QVERIFY(!window.colorLogicEnabled());
    }

    void colorLogicForwardLoopRepeatsWhileConditionHolds()
    {
        setRecord(state_);
        state_.captureWidth = 2;
        state_.captureHeight = 1;
        state_.captureColors = {qRgb(255, 0, 0), qRgb(255, 0, 0)};
        ReplicaWindow window(20, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        window.setStatisticsInterval(500);
        window.setColorRules({
            {1, QStringLiteral("循环"), QStringLiteral("红色"), QStringLiteral(">=2"),
             TriggerFeedback::None, QString{}, PostTriggerAction::Loop},
        });
        window.setColorLogicSlot(1, 1);
        window.setColorLogicEnabled(true);
        window.setActiveColorLogicSlot(1);
        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});

        QTRY_VERIFY_WITH_TIMEOUT(window.colorLogicTriggerCount(1) >= 3, 2500);
    }

    void colorLogicReverseWaitRepeatsOnlyAfterConditionDrops()
    {
        setRecord(state_);
        state_.captureWidth = 2;
        state_.captureHeight = 1;
        state_.captureColors = {qRgb(255, 0, 0), qRgb(255, 0, 0)};
        ReplicaWindow window(21, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        window.setStatisticsInterval(500);
        window.setColorRules({
            {1, QStringLiteral("等待"), QStringLiteral("红色"), QStringLiteral(">=2"),
             TriggerFeedback::None, QString{}, PostTriggerAction::ReverseWait},
        });
        window.setColorLogicSlot(1, 1);
        window.setColorLogicEnabled(true);
        window.setActiveColorLogicSlot(1);
        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});

        QTRY_COMPARE_WITH_TIMEOUT(window.colorLogicTriggerCount(1), 1, 1500);
        QTest::qWait(1200);
        QCOMPARE(window.colorLogicTriggerCount(1), 1);

        state_.captureColors = {qRgb(0, 0, 0), qRgb(0, 0, 0)};
        QTest::qWait(1200);
        QCOMPARE(window.colorLogicTriggerCount(1), 1);

        state_.captureColors = {qRgb(255, 0, 0), qRgb(255, 0, 0)};
        QTRY_COMPARE_WITH_TIMEOUT(window.colorLogicTriggerCount(1), 2, 2000);
    }

    void jumpToSlotThenReverseWaitOnTargetWaitsForDrop()
    {
        setRecord(state_);
        state_.captureWidth = 4;
        state_.captureHeight = 1;
        state_.captureColors = {
            qRgb(255, 0, 0), qRgb(255, 0, 0), qRgb(255, 255, 255), qRgb(255, 255, 255),
        };
        ReplicaWindow window(44, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        QSignalSpy slotChanged(&window, &ReplicaWindow::activeColorLogicSlotChanged);
        window.setStatisticsInterval(500);
        window.setColorRules({
            {1, QStringLiteral("跳转"), QStringLiteral("红色"), QStringLiteral(">=2"),
             TriggerFeedback::None, QString{}, PostTriggerAction::Jump},
            {2, QStringLiteral("等待"), QStringLiteral("白色"), QStringLiteral(">=2"),
             TriggerFeedback::None, QString{}, PostTriggerAction::ReverseWait},
        });
        window.setColorLogicSlot(1, 1);
        window.setColorLogicSlot(2, 2);
        window.setColorLogicEnabled(true);
        window.setActiveColorLogicSlot(1);
        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});

        QTRY_COMPARE_WITH_TIMEOUT(slotChanged.count(), 1, 1500);
        QCOMPARE(window.activeColorLogicSlot(), 2);
        QTRY_COMPARE_WITH_TIMEOUT(window.colorLogicTriggerCount(2), 1, 1500);
        QTest::qWait(1200);
        QCOMPARE(window.colorLogicTriggerCount(2), 1);

        state_.captureColors = {
            qRgb(255, 0, 0), qRgb(255, 0, 0), qRgb(0, 0, 0), qRgb(0, 0, 0),
        };
        QTest::qWait(1200);
        QCOMPARE(window.colorLogicTriggerCount(2), 1);

        state_.captureColors = {
            qRgb(255, 0, 0), qRgb(255, 0, 0), qRgb(255, 255, 255), qRgb(255, 255, 255),
        };
        QTRY_COMPARE_WITH_TIMEOUT(window.colorLogicTriggerCount(2), 2, 2000);
    }

    void jumpLoopComplementRulesSwitchWhenConditionClears()
    {
        setRecord(state_);
        state_.captureWidth = 2;
        state_.captureHeight = 1;
        state_.captureColors = {qRgb(255, 0, 0), qRgb(255, 0, 0)};
        ReplicaWindow window(45, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        QSignalSpy slotChanged(&window, &ReplicaWindow::activeColorLogicSlotChanged);
        window.setStatisticsInterval(500);
        window.setColorRules({
            {1, QStringLiteral("红色预警"), QStringLiteral("红色"), QStringLiteral(">0"),
             TriggerFeedback::None, QString{}, PostTriggerAction::Loop},
            {2, QStringLiteral("反预警"), QStringLiteral("红色"), QStringLiteral("<=0"),
             TriggerFeedback::None, QString{}, PostTriggerAction::Jump},
        });
        window.setColorLogicSlot(1, 1);
        window.setColorLogicSlot(2, 2);
        window.setColorLogicEnabled(true);
        window.setActiveColorLogicSlot(1);
        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});

        QTRY_VERIFY_WITH_TIMEOUT(window.colorLogicTriggerCount(1) >= 2, 2500);
        QCOMPARE(window.activeColorLogicSlot(), 1);
    }

    void reverseWaitFlashesOnlyAtTrigger()
    {
        setRecord(state_);
        state_.captureWidth = 2;
        state_.captureHeight = 1;
        state_.captureColors = {qRgb(255, 0, 0), qRgb(255, 0, 0)};
        ReplicaWindow window(46, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        window.setStatisticsInterval(500);
        window.setColorRules({
            {1, QStringLiteral("等待"), QStringLiteral("红色"), QStringLiteral(">=2"),
             TriggerFeedback::None, QString{}, PostTriggerAction::ReverseWait},
        });
        window.setColorLogicSlot(1, 1);
        window.setColorLogicEnabled(true);
        window.setActiveColorLogicSlot(1);
        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});

        QTRY_COMPARE_WITH_TIMEOUT(window.colorLogicTriggerCount(1), 1, 1500);
        QTRY_VERIFY_WITH_TIMEOUT(window.isFlashActive(), 2500);
        QTRY_VERIFY_WITH_TIMEOUT(!window.isFlashActive(), 2500);
        QTest::qWait(1000);
        QCOMPARE(window.colorLogicTriggerCount(1), 1);
        QVERIFY(!window.isFlashActive());

        state_.captureColors = {qRgb(0, 0, 0), qRgb(0, 0, 0)};
        QTest::qWait(1200);
        QCOMPARE(window.colorLogicTriggerCount(1), 1);

        state_.captureColors = {qRgb(255, 0, 0), qRgb(255, 0, 0)};
        QTRY_COMPARE_WITH_TIMEOUT(window.colorLogicTriggerCount(1), 2, 2000);
    }

    void popupFeedbackSuppressedForVisibleWindow()
    {
        setRecord(state_);
        state_.captureWidth = 2;
        state_.captureHeight = 1;
        state_.captureColors = {qRgb(255, 0, 0), qRgb(255, 0, 0)};
        ReplicaWindow window(30, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        window.setStatisticsInterval(500);
        window.setColorRules({
            {1, QStringLiteral("弹窗"), QStringLiteral("红色"), QStringLiteral(">=2"),
             TriggerFeedback::Popup, QString{}, PostTriggerAction::Loop},
        });
        window.setColorLogicSlot(1, 1);
        window.setColorLogicEnabled(true);
        window.setActiveColorLogicSlot(1);
        window.show();
        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});

        QTRY_VERIFY_WITH_TIMEOUT(window.colorLogicTriggerCount(1) >= 1, 2000);
        QVERIFY(window.isVisible());
        QCOMPARE(state_.activateCount, 1);
        QCOMPARE(state_.activatedHwnd, std::uintptr_t{123});
    }

    void simultaneousColorTriggersDoNotActivateEitherSourceWindow()
    {
        setRecord(state_);
        state_.captureWidth = 2;
        state_.captureHeight = 1;
        state_.captureColors = {qRgb(255, 0, 0), qRgb(255, 0, 0)};
        ReplicaWindow first(30, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        ReplicaWindow second(31, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        for (auto* window : {&first, &second}) {
            window->setStatisticsInterval(500);
            window->setColorRules({
                {1, QStringLiteral("告警"), QStringLiteral("红色"), QStringLiteral(">=2"),
                 TriggerFeedback::None, QString{}, PostTriggerAction::Loop},
            });
            window->setColorLogicSlot(1, 1);
            window->setColorLogicEnabled(true);
            window->setActiveColorLogicSlot(1);
            window->selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});
        }

        QTRY_COMPARE_WITH_TIMEOUT(first.colorLogicTriggerCount(1), 1, 1500);
        QTRY_COMPARE_WITH_TIMEOUT(second.colorLogicTriggerCount(1), 1, 1500);
        QCOMPARE(state_.activateCount, 0); // One statistics batch must be conflict-free.
    }

    void popupFeedbackForHiddenWindowRevealsAndClickDismisses()
    {
        setRecord(state_);
        state_.captureWidth = 2;
        state_.captureHeight = 1;
        state_.captureColors = {qRgb(255, 0, 0), qRgb(255, 0, 0)};
        ReplicaWindow window(31, *gateway_, *statisticsCoordinator_, *audioFeedbackManager_);
        window.setStatisticsInterval(500);
        window.setColorRules({
            {1, QStringLiteral("弹窗"), QStringLiteral("红色"), QStringLiteral(">=2"),
             TriggerFeedback::Popup, QString{}, PostTriggerAction::Loop},
        });
        window.setColorLogicSlot(1, 1);
        window.setColorLogicEnabled(true);
        window.setActiveColorLogicSlot(1);
        window.selectSource(WindowInfo{123, QStringLiteral("EVE Online - Alpha")});
        window.hide();

        QTRY_VERIFY_WITH_TIMEOUT(window.isVisible(), 2000);
        QCOMPARE(state_.activateCount, 1);
        QCOMPARE(state_.activatedHwnd, std::uintptr_t{123});

        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, QPoint(100, 100));
        QCoreApplication::processEvents();
        QVERIFY(!window.isVisible());
        QCOMPARE(state_.activateCount, 2);
        QCOMPARE(state_.activatedHwnd, std::uintptr_t{123});
    }

private:
    WindowFakeState state_{};
    std::unique_ptr<NativeGateway> gateway_;
    std::unique_ptr<StatisticsCoordinator> statisticsCoordinator_;
    std::unique_ptr<AudioFeedbackManager> audioFeedbackManager_;
};

QTEST_MAIN(ReplicaWindowTest)

#include "replica_window_test.moc"
