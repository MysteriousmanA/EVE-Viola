#include "features/quick_switch/quick_switch_service_impl.h"

#include "platform/native_gateway.h"

#ifdef Q_OS_WIN

LRESULT QuickSwitchService::Impl::handleMouseEvent(WPARAM message, LPARAM parameter)
{
    const auto* mouse = reinterpret_cast<const MSLLHOOKSTRUCT*>(parameter);
    if (mouse == nullptr) {
        return CallNextHookEx(mouseHook, HC_ACTION, message, parameter);
    }

    bool keyDown = false;
    bool keyUp = false;
    int buttonIndex = -1;
    switch (message) {
    case WM_LBUTTONDOWN:
        buttonIndex = 0;
        keyDown = true;
        break;
    case WM_LBUTTONUP:
        buttonIndex = 0;
        keyUp = true;
        break;
    case WM_RBUTTONDOWN:
        buttonIndex = 1;
        keyDown = true;
        break;
    case WM_RBUTTONUP:
        buttonIndex = 1;
        keyUp = true;
        break;
    case WM_MBUTTONDOWN:
        buttonIndex = 2;
        keyDown = true;
        break;
    case WM_MBUTTONUP:
        buttonIndex = 2;
        keyUp = true;
        break;
    case WM_XBUTTONDOWN:
    case WM_XBUTTONDBLCLK: {
        const WORD button = HIWORD(mouse->mouseData);
        buttonIndex = button == XBUTTON1 ? 3 : button == XBUTTON2 ? 4 : -1;
        keyDown = buttonIndex >= 0;
        break;
    }
    case WM_XBUTTONUP: {
        const WORD button = HIWORD(mouse->mouseData);
        buttonIndex = button == XBUTTON1 ? 3 : button == XBUTTON2 ? 4 : -1;
        keyUp = buttonIndex >= 0;
        break;
    }
    default:
        break;
    }

    if (buttonIndex >= 0) {
        bool& eveButtonPressed = eveMouseButtonStates[
            static_cast<std::size_t>(buttonIndex)];
        if (keyDown && !eveButtonPressed && isEveMouseTarget(mouse->pt)) {
            eveButtonPressed = true;
            gateway.noteEveMouseButtonPressed();
        } else if (keyUp && eveButtonPressed) {
            eveButtonPressed = false;
            gateway.noteEveMouseButtonReleased();
        }
    }

    if ((message != WM_XBUTTONDOWN && message != WM_XBUTTONUP
         && message != WM_XBUTTONDBLCLK)
        || buttonIndex < 3) {
        return CallNextHookEx(mouseHook, HC_ACTION, message, parameter);
    }

    const int token = buttonIndex == 3 ? kMouse4Token : kMouse5Token;
    return processInputEvent(
        mouseDirection(token),
        quickSwitchButtonStates[static_cast<std::size_t>(buttonIndex - 3)],
        keyDown,
        keyUp,
        mouseHook,
        message,
        parameter,
        -1,
        false,
        token,
        (mouse->flags & LLMHF_INJECTED) == 0);
}

void QuickSwitchService::Impl::releaseTrackedEveMouseButtons() noexcept
{
    for (bool& pressed : eveMouseButtonStates) {
        if (pressed) {
            pressed = false;
            gateway.noteEveMouseButtonReleased();
        }
    }
}

#endif
