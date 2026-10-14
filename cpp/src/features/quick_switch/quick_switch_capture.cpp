#include "features/quick_switch/quick_switch_service_impl.h"

#include <utility>
#include "features/quick_switch/quick_switch_keys.h"

#ifdef Q_OS_WIN

void QuickSwitchService::Impl::armCapture()
{
    captureArmed.store(false, std::memory_order_release);
    for (auto& ignored : captureIgnoreUntilUp) {
        ignored.store(false, std::memory_order_relaxed);
    }

    const auto ignoreKey = [this](int virtualKey) {
        const std::size_t index = static_cast<std::size_t>(virtualKey * 2);
        captureIgnoreUntilUp[index].store(true, std::memory_order_relaxed);
        captureIgnoreUntilUp[index + 1].store(true, std::memory_order_relaxed);
    };
    for (int virtualKey = 1; virtualKey < 0xFF; ++virtualKey) {
        if ((GetAsyncKeyState(virtualKey) & 0x8000) != 0) {
            ignoreKey(virtualKey);
            if (virtualKey == VK_SHIFT) {
                ignoreKey(VK_LSHIFT);
                ignoreKey(VK_RSHIFT);
            } else if (virtualKey == VK_CONTROL) {
                ignoreKey(VK_LCONTROL);
                ignoreKey(VK_RCONTROL);
            } else if (virtualKey == VK_MENU) {
                ignoreKey(VK_LMENU);
                ignoreKey(VK_RMENU);
            }
        }
    }
    captureArmed.store(true, std::memory_order_release);
}

std::optional<LRESULT> QuickSwitchService::Impl::captureKeyDown(
    KeyState& state, HHOOK sourceHook, WPARAM message, LPARAM parameter,
    int captureVirtualKey, bool captureExtended, int captureMouseToken, bool allowCapture)
{
    const auto callNext = [sourceHook, message, parameter] {
        return CallNextHookEx(sourceHook, HC_ACTION, message, parameter);
    };
    if (allowCapture && captureArmed.load(std::memory_order_acquire)) {
        if (captureVirtualKey >= 0) {
            const std::size_t index = static_cast<std::size_t>(
                captureVirtualKey * 2 + (captureExtended ? 1 : 0));
            if (captureIgnoreUntilUp[index].load(std::memory_order_relaxed)) {
                state = KeyState::PassedThrough;
                return callNext();
            }
        }
        QString capturedKey;
        if (captureVirtualKey >= 0) {
            capturedKey = quickSwitchKeyboardKey(
                captureVirtualKey, captureExtended);
        } else if (captureMouseToken == kMouse4Token) {
            capturedKey = QStringLiteral("Mouse4");
        } else if (captureMouseToken == kMouse5Token) {
            capturedKey = QStringLiteral("Mouse5");
        }
        if (!capturedKey.isEmpty()
            && captureArmed.exchange(false, std::memory_order_acq_rel)) {
            state = KeyState::Captured;
            postInputCaptured(std::move(capturedKey));
            return 1;
        }
    }
    return std::nullopt;
}

#endif

void QuickSwitchService::beginCapture()
{
#ifdef Q_OS_WIN
    if (impl_->thread.joinable()) {
        impl_->radialCancelRequested.store(true, std::memory_order_release);
        impl_->armCapture();
    }
#endif
}

void QuickSwitchService::cancelCapture()
{
#ifdef Q_OS_WIN
    impl_->captureArmed.store(false, std::memory_order_release);
#endif
}
