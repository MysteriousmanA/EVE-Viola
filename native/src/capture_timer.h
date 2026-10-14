#pragma once

#include "eve_viola_native.h"
#include <chrono>

namespace eve_viola::native_detail {

class CaptureTimer final {
public:
    explicit CaptureTimer(EveCaptureMetrics* metrics) : metrics_(metrics) {
        if (metrics_ != nullptr) {
            *metrics_ = {};
            start_ = last_ = std::chrono::steady_clock::now();
        }
    }
    ~CaptureTimer() {
        if (metrics_ != nullptr) {
            metrics_->total_ns = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - start_).count());
        }
    }
    void mark(std::uint64_t EveCaptureMetrics::*field) {
        if (metrics_ != nullptr) {
            const auto now = std::chrono::steady_clock::now();
            metrics_->*field = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(now - last_).count());
            last_ = now;
        }
    }
private:
    EveCaptureMetrics* metrics_;
    std::chrono::steady_clock::time_point start_{};
    std::chrono::steady_clock::time_point last_{};
};

}  // namespace eve_viola::native_detail
