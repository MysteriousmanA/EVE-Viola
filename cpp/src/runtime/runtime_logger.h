#pragma once

#include <cstdint>

#include <QJsonObject>
#include <QString>

class RuntimeLogger final {
public:
    static void initialize(const QString& logDirectory);
    // Flush every two seconds from the application and again during shutdown.
    [[nodiscard]] static bool flush();
    static QString writeEvent(
        const QString& eventType,
        QJsonObject fields = {});
    [[nodiscard]] static QString formatWindowHandle(std::uintptr_t hwnd);
};
