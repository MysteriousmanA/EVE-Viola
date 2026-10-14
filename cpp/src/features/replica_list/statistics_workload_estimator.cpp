#include "features/replica_list/statistics_workload_estimator.h"
#include <algorithm>
#include <numeric>
#include <utility>
#include <QMap>
// Registrations for one source share a phase. Count the union of their
// periodic requests so coincident captures/scans contribute only once.
double estimatedStatisticsRequestsPerSecond(const QList<int> &intervals)
{
    QMap<qint64, qint64> periodCoefficients;
    for (const int interval : intervals) {
        auto nextCoefficients = periodCoefficients;
        ++nextCoefficients[interval];
        for (auto period = periodCoefficients.cbegin(); period != periodCoefficients.cend();
             ++period) {
            const qint64 commonPeriod =
                period.key() / std::gcd(period.key(), static_cast<qint64>(interval)) * interval;
            nextCoefficients[commonPeriod] -= period.value();
        }
        for (auto period = nextCoefficients.begin(); period != nextCoefficients.end();) {
            if (period.value() == 0) {
                period = nextCoefficients.erase(period);
            } else {
                ++period;
            }
        }
        periodCoefficients = std::move(nextCoefficients);
    }

    double requestsPerSecond = 0.0;
    for (auto period = periodCoefficients.cbegin(); period != periodCoefficients.cend(); ++period) {
        requestsPerSecond += static_cast<double>(period.value()) * 1000.0 / period.key();
    }
    return requestsPerSecond;
}

StatisticsWorkloadEstimate
estimateStatisticsWorkload(const QList<StatisticsWorkloadInput> &requests)
{
    struct RegionRequests {
        std::optional<QRect> region;
        QList<int> intervals;
    };
    struct SourceRequests {
        std::uintptr_t hwnd{};
        QSize frameSize;
        QList<int> intervals;
        QList<RegionRequests> regions;
    };

    QList<SourceRequests> sources;
    for (const auto &request : requests) {
        if (request.intervalMs <= 0 || request.hwnd == 0) {
            continue;
        }
        const auto hwnd = request.hwnd;
        auto source =
            std::find_if(sources.begin(), sources.end(), [hwnd](const SourceRequests &candidate) {
                return candidate.hwnd == hwnd;
            });
        if (source == sources.end()) {
            sources.append(SourceRequests{hwnd, request.frameSize, {}, {}});
            source = sources.end() - 1;
        } else if (!source->frameSize.isValid() || source->frameSize.isEmpty()) {
            source->frameSize = request.frameSize;
        }
        const int interval = request.intervalMs;
        if (!source->intervals.contains(interval)) {
            source->intervals.append(interval);
        }

        const auto &capturedRegion = request.region;
        auto region = std::find_if(source->regions.begin(), source->regions.end(),
                                   [&capturedRegion](const RegionRequests &candidate) {
                                       return candidate.region == capturedRegion;
                                   });
        if (region == source->regions.end()) {
            source->regions.append(RegionRequests{capturedRegion, {interval}});
        } else if (!region->intervals.contains(interval)) {
            region->intervals.append(interval);
        }
    }

    StatisticsWorkloadEstimate estimate;
    bool unknownCaptureSize = false;
    bool unknownScanSize = false;
    for (const auto &source : std::as_const(sources)) {
        const bool frameSizeKnown = source.frameSize.isValid() && !source.frameSize.isEmpty();
        const QRect sourceBounds(QPoint(0, 0), source.frameSize);
        if (frameSizeKnown) {
            const double framePixels =
                static_cast<double>(source.frameSize.width()) * source.frameSize.height();
            // Statistics capture reads the complete source frame even when
            // every replica scans only a small cropped region.
            estimate.capturePixelsPerSecond +=
                framePixels * estimatedStatisticsRequestsPerSecond(source.intervals);
        } else {
            unknownCaptureSize = true;
        }

        for (const auto &request : source.regions) {
            if (!frameSizeKnown && !request.region.has_value()) {
                unknownScanSize = true;
                continue;
            }
            QRect region = request.region.value_or(sourceBounds).normalized();
            if (frameSizeKnown) {
                region = region.intersected(sourceBounds);
            }
            if (!region.isEmpty()) {
                const double regionPixels = static_cast<double>(region.width()) * region.height();
                estimate.scanPixelsPerSecond +=
                    regionPixels * estimatedStatisticsRequestsPerSecond(request.intervals);
            }
        }
    }
    // Missing dimensions must not be presented as zero workload.
    if (unknownCaptureSize) {
        estimate.capturePixelsPerSecond = -1.0;
    }
    if (unknownScanSize) {
        estimate.scanPixelsPerSecond = -1.0;
    }
    return estimate;
}
