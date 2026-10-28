#include "MetadataProbeInput.h"
#include "OnlineMetadataProvider.h"
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QFile>
#include <cmath>
#include <utility>
#include <QNetworkProxyFactory>
#include <QTextStream>
#include <QTimer>

namespace {
// Bounds for asynchronous completion and event delivery; fixture values remain independent.
constexpr int RenderDeadlineMilliseconds = 120000;

// Explicit public query inputs are bounded before JSON parsing and provider dispatch.
constexpr auto MaximumReleaseYear = 9999;
constexpr auto BytesPerKiB = 1024;
constexpr auto DescriptionPreviewCharacters = 300;
}

using namespace Qt::StringLiterals;
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(u"metadata-network-probe"_s);
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOptions({ { u"kind"_s, u"introduction, credits, or enrichment"_s, u"kind"_s },
                        { u"title"_s, u"Public test album title"_s, u"title"_s },
                        { u"artist"_s, u"Public test artist"_s, u"artist"_s },
                        { u"year"_s, u"Optional publication year"_s, u"year"_s },
                        { u"release"_s, u"Optional MusicBrainz release ID"_s, u"id"_s },
                        { u"proxy"_s, u"Optional manual HTTP proxy URL"_s, u"url"_s },
                        { u"tracks"_s, u"Public test track JSON array"_s, u"file"_s } });
    parser.process(app);
    const auto kind = parser.value(u"kind"_s);
    if (!QStringList{ u"introduction"_s, u"credits"_s, u"enrichment"_s }.contains(kind)
        || parser.value(u"title"_s).isEmpty() || parser.value(u"artist"_s).isEmpty())
        return 7;
    yalahoo::AlbumQuery query;
    query.albumId = u"album-public-probe"_s;
    query.album.id = query.albumId;
    query.album.title = parser.value(u"title"_s);
    query.album.artist = parser.value(u"artist"_s);
    if (parser.isSet(u"year"_s)) {
        bool valid = false;
        const int year = parser.value(u"year"_s).toInt(&valid);
        if (!valid || year < 1 || year > MaximumReleaseYear)
            return 7;
        query.album.year = year;
    }
    if (parser.isSet(u"tracks"_s)) {
        QFile file(parser.value(u"tracks"_s));
        if (!file.open(QIODevice::ReadOnly) || file.size() > BytesPerKiB * BytesPerKiB)
            return 7;
        QJsonParseError error;
        const auto document =
                QJsonDocument::fromJson(file.read(BytesPerKiB * BytesPerKiB + 1), &error);
        if (error.error != QJsonParseError::NoError)
            return 7;
        try {
            yalahoo::probe::validateTracks(document);
        } catch (const std::runtime_error &) {
            return 7;
        }
        for (const auto &value : document.array()) {
            const auto object = value.toObject();
            yalahoo::Track track;
            track.id = u"track-public-%1"_s.arg(query.album.tracks.size());
            track.albumId = query.albumId;
            track.title = object.value(u"title"_s).toString();
            track.duration = object.value(u"duration"_s).toDouble();
            track.discNumber = object.value(u"disc"_s).toInt(1);
            track.trackNumber = object.value(u"number"_s).toInt();
            if (track.title.isEmpty() || !std::isfinite(track.duration) || track.duration < 0
                || *track.discNumber < 1 || *track.trackNumber < 1)
                return 7;
            query.album.tracks.append(std::move(track));
        }
    }
    if (parser.isSet(u"release"_s))
        query.album.online.insert(u"releaseId"_s, parser.value(u"release"_s));
    query.configuration = { { u"proxyEnabled"_s, true },
                            { u"proxyMode"_s,
                              parser.isSet(u"proxy"_s) ? u"manual"_s : u"system"_s },
                            { u"proxyUrl"_s, parser.value(u"proxy"_s) } };
    query.enabled = true;
    query.force = true;
    QNetworkProxyFactory::setUseSystemConfiguration(true);
    yalahoo::OnlineMetadataProvider provider;
    provider.configure({ { u"networkEnabled"_s, true } });
    QObject::connect(
            &provider, &yalahoo::IMetadataProvider::resultReady, &app,
            [&](const yalahoo::ProviderResult &result) {
                const QJsonObject report{ { u"kind"_s, kind },
                                          { u"status"_s, yalahoo::statusName(result.status) },
                                          { u"source"_s, result.source },
                                          { u"checkedAt"_s, result.checkedAt },
                                          { u"retryAt"_s, result.retryAt },
                                          { u"error"_s, result.error },
                                          { u"description"_s,
                                            result.payload.value(u"description"_s)
                                                    .toString()
                                                    .left(DescriptionPreviewCharacters) },
                                          { u"credits"_s,
                                            result.payload.value(u"producers"_s).toArray().size() },
                                          { u"online"_s, result.payload.value(u"online"_s) },
                                          { u"genres"_s, result.payload.value(u"genres"_s) } };
                QTextStream(stdout) << QJsonDocument(report).toJson();
                app.exit(result.status == yalahoo::ProviderStatus::Matched
                                         || result.status == yalahoo::ProviderStatus::Partial
                                 ? 0
                                 : 2);
            });
    const yalahoo::RequestToken token{ 1, 1, query.albumId };
    const bool accepted = kind == "introduction"_L1 ? provider.requestIntroduction(query, token)
            : kind == "credits"_L1                  ? provider.requestCredits(query, token)
                                                    : provider.requestEnrichment(query, token);
    if (!accepted)
        return 3;
    QTimer::singleShot(RenderDeadlineMilliseconds, &app, [&] {
        app.exit(4);
    });
    return app.exec();
}
