#pragma once

#include <cstdint>

#ifdef EVE_VIOLA_NATIVE_EXPORTS
#define EVE_VIOLA_API __declspec(dllexport)
#else
#define EVE_VIOLA_API __declspec(dllimport)
#endif

extern "C" {

struct EveRect {
    std::int32_t left;
    std::int32_t top;
    std::int32_t right;
    std::int32_t bottom;
};

struct EveWindowInfo {
    std::uintptr_t hwnd;
    wchar_t title[512];
};

// Additive profiling entry point; the existing v3 capture ABI remains valid.
// Timings are monotonic wall-clock nanoseconds, not CPU or GPU execution time.
struct EveCaptureMetrics {
    std::uint64_t setup_ns;
    std::uint64_t render_ns;
    std::uint64_t crop_ns;
    std::uint64_t readback_ns;
    std::uint64_t cleanup_ns;
    std::uint64_t total_ns;
    std::uint64_t bytes;
    std::uint32_t used_bitblt_fallback;
    std::uint32_t succeeded;
};

// An immutable client-area view into a top-down 32-bit DIB. Its row stride
// may exceed width * 4. The owner retains the surface until release_frame;
// subsequent captures (including resizes) do not overwrite a retained view.
struct EveCaptureFrame {
    std::int32_t width;
    std::int32_t height;
    std::int32_t bytes_per_line;
    const std::uint8_t* pixels;
    std::uintptr_t owner;
};

EVE_VIOLA_API std::uint32_t eve_viola_api_version();
EVE_VIOLA_API std::int32_t eve_viola_enumerate_eve_windows(
    EveWindowInfo** items,
    std::uint32_t* count);
EVE_VIOLA_API void eve_viola_free_window_list(EveWindowInfo* items);
EVE_VIOLA_API std::int32_t eve_viola_thumbnail_create(
    std::uintptr_t destination,
    std::uintptr_t source,
    std::uintptr_t* thumbnail);
EVE_VIOLA_API std::int32_t eve_viola_thumbnail_source_size(
    std::uintptr_t thumbnail,
    std::int32_t* width,
    std::int32_t* height);
EVE_VIOLA_API std::int32_t eve_viola_thumbnail_update(
    std::uintptr_t thumbnail,
    EveRect source,
    EveRect destination,
    std::uint8_t opacity,
    std::int32_t visible);
EVE_VIOLA_API std::int32_t eve_viola_thumbnail_destroy(std::uintptr_t thumbnail);
// Resume a minimized live-preview source at the bottom of the window stack,
// without activating it. Visible sources are left untouched; requests are async.
EVE_VIOLA_API std::int32_t eve_viola_resume_thumbnail_source(std::uintptr_t hwnd);
EVE_VIOLA_API std::int32_t eve_viola_activate_window(
    std::uintptr_t hwnd,
    std::uintptr_t* observed_foreground_hwnd);

EVE_VIOLA_API std::int32_t eve_viola_capture_window(
    std::uintptr_t hwnd,
    std::int32_t* width,
    std::int32_t* height,
    std::uint8_t** pixels);
EVE_VIOLA_API void eve_viola_free_pixels(std::uint8_t* pixels);

EVE_VIOLA_API std::int32_t eve_viola_capture_window_profiled(
    std::uintptr_t hwnd,
    std::int32_t* width,
    std::int32_t* height,
    std::uint8_t** pixels,
    EveCaptureMetrics* metrics);

EVE_VIOLA_API std::int32_t eve_viola_capture_frame(
    std::uintptr_t hwnd, EveCaptureFrame* frame, EveCaptureMetrics* metrics);
EVE_VIOLA_API void eve_viola_release_frame(EveCaptureFrame* frame);

}
