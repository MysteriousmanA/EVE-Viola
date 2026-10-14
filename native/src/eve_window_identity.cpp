#include "eve_window_identity.h"

#include <array>
#include <cwchar>

namespace eve_viola::native_detail {

bool is_eve_client_window(HWND hwnd) {
    DWORD process_id = 0;
    if (GetWindowThreadProcessId(hwnd, &process_id) == 0 || process_id == 0) {
        return false;
    }

    const HANDLE process = OpenProcess(
        PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
    if (process == nullptr) {
        return false;
    }

    std::array<wchar_t, 32768> image_path{};
    DWORD image_path_length = static_cast<DWORD>(image_path.size());
    const bool queried = QueryFullProcessImageNameW(
        process, 0, image_path.data(), &image_path_length) != FALSE;
    CloseHandle(process);
    if (!queried) {
        return false;
    }

    const wchar_t* filename = std::wcsrchr(image_path.data(), L'\\');
    filename = filename == nullptr ? image_path.data() : filename + 1;
    return _wcsicmp(filename, L"exefile.exe") == 0;
}

}  // namespace eve_viola::native_detail
