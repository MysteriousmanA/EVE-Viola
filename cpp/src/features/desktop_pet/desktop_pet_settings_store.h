#pragma once
#include <QJsonObject>
#include <QImage>
#include <QString>
struct DesktopPetSettingsRead {
    QJsonObject settings;
    bool loaded{};
    bool writable{true};
    bool parseFailed{};
};
class DesktopPetSettingsStore
{
  public:
    explicit DesktopPetSettingsStore(QString directory);
    DesktopPetSettingsRead load() const;
    bool save(const QJsonObject &settings) const;
    QImage loadSourceImage() const;
    bool saveSourceImage(const QImage &image) const;

  private:
    QString directory_;
};
