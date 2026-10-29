#pragma once

#include <QString>
#include <QJsonObject>

namespace ConfigDocumentIO
{
[[nodiscard]] bool fail(QString* error, const QString& message);
[[nodiscard]] QString sectionFilePath(const QString& directory, const QString& name);
[[nodiscard]] bool readJsonObject(const QString& path, QJsonObject* object, QString* error);
[[nodiscard]] bool writeJsonObject(const QString& path, const QJsonObject& object, QString* error);
// The caller must validate existing documents before committing a batch.
[[nodiscard]] bool writeJsonObjectAtomically(const QString& path, const QJsonObject& object, QString* error);
} // namespace ConfigDocumentIO
