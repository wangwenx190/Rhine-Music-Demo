// Reads and writes fixture bytes; writers create parent directories and report incomplete writes.

#pragma once
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QString>

namespace yalahoo::test {
inline bool writeFile(const QString &path, const QByteArray &bytes)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.flush();
}
inline QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
}
