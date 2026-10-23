#include "features/quick_switch/quick_switch_service_impl.h"

#include <cwchar>

#ifdef Q_OS_WIN

bool QuickSwitchService::Impl::isEveClientWindow(HWND hwnd)
{
    if (hwnd == nullptr || !IsWindow(hwnd)) {
        return false;
    }

    DWORD processId = 0;
    if (GetWindowThreadProcessId(hwnd, &processId) == 0 || processId == 0) {
        return false;
    }

    const HANDLE process = OpenProcess(
        PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (process == nullptr) {
        return false;
    }

    std::array<wchar_t, 32768> imagePath{};
    DWORD imagePathLength = static_cast<DWORD>(imagePath.size());
    const bool queried = QueryFullProcessImageNameW(
        process, 0, imagePath.data(), &imagePathLength) != FALSE;
    CloseHandle(process);
    if (!queried) {
        return false;
    }

    const wchar_t* fileName = std::wcsrchr(imagePath.data(), L'\\');
    fileName = fileName == nullptr ? imagePath.data() : fileName + 1;
    return _wcsicmp(fileName, L"exefile.exe") == 0;
}

#endif

bool QuickSwitchService::isForegroundWindow(quintptr hwnd) const
{
#ifdef Q_OS_WIN
    const HWND target = reinterpret_cast<HWND>(hwnd);
    return target != nullptr && IsWindow(target)
        && GetForegroundWindow() == target;
#else
    Q_UNUSED(hwnd);
    return false;
#endif
}
