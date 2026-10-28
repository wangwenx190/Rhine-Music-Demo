// Reads asset inputs and atomically saves generated files.
#pragma once
#include <QByteArray>
#include <QString>
namespace yalahoo::assets {
QByteArray readFile(const QString &path);
void saveFile(const QString &path, const QByteArray &bytes);
}
