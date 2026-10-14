#pragma once

#include <QString>

struct AppConfig;

class JsonConfigStore final
{
public:
    explicit JsonConfigStore(QString directoryPath);

    [[nodiscard]] const QString& directoryPath() const noexcept;
    [[nodiscard]] bool load(AppConfig& config, QString* error = nullptr) const;
    [[nodiscard]] bool save(const AppConfig& config, QString* error = nullptr) const;

private:
    QString directoryPath_;
};
