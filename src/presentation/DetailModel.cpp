#include "DetailModel.h"
#include <memory>
#include <QObject>
#include "MusicTypes.h"
#include <QAbstractListModel>
#include <QCache>
#include <QFont>
#include <QByteArray>
#include <utility>
#include "Units.h"
#include "LibrarySession.h"
#include <QCoreApplication>
#include <QSet>
#include <QTextLayout>
#include <QGuiApplication>
#include <QDataStream>
#include <QIODevice>
#include <algorithm>
#include <iterator>
#include <cmath>

using namespace Qt::StringLiterals;

namespace yalahoo {
namespace {
// Formatting uses decimal units; cached line heights account for key storage and entry overhead.
constexpr auto LineCacheEntryOverheadBytes = 64;
constexpr auto MaximumDisplayDurationSeconds = 1e12;
constexpr auto SecondsPerMinute = 60;
constexpr auto FormatSignificantDigits = 8;
constexpr auto MetricPrefixScale = 1000;

QString verifiedStatus(const QJsonObject &lookup, bool credits, bool, int trackCount)
{
    return credits ? QCoreApplication::translate("Yalahoo", "Track credits verified: %1 / %2.")
                             .arg(lookup.value(u"matchedTracks"_s).toInt())
                             .arg(lookup.value(u"totalTracks"_s).toInt(trackCount))
                   : QCoreApplication::translate("Yalahoo", "The introduction is saved locally.");
}
QString statusMessage(const QJsonObject &lookup, const QString &status, bool retained, bool credits,
                      int trackCount)
{
    struct Message final
    {
        QLatin1StringView status;
        QString (*text)(const QJsonObject &, bool, bool, int);
    };
    static constexpr Message messages[] = {
        { "error"_L1,
          [](const QJsonObject &state, bool, bool, int) {
              return QCoreApplication::translate("Yalahoo", "Query failed: ")
                      + state.value(u"error"_s)
                                .toString(QCoreApplication::translate(
                                        "Yalahoo",
                                        "The metadata source is temporarily unavailable."));
          } },
        { "uncertain"_L1,
          [](const QJsonObject &, bool, bool, int) {
              return QCoreApplication::translate(
                      "Yalahoo",
                      "The album or track could not be identified reliably. Uncertain metadata was not applied.");
          } },
        { "not-found"_L1,
          [](const QJsonObject &, bool, bool, int) {
              return QCoreApplication::translate("Yalahoo", "No verifiable metadata was found.");
          } },
        { "matched"_L1, &verifiedStatus },
        { "partial"_L1, &verifiedStatus },
        { "unqueried"_L1,
          [](const QJsonObject &, bool, bool retained, int) {
              return retained
                      ? QCoreApplication::translate("Yalahoo", "Metadata is saved locally.")
                      : QCoreApplication::translate("Yalahoo", "Metadata has not been queried.");
          } }
    };
    const auto message =
            std::find_if(std::begin(messages), std::end(messages), [&](const Message &entry) {
                return entry.status == status;
            });
    return message == std::end(messages)
            ? QCoreApplication::translate("Yalahoo", "Metadata status: %1.").arg(status)
            : message->text(lookup, credits, retained, trackCount);
}
QString formatMetadataNumber(int value, double scale, const QString &unit)
{
    double scaled = value / scale;
    if (unit == "kbps"_L1)
        scaled = std::round(scaled);
    else if (unit == "kHz"_L1)
        scaled = std::round(scaled * yalahoo::units::DecimalBase) / yalahoo::units::DecimalBase;
    return QString::number(scaled, 'g', FormatSignificantDigits);
}
QString displayMetadataRange(std::optional<int> low, std::optional<int> high, double scale,
                             const QString &unit)
{
    if (!low)
        return QCoreApplication::translate("Yalahoo", "Not provided");
    const auto number = [scale, &unit](int value) {
        return formatMetadataNumber(value, scale, unit);
    };
    return number(*low) + " "_L1 + unit
            + (*low == *high ? QString() : u"\u2013"_s + number(*high) + " "_L1 + unit);
}
QString lookupStatus(const QJsonObject &lookup, bool retained, bool credits, int trackCount = 0)
{
    const QString status = lookup.value(u"status"_s).toString(u"unqueried"_s);
    const QString kept = retained
            ? QCoreApplication::translate("Yalahoo",
                                          " Existing metadata is retained for offline reading.")
            : QString();
    QString result = statusMessage(lookup, status, retained, credits, trackCount);
    if (status != "error"_L1 && !lookup.value(u"error"_s).toString().isEmpty())
        result += " "_L1 + lookup.value(u"error"_s).toString();
    if (!lookup.value(u"retryAt"_s).toString().isEmpty())
        result += QCoreApplication::translate("Yalahoo", " Retry at: %1.")
                          .arg(lookup.value(u"retryAt"_s).toString());
    return result + kept
            + QCoreApplication::translate("Yalahoo", " Online queries are not enabled yet.");
}
}
struct DetailModel::Private final
{
    Q_DISABLE_COPY_MOVE(Private)
    Private() = default;

    QCache<QByteArray, double> m_lineHeights{ detailText::HeightCacheKiB
                                              * yalahoo::units::BytesPerKiB };
    Snapshot m_snapshot;
    const Album *m_album = nullptr;
    Album m_resolved;
    QVariantMap m_info;
};
DetailModel::DetailModel(QObject *parent)
    : QAbstractListModel(parent), d(std::make_unique<Private>())
{
    if (auto *application = qobject_cast<QGuiApplication *>(QCoreApplication::instance()))
        connect(application, &QGuiApplication::fontDatabaseChanged, this, [this] {
            d->m_lineHeights.clear();
        });
}
double DetailModel::normalLineHeight(const QString &text, const QFont &font)
{
    QByteArray key;
    QDataStream stream(&key, QIODevice::WriteOnly);
    stream << font << text;
    if (const auto *cached = d->m_lineHeights.object(key))
        return *cached;
    QTextLayout layout(text, font);
    layout.beginLayout();
    const auto line = layout.createLine();
    layout.endLayout();
    // CSS normal line boxes round the resolved ascent/descent independently.
    const double height =
            line.isValid() ? std::round(line.ascent()) + std::round(line.descent()) : 0;
    const qsizetype cost = key.size() + LineCacheEntryOverheadBytes;
    if (cost <= d->m_lineHeights.maxCost())
        d->m_lineHeights.insert(key, new double(height), int(cost));
    return height;
}
QString DetailModel::time(double seconds)
{
    const qint64 value = std::isfinite(seconds)
            ? qint64(std::clamp(seconds, 0.0, MaximumDisplayDurationSeconds))
            : 0;
    return u"%1:%2"_s.arg(value / SecondsPerMinute)
            .arg(value % SecondsPerMinute, 2, yalahoo::units::DecimalBase, '0'_L1);
}
int DetailModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() || !d->m_album ? 0 : d->m_album->tracks.size();
}
QVariant DetailModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.model() != this || index.column() != 0 || index.row() < 0
        || index.row() >= rowCount())
        return { };
    const auto &track = d->m_album->tracks[index.row()];
    struct RoleReader final
    {
        int role;
        QVariant (*read)(const DetailModel &self, const Track &track, int row);
    };
    static constexpr RoleReader readers[] = {
        { TrackId,
          [](const DetailModel &, const Track &track, int) -> QVariant {
              return track.id;
          } },
        { Number,
          [](const DetailModel &, const Track &track, int row) -> QVariant {
              return u"%1"_s.arg(track.trackNumber.value_or(row + 1), 2,
                                 yalahoo::units::DecimalBase, '0'_L1);
          } },
        { Title,
          [](const DetailModel &, const Track &track, int) -> QVariant {
              return track.title;
          } },
        { Artist,
          [](const DetailModel &, const Track &track, int) -> QVariant {
              return artistDisplayName(track.artist);
          } },
        { Format,
          [](const DetailModel &, const Track &track, int) -> QVariant {
              return track.format;
          } },
        { Duration,
          [](const DetailModel &, const Track &track, int) -> QVariant {
              return time(track.duration);
          } },
        { Disc,
          [](const DetailModel &, const Track &track, int) -> QVariant {
              return track.discNumber.value_or(1);
          } },
        { DiscHeading,
          [](const DetailModel &self, const Track &track, int row) -> QVariant {
              return self.d->m_info.value(u"discs"_s).toInt() > 1
                      && (row == 0
                          || self.d->m_album->tracks[row - 1].discNumber.value_or(1)
                                  != track.discNumber.value_or(1));
          } }
    };
    static_assert([] {
        int next = TrackId;
        for (const auto &reader : readers) {
            if (reader.role != next++)
                return false;
        }
        return next == DiscHeading + 1;
    }());
    if (role < TrackId || role > DiscHeading)
        return { };
    return readers[role - TrackId].read(*this, track, index.row());
}
QHash<int, QByteArray> DetailModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        { TrackId, "trackId"_ba }, { Number, "number"_ba },          { Title, "trackTitle"_ba },
        { Artist, "artist"_ba },   { Format, "format"_ba },          { Duration, "duration"_ba },
        { Disc, "disc"_ba },       { DiscHeading, "discHeading"_ba }
    };
    return roles;
}
void DetailModel::prepare(const LibrarySession &library, bool metadataOnly)
{
    const auto snapshot = library.snapshot();
    const auto *album = library.navigationAlbum();
    if (!metadataOnly && snapshot == d->m_snapshot && album && album->id == d->m_resolved.id
        && d->m_info.value(u"code"_s) == library.browseInfo().value(u"code"_s))
        return;
    if (!metadataOnly)
        beginResetModel();
    d->m_snapshot = snapshot;
    d->m_resolved = album ? library.resolvedAlbum(album->id) : Album();
    d->m_album = album ? &d->m_resolved : nullptr;
    album = d->m_album;
    d->m_info.clear();
    if (album) {
        const auto &a = *album;
        QVariantList facts, producers;
        producers.reserve(a.producers.size());
        QHash<QString, QString> trackTitles;
        trackTitles.reserve(a.tracks.size());
        const auto fact = [&](const QString &label, const QString &value) {
            facts.append(QVariantMap{ { u"label"_s, label }, { u"value"_s, value } });
        };
        QString genre = QCoreApplication::translate("Yalahoo", "Unclassified");
        for (const auto &g : snapshot->genres) {
            if (g.id == a.genreId) {
                genre = genreDisplayName(g);
                break;
            }
        }
        QStringList onlineGenres;
        for (const auto &value : a.fields.value(u"_onlineGenres"_s).toArray())
            onlineGenres.append(value.toString());
        if (!onlineGenres.isEmpty())
            fact(QCoreApplication::translate("Yalahoo", "MusicBrainz genres"),
                 onlineGenres.join(u", "_s));
        QStringList formats;
        QSet<QString> formatSet;
        int discs = a.discCount.value_or(1);
        double duration = 0;
        bool lossy = !a.tracks.isEmpty();
        std::optional<int> minBits, maxBits, minRate, maxRate, minBitrate, maxBitrate;
        const auto range = [](std::optional<int> value, std::optional<int> &low,
                              std::optional<int> &high) {
            if (value && *value > 0) {
                low = std::min(low.value_or(*value), *value);
                high = std::max(high.value_or(*value), *value);
            }
        };
        for (const auto &track : a.tracks) {
            trackTitles.insert(track.id, track.title);
            const auto format =
                    track.codec.isEmpty() ? track.format : track.format + " / "_L1 + track.codec;
            if (!format.isEmpty()) {
                const auto count = formatSet.size();
                formatSet.insert(format);
                if (formatSet.size() != count)
                    formats.append(format);
            }
            discs = std::max(discs, track.discNumber.value_or(1));
            duration += std::max(0.0, track.duration);
            lossy = lossy && track.lossless == false;
            range(track.bitsPerSample, minBits, maxBits);
            range(track.sampleRate, minRate, maxRate);
            range(track.bitrate, minBitrate, maxBitrate);
        }

        fact(QCoreApplication::translate("Yalahoo", "RELEASE / Year"),
             a.year ? QString::number(*a.year)
                    : QCoreApplication::translate("Yalahoo", "Not provided"));
        fact(QCoreApplication::translate("Yalahoo", "ARTIST"), artistDisplayName(a.artist));
        fact(QCoreApplication::translate("Yalahoo", "GENRE"), genre);
        fact(QCoreApplication::translate("Yalahoo", "VOLUMES / CDs"),
             QCoreApplication::translate("Yalahoo", "%1 CD \u00b7 Tracks: %2")
                     .arg(discs)
                     .arg(a.tracks.size()));
        fact(QCoreApplication::translate("Yalahoo", "FORMAT / File format"),
             formats.isEmpty() ? QCoreApplication::translate("Yalahoo", "Not provided")
                               : formats.join(u" \u00b7 "_s));
        fact(QCoreApplication::translate("Yalahoo", "RESOLUTION / Bit depth and sample rate"),
             (lossy ? QCoreApplication::translate("Yalahoo", "Lossy encoding")
                    : displayMetadataRange(minBits, maxBits, 1, u"bit"_s))
                     + " / "_L1
                     + displayMetadataRange(minRate, maxRate, MetricPrefixScale, u"kHz"_s));
        fact(QCoreApplication::translate("Yalahoo", "BITRATE"),
             displayMetadataRange(minBitrate, maxBitrate, MetricPrefixScale, u"kbps"_s));
        fact(QCoreApplication::translate("Yalahoo", "DURATION / Total time"), time(duration));
        for (const auto &producer : a.producers) {
            producers.append(QVariantMap{
                    { u"name"_s, producer.name },
                    { u"role"_s, producer.role },
                    { u"source"_s, producer.source },
                    { u"url"_s, producer.url },
                    { u"trackTitle"_s, trackTitles.value(producer.trackId, producer.trackTitle) },
                    { u"trackId"_s, producer.trackId } });
        }
        // Group once per immutable album. Stable track IDs resolve through the index above.
        struct CreditGroup final
        {
            QString name;
            QStringList roles, sources, tracks, unscoped;
            QSet<QString> roleKeys, sourceKeys, trackKeys, unscopedKeys;
        };
        QList<CreditGroup> grouped;
        QHash<QString, int> groupIndex;
        const auto appendUnique = [](QStringList &values, QSet<QString> &keys,
                                     const QString &text) {
            if (text.isEmpty())
                return;
            const auto count = keys.size();
            keys.insert(text);
            if (keys.size() != count)
                values.append(text);
        };
        for (const auto &producer : a.producers) {
            const auto key =
                    producer.name.trimmed().normalized(QString::NormalizationForm_KC).toLower()
                    + QChar(0) + producer.source;
            const auto entry = groupIndex.tryEmplace(key, int(grouped.size()));
            const int index = *entry.iterator;
            if (entry.inserted) {
                grouped.append(
                        CreditGroup{ producer.name, { }, { }, { }, { }, { }, { }, { }, { } });
            }
            auto &group = grouped[index];
            appendUnique(group.roles, group.roleKeys, producer.role);
            const auto &source = producer.source;
            const auto &url = producer.url;
            appendUnique(group.sources, group.sourceKeys,
                         source + (url.isEmpty() ? QString() : u" \u00b7 "_s + url));
            const auto trackTitle = trackTitles.value(producer.trackId, producer.trackTitle);
            if (trackTitle.isEmpty()) {
                appendUnique(group.unscoped, group.unscopedKeys, producer.role);
                continue;
            }
            const auto trackKey = producer.trackId + QChar(0) + trackTitle + QChar(0)
                    + producer.role + QChar(0) + url;
            const auto count = group.trackKeys.size();
            group.trackKeys.insert(trackKey);
            if (group.trackKeys.size() == count)
                continue;
            group.tracks.append(trackTitle + u" \u00b7 "_s + producer.role
                                + QCoreApplication::translate("Yalahoo", "\nSource: ") + source
                                + (url.isEmpty() ? QString() : u" \u00b7 "_s + url));
        }
        QVariantList groups;
        groups.reserve(grouped.size());
        for (const auto &group : grouped)
            groups.append(QVariantMap{ { u"name"_s, group.name },
                                       { u"roles"_s, group.roles },
                                       { u"sources"_s, group.sources },
                                       { u"tracks"_s, group.tracks },
                                       { u"unscoped"_s, group.unscoped } });
        const auto title = albumTitleParts(a.title);
        QStringList descriptionSource{ a.descriptionSource.value(u"name"_s).toString(),
                                       a.descriptionSource.value(u"url"_s).toString(),
                                       a.descriptionSource.value(u"license"_s).toString() };
        descriptionSource.removeAll(QString());
        d->m_info = { { u"id"_s, a.id },
                      { u"title"_s, a.title },
                      { u"mainTitle"_s, title.main },
                      { u"translation"_s, title.translation },
                      { u"artist"_s, artistDisplayName(a.artist) },
                      { u"offline"_s, a.offline },
                      { u"description"_s, a.description },
                      { u"localNote"_s, a.localNote },
                      { u"descriptionSource"_s, a.descriptionSource.toVariantMap() },
                      { u"descriptionSourceText"_s, descriptionSource.join(u" \u00b7 "_s) },
                      { u"introduction"_s, a.introduction.toVariantMap() },
                      { u"creditsLookup"_s, a.creditsLookup.toVariantMap() },
                      { u"producers"_s, producers },
                      { u"producerGroups"_s, groups },
                      { u"introductionStatus"_s,
                        lookupStatus(a.introduction, !a.description.isEmpty(), false) },
                      { u"creditsStatus"_s,
                        lookupStatus(a.creditsLookup, !a.producers.isEmpty(), true,
                                     a.tracks.size()) },
                      { u"folder"_s, a.folder.toDisplayString() },
                      { u"rawGenres"_s, a.rawGenres },
                      { u"facts"_s, facts },
                      { u"discs"_s, discs },
                      { u"code"_s, library.browseInfo().value(u"code"_s) } };
    }
    if (!metadataOnly)
        endResetModel();
    Q_EMIT albumChanged();
}
QVariantMap DetailModel::album() const
{
    return d->m_info;
}
DetailModel::~DetailModel() = default;

}
