#include "features/quick_switch/quick_switch_service_impl.h"


#ifdef Q_OS_WIN

LRESULT QuickSwitchService::Impl::processInputEvent(
    int direction,
    KeyState& state,
    bool keyDown,
    bool keyUp,
    HHOOK sourceHook,
    WPARAM message,
    LPARAM parameter,
    int captureVirtualKey,
    bool captureExtended,
    int captureMouseToken,
    bool allowCapture)
{
    const auto callNext = [sourceHook, message, parameter] {
        return CallNextHookEx(sourceHook, HC_ACTION, message, parameter);
    };
    if (!keyDown && !keyUp) {
        return callNext();
    }

    if (keyDown) {
        if (state == KeyState::Consumed || state == KeyState::Captured) {
            return 1;
        }
        if (state == KeyState::PassedThrough) {
            return callNext();
        }

        if (const auto captureResult = captureKeyDown(
                state, sourceHook, message, parameter, captureVirtualKey,
                captureExtended, captureMouseToken, allowCapture)) {
            return *captureResult;
        }

        if (direction == kNoDirection) {
            state = KeyState::PassedThrough;
            return callNext();
        }

        const HWND foreground = GetForegroundWindow();
        if (!isEveClientWindow(foreground)) {
            state = KeyState::PassedThrough;
            return callNext();
        }

        state = KeyState::Consumed;
        postSwitch(direction == kNextDirection, foreground);
        return 1;
    }

    if (keyUp && captureVirtualKey >= 0) {
        const std::size_t index = static_cast<std::size_t>(captureVirtualKey * 2);
        // GetAsyncKeyState cannot distinguish the extended and non-extended
        // identities of keys such as Enter. Clear both snapshots when either
        // identity is released so a later press of its sibling can be captured.
        captureIgnoreUntilUp[index].store(false, std::memory_order_relaxed);
        captureIgnoreUntilUp[index + 1].store(false, std::memory_order_relaxed);
    }
    const bool consumed = state == KeyState::Consumed
        || state == KeyState::Captured;
    state = KeyState::Up;
    return consumed ? 1 : callNext();
}

#endif
