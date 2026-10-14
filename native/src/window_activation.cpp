#include "native_platform.h"
#include "native_status.h"
#include "eve_viola_native.h"
#include "eve_window_identity.h"
#include <array>

using namespace eve_viola::native_detail;

namespace {

constexpr std::array<DWORD, 2> kActivationRetryDelaysMs{50, 100};

class ScopedThreadInputAttachment final {
public:
    explicit ScopedThreadInputAttachment(DWORD other_thread_id) noexcept
        : current_thread_id_(GetCurrentThreadId())
        , other_thread_id_(other_thread_id)
        , attached_(other_thread_id_ != 0 && other_thread_id_ != current_thread_id_ &&
                    AttachThreadInput(current_thread_id_, other_thread_id_, TRUE) != FALSE) {
    }

    ~ScopedThreadInputAttachment() {
        detach();
    }

    ScopedThreadInputAttachment(const ScopedThreadInputAttachment&) = delete;
    ScopedThreadInputAttachment& operator=(const ScopedThreadInputAttachment&) = delete;

    void detach() noexcept {
        if (attached_) {
            AttachThreadInput(current_thread_id_, other_thread_id_, FALSE);
            attached_ = false;
        }
    }

private:
    DWORD current_thread_id_{};
    DWORD other_thread_id_{};
    bool attached_{};
};

}  // namespace

extern "C" {

std::int32_t eve_viola_activate_window(
    std::uintptr_t hwnd,
    std::uintptr_t* observed_foreground_hwnd) {
    if (observed_foreground_hwnd == nullptr) {
        return kInvalidArgument;
    }
    *observed_foreground_hwnd = reinterpret_cast<std::uintptr_t>(GetForegroundWindow());
    if (hwnd == 0) {
        return kInvalidArgument;
    }

    const HWND window = reinterpret_cast<HWND>(hwnd);
    if (!IsWindow(window) || !is_eve_client_window(window)) {
        return kInvalidArgument;
    }

    HWND foreground = nullptr;
    for (std::size_t attempt = 0; attempt <= kActivationRetryDelaysMs.size(); ++attempt) {
        const HWND previous_foreground = GetForegroundWindow();
        const DWORD foreground_thread_id = previous_foreground == nullptr
            ? 0
            : GetWindowThreadProcessId(previous_foreground, nullptr);
        ScopedThreadInputAttachment input_attachment(foreground_thread_id);

        if (IsIconic(window)) {
            ShowWindow(window, SW_RESTORE);
        }
        BringWindowToTop(window);
        if (!SetForegroundWindow(window)) {
            SwitchToThisWindow(window, FALSE);
        }
        input_attachment.detach();

        foreground = GetForegroundWindow();
        if (foreground == window || GetAncestor(foreground, GA_ROOTOWNER) == window) {
            break;
        }
        if (attempt < kActivationRetryDelaysMs.size()) {
            Sleep(kActivationRetryDelaysMs[attempt]);
        }
    }

    *observed_foreground_hwnd = reinterpret_cast<std::uintptr_t>(foreground);
    if (foreground != window && GetAncestor(foreground, GA_ROOTOWNER) != window) {
        FLASHWINFO flash{};
        flash.cbSize = sizeof(flash);
        flash.hwnd = window;
        flash.dwFlags = FLASHW_TRAY | FLASHW_TIMERNOFG;
        FlashWindowEx(&flash);
        return kActivationFailed;
    }
    SetFocus(window);
    return kOk;
}

}  // extern "C"
