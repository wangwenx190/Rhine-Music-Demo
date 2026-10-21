#include "ReviewSupport.h"
#include "ReviewApplication.h"
#include "CoverCache.h"
#include <QScopeGuard>
#include "AtlasAllocator.h"
#include "RhiItem.h"
#include "SceneAdapter.h"
#include "GraphicsBackend.h"
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

using namespace Qt::StringLiterals;

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
    return yalahoo::review::runApplication(argc, argv, [&](QGuiApplication &) {
        const auto coverShutdown = qScopeGuard([] {
            yalahoo::CoverCache::finishDecoders();
        });
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
            struct Wait final
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
                timeout.start(yalahoo::review::WorkerDrainDeadlineMilliseconds);
                loop.exec();
                state->loop = nullptr;
                require(state->capture.has_value(), "Cover GPU timeout");
                require(verification::windowPolicySatisfied(window),
                        "Cover window policy violated");
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
            detailed.cover = CoverCache::paint(pixels(u"selected"_s, Qt::red)->image, 1024, { });
            detailed.coverPainted = true;
            const auto high = capture(detailed);
            require(high.metadata[u"selectedUploadBytes"_s].toInteger() == 1024 * 1024 * 4,
                    "Selected cover upload payload differs from 1024 RGBA");
            require(high.metadata[u"coverSnapshotCopies"_s].toInteger() == 0
                            && high.metadata[u"resourceGeneration"_s]
                                    == empty.metadata[u"resourceGeneration"_s],
                    "Selected resolution copied immutable pixels or rebuilt targets");
            const auto selectedNative = high.metadata[u"nativeSelectedTransfers"_s].toObject();
            if (selectedNative[u"available"_s].toBool())
                require(selectedNative[u"destinationBytes"_s].toInteger() == 1024 * 1024 * 4,
                        "Native selected texture transfer differs from pixel payload");
            const auto highStable = capture(detailed);
            require(highStable.metadata[u"selectedUploadBytes"_s].toInteger() == 0,
                    "Stable selected cover uploaded again");
            QJsonArray shapes;
            for (const QSize size : { QSize(256, 256), QSize(128, 256), QSize(256, 128) }) {
                auto raw = base;
                raw.cover = QImage(size, QImage::Format_RGBA8888_Premultiplied);
                raw.cover.fill(QColor(220, 40, 80));
                raw.cover = CoverCache::paint(raw.cover, 256, { });
                raw.coverPainted = true;
                const auto painted = capture(raw);
                require(painted.metadata[u"resourceGeneration"_s]
                                == empty.metadata[u"resourceGeneration"_s],
                        "Cover resolution rebuilt unrelated render targets/pipelines");
                require(painted.metadata[u"selectedCoverWidth"_s].toInt() == 256
                                && painted.metadata[u"selectedCoverHeight"_s].toInt() == 256
                                && painted.metadata[u"selectedUploadBytes"_s].toInteger()
                                        == 256 * 256 * 4,
                        "Painted cover did not reach the actual GPU upload at the declared size");
                // Contain/inset is checked analytically by CoverCacheTest and source UV formulas.
                // Own output is retained for quality inspection, never scored against another image.
                shapes.append(QJsonObject{ { u"width"_s, size.width() },
                                           { u"height"_s, size.height() },
                                           { u"uploaded"_s, painted.metadata } });
                painted.color.save(output
                                   + u"/shape-%1-%2.png"_s.arg(size.width()).arg(size.height()));
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
            require(bounded.metadata[u"atlasUploadBytes"_s].toInteger()
                            == AtlasUploadQueue::steadyBudget,
                    "Actual first frame exceeded/missed eight-tile budget");
            require(bounded.metadata[u"atlasPending"_s].toInt() == 1, "Ninth tile did not defer");
            require(bounded.color == empty.color,
                    "Unuploaded slot exposed old/uninitialized pixels");
            input.atlasUploads.clear();
            const auto drained = capture(input);
            require(drained.metadata[u"atlasPending"_s].toInt() == 0,
                    "Deferred upload did not drain");
            require(drained.color != empty.color, "Uploaded slot remained hidden");
            const auto stable = capture(input);
            require(stable.metadata[u"atlasUploadBytes"_s].toInteger() == 0
                            && stable.atlas == drained.atlas,
                    "Stable frame copied/uploaded pixels");
            const auto previous = leases[8];
            atlas.release(previous);
            const auto next = atlas.acquire(u"replacement"_s);
            input.coverTiles = { atlas.tile(next) };
            input.coverLeases = { next.lease };
            const auto hidden = capture(input);
            require(hidden.color == empty.color, "Reused slot showed previous album art");
            // Late uploads may have larger serials; lease must still reject before copying.
            input.atlasUploads = { { atlas.generation(), previous.lease, 999, previous.slot,
                                     pixels(previous.identity, Qt::green)->image } };
            const auto late = capture(input);
            require(late.color == empty.color
                            && late.metadata[u"atlasUploadBytes"_s].toInteger() == 0,
                    "Late upload replaced a new lease");
            require(atlas.publish(next, pixels(u"replacement"_s, Qt::red)),
                    "Replacement publish failed");
            input.atlasUploads = atlas.uploads(0, false);
            const auto replaced = capture(input);
            require(replaced.metadata[u"atlasUploadBytes"_s].toInteger()
                            == AtlasUploadQueue::tileBytes,
                    "Replacement did not upload exactly one tile");
            require(replaced.atlas.pixelColor(atlas.tile(next).center()) == QColor(Qt::red),
                    "GPU subregion differs from immutable worker payload");
            const auto native = replaced.metadata[u"nativeAtlasTransfers"_s].toObject();
            if (native[u"available"_s].toBool())
                require(native[u"destinationBytes"_s].toInteger() == AtlasUploadQueue::tileBytes,
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
            const auto rebuiltLease = atlas.acquire(u"rebuilt"_s);
            require(atlas.publish(rebuiltLease, pixels(u"rebuilt"_s, Qt::yellow)),
                    "Rebuild publish failed");
            input.atlasGeneration = atlas.generation();
            input.coverTiles = { atlas.tile(rebuiltLease) };
            input.coverLeases = { rebuiltLease.lease };
            input.atlasUploads = atlas.replay(0);
            input.atlasUploads.append(
                    { oldGeneration, 999, 1000, 0, pixels(u"old"_s, Qt::green)->image });
            const auto rebuilt = capture(input);
            require(rebuilt.atlas.pixelColor(128, 128) == QColor(Qt::yellow)
                            && rebuilt.metadata[u"atlasRejected"_s].toInteger() > 0,
                    "Rebuilt atlas accepted a late generation");
            QJsonObject report{ { u"passed"_s, true },
                                { u"selected"_s, high.metadata },
                                { u"selectedStable"_s, highStable.metadata },
                                { u"windowPolicy"_s, verification::windowPolicy(window) },
                                { u"shapes"_s, shapes },
                                { u"bounded"_s, bounded.metadata },
                                { u"stable"_s, stable.metadata },
                                { u"replacement"_s, replaced.metadata },
                                { u"restored"_s, restored.metadata },
                                { u"rebuilt"_s, rebuilt.metadata },
                                { u"measuredVramBytes"_s, QJsonValue() } };
            QSaveFile file(output + "/runtime.json"_L1);
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
    });
}
