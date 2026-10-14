#include "native_platform.h"
#include "native_status.h"
#include "eve_viola_native.h"
#include "capture_timer.h"
#include <new>

using namespace eve_viola::native_detail;

extern "C" {

std::int32_t eve_viola_capture_window(
    std::uintptr_t hwnd,
    std::int32_t* width,
    std::int32_t* height,
    std::uint8_t** pixels) {
    return eve_viola_capture_window_profiled(hwnd, width, height, pixels, nullptr);
}

std::int32_t eve_viola_capture_window_profiled(
    std::uintptr_t hwnd,
    std::int32_t* width,
    std::int32_t* height,
    std::uint8_t** pixels,
    EveCaptureMetrics* metrics) {
    CaptureTimer timer(metrics);
    if (hwnd == 0 || width == nullptr || height == nullptr || pixels == nullptr) {
        return kInvalidArgument;
    }
    *width = 0;
    *height = 0;
    *pixels = nullptr;

    const HWND window = reinterpret_cast<HWND>(hwnd);
    RECT client_rect{};
    if (!GetClientRect(window, &client_rect)) {
        return kCaptureFailed;
    }
    const auto capture_width = client_rect.right - client_rect.left;
    const auto capture_height = client_rect.bottom - client_rect.top;
    if (capture_width <= 0 || capture_height <= 0) {
        return kCaptureFailed;
    }

    RECT window_rect{};
    if (!GetWindowRect(window, &window_rect)) {
        return kCaptureFailed;
    }
    const auto window_width = window_rect.right - window_rect.left;
    const auto window_height = window_rect.bottom - window_rect.top;
    if (window_width <= 0 || window_height <= 0) {
        return kCaptureFailed;
    }

    POINT client_screen{};
    if (!ClientToScreen(window, &client_screen)) {
        return kCaptureFailed;
    }
    const auto offset_x = client_screen.x - window_rect.left;
    const auto offset_y = client_screen.y - window_rect.top;

    HDC window_dc = GetDC(window);
    if (window_dc == nullptr) {
        return kCaptureFailed;
    }

    HDC full_dc = CreateCompatibleDC(window_dc);
    if (full_dc == nullptr) {
        ReleaseDC(window, window_dc);
        return kCaptureFailed;
    }
    HBITMAP full_bitmap = CreateCompatibleBitmap(
        window_dc, window_width, window_height);
    if (full_bitmap == nullptr) {
        DeleteDC(full_dc);
        ReleaseDC(window, window_dc);
        return kCaptureFailed;
    }
    const HGDIOBJ old_full = SelectObject(full_dc, full_bitmap);
    timer.mark(&EveCaptureMetrics::setup_ns);

    const BOOL printed = PrintWindow(
        window, full_dc, PW_RENDERFULLCONTENT);
    if (!printed) {
        if (metrics != nullptr) { metrics->used_bitblt_fallback = 1; }
        BitBlt(
            full_dc, offset_x, offset_y,
            capture_width, capture_height,
            window_dc, 0, 0, SRCCOPY);
    }
    timer.mark(&EveCaptureMetrics::render_ns);

    HDC client_dc = CreateCompatibleDC(window_dc);
    if (client_dc == nullptr) {
        SelectObject(full_dc, old_full);
        DeleteObject(full_bitmap);
        DeleteDC(full_dc);
        ReleaseDC(window, window_dc);
        return kCaptureFailed;
    }
    HBITMAP bitmap = CreateCompatibleBitmap(
        window_dc, capture_width, capture_height);
    if (bitmap == nullptr) {
        DeleteDC(client_dc);
        SelectObject(full_dc, old_full);
        DeleteObject(full_bitmap);
        DeleteDC(full_dc);
        ReleaseDC(window, window_dc);
        return kCaptureFailed;
    }
    const HGDIOBJ old_bitmap = SelectObject(client_dc, bitmap);
    BitBlt(
        client_dc, 0, 0, capture_width, capture_height,
        full_dc, offset_x, offset_y, SRCCOPY);
    timer.mark(&EveCaptureMetrics::crop_ns);

    const auto row_bytes = static_cast<std::size_t>(capture_width) * 4;
    auto* buffer = new (std::nothrow) std::uint8_t[
        row_bytes * static_cast<std::size_t>(capture_height)];
    if (buffer == nullptr) {
        SelectObject(client_dc, old_bitmap);
        DeleteObject(bitmap);
        DeleteDC(client_dc);
        SelectObject(full_dc, old_full);
        DeleteObject(full_bitmap);
        DeleteDC(full_dc);
        ReleaseDC(window, window_dc);
        return kOutOfMemory;
    }

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = capture_width;
    info.bmiHeader.biHeight = -capture_height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    const auto scanned_lines = GetDIBits(
        client_dc, bitmap, 0, static_cast<UINT>(capture_height),
        buffer, &info, DIB_RGB_COLORS);
    timer.mark(&EveCaptureMetrics::readback_ns);

    SelectObject(client_dc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(client_dc);
    SelectObject(full_dc, old_full);
    DeleteObject(full_bitmap);
    DeleteDC(full_dc);
    ReleaseDC(window, window_dc);
    timer.mark(&EveCaptureMetrics::cleanup_ns);

    if (scanned_lines != capture_height) {
        delete[] buffer;
        return kCaptureFailed;
    }

    *width = capture_width;
    *height = capture_height;
    *pixels = buffer;
    if (metrics != nullptr) {
        metrics->bytes = row_bytes * static_cast<std::size_t>(capture_height);
        metrics->succeeded = 1;
    }
    return kOk;
}

void eve_viola_free_pixels(std::uint8_t* pixels) {
    delete[] pixels;
}

}  // extern "C"
