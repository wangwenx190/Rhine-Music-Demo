#include "DetailModel.h"
#include <QSet>
#include <QTextLayout>
#include <algorithm>
#include <cmath>

namespace yalahoo {
namespace {
QString lookupStatus(const QJsonObject &lookup, bool retained, bool credits, int trackCount = 0)
{
    const QString status = lookup.value("status").toString("unqueried");
    const QString kept = retained ? QStringLiteral(" 已有资料仍然保留，可离线阅读。") : QString();
    QString result;
    if (status == "error")
        result = "查询失败：" + lookup.value("error").toString("资料来源暂时无法访问。");
    else if (status == "uncertain")
        result = "尚无法可靠确认对应专辑或曲目，未采用存疑资料。";
    else if (status == "not-found")
        result = "未找到可核实的资料。";
    else if (status == "partial" || status == "matched")
        result = credits ? QStringLiteral("已核对 %1 / %2 首曲目的参与人员。")
                                   .arg(lookup.value("matchedTracks").toInt())
                                   .arg(lookup.value("totalTracks").toInt(trackCount))
                         : QStringLiteral("介绍已保存在本机。");
    else if (status == "unqueried")
        result = retained ? QStringLiteral("资料已保存在本机。") : QStringLiteral("尚未查询资料。");
    else
        result = "资料状态：" + status + "。";
    if (status != "error" && !lookup.value("error").toString().isEmpty())
        result += " " + lookup.value("error").toString();
    if (!lookup.value("retryAt").toString().isEmpty())
        result += " 重试时间：" + lookup.value("retryAt").toString() + "。";
    return result + kept + " 在线查询暂未启用。";
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
    const qsizetype cost = key.size() * 2 + 64;
    if (cost <= m_lineHeights.maxCost())
        m_lineHeights.insert(key, new double(height), int(cost));
    return height;
}
QString DetailModel::time(double seconds)
{
    const qint64 value = std::isfinite(seconds) ? qint64(std::clamp(seconds, 0.0, 1e12)) : 0;
    return QStringLiteral("%1:%2").arg(value / 60).arg(value % 60, 2, 10, QLatin1Char('0'));
}
int DetailModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() || !m_album ? 0 : m_album->tracks.size();
}
QVariant DetailModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
        return { };
    const auto &track = m_album->tracks[index.row()];
    switch (role) {
    case TrackId:
        return track.id;
    case Number:
        return QStringLiteral("%1").arg(track.trackNumber.value_or(index.row() + 1), 2, 10,
                                        QLatin1Char('0'));
    case Title:
        return track.title;
    case Artist:
        return track.artist;
    case Format:
        return track.format;
    case Duration:
        return time(track.duration);
    case Disc:
        return track.discNumber.value_or(1);
    case DiscHeading:
        return m_info.value("discs").toInt() > 1
                && (index.row() == 0
                    || m_album->tracks[index.row() - 1].discNumber.value_or(1)
                            != track.discNumber.value_or(1));
    default:
        return { };
    }
}
QHash<int, QByteArray> DetailModel::roleNames() const
{
    return { { TrackId, "trackId" }, { Number, "number" },          { Title, "trackTitle" },
             { Artist, "artist" },   { Format, "format" },          { Duration, "duration" },
             { Disc, "disc" },       { DiscHeading, "discHeading" } };
}
void DetailModel::prepare(const LibrarySession &library)
{
    const auto snapshot = library.snapshot();
    const auto *album = library.navigationAlbum();
    if (snapshot == m_snapshot && album == m_album
        && m_info.value("code") == library.browseInfo().value("code"))
        return;
    beginResetModel();
    m_snapshot = snapshot;
    m_album = album;
    m_info.clear();
    if (album) {
        const auto &a = *album;
        QVariantList facts, producers;
        QHash<QString, QString> trackTitles;
        const auto fact = [&](const QString &label, const QString &value) {
            facts.append(QVariantMap{ { "label", label }, { "value", value } });
        };
        QString genre = QStringLiteral("未分类");
        for (const auto &g : snapshot->genres) {
            if (g.id == a.genreId) {
                genre = g.name;
                break;
            }
        }
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
                    track.codec.isEmpty() ? track.format : track.format + " / " + track.codec;
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
                return QStringLiteral("未提供");
            const auto number = [scale, &unit](int value) {
                double scaled = value / scale;
                if (unit == "kbps")
                    scaled = std::round(scaled);
                else if (unit == "kHz")
                    scaled = std::round(scaled * 10) / 10;
                return QString::number(scaled, 'g', 8);
            };
            return number(*low) + " " + unit
                    + (*low == *high ? QString() : "–" + number(*high) + " " + unit);
        };
        fact("RELEASE / 发行年份", a.year ? QString::number(*a.year) : "未提供");
        fact("ARTIST / 歌手", a.artist);
        fact("GENRE / 流派", genre);
        fact("VOLUMES / 内含 CD", QStringLiteral("%1 CD · %2 首").arg(discs).arg(a.tracks.size()));
        fact("FORMAT / 文件格式", formats.isEmpty() ? "未提供" : formats.join(" · "));
        fact("RESOLUTION / 位深与采样率",
             (lossy ? QStringLiteral("有损编码") : display(minBits, maxBits, 1, "bit")) + " / "
                     + display(minRate, maxRate, 1000, "kHz"));
        fact("BITRATE / 码率", display(minBitrate, maxBitrate, 1000, "kbps"));
        fact("DURATION / 总时长", time(duration));
        for (const auto &producer : a.producers) {
            producers.append(QVariantMap{
                    { "name", producer.name },
                    { "role", producer.role },
                    { "source", producer.source },
                    { "url", producer.url },
                    { "trackTitle", trackTitles.value(producer.trackId, producer.trackTitle) },
                    { "trackId", producer.trackId } });
        }
        // Group once per immutable album. Stable track IDs resolve through the index above.
        struct CreditGroup
        {
            QString name;
            QStringList roles, sources, tracks, unscoped;
            QSet<QString> roleKeys, sourceKeys, trackKeys, unscopedKeys;
        };
        QList<CreditGroup> grouped;
        QHash<QString, int> groupIndex;
        for (const auto &value : producers) {
            const auto producer = value.toMap();
            const auto key = producer["name"]
                                     .toString()
                                     .trimmed()
                                     .normalized(QString::NormalizationForm_KC)
                                     .toLower()
                    + QChar(0) + producer["source"].toString();
            int index = groupIndex.value(key, -1);
            if (index < 0) {
                index = grouped.size();
                groupIndex.insert(key, index);
                grouped.append(CreditGroup{
                        producer["name"].toString(), { }, { }, { }, { }, { }, { }, { }, { } });
            }
            auto &group = grouped[index];
            const auto appendUnique = [](QStringList &values, QSet<QString> &keys,
                                         const QString &text) {
                if (!text.isEmpty() && !keys.contains(text)) {
                    keys.insert(text);
                    values.append(text);
                }
            };
            appendUnique(group.roles, group.roleKeys, producer["role"].toString());
            const auto source = producer["source"].toString();
            const auto url = producer["url"].toString();
            appendUnique(group.sources, group.sourceKeys,
                         source + (url.isEmpty() ? QString() : " · " + url));
            const auto trackTitle = producer["trackTitle"].toString();
            if (!trackTitle.isEmpty()) {
                const auto trackKey = producer["trackId"].toString() + QChar(0) + trackTitle
                        + QChar(0) + producer["role"].toString() + QChar(0) + url;
                if (!group.trackKeys.contains(trackKey)) {
                    group.trackKeys.insert(trackKey);
                    group.tracks.append(trackTitle + " · " + producer["role"].toString()
                                        + "\n来源：" + source
                                        + (url.isEmpty() ? QString() : " · " + url));
                }
            } else
                appendUnique(group.unscoped, group.unscopedKeys, producer["role"].toString());
        }
        QVariantList groups;
        for (const auto &group : grouped)
            groups.append(QVariantMap{ { "name", group.name },
                                       { "roles", group.roles },
                                       { "sources", group.sources },
                                       { "tracks", group.tracks },
                                       { "unscoped", group.unscoped } });
        const auto title = albumTitleParts(a.title);
        m_info = { { "id", a.id },
                   { "title", a.title },
                   { "mainTitle", title.first },
                   { "translation", title.second },
                   { "artist", a.artist },
                   { "offline", a.offline },
                   { "description", a.description },
                   { "localNote", a.localNote },
                   { "descriptionSource", a.descriptionSource.toVariantMap() },
                   { "introduction", a.introduction.toVariantMap() },
                   { "creditsLookup", a.creditsLookup.toVariantMap() },
                   { "producers", producers },
                   { "producerGroups", groups },
                   { "introductionStatus",
                     lookupStatus(a.introduction, !a.description.isEmpty(), false) },
                   { "creditsStatus",
                     lookupStatus(a.creditsLookup, !a.producers.isEmpty(), true, a.tracks.size()) },
                   { "folder", a.folder.toDisplayString() },
                   { "rawGenres", a.rawGenres },
                   { "facts", facts },
                   { "discs", discs },
                   { "code", library.browseInfo().value("code") } };
    }
    endResetModel();
    emit albumChanged();
}
}
