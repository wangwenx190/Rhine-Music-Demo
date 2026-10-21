// Reads fixture bytes for tests; a missing or unreadable file is represented by empty bytes.

#pragma once
#include <QFile>
#include <QString>

namespace yalahoo::test {
inline QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
}
