#include "config/json_document_io.h"
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>

namespace ConfigDocumentIO
{
bool fail(QString* error, const QString& message)
{
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

QString sectionFilePath(const QString& directory, const QString& name)
{
    return directory + QStringLiteral("/") + name;
}

bool readJsonObject(const QString& path, QJsonObject* object, QString* error)
{
    QFile file(path);
    if (!file.exists()) {
        *object = {};
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return fail(error, QStringLiteral("无法读取配置文件：%1").arg(file.errorString()));
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return fail(error,
                    QStringLiteral("配置文件不是有效 JSON：%1").arg(parseError.errorString()));
    }
    *object = document.object();
    return true;
}

bool writeJsonObject(const QString& path, const QJsonObject& object, QString* error)
{
    QJsonObject previous;
    if (!readJsonObject(path, &previous, error)) {
        return false;
    }
    if (QFile::exists(path) && previous == object) {
        return true;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return fail(error, QStringLiteral("无法写入配置文件：%1").arg(file.errorString()));
    }
    if (file.write(QJsonDocument(object).toJson(QJsonDocument::Indented)) < 0) {
        return fail(error, QStringLiteral("写入配置文件失败：%1").arg(file.errorString()));
    }
    if (!file.commit()) {
        return fail(error, QStringLiteral("保存配置文件失败：%1").arg(file.errorString()));
    }
    return true;
}
} // namespace ConfigDocumentIO
