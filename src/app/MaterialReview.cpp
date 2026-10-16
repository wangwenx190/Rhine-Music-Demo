#include "MaterialReview.h"
#include "OffscreenSurface.h"
#include "ReviewGlCapabilities.h"
#include "RenderProbe.h"
#include "VerificationWindow.h"
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuick3DObject>
#include "SceneAdapter.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSaveFile>
#include <QSGRendererInterface>
#include <QTimer>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
namespace yalahoo {
namespace {
QVector3D project(QQuickItem *view, QVector3D world)
{
    QVector3D result;
    QMetaObject::invokeMethod(view, "qtProjection", Q_RETURN_ARG(QVector3D, result),
                              Q_ARG(QVector3D, world));
    return result;
}

}
int reviewMaterials(QGuiApplication &app, SceneAdapter &scene, const QString &fixture,
                    const QString &report)
{
    QFile input(fixture);
    if (!input.open(QIODevice::ReadOnly))
        return 12;
    const auto bytes = input.readAll();
    const auto oracle = QJsonDocument::fromJson(bytes).object();
    const bool backendReview = oracle["renderBackend"].toBool();
    const bool postprocessReview = oracle["postprocess"].toBool();
    const bool occlusionReview = oracle["occlusion"].toBool(),
               lightingReview = oracle["lighting"].toBool() || occlusionReview || postprocessReview;
    auto cases = oracle["cases"].toArray();
    for (const auto &value : cases) {
        const auto state = value.toObject();
        if (state["position"].toArray().size() != 3)
            return 12;
        if (!backendReview && !occlusionReview && !postprocessReview
            && (state["parameters"].toArray().size() != 3
                || state["projections"].toArray().size() != 5))
            return 12;
    }
    if (cases.isEmpty() || (!lightingReview && cases.size() != 90))
        return 12;
    if (!lightingReview)
        for (int coverIndex : { 1, 6, 11 })
            for (double light : { 0.0, 0.25, 2.0 }) {
                auto c = cases[coverIndex].toObject();
                c.insert("paperLight", light);
                c.insert("paperProbe", true);
                cases.append(c);
            }
    if (qEnvironmentVariableIntValue("YALAHOO_VERIFY_VISIBLE") == 1)
        OffscreenSurface::Enabled = false;
    QQuickRenderControl control;
    std::atomic<int> invalidations = 0;
    std::atomic<bool> multisampleBufferAdaptation = false;
    QMutex targetMutex;
    QJsonObject latestQuick3DTarget;
    std::unique_ptr<QQuickWindow> window;
    std::unique_ptr<OffscreenSurface> surface;
    quint64 windowEpoch = 0;
    const bool quitOnClose = app.quitOnLastWindowClosed();
    app.setQuitOnLastWindowClosed(false);
    const auto makeWindow = [&] {
        ++windowEpoch;
        window = std::make_unique<QQuickWindow>(OffscreenSurface::Enabled ? &control : nullptr);
        verification::prepareWindow(*window, true);
        window->resize(960, 720);
        window->setPersistentSceneGraph(false);
        window->setPersistentGraphics(false);
        surface = std::make_unique<OffscreenSurface>(*window, control);
        QObject::connect(
                window.get(), &QQuickWindow::sceneGraphInvalidated, &app,
                [&] {
                    ++invalidations;
                },
                Qt::DirectConnection);
        QObject::connect(
                window.get(), &QQuickWindow::afterRendering, window.get(),
                [&, targetWindow = window.get()] {
                    const auto target = review::quick3DTarget(*targetWindow);
                    const QMutexLocker lock(&targetMutex);
                    latestQuick3DTarget = target;
                },
                Qt::DirectConnection);
        QObject::connect(
                window.get(), &QQuickWindow::sceneGraphInitialized, window.get(),
                [&, targetSurface = surface.get()] {
                    multisampleBufferAdaptation =
                            review::useReviewGlMultisampleBuffers(targetSurface->rhi());
                },
                Qt::DirectConnection);
    };
    makeWindow();
    QStringList warnings;
    QQmlEngine engine;
    QObject::connect(&engine, &QQmlEngine::warnings, &app, [&](const QList<QQmlError> &errors) {
        for (const auto &error : errors)
            warnings.append(error.toString());
    });
    QQmlComponent component(&engine);
    component.loadFromModule("Yalahoo.Music", "GeometryScene");
    std::unique_ptr<QObject> object(component.createWithInitialProperties(
            { { "adapter", QVariant::fromValue(&scene) }, { "materialReview", true } }));
    auto *view = qobject_cast<QQuickItem *>(object.get());
    if (!view) {
        qWarning() << component.errors();
        return 12;
    }
    view->setParent(window->contentItem());
    view->setParentItem(window->contentItem());
    const QString output = QFileInfo(report).absolutePath();
    QDir().mkpath(output);
    int frames = 0;
    RenderProbeInstances table;
    std::unique_ptr<RenderProbeAtlas> atlas;
    std::unique_ptr<QObject> pool;
    if (backendReview) {
        atlas = std::make_unique<RenderProbeAtlas>();
        qInfo() << "R10: creating instance probe";
        QQmlComponent component(&engine, QUrl("qrc:/review/InstanceProbe.qml"));
        pool.reset(component.createWithInitialProperties(
                { { "adapter", QVariant::fromValue(&scene) },
                  { "table", QVariant::fromValue(static_cast<QQuick3DInstancing *>(&table)) },
                  { "atlas",
                    QVariant::fromValue(static_cast<QQuick3DTextureData *>(atlas.get())) } }));
        if (!pool) {
            qWarning() << component.errors();
            return 12;
        }
        auto *node = qobject_cast<QQuick3DObject *>(pool.get());
        node->setParentItem(window->findChild<QQuick3DObject *>("inspectionCase")->parentItem());
        node->setProperty("visible", false);
        qInfo() << "R10: instance probe ready";
    }
    int index = -1, start = 0;
    QJsonArray results, restores;
    int restore = 0, previousInvalidations = 0;
    const int restoreCount = backendReview ? 0 : lightingReview ? 1 : 10;
    bool pass = true;
    double maxProjection = 0;
    const auto applyState = [&] {
        const auto c = cases[index].toObject();
        if (backendReview)
            qInfo() << "R10 GPU case" << index;
        view->setProperty("warmth", c["day"].toBool() ? 1.0 : 0.0);
        view->setProperty("clarity", c["clarity"].toDouble());
        if (pool) {
            const bool enabled = c["instancePool"].toBool();
            pool->setProperty("visible", enabled);
            table.setPositions(c["instances"].toArray().toVariantList());
            table.setDepthSortingEnabled(c["depthSort"].toBool());
            pool->setProperty("atlasEnabled", c["atlas"].toBool());
            pool->setProperty("coverSource",
                              QUrl("qrc:/assets/demo-covers/" + c["cover"].toString() + ".png"));
            if (c["atlas"].toBool())
                atlas->setPhase(c["atlasPhase"].toInt());
        }
        view->setProperty("reviewLayer",
                          c["instancePool"].toBool() ? QString("none") : c["layer"].toString());
        view->setProperty("coverSource",
                          c["paperProbe"].toBool() ? QUrl("qrc:/review/paper-probe.png")
                                                   : QUrl("qrc:/assets/demo-covers/"
                                                          + c["cover"].toString() + ".png"));
        const auto origin = c["origin"].toArray();
        const SceneVector caseOrigin{ origin.size() == 3 ? origin[0].toDouble() : 0,
                                      origin.size() == 3 ? origin[1].toDouble() : 0,
                                      origin.size() == 3 ? origin[2].toDouble() : 0 };
        view->setProperty("reviewCasePosition", QVariant::fromValue(caseOrigin.qt()));
        view->setProperty("lightingEnabled", lightingReview && !c["materialBaseline"].toBool());
        view->setProperty("occlusionEnabled",
                          (occlusionReview || postprocessReview)
                                  && !c["materialBaseline"].toBool());
        view->setProperty("postprocessEnabled",
                          postprocessReview && !c["materialBaseline"].toBool());
        view->setProperty("reviewDisableBokeh", c["disableBokeh"].toBool());
        view->setProperty("reviewDisableSmaa", c["disableSmaa"].toBool());
        view->setProperty("reviewAntialiasing", c.value("aaMode").toInt(1));
        const int w = c.value("width").toInt(960), h = c.value("height").toInt(720);
        window->resize(w, h);
        view->setProperty("shadowsEnabled",
                          (occlusionReview || backendReview) && c.value("shadows").toBool());
        view->setProperty("occlusionChannel", c.value("channel").toInt());
        view->setProperty("renderNear", c.value("clipNear").toDouble(scene.clipNear()));
        view->setProperty("renderFar", c.value("clipFar").toDouble(scene.clipFar()));
        view->setProperty("aoRadius", c.value("kernelRadius").toDouble(.18));
        view->setProperty("shadowProbe", c.value("shadowProbe").toBool());
        view->setProperty("reviewNeighbors",
                          c["instancePool"].toBool()
                                  ? QVariantList{ }
                                  : c.value("neighbors").toArray().toVariantList());
        if (occlusionReview || postprocessReview)
            scene.quality()->setPreset(c["quality"].toString());
        if (c.contains("shadowSize")) {
            auto settings = scene.quality()->values();
            settings.insert("shadows", c["shadowSize"].toInt());
            scene.quality()->configure(settings);
        }
        if (lightingReview) {
            scene.lighting()->setReducedMotion(true);
            scene.lighting()->setNight(!c["day"].toBool());
            auto settings = scene.lighting()->ribbon.settings;
            const auto mode = c["mode"].toString();
            settings.mode = mode == "baseline" ? RowLightingMode::Baseline
                    : mode == "area"           ? RowLightingMode::Area
                    : mode == "hybrid"         ? RowLightingMode::Hybrid
                                               : RowLightingMode::Guided;
            scene.lighting()->setSettings(settings);
        }
        const auto p = c["position"].toArray();
        const SceneVector pos{ p[0].toDouble(), p[1].toDouble(), p[2].toDouble() },
                aim = caseOrigin + SceneVector{ 0, 1.85, 0 };
        scene.layout()->setViewport(c.value("width").toInt(960), c.value("height").toInt(720));
        scene.observe({ pos, aim, 2 * (pos - aim).length() * std::tan(32 * std::acos(-1.0) / 360) },
                      0);
        if (occlusionReview || postprocessReview)
            scene.quality()->resize(c.value("width").toInt(960), c.value("height").toInt(720), 1,
                                    window->devicePixelRatio(), 4096);
        if (lightingReview) {
            const auto offset = scene.cameraRotation().rotatedVector({ -48, -17.6f, 36 });
            RowLightingInput i;
            i.reduced = true;
            scene.lighting()->update(i, { -2.085, 2.252, 0 },
                                     { offset.x(), offset.y(), offset.z() }, c["detail"].toDouble(),
                                     0);
        }
        double light = c.value("paperLight").toDouble(1.0);
        view->setProperty("paperIrradiance",
                          QVariant::fromValue(QVector3D(float(light), float(light), float(light))));
        view->setSize(QSizeF(w, h));
        start = frames;
        surface->requestFrame();
    };
    const auto next = [&] {
        ++index;
        applyState();
    };
    std::function<void()> collectFrame;
    QObject reviewLifetime;
    const auto bindFrames = [&] {
        const auto epoch = windowEpoch;
        surface->connectFrames(&reviewLifetime, [&, epoch] {
            if (epoch == windowEpoch)
                collectFrame();
        });
    };
    collectFrame = [&] {
        ++frames;
        if (frames - start < 4) {
            surface->requestFrame();
            return;
        }
        const auto c = cases[index].toObject();
        const bool paperProbe = c["paperProbe"].toBool();
        const QString name = restore == 0 ? QString("case-%1.png").arg(index)
                                          : QString("restore-%1.png").arg(restore);
        const auto actual = surface->grab().convertToFormat(QImage::Format_RGB32);
        if (actual.isNull() || actual.size() != window->size() * window->devicePixelRatio()) {
            qWarning() << "Material output/readback size mismatch" << actual.size();
            app.exit(15);
            return;
        }
        int paperProbeError = 0;
        if (paperProbe) {
            const double light = c["paperLight"].toDouble();
            const auto center = project(view, { .17f, 1.85f, .027f });
            // Constant test ink is a true input, independent of any rendered reference.
            const QImage ink(":/review/paper-probe.png");
            const auto sample = ink.pixelColor(0, 0);
            auto channel = [light](double s) {
                s /= 255;
                double l = s <= .04045 ? s / 12.92 : std::pow((s + .055) / 1.055, 2.4);
                l *= std::min(1.0, light);
                return qRound(255
                              * (l <= .0031308 ? 12.92 * l : 1.055 * std::pow(l, 1 / 2.4) - .055));
            };
            const auto actualCenter =
                    actual.pixelColor(qRound(center.x() * window->devicePixelRatio()),
                                      qRound(center.y() * window->devicePixelRatio()));
            paperProbeError = std::max({ std::abs(actualCenter.red() - channel(sample.red())),
                                         std::abs(actualCenter.green() - channel(sample.green())),
                                         std::abs(actualCenter.blue() - channel(sample.blue())) });
        }
        bool valid = actual.save(output + "/" + name) && (!paperProbe || paperProbeError <= 3);
        const auto background = actual.pixelColor(0, 0);
        int foreground = 0;
        for (int y = 0; y < actual.height(); ++y) {
            const auto *row = reinterpret_cast<const QRgb *>(actual.constScanLine(y));
            for (int x = 0; x < actual.width(); ++x) {
                const QColor a = QColor::fromRgb(row[x]);
                if (std::abs(a.red() - background.red()) + std::abs(a.green() - background.green())
                            + std::abs(a.blue() - background.blue())
                    > 9)
                    ++foreground;
            }
        }
        valid = valid && (occlusionReview || postprocessReview || foreground > 100);
        double webProjection = 0;
        for (auto value : c["projections"].toArray()) {
            const auto p = value.toObject();
            const auto w = p["world"].toArray(), pixel = p["pixel"].toArray();
            const auto qt = project(
                    view,
                    { float(w[0].toDouble()), float(w[1].toDouble()), float(w[2].toDouble()) });
            webProjection = std::max(
                    webProjection,
                    std::hypot(qt.x() - pixel[0].toDouble(), qt.y() - pixel[1].toDouble()));
        }
        valid = valid && webProjection <= 2;
        bool parameters = true;
        for (auto value : c["parameters"].toArray()) {
            const auto p = value.toObject();
            int surface = p["surface"] == "Frosted_Polymer" ? 0
                    : p["surface"] == "Ivory_Edges"         ? 1
                                                            : 2;
            const auto *material =
                    window->findChild<QObject *>(QString("caseMaterial%1").arg(surface));
            if (!material) {
                parameters = false;
                continue;
            }
            for (auto name : { "transmission", "roughness", "thickness", "attenuationDistance" }) {
                parameters = parameters
                        && std::abs(material->property(name).toDouble() - p[name].toDouble())
                                <= 1e-6;
            }
        }
        valid = valid && parameters;
        // Compare independent CPU/Quick3D projections, never image edges or ROI scores.
        for (const auto point :
             { QVector3D(-2.225f, .175f, .07f), QVector3D(2.225f, 3.525f, .07f) }) {
            const auto origin = view->property("reviewCasePosition").value<QVector3D>();
            const auto world = point + origin;
            const auto qt = project(view, world), cpu = scene.projectWorldPoint(world);
            const double error = std::hypot(qt.x() - cpu.x(), qt.y() - cpu.y());
            maxProjection = std::max(maxProjection, error);
            valid = valid && std::isfinite(error) && error <= 2;
        }
        if (occlusionReview || postprocessReview)
            valid = valid
                    && view->property("effectiveTextureSize").toSize()
                            == scene.quality()->targetSize();
        const auto *bokeh = window->findChild<QObject *>("bokehEffect");
        const double focus = bokeh->property("focus").toDouble(),
                     aperture = bokeh->property("aperture").toDouble();
        if (postprocessReview)
            valid = valid && std::abs(focus - c["focus"].toDouble()) < 1e-4
                    && std::abs(aperture - c["aperture"].toDouble()) < 1e-10;
        QJsonObject quick3DTarget;
        {
            const QMutexLocker lock(&targetMutex);
            quick3DTarget = latestQuick3DTarget;
        }
        if (multisampleBufferAdaptation.load() && view->property("reviewAntialiasing").toInt() == 1)
            valid = valid && quick3DTarget["samples"].toInt() == 4
                    && quick3DTarget["renderbuffer"].toBool() && quick3DTarget["resolved"].toBool()
                    && quick3DTarget["formatPreserved"].toBool();
        pass = pass && valid;
        QJsonObject result{
            { "quick3DTarget", quick3DTarget },
            { "multisampleBufferAdaptation", multisampleBufferAdaptation.load() },
            { "instancePool", c["instancePool"] },
            { "depthSort", c["depthSort"] },
            { "atlas", c["atlas"] },
            { "atlasPhase", atlas ? atlas->phase() : -1 },
            { "atlasSubmissions", atlas ? atlas->submissions() : 0 },
            { "atlasCpuPayloadBytes", atlas ? RenderProbeAtlas::pageBytes : 0 },
            { "atlasModifiedTileBytes", atlas ? RenderProbeAtlas::tileBytes : 0 },
            { "atlasGpuUploadBytes", QJsonValue(QJsonValue::Null) },
            { "index", postprocessReview ? c["index"].toInt() : index },
            { "focus", focus },
            { "aperture", aperture },
            { "bokehActive", view->property("bokehActive").toBool() },
            { "smaaActive", view->property("smaaActive").toBool() },
            { "aaMode", c.value("aaMode").toInt(1) },
            { "width", window->width() },
            { "height", window->height() },
            { "channel", c["channel"] },
            { "shadows", c["shadows"] },
            { "quality", c["quality"] },
            { "targetWidth", view->property("effectiveTextureSize").toSize().width() },
            { "targetHeight", view->property("effectiveTextureSize").toSize().height() },
            { "aoWidth", scene.quality()->aoSize().width() },
            { "aoHeight", scene.quality()->aoSize().height() },
            { "paperProbe", paperProbe },
            { "paperProbeError", paperProbeError },
            { "paperLight", c["paperLight"] },
            { "foregroundPixels", foreground },
            { "sourceParameterMatch", parameters },
            { "webProjectionPixels", webProjection },
            { "theme", c["day"].toBool() ? "day" : "night" },
            { "angle", c["angle"] },
            { "cover", c["cover"] },
            { "layer", c["layer"] },
            { "mode", c["mode"] },
            { "origin", c["origin"] },
            { "passed", valid },
            { "frames", frames - start },
            { "screenshot", name }
        };
        if (restore == 0)
            results.append(result);
        else {
            const bool recreated = invalidations > previousInvalidations;
            result.insert("sceneGraphRecreated", recreated);
            result.insert("qmlRootRecreated", true);
            restores.append(result);
            pass = pass && recreated;
        }
        if (index + 1 < cases.size()) {
            next();
            return;
        }
        if (restore < restoreCount) {
            ++restore;
            previousInvalidations = invalidations.load();
            // Rebuild the complete Review scene, including Quick3D scene-manager ownership.
            // Reusing a View3D after this manually driven context loss can leave it blank.
            pool.reset();
            view->setParentItem(nullptr);
            object.reset();
            surface->hide();
            window->releaseResources();
            if (!OffscreenSurface::Enabled) {
                auto previousSurface = std::move(surface);
                window.reset();
                previousSurface.reset();
                makeWindow();
                bindFrames();
            }
            object.reset(component.createWithInitialProperties(
                    { { "adapter", QVariant::fromValue(&scene) }, { "materialReview", true } }));
            view = qobject_cast<QQuickItem *>(object.get());
            if (!view) {
                app.exit(12);
                return;
            }
            view->setParent(window->contentItem());
            view->setParentItem(window->contentItem());
            applyState();
            surface->show();
            return;
        }
        pass = pass && warnings.isEmpty() && verification::windowPolicySatisfied(*window);
        QSaveFile file(report);
        bool saved = file.open(QIODevice::WriteOnly);
        if (saved) {
            file.write(
                    QJsonDocument(
                            QJsonObject{
                                    { "renderBackendContract",
                                      QJsonObject::fromVariantMap(scene.renderBackendContract()) },
                                    { "passed", pass },
                                    { "validation", "source-state-and-own-output" },
                                    { "warnings", QJsonArray::fromStringList(warnings) },
                                    { "offscreen", surface->diagnostics() },
                                    { "windowPolicy", verification::windowPolicy(*window) },
                                    { "qtVersion", qVersion() },
                                    { "graphicsApi",
                                      int(window->rendererInterface()->graphicsApi()) },
                                    { "dpr", window->devicePixelRatio() },
                                    { "width", window->width() },
                                    { "height", window->height() },
                                    { "baseline", oracle["baseline"] },
                                    { "fixtureSha256",
                                      QString::fromLatin1(QCryptographicHash::hash(
                                                                  bytes, QCryptographicHash::Sha256)
                                                                  .toHex()) },
                                    { "maxProjectionPixels", maxProjection },
                                    { "cases", results },
                                    { "restores", restores } })
                            .toJson());
            saved = file.commit();
        }
        app.exit(pass && saved ? 0 : 13);
    };
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &app, [&] {
        app.exit(14);
    });
    bindFrames();
    next();
    timeout.start(postprocessReview ? 600000 : 120000);
    surface->show();
    const int code = app.exec();
    surface->hide();
    pool.reset();
    object.reset();
    auto previousSurface = std::move(surface);
    window.reset();
    previousSurface.reset();
    app.setQuitOnLastWindowClosed(quitOnClose);
    return code;
}
}
