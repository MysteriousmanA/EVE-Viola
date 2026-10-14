#pragma once

#include <cstdint>

#include <QJsonObject>
#include <QString>

class RuntimeLogger final {
public:
    static void initialize(const QString& logDirectory);
    static QString writeEvent(
        const QString& eventType,
        QJsonObject fields = {});
    [[nodiscard]] static QString formatWindowHandle(std::uintptr_t hwnd);
};
