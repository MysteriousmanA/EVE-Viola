#include "native_platform.h"
#include "native_status.h"
#include "eve_viola_native.h"
#include "core.h"
#include "eve_window_identity.h"
#include <dwmapi.h>
#include <algorithm>
#include <cwchar>
#include <iterator>
#include <new>
#include <vector>

using namespace eve_viola::native_detail;

namespace {

struct EnumerationContext {
    std::vector<EveWindowInfo> windows;
    bool allocation_failed = false;
};

bool is_task_window(HWND hwnd) {
    if (!IsWindowVisible(hwnd)) {
        return false;
    }

    DWORD process_id = 0;
    GetWindowThreadProcessId(hwnd, &process_id);
    if (process_id == GetCurrentProcessId()) {
        return false;
    }

    const auto ex_style = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
    const bool app_window = (ex_style & WS_EX_APPWINDOW) != 0;
    const bool tool_window = (ex_style & WS_EX_TOOLWINDOW) != 0;
    if (tool_window && !app_window) {
        return false;
    }
    if (GetWindow(hwnd, GW_OWNER) != nullptr && !app_window) {
        return false;
    }

    DWORD cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked) {
        return false;
    }
    return true;
}

BOOL CALLBACK collect_eve_window(HWND hwnd, LPARAM parameter) {
    auto* context = reinterpret_cast<EnumerationContext*>(parameter);
    if (!is_task_window(hwnd)) {
        return TRUE;
    }

    wchar_t title[512]{};
    if (GetWindowTextW(hwnd, title, static_cast<int>(std::size(title))) <= 0 ||
        !eve_viola::matches_game_window_title(title) ||
        !is_eve_client_window(hwnd)) {
        return TRUE;
    }

    EveWindowInfo item{};
    item.hwnd = reinterpret_cast<std::uintptr_t>(hwnd);
    wcscpy_s(item.title, title);
    try {
        context->windows.push_back(item);
    } catch (const std::bad_alloc&) {
        context->allocation_failed = true;
        return FALSE;
    }
    return TRUE;
}

}  // namespace

extern "C" {

std::int32_t eve_viola_enumerate_eve_windows(EveWindowInfo** items, std::uint32_t* count) {
    if (items == nullptr || count == nullptr) {
        return kInvalidArgument;
    }
    *items = nullptr;
    *count = 0;

    EnumerationContext context;
    SetLastError(ERROR_SUCCESS);
    if (!EnumWindows(collect_eve_window, reinterpret_cast<LPARAM>(&context)) &&
        !context.allocation_failed && GetLastError() != ERROR_SUCCESS) {
        return kEnumerationFailed;
    }
    if (context.allocation_failed) {
        return kOutOfMemory;
    }

    std::sort(context.windows.begin(), context.windows.end(), [](const auto& left, const auto& right) {
        const int title_order = _wcsicmp(left.title, right.title);
        return title_order != 0 ? title_order < 0 : left.hwnd < right.hwnd;
    });

    if (context.windows.empty()) {
        return kOk;
    }
    if (context.windows.size() > UINT32_MAX) {
        return kOutOfMemory;
    }

    auto* result = new (std::nothrow) EveWindowInfo[context.windows.size()];
    if (result == nullptr) {
        return kOutOfMemory;
    }
    std::copy(context.windows.begin(), context.windows.end(), result);
    *items = result;
    *count = static_cast<std::uint32_t>(context.windows.size());
    return kOk;
}

void eve_viola_free_window_list(EveWindowInfo* items) {
    delete[] items;
}

}  // extern "C"
