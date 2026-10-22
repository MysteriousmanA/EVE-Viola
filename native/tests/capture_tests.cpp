#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

#include "eve_viola_native.h"

namespace {
constexpr DWORD kSourceStyle = WS_POPUP | WS_BORDER;
void require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

struct PaintState { bool alternate{}; int prints{}; };
LRESULT CALLBACK paintWindow(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }
    if (message == WM_PRINTCLIENT || message == WM_PRINT || message == WM_PAINT) {
        auto* state = reinterpret_cast<PaintState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        ++state->prints;
        RECT rect{};
        GetClientRect(hwnd, &rect);
        PAINTSTRUCT paint{};
        const auto dc = message == WM_PAINT ? BeginPaint(hwnd, &paint) : reinterpret_cast<HDC>(wparam);
        const int saved = SaveDC(dc);
        if (message == WM_PRINT) {
            RECT window{};
            POINT client{};
            GetWindowRect(hwnd, &window);
            ClientToScreen(hwnd, &client);
            SetViewportOrgEx(dc, client.x - window.left, client.y - window.top, nullptr);
        }
        const auto white = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(dc, &rect, white);
        DeleteObject(white);
        rect.bottom /= 2;
        const auto color = CreateSolidBrush(state->alternate ? RGB(0, 255, 0) : RGB(255, 0, 0));
        FillRect(dc, &rect, color);
        DeleteObject(color);
        RestoreDC(dc, saved);
        if (message == WM_PAINT) { EndPaint(hwnd, &paint); }
        return 1;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

class SourceWindow final {
public:
    SourceWindow(int width, int height) {
        RECT rect{0, 0, width, height};
        AdjustWindowRectEx(&rect, kSourceStyle, FALSE, 0);
        hwnd = CreateWindowExW(0, L"EVEViolaCaptureTest", L"Capture regression",
                               kSourceStyle, -20000, -20000,
                               rect.right - rect.left, rect.bottom - rect.top,
                               nullptr, nullptr, GetModuleHandleW(nullptr), &state);
        require(hwnd != nullptr, "create hidden source window");
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        repaint();
    }
    ~SourceWindow() { DestroyWindow(hwnd); }
    void repaint() {
        InvalidateRect(hwnd, nullptr, TRUE);
        UpdateWindow(hwnd);
        GdiFlush();
        DwmFlush();
    }
    HWND hwnd{};
    PaintState state;
};

std::uint32_t pixel(const EveCaptureFrame& frame, int x, int y) {
    std::uint32_t value{};
    std::memcpy(&value, frame.pixels + static_cast<std::size_t>(y) * frame.bytes_per_line + x * 4, 4);
    return value & 0x00ffffffu;
}

void frameOwnershipAndClientCoordinates() {
    SourceWindow source(73, 51);
    const auto hwnd = reinterpret_cast<std::uintptr_t>(source.hwnd);
    EveCaptureFrame first{};
    EveCaptureMetrics metrics{};
    require(eve_viola_capture_frame(hwnd, &first, &metrics) == 0, "capture borrowed client view");
    require(first.width == 73 && first.height == 51, "physical client dimensions");
    require(first.bytes_per_line > first.width * 4, "view preserves full-window row stride");
    require(metrics.succeeded == 1 && metrics.total_ns > 0, "capture metrics are populated");
    require(metrics.bytes == 73 * 51 * 4, "metrics client byte count");
    for (int y = 0; y < first.height; ++y) {
        for (int x = 0; x < first.width; ++x) {
            if (pixel(first, x, y) != (y < first.height / 2 ? 0x00ff0000u : 0x00ffffffu)) {
                std::fprintf(stderr, "pixel (%d,%d)=%08x prints=%d fallback=%u stride=%d\n",
                             x, y, pixel(first, x, y), source.state.prints,
                             metrics.used_bitblt_fallback, first.bytes_per_line);
            }
            require(pixel(first, x, y) == (y < first.height / 2 ? 0x00ff0000u : 0x00ffffffu),
                    "exact client crop and top-down color bytes");
        }
    }
    std::int32_t width{}, height{};
    std::uint8_t* pixels{};
    require(eve_viola_capture_window(hwnd, &width, &height, &pixels) == 0, "legacy capture ABI");
    require(width == first.width && height == first.height, "legacy capture dimensions");
    for (int y = 0; y < height; ++y) {
        require(std::memcmp(pixels + static_cast<std::size_t>(y) * width * 4,
                            first.pixels + static_cast<std::size_t>(y) * first.bytes_per_line,
                            static_cast<std::size_t>(width) * 4) == 0, "legacy copy equals retained view");
    }
    eve_viola_free_pixels(pixels);
    source.state.alternate = true;
    source.repaint();
    EveCaptureFrame second{};
    require(eve_viola_capture_frame(hwnd, &second, nullptr) == 0, "capture changed source");
    require(pixel(second, 0, 0) == 0x0000ff00u, "new frame sees changed content");
    require(pixel(first, 0, 0) == 0x00ff0000u, "retained frame is not overwritten");
    RECT resized{0, 0, 91, 37};
    AdjustWindowRectEx(&resized, kSourceStyle, FALSE, 0);
    SetWindowPos(source.hwnd, nullptr, 0, 0, resized.right - resized.left,
                 resized.bottom - resized.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    source.repaint();
    EveCaptureFrame third{};
    require(eve_viola_capture_frame(hwnd, &third, nullptr) == 0, "capture resized source");
    require(third.width == 91 && third.height == 37, "resized client dimensions");
    require(pixel(first, 72, 50) == 0x00ffffffu, "retained frame survives source resize");
    eve_viola_release_frame(&third);
    eve_viola_release_frame(&second);
    eve_viola_release_frame(&first);
    require(first.owner == 0 && first.pixels == nullptr, "release clears frame handle");
    eve_viola_release_frame(&first);
    const DWORD before = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    for (int i = 0; i < 200; ++i) {
        EveCaptureFrame frame{};
        require(eve_viola_capture_frame(hwnd, &frame, nullptr) == 0, "repeated cached capture");
        eve_viola_release_frame(&frame);
    }
    const DWORD after = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    require(after <= before + 2, "GDI handles remain bounded");
    require(eve_viola_capture_frame(0, &first, &metrics) != 0 && first.owner == 0,
            "invalid source produces no owned frame");
    require(metrics.succeeded == 0, "failed capture is marked unsuccessful");
}

void thumbnailRelationshipReplacement() {
    SourceWindow source(73, 51);
    SourceWindow destination(146, 102);
    const auto source_hwnd = reinterpret_cast<std::uintptr_t>(source.hwnd);
    const auto destination_hwnd = reinterpret_cast<std::uintptr_t>(destination.hwnd);
    const HWND foreground = GetForegroundWindow();
    std::uintptr_t current{};
    require(eve_viola_thumbnail_create(destination_hwnd, source_hwnd, &current) == 0,
            "create initial DWM relationship");
    const EveRect source_rect{0, 0, 73, 51};
    const EveRect destination_rect{0, 0, 146, 102};
    require(eve_viola_thumbnail_update(current, source_rect, destination_rect, 255, 1) == 0,
            "configure initial DWM relationship");
    for (int i = 0; i < 30; ++i) {
        std::uintptr_t replacement{};
        require(eve_viola_thumbnail_create(destination_hwnd, source_hwnd, &replacement) == 0,
                "register replacement while original relationship exists");
        require(replacement != current, "replacement owns a distinct DWM handle");
        require(eve_viola_thumbnail_update(replacement, source_rect, destination_rect, 255, 1) == 0,
                "configure replacement before releasing old relationship");
        require(eve_viola_thumbnail_destroy(current) == 0, "release old DWM relationship");
        current = replacement;
        std::int32_t width{}, height{};
        require(eve_viola_thumbnail_source_size(current, &width, &height) == 0
                    && width >= 73 && height >= 51,
                "replacement remains valid after old relationship is released");
        require(eve_viola_thumbnail_update(current, source_rect, destination_rect, 255, 1) == 0,
                "replacement continues accepting updates");
    }
    require(GetForegroundWindow() == foreground, "DWM renewal does not take input focus");
    require(eve_viola_thumbnail_destroy(current) == 0, "release final DWM relationship");
}

void minimizedThumbnailSourceResumesInBackground() {
    SourceWindow source(73, 51);
    SourceWindow other(146, 102);
    const auto hwnd = reinterpret_cast<std::uintptr_t>(source.hwnd);
    const HWND foreground = GetForegroundWindow();
    RECT original{};
    GetWindowRect(source.hwnd, &original);
    const auto wait_for_restore = [&source] {
        const auto deadline = GetTickCount64() + 1000;
        while (IsIconic(source.hwnd) && GetTickCount64() < deadline) {
            MSG message{};
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            Sleep(1);
        }
    };
    for (int i = 0; i < 3; ++i) {
        ShowWindow(source.hwnd, SW_SHOWMINNOACTIVE);
        require(IsIconic(source.hwnd), "source minimized as by Show Desktop");
        require(eve_viola_resume_thumbnail_source(hwnd) == 0, "request background restore");
        wait_for_restore();
        require(!IsIconic(source.hwnd), "source can render without clicking the thumbnail");
        require(GetForegroundWindow() == foreground, "background restore preserves foreground");
        RECT restored{};
        GetWindowRect(source.hwnd, &restored);
        require(EqualRect(&original, &restored), "background restore preserves source geometry");
        bool other_is_above = false;
        for (HWND previous = GetWindow(source.hwnd, GW_HWNDPREV); previous;
             previous = GetWindow(previous, GW_HWNDPREV)) {
            if (previous == other.hwnd) { other_is_above = true; break; }
        }
        require(other_is_above, "restored source stays behind other windows");
    }
    const HWND previous = GetWindow(source.hwnd, GW_HWNDPREV);
    require(eve_viola_resume_thumbnail_source(hwnd) == 0, "visible source is a no-op");
    require(GetWindow(source.hwnd, GW_HWNDPREV) == previous, "visible source keeps its stacking order");
    require(eve_viola_resume_thumbnail_source(0) != 0, "invalid resume source rejected");

    // A game can also have been maximized before Show Desktop.
    ShowWindow(source.hwnd, SW_SHOWMAXIMIZED);
    ShowWindow(source.hwnd, SW_SHOWMINNOACTIVE);
    const HWND foreground_before_resume = GetForegroundWindow();
    require(eve_viola_resume_thumbnail_source(hwnd) == 0, "resume maximized source");
    wait_for_restore();
    require(!IsIconic(source.hwnd) && IsZoomed(source.hwnd), "preserve maximized source state");
    require(GetForegroundWindow() == foreground_before_resume, "maximized restore preserves focus");
}

void busyThumbnailSourceDoesNotBlockRestoreRequest() {
    SourceWindow other(146, 102);
    const HWND foreground = GetForegroundWindow();
    std::atomic<HWND> source_hwnd{};
    std::atomic<bool> pump_messages{};
    bool restored_behind_other = false;
    std::thread source_thread([&] {
        SourceWindow source(73, 51);
        ShowWindow(source.hwnd, SW_SHOWMINNOACTIVE);
        source_hwnd = source.hwnd;
        // Simulate a game thread that is temporarily busy and not dispatching.
        while (!pump_messages) { Sleep(1); }
        const auto deadline = GetTickCount64() + 2000;
        while (GetTickCount64() < deadline) {
            MSG message{};
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            if (!IsIconic(source.hwnd)) { break; }
            Sleep(1);
        }
        if (!IsIconic(source.hwnd)) {
            for (HWND previous = GetWindow(source.hwnd, GW_HWNDPREV); previous;
                 previous = GetWindow(previous, GW_HWNDPREV)) {
                if (previous == other.hwnd) { restored_behind_other = true; break; }
            }
        }
    });
    const auto ready_deadline = GetTickCount64() + 2000;
    while (!source_hwnd && GetTickCount64() < ready_deadline) { Sleep(1); }
    require(source_hwnd != nullptr, "busy source is ready");
    const auto start = GetTickCount64();
    const auto status = eve_viola_resume_thumbnail_source(
        reinterpret_cast<std::uintptr_t>(source_hwnd.load()));
    const auto elapsed = GetTickCount64() - start;
    pump_messages = true;
    source_thread.join();
    require(status == 0 && elapsed < 500, "restore request does not wait for busy game thread");
    require(restored_behind_other, "cross-thread restore resumes behind other windows");
    require(GetForegroundWindow() == foreground, "cross-thread restore preserves focus");
}

void independentWorkerSurfaces() {
    std::atomic<int> completed{};
    std::vector<std::thread> workers;
    for (int i = 0; i < 2; ++i) {
        workers.emplace_back([&completed, i] {
            SourceWindow source(61 + i, 35);
            source.state.alternate = i != 0;
            source.repaint();
            for (int n = 0; n < 30; ++n) {
                EveCaptureFrame frame{};
                require(eve_viola_capture_frame(reinterpret_cast<std::uintptr_t>(source.hwnd), &frame, nullptr) == 0,
                        "worker capture");
                require(pixel(frame, 0, 0) == (i == 0 ? 0x00ff0000u : 0x0000ff00u),
                        "capture workers do not share mutable surfaces");
                eve_viola_release_frame(&frame);
            }
            ++completed;
        });
    }
    for (auto& worker : workers) { worker.join(); }
    require(completed == 2, "both capture workers complete");
}
} // namespace

int main() {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = paintWindow;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = L"EVEViolaCaptureTest";
    require(RegisterClassW(&windowClass) != 0, "register hidden test window class");
    frameOwnershipAndClientCoordinates();
    thumbnailRelationshipReplacement();
    minimizedThumbnailSourceResumesInBackground();
    busyThumbnailSourceDoesNotBlockRestoreRequest();
    independentWorkerSurfaces();
    std::puts("PASS: native client pixels, legacy ABI, retained frames, resizing, worker isolation, GDI lifecycle, DWM renewal");
    return 0;
}
