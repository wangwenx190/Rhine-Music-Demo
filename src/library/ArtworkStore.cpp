#include <QCoreApplication>
#include "ArtworkStore.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QSaveFile>

using namespace Qt::StringLiterals;

namespace yalahoo {
namespace {
bool withinImageAllocationLimit(const QImage &image)
{
    // Embedded artwork is already decoded by Qt Multimedia. Apply the same
    // minimum 32-bit allocation accounting as QImageIOHandler before encoding it.
    const auto limit = QImageReader::allocationLimit();
    if (limit == 0)
        return true;
    const qint64 bytes = qint64(limit) * 1024 * 1024;
    return image.sizeInBytes() <= bytes && qint64(image.width()) * image.height() <= bytes / 4;
}
QUrl persist(const QByteArray &bytes, const QByteArray &format, const QString &root, QString *error)
{
    const auto hash = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
    const auto directory = QDir(root).filePath(u"artwork"_s);
    const auto path = QDir(directory).filePath(QString::fromUtf8(hash + '.' + format));
    if (QFileInfo(directory).isSymLink() || QFileInfo(path).isSymLink()) {
        if (error)
            *error = QCoreApplication::translate(
                    "Yalahoo", "Cover cache directories and files cannot be symbolic links.");
        return { };
    }
    QFile previous(path);
    if (previous.open(QIODevice::ReadOnly) && previous.size() == bytes.size()
        && previous.read(ArtworkStore::maximumBytes + 1) == bytes)
        return QUrl::fromLocalFile(path);
    if (!QDir().mkpath(directory)) {
        if (error)
            *error = QCoreApplication::translate("Yalahoo",
                                                 "Cannot create the cover cache directory.");
        return { };
    }
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)
        || !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)
        || file.write(bytes) != bytes.size() || !file.commit()) {
        if (error)
            *error = QCoreApplication::translate("Yalahoo", "Cannot atomically save the cover: ")
                    + file.errorString();
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
            *error = QCoreApplication::translate("Yalahoo",
                                                 "Cannot read the cover, or it exceeds 30 MiB.");
        return { };
    }
    auto bytes = file.read(maximumBytes + 1);
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    const auto format = reader.format().toLower();
    if (bytes.size() > maximumBytes || file.error() != QFileDevice::NoError
        || !reader.size().isValid()
        || (format != "png"_ba && format != "jpeg"_ba && format != "webp"_ba)) {
        if (error)
            *error = QCoreApplication::translate("Yalahoo", "Invalid cover format or dimensions.");
        return { };
    }
    // Decode once here to reject corrupt images before selecting their priority.
    reader.setAutoTransform(true);
    if (reader.read().isNull()) {
        if (error)
            *error = QCoreApplication::translate("Yalahoo", "Cannot decode the cover: ")
                    + reader.errorString();
        return { };
    }
    return persist(bytes, format, dataRoot, error);
}
QUrl ArtworkStore::save(const QImage &image, const QString &dataRoot, QString *error)
{
    if (error)
        error->clear();
    if (image.isNull() || !withinImageAllocationLimit(image)) {
        if (error)
            *error = QCoreApplication::translate(
                    "Yalahoo",
                    "Invalid embedded cover, or it exceeds Qt's image allocation limit.");
        return { };
    }
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "PNG") || bytes.size() > maximumBytes) {
        if (error)
            *error = QCoreApplication::translate(
                    "Yalahoo", "Cannot encode the embedded cover, or it exceeds 30 MiB.");
        return { };
    }
    return persist(bytes, "png"_ba, dataRoot, error);
}
}
