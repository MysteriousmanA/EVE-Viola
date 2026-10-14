#include "features/desktop_pet/desktop_pet_settings_store.h"
#include <QDir>
#include <QBuffer>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QImageReader>
#include <utility>
namespace
{
bool saveImageAtomically(const QImage &image, const QString &path)
{
    QByteArray encoded;
    QBuffer buffer(&encoded);
    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG")) {
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(encoded) != encoded.size()) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}
} // namespace
DesktopPetSettingsStore::DesktopPetSettingsStore(QString directory)
    : directory_(std::move(directory))
{
}
DesktopPetSettingsRead DesktopPetSettingsStore::load() const
{
    QFile file(QDir(directory_).filePath(QStringLiteral("desktopPet.json")));
    if (!file.open(QIODevice::ReadOnly)) {
        return {{}, false, !file.exists(), false};
    }
    QJsonParseError error{};
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return {{}, false, false, true};
    }
    return {document.object(), true, true, false};
}
bool DesktopPetSettingsStore::save(const QJsonObject &settings) const
{
    if (!load().writable) {
        return false;
    }
    if (directory_.isEmpty() || !QDir().mkpath(directory_)) {
        return false;
    }
    QSaveFile file(QDir(directory_).filePath(QStringLiteral("desktopPet.json")));
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    const auto bytes = QJsonDocument(settings).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size()) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}
QImage DesktopPetSettingsStore::loadSourceImage() const
{
    QImageReader reader(QDir(directory_).filePath(QStringLiteral("desktopPetSource.png")));
    reader.setAutoTransform(true);
    return reader.read();
}
bool DesktopPetSettingsStore::saveSourceImage(const QImage &image) const
{
    return QDir().mkpath(directory_) &&
           saveImageAtomically(image,
                               QDir(directory_).filePath(QStringLiteral("desktopPetSource.png")));
}
