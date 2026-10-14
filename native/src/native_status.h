#pragma once

#include <cstdint>

namespace eve_viola::native_detail {

constexpr std::int32_t kOk = 0;
constexpr std::int32_t kInvalidArgument = -1;
constexpr std::int32_t kOutOfMemory = -2;
constexpr std::int32_t kEnumerationFailed = -3;
constexpr std::int32_t kCaptureFailed = -4;
constexpr std::int32_t kActivationFailed = -5;

}  // namespace eve_viola::native_detail
