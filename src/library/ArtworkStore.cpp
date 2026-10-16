#include "ArtworkStore.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QSaveFile>

namespace yalahoo {
namespace {
bool validSize(QSize size)
{
    return size.width() > 0 && size.height() > 0
            && qint64(size.width()) * size.height() <= ArtworkStore::maximumPixels;
}
QUrl persist(const QByteArray &bytes, const QByteArray &format, const QString &root, QString *error)
{
    const auto hash = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
    const auto directory = QDir(root).filePath("artwork");
    const auto path = QDir(directory).filePath(QString::fromLatin1(hash + '.' + format));
    if (QFileInfo(directory).isSymLink() || QFileInfo(path).isSymLink()) {
        if (error)
            *error = QStringLiteral("封面缓存目录/文件不能是符号链接。");
        return { };
    }
    QFile previous(path);
    if (previous.open(QIODevice::ReadOnly) && previous.size() == bytes.size()
        && previous.read(ArtworkStore::maximumBytes + 1) == bytes)
        return QUrl::fromLocalFile(path);
    if (!QDir().mkpath(directory)) {
        if (error)
            *error = QStringLiteral("无法创建封面缓存目录。");
        return { };
    }
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)
        || !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)
        || file.write(bytes) != bytes.size() || !file.commit()) {
        if (error)
            *error = QStringLiteral("无法原子保存封面：") + file.errorString();
        return { };
    }
    return QUrl::fromLocalFile(path);
}
}
QUrl ArtworkStore::copy(const QString &source, const QString &dataRoot, QString *error)
{
    if (error)
        error->clear();
    if (source.isEmpty())
        return { };
    QFile file(source);
    if (QFileInfo(source).isSymLink() || !file.open(QIODevice::ReadOnly) || file.size() <= 0
        || file.size() > maximumBytes) {
        if (error)
            *error = QStringLiteral("封面无法读取或超过 30MiB。");
        return { };
    }
    auto bytes = file.read(maximumBytes + 1);
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    const auto format = reader.format().toLower();
    if (bytes.size() > maximumBytes || file.error() != QFileDevice::NoError
        || !validSize(reader.size()) || (format != "png" && format != "jpeg" && format != "webp")) {
        if (error)
            *error = QStringLiteral("封面格式无效或展开尺寸超过 4194304 像素。");
        return { };
    }
    // Decode once here to reject corrupt images before selecting their priority.
    reader.setAutoTransform(true);
    if (reader.read().isNull()) {
        if (error)
            *error = QStringLiteral("封面已损坏。");
        return { };
    }
    return persist(bytes, format, dataRoot, error);
}
QUrl ArtworkStore::save(const QImage &image, const QString &dataRoot, QString *error)
{
    if (error)
        error->clear();
    if (!validSize(image.size())) {
        if (error)
            *error = QStringLiteral("内嵌封面展开尺寸无效或过大。");
        return { };
    }
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "PNG") || bytes.size() > maximumBytes) {
        if (error)
            *error = QStringLiteral("内嵌封面编码失败或超过 30MiB。");
        return { };
    }
    return persist(bytes, "png", dataRoot, error);
}
}
