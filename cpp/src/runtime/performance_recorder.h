#pragma once

#include <cstdint>

#include <QJsonObject>
#include <QString>

// Explicitly enabled. No clock reads, locks or disk I/O on the disabled path.
class PerformanceRecorder final {
public:
    static bool initialize(const QString& filePath, QString* error = nullptr);
    static bool enabled() noexcept;
    static qint64 nowNs() noexcept;
    static void record(const char* stage, qint64 nanoseconds,
                       std::uintptr_t source = 0, quint64 units = 0,
                       bool failed = false);
    static QJsonObject snapshot();
    // Writes and resets the current interval only after a successful write.
    static bool flush(QString* error = nullptr);
    static void disable();
};

class PerformanceScope final {
public:
    explicit PerformanceScope(const char* stage, std::uintptr_t source = 0) noexcept;
    ~PerformanceScope();
    PerformanceScope(const PerformanceScope&) = delete;
    PerformanceScope& operator=(const PerformanceScope&) = delete;

private:
    const char* stage_;
    std::uintptr_t source_;
    qint64 start_;
};
