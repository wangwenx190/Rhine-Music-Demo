#include "LibrarySession.h"
#include <QCoreApplication>
#include "DetailModel.h"
#include <QSet>
#include <QTextLayout>
#include <algorithm>
#include <cmath>

namespace {
// Formatting uses decimal units; cached line heights account for key storage and entry overhead.
constexpr auto LineCacheEntryOverheadBytes = 64;
constexpr auto MaximumDisplayDurationSeconds = 1e12;
constexpr auto SecondsPerMinute = 60;
constexpr auto DecimalBase = 10;
constexpr auto FormatSignificantDigits = 8;
constexpr auto MetricPrefixScale = 1000;
}

using namespace Qt::StringLiterals;

namespace yalahoo {
namespace {
QString lookupStatus(const QJsonObject &lookup, bool retained, bool credits, int trackCount = 0)
{
    const QString status = lookup.value(u"status"_s).toString(u"unqueried"_s);
    const QString kept = retained
            ? QCoreApplication::translate("Yalahoo",
                                          " Existing metadata is retained for offline reading.")
            : QString();
    QString result;
    if (status == "error"_L1)
        result = QCoreApplication::translate("Yalahoo", "Query failed: ")
                + lookup.value(u"error"_s)
                          .toString(QCoreApplication::translate(
                                  "Yalahoo", "The metadata source is temporarily unavailable."));
    else if (status == "uncertain"_L1)
        result = QCoreApplication::translate(
                "Yalahoo",
                "The album or track could not be identified reliably. Uncertain metadata was not applied.");
    else if (status == "not-found"_L1)
        result = QCoreApplication::translate("Yalahoo", "No verifiable metadata was found.");
    else if (status == "partial"_L1 || status == "matched"_L1)
        result = credits
                ? QCoreApplication::translate("Yalahoo", "Track credits verified: %1 / %2.")
                          .arg(lookup.value(u"matchedTracks"_s).toInt())
                          .arg(lookup.value(u"totalTracks"_s).toInt(trackCount))
                : QCoreApplication::translate("Yalahoo", "The introduction is saved locally.");
    else if (status == "unqueried"_L1)
        result = retained
                ? QCoreApplication::translate("Yalahoo", "Metadata is saved locally.")
                : QCoreApplication::translate("Yalahoo", "Metadata has not been queried.");
    else
        result = QCoreApplication::translate("Yalahoo", "Metadata status: %1.").arg(status);
    if (status != "error"_L1 && !lookup.value(u"error"_s).toString().isEmpty())
        result += " "_L1 + lookup.value(u"error"_s).toString();
    if (!lookup.value(u"retryAt"_s).toString().isEmpty())
        result += QCoreApplication::translate("Yalahoo", " Retry at: %1.")
                          .arg(lookup.value(u"retryAt"_s).toString());
    return result + kept
            + QCoreApplication::translate("Yalahoo", " Online queries are not enabled yet.");
}
}
double DetailModel::normalLineHeight(const QString &text, const QFont &font)
{
    const QString key = font.toString() + QChar(0) + text;
    if (const auto *cached = m_lineHeights.object(key))
        return *cached;
    QTextLayout layout(text, font);
    layout.beginLayout();
    const auto line = layout.createLine();
    layout.endLayout();
    // CSS normal line boxes round the resolved ascent/descent independently.
    const double height =
            line.isValid() ? std::round(line.ascent()) + std::round(line.descent()) : 0;
    const qsizetype cost = key.size() * 2 + LineCacheEntryOverheadBytes;
    if (cost <= m_lineHeights.maxCost())
        m_lineHeights.insert(key, new double(height), int(cost));
    return height;
}
QString DetailModel::time(double seconds)
{
    const qint64 value = std::isfinite(seconds)
            ? qint64(std::clamp(seconds, 0.0, MaximumDisplayDurationSeconds))
            : 0;
    return u"%1:%2"_s.arg(value / SecondsPerMinute)
            .arg(value % SecondsPerMinute, 2, DecimalBase, '0'_L1);
}
int DetailModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() || !m_album ? 0 : m_album->tracks.size();
}
QVariant DetailModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.model() != this || index.column() != 0 || index.row() < 0
        || index.row() >= rowCount())
        return { };
    const auto &track = m_album->tracks[index.row()];
    switch (role) {
    case TrackId:
        return track.id;
    case Number:
        return u"%1"_s.arg(track.trackNumber.value_or(index.row() + 1), 2, DecimalBase, '0'_L1);
    case Title:
        return track.title;
    case Artist:
        return artistDisplayName(track.artist);
    case Format:
        return track.format;
    case Duration:
        return time(track.duration);
    case Disc:
        return track.discNumber.value_or(1);
    case DiscHeading:
        return m_info.value(u"discs"_s).toInt() > 1
                && (index.row() == 0
                    || m_album->tracks[index.row() - 1].discNumber.value_or(1)
                            != track.discNumber.value_or(1));
    default:
        return { };
    }
}
QHash<int, QByteArray> DetailModel::roleNames() const
{
    return { { TrackId, "trackId"_ba },  { Number, "number"_ba },
             { Title, "trackTitle"_ba }, { Artist, "artist"_ba },
             { Format, "format"_ba },    { Duration, "duration"_ba },
             { Disc, "disc"_ba },        { DiscHeading, "discHeading"_ba } };
}
void DetailModel::prepare(const LibrarySession &library, bool metadataOnly)
{
    const auto snapshot = library.snapshot();
    const auto *album = library.navigationAlbum();
    if (!metadataOnly && snapshot == m_snapshot && album && album->id == m_resolved.id
        && m_info.value(u"code"_s) == library.browseInfo().value(u"code"_s))
        return;
    if (!metadataOnly)
        beginResetModel();
    m_snapshot = snapshot;
    m_resolved = album ? library.resolvedAlbum(album->id) : Album();
    m_album = album ? &m_resolved : nullptr;
    album = m_album;
    m_info.clear();
    if (album) {
        const auto &a = *album;
        QVariantList facts, producers;
        QHash<QString, QString> trackTitles;
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
            if (!format.isEmpty() && !formatSet.contains(format)) {
                formats.append(format);
                formatSet.insert(format);
            }
            discs = std::max(discs, track.discNumber.value_or(1));
            duration += std::max(0.0, track.duration);
            lossy = lossy && track.lossless == false;
            range(track.bitsPerSample, minBits, maxBits);
            range(track.sampleRate, minRate, maxRate);
            range(track.bitrate, minBitrate, maxBitrate);
        }
        const auto display = [](std::optional<int> low, std::optional<int> high, double scale,
                                const QString &unit) {
            if (!low)
                return QCoreApplication::translate("Yalahoo", "Not provided");
            const auto number = [scale, &unit](int value) {
                double scaled = value / scale;
                if (unit == "kbps"_L1)
                    scaled = std::round(scaled);
                else if (unit == "kHz"_L1)
                    scaled = std::round(scaled * DecimalBase) / DecimalBase;
                return QString::number(scaled, 'g', FormatSignificantDigits);
            };
            return number(*low) + " "_L1 + unit
                    + (*low == *high ? QString() : u"\u2013"_s + number(*high) + " "_L1 + unit);
        };
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
                    : display(minBits, maxBits, 1, u"bit"_s))
                     + " / "_L1 + display(minRate, maxRate, MetricPrefixScale, u"kHz"_s));
        fact(QCoreApplication::translate("Yalahoo", "BITRATE"),
             display(minBitrate, maxBitrate, MetricPrefixScale, u"kbps"_s));
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
        for (const auto &value : producers) {
            const auto producer = value.toMap();
            const auto key = producer[u"name"_s]
                                     .toString()
                                     .trimmed()
                                     .normalized(QString::NormalizationForm_KC)
                                     .toLower()
                    + QChar(0) + producer[u"source"_s].toString();
            int index = groupIndex.value(key, -1);
            if (index < 0) {
                index = grouped.size();
                groupIndex.insert(key, index);
                grouped.append(CreditGroup{
                        producer[u"name"_s].toString(), { }, { }, { }, { }, { }, { }, { }, { } });
            }
            auto &group = grouped[index];
            const auto appendUnique = [](QStringList &values, QSet<QString> &keys,
                                         const QString &text) {
                if (!text.isEmpty() && !keys.contains(text)) {
                    keys.insert(text);
                    values.append(text);
                }
            };
            appendUnique(group.roles, group.roleKeys, producer[u"role"_s].toString());
            const auto source = producer[u"source"_s].toString();
            const auto url = producer[u"url"_s].toString();
            appendUnique(group.sources, group.sourceKeys,
                         source + (url.isEmpty() ? QString() : u" \u00b7 "_s + url));
            const auto trackTitle = producer[u"trackTitle"_s].toString();
            if (!trackTitle.isEmpty()) {
                const auto trackKey = producer[u"trackId"_s].toString() + QChar(0) + trackTitle
                        + QChar(0) + producer[u"role"_s].toString() + QChar(0) + url;
                if (!group.trackKeys.contains(trackKey)) {
                    group.trackKeys.insert(trackKey);
                    group.tracks.append(trackTitle + u" \u00b7 "_s + producer[u"role"_s].toString()
                                        + QCoreApplication::translate("Yalahoo", "\nSource: ")
                                        + source
                                        + (url.isEmpty() ? QString() : u" \u00b7 "_s + url));
                }
            } else
                appendUnique(group.unscoped, group.unscopedKeys, producer[u"role"_s].toString());
        }
        QVariantList groups;
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
        m_info = { { u"id"_s, a.id },
                   { u"title"_s, a.title },
                   { u"mainTitle"_s, title.first },
                   { u"translation"_s, title.second },
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
                     lookupStatus(a.creditsLookup, !a.producers.isEmpty(), true, a.tracks.size()) },
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
}
