#include "AssetFiles.h"
#include "Require.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
namespace yalahoo::assets {
QByteArray readFile(const QString &path)
{
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "Cannot open asset");
    return file.readAll();
}
void saveFile(const QString &path, const QByteArray &bytes)
{
    require(QDir().mkpath(QFileInfo(path).absolutePath()), "Cannot create asset directory");
    QSaveFile file(path);
    require(file.open(QIODevice::WriteOnly), "Cannot write asset");
    require(file.write(bytes) == bytes.size() && file.commit(), "Cannot commit asset");
}

}
