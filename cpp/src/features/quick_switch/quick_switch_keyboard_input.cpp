#include "features/quick_switch/quick_switch_service_impl.h"

#include "platform/native_gateway.h"

#ifdef Q_OS_WIN

LRESULT QuickSwitchService::Impl::handleKeyboardEvent(WPARAM message, LPARAM parameter)
{
    const auto* key = reinterpret_cast<const KBDLLHOOKSTRUCT*>(parameter);
    if (key == nullptr || key->vkCode == 0 || key->vkCode >= 0xFF) {
        return CallNextHookEx(keyboardHookHandle, HC_ACTION, message, parameter);
    }

    const auto callNext = [this, message, parameter] {
        return CallNextHookEx(keyboardHookHandle, HC_ACTION, message, parameter);
    };
    if (key->dwExtraInfo == kRadialSyntheticInputTag) {
        return callNext();
    }

    const bool keyDown = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
    const bool keyUp = message == WM_KEYUP || message == WM_SYSKEYUP;
    DWORD virtualKey = key->vkCode;
    const bool extended = (key->flags & LLKHF_EXTENDED) != 0;
    if (virtualKey == VK_SHIFT || virtualKey == VK_CONTROL || virtualKey == VK_MENU) {
        UINT scanCode = key->scanCode;
        if (extended) {
            scanCode |= 0xE000;
        }
        const UINT sidedKey = MapVirtualKeyW(scanCode, MAPVK_VSC_TO_VK_EX);
        if (sidedKey != 0) {
            virtualKey = sidedKey;
        }
    }
    const bool keyExtended = extended;
    const std::size_t index = static_cast<std::size_t>(
        virtualKey * 2 + (keyExtended ? 1 : 0));
    // Track every key before shortcut/capture handling can consume the event.
    // Auto-repeat must not add another held key to the shared input count.
    bool& pressed = keyboardPressedStates[index];
    if (keyDown && !pressed) {
        pressed = true;
        gateway.noteInputPressed();
    } else if (keyUp && pressed) {
        pressed = false;
        gateway.noteInputReleased();
    }

    KeyState& state = keyStates[index];
    const bool injected = (key->flags & LLKHF_INJECTED) != 0;

    if (const auto radialResult = processRadialKeyboardEvent(
            virtualKey, keyExtended, index, keyDown, keyUp, injected, message, parameter)) {
        return *radialResult;
    }

    const int direction = keyboardDirection(index);
    return processInputEvent(
        direction,
        state,
        keyDown,
        keyUp,
        keyboardHookHandle,
        message,
        parameter,
        static_cast<int>(virtualKey),
        keyExtended,
        -1,
        !injected);
}

void QuickSwitchService::Impl::releaseTrackedKeyboardKeys() noexcept
{
    for (bool& pressed : keyboardPressedStates) {
        if (pressed) {
            pressed = false;
            gateway.noteInputReleased();
        }
    }
}

#endif
