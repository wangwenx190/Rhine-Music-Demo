#include "DetailModel.h"
#include <QSet>
#include <algorithm>
#include <cmath>

namespace yalahoo {
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
            if (!track.format.isEmpty() && !formatSet.contains(track.format)) {
                formats.append(track.format);
                formatSet.insert(track.format);
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
            return number(*low) + (*low == *high ? QString() : "–" + number(*high)) + " " + unit;
        };
        fact("RELEASE / 发行年份", a.year ? QString::number(*a.year) : "未提供");
        fact("ARTIST / 歌手", a.artist);
        fact("GENRE / 流派", genre);
        fact("VOLUMES / 内含 CD", QStringLiteral("%1 CD · %2 首").arg(discs).arg(a.tracks.size()));
        fact("FORMAT / 文件格式", formats.isEmpty() ? "未提供" : formats.join(" / "));
        fact("RESOLUTION / 位深与采样率",
             (lossy ? QStringLiteral("有损编码") : display(minBits, maxBits, 1, "bit")) + " / "
                     + display(minRate, maxRate, 1000, "kHz"));
        fact("BITRATE / 码率", display(minBitrate, maxBitrate, 1000, "kbps"));
        fact("DURATION / 总时长", time(duration));
        for (const auto &producer : a.producers) {
            producers.append(QVariantMap{ { "name", producer.name },
                                          { "role", producer.role },
                                          { "source", producer.source },
                                          { "url", producer.url },
                                          { "trackTitle", producer.trackTitle } });
        }
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
