#pragma once

#include "features/quick_switch/quick_switch_service.h"

#ifdef Q_OS_WIN

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <thread>

struct QuickSwitchService::Impl {
    static constexpr int kMouse4Token = 0;
    static constexpr int kMouse5Token = 1;
    static constexpr int kKeyboardIdentityCount = 256 * 2;
    static constexpr ULONG_PTR kRadialSyntheticInputTag =
        static_cast<ULONG_PTR>(0x45564556u);
    static constexpr std::int8_t kNoDirection = -1;
    static constexpr std::int8_t kPreviousDirection = 0;
    static constexpr std::int8_t kNextDirection = 1;

    enum class KeyState : std::uint8_t {
        Up,
        PassedThrough,
        Consumed,
        Captured,
        RadialPending,
    };

    explicit Impl(QuickSwitchService* service, NativeGateway& nativeGateway);

    static int keyboardIndexForKey(const QString& key);

    void armCapture();

    static std::uint8_t mouseMaskForKeys(
        const QStringList& previousKeys,
        const QStringList& nextKeys);

    void setKeys(const QStringList& previousKeys, const QStringList& nextKeys);

    void setRadialSwitchKey(const QString& key);

    void postError(QString message);

    void postSwitch(bool next, HWND foreground);

    void postRadialHoldStarted(quint64 sessionId, HWND foreground);

    void postRadialHoldEnded(
        quint64 sessionId,
        bool heldLongEnough,
        bool foregroundStillMatches,
        bool cancelled,
        HWND foreground);

    void postInputCaptured(QString key);

    void signalReady(bool success, QString error = {});

    static bool isEveClientWindow(HWND hwnd);

    int mouseDirection(int token) const;

    int keyboardDirection(std::size_t index) const;

    LRESULT processInputEvent(
        int direction,
        KeyState& state,
        bool keyDown,
        bool keyUp,
        HHOOK sourceHook,
        WPARAM message,
        LPARAM parameter,
        int captureVirtualKey = -1,
        bool captureExtended = false,
        int captureMouseToken = -1,
        bool allowCapture = true);

    LRESULT handleKeyboardEvent(WPARAM message, LPARAM parameter);

    LRESULT handleMouseEvent(WPARAM message, LPARAM parameter);

    void releaseTrackedMouseButtons() noexcept;

    void releaseTrackedKeyboardKeys() noexcept;

    static LRESULT CALLBACK keyboardHook(int code, WPARAM message, LPARAM parameter);

    static LRESULT CALLBACK mouseHookCallback(int code, WPARAM message, LPARAM parameter);

    void run();

    std::optional<LRESULT> processRadialKeyboardEvent(
        DWORD virtualKey, bool keyExtended, std::size_t index,
        bool keyDown, bool keyUp, bool injected, WPARAM message, LPARAM parameter);
    std::optional<LRESULT> captureKeyDown(
        KeyState& state, HHOOK sourceHook, WPARAM message, LPARAM parameter,
        int captureVirtualKey, bool captureExtended, int captureMouseToken, bool allowCapture);

    QuickSwitchService* owner{};
    NativeGateway& gateway;
    std::atomic<std::uint8_t> switchKeys{0};
    std::atomic<std::uint32_t> switchMappingVersion{0};
    std::atomic<int> radialSwitchKeyIndex{0x20 * 2};
    std::array<std::atomic<std::int8_t>, kKeyboardIdentityCount> keyboardDirections{};
    std::array<KeyState, kKeyboardIdentityCount> keyStates{};
    std::array<bool, kKeyboardIdentityCount> keyboardPressedStates{};
    std::array<HWND, kKeyboardIdentityCount> radialForegrounds{};
    std::array<quint64, kKeyboardIdentityCount> radialSessionIds{};
    std::array<ULONGLONG, kKeyboardIdentityCount> radialStartedAt{};
    std::array<std::int8_t, kKeyboardIdentityCount> radialStartDirections{};
    std::array<KeyState, 2> quickSwitchButtonStates{};
    std::array<bool, 5> mouseButtonStates{};
    std::array<std::atomic<bool>, kKeyboardIdentityCount> captureIgnoreUntilUp{};
    std::atomic<bool> captureArmed{false};
    std::atomic<bool> radialCancelRequested{false};
    quint64 nextRadialSessionId{1};
    std::thread thread;
    std::atomic<DWORD> threadId{0};
    std::mutex startMutex;
    std::condition_variable startCondition;
    bool startFinished{};
    bool startSucceeded{};
    QString startError;
    HHOOK keyboardHookHandle{};
    HHOOK mouseHook{};

    inline static std::atomic<Impl*> activeImpl{nullptr};
};

#else

struct QuickSwitchService::Impl {
    explicit Impl(QuickSwitchService*, NativeGateway&) {}
};

#endif
