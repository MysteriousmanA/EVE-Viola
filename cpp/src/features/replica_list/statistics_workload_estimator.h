#pragma once
#include <QList>
#include <QRect>
#include <QSize>
#include <optional>
#include <cstdint>
struct StatisticsWorkloadInput {
    std::uintptr_t hwnd{};
    QSize frameSize;
    std::optional<QRect> region;
    int intervalMs{};
};
struct StatisticsWorkloadEstimate {
    double capturePixelsPerSecond{};
    double scanPixelsPerSecond{};
};
StatisticsWorkloadEstimate
estimateStatisticsWorkload(const QList<StatisticsWorkloadInput> &requests);
