#include "AtlasAllocator.h"
#include "RhiItem.h"
#include "SceneAdapter.h"
#include "GraphicsBackend.h"
#include "ImageComparison.h"
#include "VerificationWindow.h"
#include <QDir>
#include <QEventLoop>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QPointer>
#include <QQuickWindow>
#include <QSaveFile>
#include <QTimer>
#include <optional>
#include <stdexcept>
using namespace yalahoo;
using namespace yalahoo::rhi;
namespace {
void require(bool condition, const char *reason)
{
    if (!condition)
        throw std::runtime_error(reason);
}
CoverSnapshot pixels(const QString &id, QColor color)
{
    auto p = std::make_shared<CoverPixels>();
    p->identity = id;
    p->generation = 1;
    p->image = QImage(256, 256, QImage::Format_RGBA8888_Premultiplied);
    p->image.fill(color);
    return p;
}
}
int main(int argc, char **argv)
{
    yalahoo::verification::prepareApplication();
    if (!selectGraphicsBackend())
        return 2;
    QGuiApplication app(argc, argv);
    if (argc != 2)
        return 2;
    try {
        const QString output = QString::fromLocal8Bit(argv[1]);
        require(QDir().mkpath(output), "Cannot create cover audit output");
        QQuickWindow window;
        verification::prepareWindow(window, true);
        window.resize(960, 720);
        window.setPersistentSceneGraph(false);
        window.setPersistentGraphics(false);
        auto *item = new RhiItem(window.contentItem());
        item->setSize({ 960, 720 });
        struct Wait
        {
            quint64 request = 0;
            std::optional<Capture> capture;
            QPointer<QEventLoop> loop;
        };
        auto state = std::make_shared<Wait>();
        QPointer<QQuickWindow> guard(&window);
        item->captured = [state, guard](Capture capture) {
            if (guard)
                QMetaObject::invokeMethod(
                        guard,
                        [state, capture = std::move(capture)]() mutable {
                            if (capture.request == state->request && state->loop) {
                                state->capture = std::move(capture);
                                state->loop->quit();
                            }
                        },
                        Qt::QueuedConnection);
        };
        auto capture = [&](Input input) {
            QEventLoop loop;
            QTimer timeout;
            timeout.setSingleShot(true);
            QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
            state->loop = &loop;
            state->capture.reset();
            input.request = ++state->request;
            item->submit(std::move(input), { 960, 720 });
            window.show();
            timeout.start(30000);
            loop.exec();
            state->loop = nullptr;
            require(state->capture.has_value(), "Cover GPU timeout");
            require(verification::windowPolicySatisfied(window), "Cover window policy violated");
            return *state->capture;
        };
        SceneAdapter adapter;
        adapter.layout()->setViewport(960, 720);
        Input base;
        base.scene = adapter.frame();
        base.entities = { base.scene.caseTransform };
        base.entityIds = { 1 };
        base.shadows = false;
        base.lightingEnabled = false;
        base.background = { .1f, .1f, .1f };
        base.captureSamples = 4;
        base.cover = QImage(256, 256, QImage::Format_RGBA8888_Premultiplied);
        base.cover.fill(Qt::transparent);
        base.immutableCover = true;
        const auto empty = capture(base);
        auto detailed = base;
        detailed.cover = CoverCache::paint(pixels("selected", Qt::red)->image, 1024, { });
        detailed.coverPainted = true;
        const auto high = capture(detailed);
        require(high.metadata["selectedUploadBytes"].toInteger() == 1024 * 1024 * 4,
                "Selected cover upload payload differs from 1024 RGBA");
        require(high.metadata["coverSnapshotCopies"].toInteger() == 0
                        && high.metadata["resourceGeneration"]
                                == empty.metadata["resourceGeneration"],
                "Selected resolution copied immutable pixels or rebuilt targets");
        const auto selectedNative = high.metadata["nativeSelectedTransfers"].toObject();
        if (selectedNative["available"].toBool())
            require(selectedNative["destinationBytes"].toInteger() == 1024 * 1024 * 4,
                    "Native selected texture transfer differs from pixel payload");
        const auto highStable = capture(detailed);
        require(highStable.metadata["selectedUploadBytes"].toInteger() == 0,
                "Stable selected cover uploaded again");
        QJsonArray shapes;
        for (const QSize size : { QSize(256, 256), QSize(128, 256), QSize(256, 128) }) {
            auto raw = base;
            raw.cover = QImage(size, QImage::Format_RGBA8888_Premultiplied);
            raw.cover.fill(QColor(220, 40, 80));
            const auto before = capture(raw);
            raw.cover = CoverCache::paint(raw.cover, 256, { });
            raw.coverPainted = true;
            const auto painted = capture(raw);
            require(painted.metadata["resourceGeneration"] == empty.metadata["resourceGeneration"],
                    "Cover resolution rebuilt unrelated render targets/pipelines");
            QPolygonF region;
            for (const auto &corner :
                 { QVector3D(-1.36f, .32f, .027f), QVector3D(1.7f, .32f, .027f),
                   QVector3D(1.7f, 3.38f, .027f), QVector3D(-1.36f, 3.38f, .027f) }) {
                const auto clip = raw.scene.projection * raw.scene.view * raw.scene.caseTransform
                        * QVector4D(corner, 1);
                require(clip.w() > 0, "Cover ROI lies behind camera");
                region.append({ (clip.x() / clip.w() + 1) * 480, (1 - clip.y() / clip.w()) * 360 });
            }
            const auto metric = compareImages(painted.color, before.color, region);
            require(metric["ssim"].toDouble() >= .98,
                    "Painted cover differs from source contain/inset");
            shapes.append(QJsonObject{
                    { "width", size.width() }, { "height", size.height() }, { "metric", metric } });
            painted.color.save(output
                               + QString("/shape-%1-%2.png").arg(size.width()).arg(size.height()));
        }
        AtlasAllocator atlas(2048, 9);
        QList<AtlasLease> leases;
        for (int i = 0; i < 9; ++i) {
            auto lease = atlas.acquire(QString::number(i));
            require(atlas.publish(lease, pixels(lease.identity, Qt::blue)),
                    "Cannot publish atlas pixels");
            leases.append(lease);
        }
        Input input = base;
        input.atlasSize = atlas.pageSize();
        input.atlasGeneration = atlas.generation();
        input.coverTiles = { atlas.tile(leases[8]) };
        input.coverLeases = { leases[8].lease };
        // Replay is bounded by one page; render still enforces its own per-frame budget.
        input.atlasUploads = atlas.uploads(0, true);
        const auto bounded = capture(input);
        require(bounded.metadata["atlasUploadBytes"].toInteger() == AtlasUploadQueue::steadyBudget,
                "Actual first frame exceeded/missed eight-tile budget");
        require(bounded.metadata["atlasPending"].toInt() == 1, "Ninth tile did not defer");
        require(bounded.color == empty.color, "Unuploaded slot exposed old/uninitialized pixels");
        input.atlasUploads.clear();
        const auto drained = capture(input);
        require(drained.metadata["atlasPending"].toInt() == 0, "Deferred upload did not drain");
        require(drained.color != empty.color, "Uploaded slot remained hidden");
        const auto stable = capture(input);
        require(stable.metadata["atlasUploadBytes"].toInteger() == 0
                        && stable.atlas == drained.atlas,
                "Stable frame copied/uploaded pixels");
        const auto previous = leases[8];
        atlas.release(previous);
        const auto next = atlas.acquire("replacement");
        input.coverTiles = { atlas.tile(next) };
        input.coverLeases = { next.lease };
        const auto hidden = capture(input);
        require(hidden.color == empty.color, "Reused slot showed previous album art");
        // Late uploads may have larger serials; lease must still reject before copying.
        input.atlasUploads = { { atlas.generation(), previous.lease, 999, previous.slot,
                                 pixels(previous.identity, Qt::green)->image } };
        const auto late = capture(input);
        require(late.color == empty.color && late.metadata["atlasUploadBytes"].toInteger() == 0,
                "Late upload replaced a new lease");
        require(atlas.publish(next, pixels("replacement", Qt::red)), "Replacement publish failed");
        input.atlasUploads = atlas.uploads(0, false);
        const auto replaced = capture(input);
        require(replaced.metadata["atlasUploadBytes"].toInteger() == AtlasUploadQueue::tileBytes,
                "Replacement did not upload exactly one tile");
        require(replaced.atlas.pixelColor(atlas.tile(next).center()) == QColor(Qt::red),
                "GPU subregion differs from immutable worker payload");
        const auto native = replaced.metadata["nativeAtlasTransfers"].toObject();
        if (native["available"].toBool())
            require(native["destinationBytes"].toInteger() == AtlasUploadQueue::tileBytes,
                    "Native transfer bytes differ from submitted region");
        const auto retained = atlas.replay(0);
        // Real Qt scene graph destruction and restoration, with immutable page replay.
        window.hide();
        window.releaseResources();
        input.atlasUploads = retained;
        input.initializingAtlas = true;
        const auto restored = capture(input);
        for (const auto &lease : leases) {
            const auto tile = atlas.tile(lease);
            require(restored.atlas.copy(tile) == replaced.atlas.copy(tile),
                    "Scene graph reconstruction lost retained covers");
        }
        require(restored.color == replaced.color, "Reconstruction changed selected art");
        const auto oldGeneration = input.atlasGeneration;
        atlas.reset();
        const auto rebuiltLease = atlas.acquire("rebuilt");
        require(atlas.publish(rebuiltLease, pixels("rebuilt", Qt::yellow)),
                "Rebuild publish failed");
        input.atlasGeneration = atlas.generation();
        input.coverTiles = { atlas.tile(rebuiltLease) };
        input.coverLeases = { rebuiltLease.lease };
        input.atlasUploads = atlas.replay(0);
        input.atlasUploads.append({ oldGeneration, 999, 1000, 0, pixels("old", Qt::green)->image });
        const auto rebuilt = capture(input);
        require(rebuilt.atlas.pixelColor(128, 128) == QColor(Qt::yellow)
                        && rebuilt.metadata["atlasRejected"].toInteger() > 0,
                "Rebuilt atlas accepted a late generation");
        QJsonObject report{ { "passed", true },
                            { "selected", high.metadata },
                            { "selectedStable", highStable.metadata },
                            { "windowPolicy", verification::windowPolicy(window) },
                            { "shapes", shapes },
                            { "bounded", bounded.metadata },
                            { "stable", stable.metadata },
                            { "replacement", replaced.metadata },
                            { "restored", restored.metadata },
                            { "rebuilt", rebuilt.metadata },
                            { "measuredVramBytes", QJsonValue() } };
        QSaveFile file(output + "/runtime.json");
        require(file.open(QIODevice::WriteOnly), "Cannot save cover GPU report");
        file.write(QJsonDocument(report).toJson());
        require(file.commit(), "Cannot commit cover GPU report");
        item->captured = { };
        window.hide();
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "R11: %s\n", error.what());
        return 1;
    }
}
