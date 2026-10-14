#include "native_platform.h"
#include "native_status.h"
#include "eve_viola_native.h"
#include <dwmapi.h>

using namespace eve_viola::native_detail;

namespace {

std::int32_t hresult_status(HRESULT result) {
    return static_cast<std::int32_t>(result);
}

}  // namespace

extern "C" {

std::int32_t eve_viola_thumbnail_create(
    std::uintptr_t destination,
    std::uintptr_t source,
    std::uintptr_t* thumbnail) {
    if (destination == 0 || source == 0 || thumbnail == nullptr) {
        return kInvalidArgument;
    }
    *thumbnail = 0;
    HTHUMBNAIL native_thumbnail = nullptr;
    const auto result = DwmRegisterThumbnail(
        reinterpret_cast<HWND>(destination),
        reinterpret_cast<HWND>(source),
        &native_thumbnail);
    if (FAILED(result)) {
        return hresult_status(result);
    }
    *thumbnail = reinterpret_cast<std::uintptr_t>(native_thumbnail);
    return kOk;
}

std::int32_t eve_viola_thumbnail_source_size(
    std::uintptr_t thumbnail,
    std::int32_t* width,
    std::int32_t* height) {
    if (thumbnail == 0 || width == nullptr || height == nullptr) {
        return kInvalidArgument;
    }
    SIZE size{};
    const auto result = DwmQueryThumbnailSourceSize(
        reinterpret_cast<HTHUMBNAIL>(thumbnail), &size);
    if (FAILED(result)) {
        return hresult_status(result);
    }
    *width = size.cx;
    *height = size.cy;
    return kOk;
}

std::int32_t eve_viola_thumbnail_update(
    std::uintptr_t thumbnail,
    EveRect source,
    EveRect destination,
    std::uint8_t opacity,
    std::int32_t visible) {
    if (thumbnail == 0 || destination.right <= destination.left ||
        destination.bottom <= destination.top) {
        return kInvalidArgument;
    }

    DWM_THUMBNAIL_PROPERTIES properties{};
    properties.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_VISIBLE |
                         DWM_TNP_OPACITY | DWM_TNP_SOURCECLIENTAREAONLY;
    properties.rcDestination = RECT{
        destination.left, destination.top, destination.right, destination.bottom};
    properties.opacity = opacity;
    properties.fVisible = visible ? TRUE : FALSE;
    properties.fSourceClientAreaOnly = TRUE;

    if (source.right > source.left && source.bottom > source.top) {
        properties.dwFlags |= DWM_TNP_RECTSOURCE;
        properties.rcSource = RECT{
            source.left, source.top, source.right, source.bottom};
    }
    return hresult_status(DwmUpdateThumbnailProperties(
        reinterpret_cast<HTHUMBNAIL>(thumbnail), &properties));
}

std::int32_t eve_viola_thumbnail_destroy(std::uintptr_t thumbnail) {
    if (thumbnail == 0) {
        return kOk;
    }
    return hresult_status(DwmUnregisterThumbnail(
        reinterpret_cast<HTHUMBNAIL>(thumbnail)));
}

}  // extern "C"
