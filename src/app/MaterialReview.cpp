#include "RowLightController.h"
#include "Logging.h"
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

using namespace Qt::StringLiterals;

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
    const bool backendReview = oracle[u"renderBackend"_s].toBool();
    const bool postprocessReview = oracle[u"postprocess"_s].toBool();
    const bool occlusionReview = oracle[u"occlusion"_s].toBool(),
               lightingReview =
                       oracle[u"lighting"_s].toBool() || occlusionReview || postprocessReview;
    auto cases = oracle[u"cases"_s].toArray();
    for (const auto &value : cases) {
        const auto state = value.toObject();
        if (state[u"position"_s].toArray().size() != 3)
            return 12;
        if (!backendReview && !occlusionReview && !postprocessReview
            && (state[u"parameters"_s].toArray().size() != 3
                || state[u"projections"_s].toArray().size() != 5))
            return 12;
    }
    if (cases.isEmpty() || (!lightingReview && cases.size() != 90))
        return 12;
    if (!lightingReview)
        for (int coverIndex : { 1, 6, 11 })
            for (double light : { 0.0, 0.25, 2.0 }) {
                auto c = cases[coverIndex].toObject();
                c.insert(u"paperLight"_s, light);
                c.insert(u"paperProbe"_s, true);
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
        auto nextSurface = std::make_unique<OffscreenSurface>(*window, control);
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
                [&, targetSurface = nextSurface.get()] {
                    multisampleBufferAdaptation =
                            review::useReviewGlMultisampleBuffers(targetSurface->rhi());
                },
                Qt::DirectConnection);
        return nextSurface;
    };
    surface = makeWindow();
    QStringList warnings;
    QQmlEngine engine;
    QObject::connect(&engine, &QQmlEngine::warnings, &app, [&](const QList<QQmlError> &errors) {
        for (const auto &error : errors)
            warnings.append(error.toString());
    });
    QQmlComponent component(&engine);
    component.loadFromModule("Yalahoo.Music", "GeometryScene");
    std::unique_ptr<QObject> object(component.createWithInitialProperties(
            { { u"adapter"_s, QVariant::fromValue(&scene) }, { u"materialReview"_s, true } }));
    auto *view = qobject_cast<QQuickItem *>(object.get());
    if (!view) {
        qCWarning(yalahoo::logRender) << component.errors();
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
        qCInfo(yalahoo::logRender) << "R10: creating instance probe";
        QQmlComponent component(&engine, QUrl(u"qrc:/review/InstanceProbe.qml"_s));
        pool.reset(component.createWithInitialProperties(
                { { u"adapter"_s, QVariant::fromValue(&scene) },
                  { u"table"_s, QVariant::fromValue(static_cast<QQuick3DInstancing *>(&table)) },
                  { u"atlas"_s,
                    QVariant::fromValue(static_cast<QQuick3DTextureData *>(atlas.get())) } }));
        if (!pool) {
            qCWarning(yalahoo::logRender) << component.errors();
            return 12;
        }
        auto *node = qobject_cast<QQuick3DObject *>(pool.get());
        node->setParentItem(window->findChild<QQuick3DObject *>("inspectionCase")->parentItem());
        node->setProperty("visible", false);
        qCInfo(yalahoo::logRender) << "R10: instance probe ready";
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
            qCInfo(yalahoo::logRender) << "R10 GPU case" << index;
        view->setProperty("warmth", c[u"day"_s].toBool() ? 1.0 : 0.0);
        view->setProperty("clarity", c[u"clarity"_s].toDouble());
        if (pool) {
            const bool enabled = c[u"instancePool"_s].toBool();
            pool->setProperty("visible", enabled);
            table.setPositions(c[u"instances"_s].toArray().toVariantList());
            table.setDepthSortingEnabled(c[u"depthSort"_s].toBool());
            pool->setProperty("atlasEnabled", c[u"atlas"_s].toBool());
            pool->setProperty(
                    "coverSource",
                    QUrl("qrc:/assets/demo-covers/"_L1 + c[u"cover"_s].toString() + ".png"_L1));
            if (c[u"atlas"_s].toBool())
                atlas->setPhase(c[u"atlasPhase"_s].toInt());
        }
        view->setProperty("reviewLayer",
                          c[u"instancePool"_s].toBool() ? u"none"_s : c[u"layer"_s].toString());
        view->setProperty("coverSource",
                          c[u"paperProbe"_s].toBool()
                                  ? QUrl(u"qrc:/review/paper-probe.png"_s)
                                  : QUrl("qrc:/assets/demo-covers/"_L1 + c[u"cover"_s].toString()
                                         + ".png"_L1));
        const auto origin = c[u"origin"_s].toArray();
        const SceneVector caseOrigin{ origin.size() == 3 ? origin[0].toDouble() : 0,
                                      origin.size() == 3 ? origin[1].toDouble() : 0,
                                      origin.size() == 3 ? origin[2].toDouble() : 0 };
        view->setProperty("reviewCasePosition", QVariant::fromValue(caseOrigin.qt()));
        view->setProperty("lightingEnabled", lightingReview && !c[u"materialBaseline"_s].toBool());
        view->setProperty("occlusionEnabled",
                          (occlusionReview || postprocessReview)
                                  && !c[u"materialBaseline"_s].toBool());
        view->setProperty("postprocessEnabled",
                          postprocessReview && !c[u"materialBaseline"_s].toBool());
        view->setProperty("reviewDisableBokeh", c[u"disableBokeh"_s].toBool());
        view->setProperty("reviewDisableSmaa", c[u"disableSmaa"_s].toBool());
        view->setProperty("reviewAntialiasing", c.value(u"aaMode"_s).toInt(1));
        const int w = c.value(u"width"_s).toInt(960), h = c.value(u"height"_s).toInt(720);
        window->resize(w, h);
        view->setProperty("shadowsEnabled",
                          (occlusionReview || backendReview) && c.value(u"shadows"_s).toBool());
        view->setProperty("occlusionChannel", c.value(u"channel"_s).toInt());
        view->setProperty("renderNear", c.value(u"clipNear"_s).toDouble(scene.clipNear()));
        view->setProperty("renderFar", c.value(u"clipFar"_s).toDouble(scene.clipFar()));
        view->setProperty("aoRadius", c.value(u"kernelRadius"_s).toDouble(.18));
        view->setProperty("shadowProbe", c.value(u"shadowProbe"_s).toBool());
        view->setProperty("reviewNeighbors",
                          c[u"instancePool"_s].toBool()
                                  ? QVariantList{ }
                                  : c.value(u"neighbors"_s).toArray().toVariantList());
        if (lightingReview) {
            scene.lighting()->setReducedMotion(true);
            scene.lighting()->setNight(!c[u"day"_s].toBool());
            auto settings = scene.lighting()->ribbon.settings;
            const auto mode = c[u"mode"_s].toString();
            settings.mode = mode == "baseline"_L1 ? RowLightingMode::Baseline
                    : mode == "area"_L1           ? RowLightingMode::Area
                    : mode == "hybrid"_L1         ? RowLightingMode::Hybrid
                                                  : RowLightingMode::Guided;
            scene.lighting()->setSettings(settings);
        }
        const auto p = c[u"position"_s].toArray();
        const SceneVector pos{ p[0].toDouble(), p[1].toDouble(), p[2].toDouble() },
                aim = caseOrigin + SceneVector{ 0, 1.85, 0 };
        scene.layout()->setViewport(c.value(u"width"_s).toInt(960),
                                    c.value(u"height"_s).toInt(720));
        scene.observe({ pos, aim, 2 * (pos - aim).length() * std::tan(32 * std::acos(-1.0) / 360) },
                      0);
        if (occlusionReview || postprocessReview)
            scene.quality()->resize(c.value(u"width"_s).toInt(960), c.value(u"height"_s).toInt(720),
                                    window->devicePixelRatio());
        if (lightingReview) {
            const auto offset = scene.cameraRotation().rotatedVector({ -48, -17.6f, 36 });
            RowLightingInput i;
            i.reduced = true;
            scene.lighting()->update(i, { -2.085, 2.252, 0 },
                                     { offset.x(), offset.y(), offset.z() },
                                     c[u"detail"_s].toDouble(), 0);
        }
        double light = c.value(u"paperLight"_s).toDouble(1.0);
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
        const bool paperProbe = c[u"paperProbe"_s].toBool();
        const QString name =
                restore == 0 ? u"case-%1.png"_s.arg(index) : u"restore-%1.png"_s.arg(restore);
        const auto actual = surface->grab().convertToFormat(QImage::Format_RGB32);
        if (actual.isNull() || actual.size() != window->size() * window->devicePixelRatio()) {
            qCWarning(yalahoo::logRender)
                    << "Material output/readback size mismatch" << actual.size();
            app.exit(15);
            return;
        }
        int paperProbeError = 0;
        if (paperProbe) {
            const double light = c[u"paperLight"_s].toDouble();
            const auto center = project(view, { .17f, 1.85f, .027f });
            // Constant test ink is a true input, independent of any rendered reference.
            const QImage ink(u":/review/paper-probe.png"_s);
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
        bool valid = actual.save(output + "/"_L1 + name) && (!paperProbe || paperProbeError <= 3);
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
        for (auto value : c[u"projections"_s].toArray()) {
            const auto p = value.toObject();
            const auto w = p[u"world"_s].toArray(), pixel = p[u"pixel"_s].toArray();
            const auto qt = project(
                    view,
                    { float(w[0].toDouble()), float(w[1].toDouble()), float(w[2].toDouble()) });
            webProjection = std::max(
                    webProjection,
                    std::hypot(qt.x() - pixel[0].toDouble(), qt.y() - pixel[1].toDouble()));
        }
        valid = valid && webProjection <= 2;
        bool parameters = true;
        for (auto value : c[u"parameters"_s].toArray()) {
            const auto p = value.toObject();
            int surface = p[u"surface"_s] == u"Frosted_Polymer"_s ? 0
                    : p[u"surface"_s] == u"Ivory_Edges"_s         ? 1
                                                                  : 2;
            const auto *material = window->findChild<QObject *>(u"caseMaterial%1"_s.arg(surface));
            if (!material) {
                parameters = false;
                continue;
            }
            for (auto name : { "transmission", "roughness", "thickness", "attenuationDistance" }) {
                parameters = parameters
                        && std::abs(material->property(name).toDouble()
                                    - p[QString::fromUtf8(name)].toDouble())
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
            valid = valid && std::abs(focus - c[u"focus"_s].toDouble()) < 1e-4
                    && std::abs(aperture - c[u"aperture"_s].toDouble()) < 1e-10;
        QJsonObject quick3DTarget;
        {
            const QMutexLocker lock(&targetMutex);
            quick3DTarget = latestQuick3DTarget;
        }
        if (multisampleBufferAdaptation.load() && view->property("reviewAntialiasing").toInt() == 1)
            valid = valid && quick3DTarget[u"samples"_s].toInt() == 4
                    && quick3DTarget[u"renderbuffer"_s].toBool()
                    && quick3DTarget[u"resolved"_s].toBool()
                    && quick3DTarget[u"formatPreserved"_s].toBool();
        pass = pass && valid;
        QJsonObject result{
            { u"quick3DTarget"_s, quick3DTarget },
            { u"multisampleBufferAdaptation"_s, multisampleBufferAdaptation.load() },
            { u"instancePool"_s, c[u"instancePool"_s] },
            { u"depthSort"_s, c[u"depthSort"_s] },
            { u"atlas"_s, c[u"atlas"_s] },
            { u"atlasPhase"_s, atlas ? atlas->phase() : -1 },
            { u"atlasSubmissions"_s, atlas ? atlas->submissions() : 0 },
            { u"atlasCpuPayloadBytes"_s, atlas ? RenderProbeAtlas::pageBytes : 0 },
            { u"atlasModifiedTileBytes"_s, atlas ? RenderProbeAtlas::tileBytes : 0 },
            { u"atlasGpuUploadBytes"_s, QJsonValue(QJsonValue::Null) },
            { u"index"_s, postprocessReview ? c[u"index"_s].toInt() : index },
            { u"focus"_s, focus },
            { u"aperture"_s, aperture },
            { u"bokehActive"_s, view->property("bokehActive").toBool() },
            { u"smaaActive"_s, view->property("smaaActive").toBool() },
            { u"aaMode"_s, c.value(u"aaMode"_s).toInt(1) },
            { u"width"_s, window->width() },
            { u"height"_s, window->height() },
            { u"channel"_s, c[u"channel"_s] },
            { u"shadows"_s, c[u"shadows"_s] },
            { u"quality"_s, c[u"quality"_s] },
            { u"targetWidth"_s, view->property("effectiveTextureSize").toSize().width() },
            { u"targetHeight"_s, view->property("effectiveTextureSize").toSize().height() },
            { u"aoWidth"_s, scene.quality()->targetSize().width() },
            { u"aoHeight"_s, scene.quality()->targetSize().height() },
            { u"paperProbe"_s, paperProbe },
            { u"paperProbeError"_s, paperProbeError },
            { u"paperLight"_s, c[u"paperLight"_s] },
            { u"foregroundPixels"_s, foreground },
            { u"sourceParameterMatch"_s, parameters },
            { u"webProjectionPixels"_s, webProjection },
            { u"theme"_s, c[u"day"_s].toBool() ? u"day"_s : u"night"_s },
            { u"angle"_s, c[u"angle"_s] },
            { u"cover"_s, c[u"cover"_s] },
            { u"layer"_s, c[u"layer"_s] },
            { u"mode"_s, c[u"mode"_s] },
            { u"origin"_s, c[u"origin"_s] },
            { u"passed"_s, valid },
            { u"frames"_s, frames - start },
            { u"screenshot"_s, name }
        };
        if (restore == 0)
            results.append(result);
        else {
            const bool recreated = invalidations > previousInvalidations;
            result.insert(u"sceneGraphRecreated"_s, recreated);
            result.insert(u"qmlRootRecreated"_s, true);
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
                surface = makeWindow();
                bindFrames();
            }
            object.reset(component.createWithInitialProperties(
                    { { u"adapter"_s, QVariant::fromValue(&scene) },
                      { u"materialReview"_s, true } }));
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
                                    { u"renderBackendContract"_s,
                                      QJsonObject::fromVariantMap(scene.renderBackendContract()) },
                                    { u"passed"_s, pass },
                                    { u"validation"_s, u"source-state-and-own-output"_s },
                                    { u"warnings"_s, QJsonArray::fromStringList(warnings) },
                                    { u"offscreen"_s, surface->diagnostics() },
                                    { u"windowPolicy"_s, verification::windowPolicy(*window) },
                                    { u"qtVersion"_s, QString::fromUtf8(qVersion()) },
                                    { u"graphicsApi"_s,
                                      int(window->rendererInterface()->graphicsApi()) },
                                    { u"dpr"_s, window->devicePixelRatio() },
                                    { u"width"_s, window->width() },
                                    { u"height"_s, window->height() },
                                    { u"baseline"_s, oracle[u"baseline"_s] },
                                    { u"fixtureSha256"_s,
                                      QString::fromUtf8(QCryptographicHash::hash(
                                                                bytes, QCryptographicHash::Sha256)
                                                                .toHex()) },
                                    { u"maxProjectionPixels"_s, maxProjection },
                                    { u"cases"_s, results },
                                    { u"restores"_s, restores } })
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
