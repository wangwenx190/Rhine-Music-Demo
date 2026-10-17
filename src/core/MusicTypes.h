#pragma once

#include <QJsonObject>
#include <QJsonArray>
#include <QList>
#include <QMetaType>
#include <QStringList>
#include <QUrl>
#include <memory>
#include <optional>

namespace yalahoo {
enum class MediaSupport {
    Unknown,
    Supported,
    Unsupported
};
struct Track
{
    QString id, albumId, title, artist, format, codec, relativePath;
    std::optional<int> trackNumber, discNumber, bitsPerSample, sampleRate, bitrate;
    std::optional<bool> lossless;
    double duration = 0;
    MediaSupport mediaSupport = MediaSupport::Unknown;
    QUrl sourceUrl;
    QJsonObject fields;
};
struct Producer
{
    QString name, role, source, trackId, trackTitle, url;
    QJsonObject fields;
};
struct Album
{
    QString id, title, artist, genreId, description, localNote, folderLabel;
    std::optional<int> year, discCount;
    QStringList rawGenres;
    QUrl folder, coverUrl;
    QList<Track> tracks;
    QList<Producer> producers;
    QJsonObject descriptionSource, introduction, creditsLookup, online;
    bool offline = false;
    QJsonObject fields;
};
struct Genre
{
    QString id, name;
    QStringList aliases;
    std::optional<int> albumCount;
    QJsonObject fields;
};
struct LibraryRoot
{
    QUrl path;
    QString status, error;
    QJsonObject fields;
};
struct LibrarySnapshot
{
    QList<Album> albums;
    QList<Genre> genres;
    QList<LibraryRoot> roots;
    QJsonObject scan, enrich, introductions, credits, fields;
    bool onlineEnabled = false;
    const Album *album(const QString &id) const;
    const Track *track(const QString &albumId, const QString &trackId) const;
};
using Snapshot = std::shared_ptr<const LibrarySnapshot>;

// Transactional parsing: malformed input never replaces the caller's snapshot.
bool readLibrary(const QJsonObject &json, LibrarySnapshot &output, QString *error = nullptr);
QJsonArray producerData(const QList<Producer> &producers);
QJsonObject toJson(const Track &track);
QJsonObject toJson(const Album &album);
QJsonObject toJson(const LibrarySnapshot &library);
QString supportName(MediaSupport support);
QPair<QString, QString> albumTitleParts(const QString &text);
QString graphemePrefix(const QString &text, qsizetype maxCodeUnits);
}
