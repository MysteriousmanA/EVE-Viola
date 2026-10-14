#include "features/quick_switch/quick_switch_service_impl.h"


#ifdef Q_OS_WIN

std::optional<LRESULT> QuickSwitchService::Impl::processRadialKeyboardEvent(
    DWORD virtualKey, bool keyExtended, std::size_t index,
    bool keyDown, bool keyUp, bool injected, WPARAM message, LPARAM parameter)
{
    KeyState& state = keyStates[index];
    const auto callNext = [this, message, parameter] {
        return CallNextHookEx(keyboardHookHandle, HC_ACTION, message, parameter);
    };
    if (state == KeyState::RadialPending) {
        if (keyDown) {
            return 1;
        }
        if (!keyUp) {
            return callNext();
        }

        const HWND foreground = radialForegrounds[index];
        const quint64 sessionId = radialSessionIds[index];
        const ULONGLONG heldMs = GetTickCount64() - radialStartedAt[index];
        const bool heldLongEnough = heldMs >= kRadialSwitchHoldDurationMs;
        const bool cancelled = radialCancelRequested.exchange(
            false, std::memory_order_acq_rel);
        const bool foregroundStillMatches = foreground != nullptr
            && GetForegroundWindow() == foreground
            && isEveClientWindow(foreground);
        const std::size_t virtualKeyIndex = static_cast<std::size_t>(virtualKey * 2);
        captureIgnoreUntilUp[virtualKeyIndex].store(
            false, std::memory_order_relaxed);
        captureIgnoreUntilUp[virtualKeyIndex + 1].store(
            false, std::memory_order_relaxed);
        state = KeyState::Up;

        if (!cancelled && foregroundStillMatches && !heldLongEnough) {
            const std::int8_t direction = radialStartDirections[index];
            if (direction == kPreviousDirection || direction == kNextDirection) {
                postSwitch(direction == kNextDirection, foreground);
            } else {
                INPUT events[2]{};
                events[0].type = INPUT_KEYBOARD;
                events[0].ki.wVk = static_cast<WORD>(virtualKey);
                events[0].ki.dwFlags = keyExtended ? KEYEVENTF_EXTENDEDKEY : 0;
                events[0].ki.dwExtraInfo = kRadialSyntheticInputTag;
                events[1] = events[0];
                events[1].ki.dwFlags |= KEYEVENTF_KEYUP;
                if (GetForegroundWindow() == foreground) {
                    static_cast<void>(SendInput(
                        2, events, static_cast<int>(sizeof(INPUT))));
                }
            }
        }

        postRadialHoldEnded(
            sessionId,
            heldLongEnough,
            foregroundStillMatches,
            cancelled,
            foreground);
        return 1;
    }

    const int radialIndex = radialSwitchKeyIndex.load(std::memory_order_acquire);
    if (keyDown && state == KeyState::Up
        && static_cast<int>(index) == radialIndex
        && !injected
        && !captureArmed.load(std::memory_order_acquire)) {
        const auto modifierPressed = [virtualKey](int generic, int left, int right) {
            if (virtualKey == generic || virtualKey == left || virtualKey == right) {
                return false;
            }
            return ((GetAsyncKeyState(generic) | GetAsyncKeyState(left)
                     | GetAsyncKeyState(right)) & 0x8000) != 0;
        };
        const bool hasModifier = modifierPressed(VK_SHIFT, VK_LSHIFT, VK_RSHIFT)
            || modifierPressed(VK_CONTROL, VK_LCONTROL, VK_RCONTROL)
            || modifierPressed(VK_MENU, VK_LMENU, VK_RMENU)
            || ((virtualKey != VK_LWIN && virtualKey != VK_RWIN)
                && ((GetAsyncKeyState(VK_LWIN) | GetAsyncKeyState(VK_RWIN))
                    & 0x8000) != 0);
        if (hasModifier) {
            state = KeyState::PassedThrough;
            return callNext();
        }

        const HWND foreground = GetForegroundWindow();
        if (!isEveClientWindow(foreground)) {
            state = KeyState::PassedThrough;
            return callNext();
        }

        const quint64 sessionId = nextRadialSessionId++;
        state = KeyState::RadialPending;
        radialForegrounds[index] = foreground;
        radialSessionIds[index] = sessionId;
        radialStartedAt[index] = GetTickCount64();
        radialStartDirections[index] = keyboardDirection(index);
        radialCancelRequested.store(false, std::memory_order_release);
        postRadialHoldStarted(sessionId, foreground);
        return 1;
    }
    return std::nullopt;
}

#endif
