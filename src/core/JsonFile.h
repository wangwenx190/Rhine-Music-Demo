// Writes generated JSON atomically and checks the complete payload before committing.
// Callers own directory creation, private-storage permissions and error presentation.

#pragma once
#include <QJsonDocument>
#include <QSaveFile>

namespace yalahoo {
[[nodiscard]] inline bool writeJsonFile(const QString &path, const QJsonDocument &document)
{
    if (document.isNull())
        return false;
    QSaveFile file(path);
    const auto bytes = document.toJson();
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}
}
