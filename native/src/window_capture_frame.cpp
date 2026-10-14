#include "native_platform.h"
#include "native_status.h"
#include "eve_viola_native.h"
#include "capture_timer.h"
#include <limits>
#include <memory>
#include <new>

using namespace eve_viola::native_detail;

namespace {

struct CaptureSurface final {
    HDC dc{};
    HBITMAP bitmap{};
    HGDIOBJ previous{};
    void* pixels{};
    int width{};
    int height{};

    ~CaptureSurface() {
        if (dc != nullptr && previous != nullptr && previous != HGDI_ERROR) {
            SelectObject(dc, previous);
        }
        if (bitmap != nullptr) { DeleteObject(bitmap); }
        if (dc != nullptr) { DeleteDC(dc); }
    }

    bool initialize(HDC source, int w, int h) {
        width = w;
        height = h;
        dc = CreateCompatibleDC(source);
        if (dc == nullptr) { return false; }
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = w;
        info.bmiHeader.biHeight = -h;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        bitmap = CreateDIBSection(source, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        if (bitmap == nullptr || pixels == nullptr) { return false; }
        previous = SelectObject(dc, bitmap);
        return previous != nullptr && previous != HGDI_ERROR;
    }
};

// One reusable surface per capture worker. A retained frame forces allocation
// of another surface instead of allowing a subsequent capture to mutate it.
thread_local std::shared_ptr<CaptureSurface> cachedCaptureSurface;

}  // namespace

extern "C" {

std::int32_t eve_viola_capture_frame(
    std::uintptr_t hwnd, EveCaptureFrame* frame, EveCaptureMetrics* metrics) {
    CaptureTimer timer(metrics);
    if (frame == nullptr) { return kInvalidArgument; }
    *frame = {};
    if (hwnd == 0) { return kInvalidArgument; }
    const HWND window = reinterpret_cast<HWND>(hwnd);
    RECT client_rect{};
    RECT window_rect{};
    POINT client_screen{};
    if (!GetClientRect(window, &client_rect) || !GetWindowRect(window, &window_rect)
        || !ClientToScreen(window, &client_screen)) {
        return kCaptureFailed;
    }
    const int w = window_rect.right - window_rect.left;
    const int h = window_rect.bottom - window_rect.top;
    const int cw = client_rect.right - client_rect.left;
    const int ch = client_rect.bottom - client_rect.top;
    const int x = client_screen.x - window_rect.left;
    const int y = client_screen.y - window_rect.top;
    if (w <= 0 || h <= 0 || cw <= 0 || ch <= 0 || x < 0 || y < 0
        || x > w || y > h || cw > w - x || ch > h - y
        || w > std::numeric_limits<std::int32_t>::max() / 4
        || static_cast<std::size_t>(h) > std::numeric_limits<std::size_t>::max() / 4 / w) {
        return kCaptureFailed;
    }
    struct WindowDc {
        HWND window;
        HDC dc;
        ~WindowDc() { if (dc != nullptr) { ReleaseDC(window, dc); } }
    } source{window, GetDC(window)};
    if (source.dc == nullptr) { return kCaptureFailed; }
    std::shared_ptr<CaptureSurface> surface;
    try {
        if (!cachedCaptureSurface || cachedCaptureSurface.use_count() != 1
            || cachedCaptureSurface->width != w || cachedCaptureSurface->height != h) {
            auto replacement = std::make_shared<CaptureSurface>();
            if (!replacement->initialize(source.dc, w, h)) { return kCaptureFailed; }
            cachedCaptureSurface = std::move(replacement);
        }
        surface = cachedCaptureSurface;
    } catch (const std::bad_alloc&) {
        return kOutOfMemory;
    }
    auto owner = std::unique_ptr<std::shared_ptr<CaptureSurface>>(
        new (std::nothrow) std::shared_ptr<CaptureSurface>(surface));
    if (!owner) { return kOutOfMemory; }
    timer.mark(&EveCaptureMetrics::setup_ns);
    if (!PrintWindow(window, surface->dc, PW_RENDERFULLCONTENT)) {
        if (metrics != nullptr) { metrics->used_bitblt_fallback = 1; }
        if (!BitBlt(surface->dc, x, y, cw, ch, source.dc, 0, 0, SRCCOPY)) {
            return kCaptureFailed;
        }
    }
    timer.mark(&EveCaptureMetrics::render_ns);
    const int stride = w * 4;
    const auto* client_pixels = static_cast<const std::uint8_t*>(surface->pixels)
        + static_cast<std::size_t>(y) * stride + static_cast<std::size_t>(x) * 4;
    timer.mark(&EveCaptureMetrics::crop_ns);
    // Synchronize GDI writes before exposing DIB memory to CPU readers.
    // https://learn.microsoft.com/windows/win32/api/wingdi/nf-wingdi-createdibsection
    if (!GdiFlush()) { return kCaptureFailed; }
    timer.mark(&EveCaptureMetrics::readback_ns);
    ReleaseDC(window, source.dc);
    source.dc = nullptr;
    timer.mark(&EveCaptureMetrics::cleanup_ns);
    *frame = EveCaptureFrame{cw, ch, stride, client_pixels,
                            reinterpret_cast<std::uintptr_t>(owner.release())};
    if (metrics != nullptr) {
        metrics->bytes = static_cast<std::uint64_t>(cw) * ch * 4;
        metrics->succeeded = 1;
    }
    return kOk;
}

void eve_viola_release_frame(EveCaptureFrame* frame) {
    if (frame != nullptr) {
        delete reinterpret_cast<std::shared_ptr<CaptureSurface>*>(frame->owner);
        *frame = {};
    }
}

}  // extern "C"
