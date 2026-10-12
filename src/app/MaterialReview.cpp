#include "MaterialReview.h"
#include "ImageComparison.h"
#include "RenderProbe.h"
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
#include <QPolygonF>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSaveFile>
#include <QSGRendererInterface>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <atomic>
#include <memory>
namespace yalahoo {
namespace {
struct Region
{
    const char *name;
    QRectF local;
};
const Region regions[] = { { "paper", { -.7, 1.0, 1.75, 1.7 } },
                           { "margin", { 1.86, 1.0, .23, 1.7 } },
                           { "spine", { -2.17, .7, .23, 2.2 } },
                           { "rim", { -1.78, .22, 3.5, .07 } },
                           { "background", { 2.36, .7, .22, 2.2 } } };
QVector3D project(QQuickItem *view, QVector3D world)
{
    QVector3D result;
    QMetaObject::invokeMethod(view, "qtProjection", Q_RETURN_ARG(QVector3D, result),
                              Q_ARG(QVector3D, world));
    return result;
}

}
int reviewMaterials(QGuiApplication &app, QQuickWindow &window, SceneAdapter &scene,
                    const QString &fixture, const QString &report)
{
    QFile input(fixture);
    if (!input.open(QIODevice::ReadOnly))
        return 12;
    const auto bytes = input.readAll();
    const auto oracle = QJsonDocument::fromJson(bytes).object();
    const bool backendReview = oracle["renderBackend"].toBool();
    const bool postprocessReview = oracle["postprocess"].toBool();
    const bool calibration = oracle["calibration"].toBool(),
               occlusionReview = oracle["occlusion"].toBool(),
               lightingReview = oracle["lighting"].toBool() || occlusionReview || postprocessReview;
    auto cases = oracle["cases"].toArray();
    if (cases.isEmpty() || (!calibration && !lightingReview && cases.size() != 90))
        return 12;
    if (!calibration && !lightingReview)
        for (int coverIndex : { 1, 6, 11 })
            for (double light : { 0.0, 0.25, 2.0 }) {
                auto c = cases[coverIndex].toObject();
                c.insert("paperLight", light);
                c.insert("paperProbe", true);
                cases.append(c);
            }
    const auto root =
            QDir(QFileInfo(oracle.value("referenceFixture").toString(fixture)).absolutePath()
                 + "/../../..")
                    .absolutePath();
    const QString output = QFileInfo(report).absolutePath();
    QDir().mkpath(output);
    auto *view = window.findChild<QQuickItem *>("quick3DProbe");
    if (!view)
        return 12;
    window.setProperty("sceneReview", true);
    view->setProperty("geometryOnly", false);
    view->setProperty("materialReview", true);
    if (auto *label = window.findChild<QQuickItem *>("layoutProbe"))
        label->setVisible(false);
    window.setFlags(Qt::Window | Qt::FramelessWindowHint);
    window.resize(960, 720);
    window.setPosition(0, 0);
    window.show();
    const auto frames = std::make_shared<std::atomic<int>>(0);
    const auto connection = QObject::connect(
            &window, &QQuickWindow::afterRendering, &window,
            [frames] {
                ++*frames;
            },
            Qt::DirectConnection);
    RenderProbeInstances table;
    RenderProbeAtlas atlas;
    std::unique_ptr<QObject> pool;
    if (backendReview) {
        qInfo() << "R10: creating instance probe";
        QQmlComponent component(qmlEngine(&window), QUrl("qrc:/review/InstanceProbe.qml"));
        pool.reset(component.createWithInitialProperties(
                { { "adapter", QVariant::fromValue(&scene) },
                  { "table", QVariant::fromValue(static_cast<QQuick3DInstancing *>(&table)) },
                  { "atlas", QVariant::fromValue(static_cast<QQuick3DTextureData *>(&atlas)) } }));
        if (!pool) {
            qWarning() << component.errors();
            return 12;
        }
        auto *node = qobject_cast<QQuick3DObject *>(pool.get());
        node->setParentItem(window.findChild<QQuick3DObject *>("inspectionCase")->parentItem());
        node->setProperty("visible", false);
        qInfo() << "R10: instance probe ready";
    }
    int index = -1, start = 0;
    QJsonArray results;
    bool pass = true;
    double maxProjection = 0;
    const auto next = [&] {
        ++index;
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
                atlas.setPhase(c["atlasPhase"].toInt());
        }
        view->setProperty("reviewLayer",
                          c["instancePool"].toBool() ? QString("none") : c["layer"].toString());
        view->setProperty("coverSource",
                          QUrl("qrc:/assets/demo-covers/" + c["cover"].toString() + ".png"));
        const auto origin = c["origin"].toArray();
        const SceneVector caseOrigin{ origin[0].toDouble(), origin[1].toDouble(),
                                      origin[2].toDouble() };
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
        window.resize(w, h);
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
                                    window.devicePixelRatio(), 4096);
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
        const auto factors = c["factors"].toObject();
        for (int surface = 0; surface < 3; ++surface) {
            auto *material = window.findChild<QObject *>(QString("caseMaterial%1").arg(surface));
            for (auto name : { "exposureScale", "environmentScale", "transmissionScale",
                               "roughnessScale", "thicknessScale", "attenuationScale" })
                if (factors.contains(name))
                    material->setProperty(name, factors.value(name).toDouble());
        }
        start = frames->load();
        window.update();
    };
    QTimer poll;
    QObject::connect(&poll, &QTimer::timeout, &window, [&] {
        if (frames->load() - start < 4) {
            window.update();
            return;
        }
        const auto c = cases[index].toObject();
        const auto referenceName = c["screenshot"].toString();
        const bool paperProbe = c["paperProbe"].toBool();
        const QString name = paperProbe ? QString("paper-probe-%1.png").arg(index) : referenceName;
        const auto actual = window.grabWindow().convertToFormat(QImage::Format_RGB32);
        QImage expected(root
                        + (backendReview               ? "/docs/media/render-backend/web/"
                                   : postprocessReview ? "/docs/media/postprocess/web/"
                                   : occlusionReview   ? "/docs/media/occlusion/web/"
                                   : lightingReview    ? "/docs/media/lighting/web/"
                                                       : "/docs/media/materials/web/")
                        + referenceName);
        if (expected.isNull() || actual.isNull() || expected.size() != actual.size()) {
            qWarning() << "Image input/readback mismatch" << root << referenceName
                       << expected.size() << actual.size();
            app.exit(15);
            return;
        }
        int paperProbeError = 0;
        if (paperProbe) {
            const double light = c["paperLight"].toDouble();
            const auto center = project(view, { .17f, 1.85f, .027f });
            const auto sample = expected.pixelColor(qRound(center.x()), qRound(center.y()));
            auto channel = [light](double s) {
                s /= 255;
                double l = s <= .04045 ? s / 12.92 : std::pow((s + .055) / 1.055, 2.4);
                l *= std::min(1.0, light);
                return qRound(255
                              * (l <= .0031308 ? 12.92 * l : 1.055 * std::pow(l, 1 / 2.4) - .055));
            };
            const auto actualCenter = actual.pixelColor(qRound(center.x()), qRound(center.y()));
            paperProbeError = std::max({ std::abs(actualCenter.red() - channel(sample.red())),
                                         std::abs(actualCenter.green() - channel(sample.green())),
                                         std::abs(actualCenter.blue() - channel(sample.blue())) });
            pass = pass && paperProbeError <= 3;
            if (light != 2)
                expected =
                        actual; // Diffuse probes are asserted numerically above, not claimed as Web image parity.
        }
        bool valid = !actual.isNull() && !expected.isNull() && actual.size() == expected.size()
                && (postprocessReview ? window.devicePixelRatio() == oracle.value("dpr").toDouble(1)
                                      : window.devicePixelRatio() == 1);
        valid = valid && (calibration || actual.save(output + "/" + name));
        const auto background = actual.pixelColor(0, 0);
        int foreground = 0;
        for (int y = 0; y < actual.height(); ++y)
            for (int x = 0; x < actual.width(); ++x) {
                const auto a = actual.pixelColor(x, y);
                if (std::abs(a.red() - background.red()) + std::abs(a.green() - background.green())
                            + std::abs(a.blue() - background.blue())
                    > 9)
                    ++foreground;
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
        if (!calibration)
            for (auto value : c["parameters"].toArray()) {
                const auto p = value.toObject();
                int surface = p["surface"] == "Frosted_Polymer" ? 0
                        : p["surface"] == "Ivory_Edges"         ? 1
                                                                : 2;
                const auto *material =
                        window.findChild<QObject *>(QString("caseMaterial%1").arg(surface));
                for (auto pair : { std::pair{ "transmission", "transmissionScale" },
                                   std::pair{ "roughness", "roughnessScale" },
                                   std::pair{ "thickness", "thicknessScale" },
                                   std::pair{ "attenuationDistance", "attenuationScale" } }) {
                    double expectedValue =
                            p[pair.first].toDouble() * material->property(pair.second).toDouble();
                    if (QString(pair.first) == "transmission" || QString(pair.first) == "roughness")
                        expectedValue = std::clamp(expectedValue, 0.0, 1.0);
                    parameters = parameters
                            && std::abs(material->property(pair.first).toDouble() - expectedValue)
                                    <= 1e-6;
                }
            }
        valid = valid && parameters;
        QJsonObject roi;
        for (const auto &region : regions) {
            QPolygonF polygon;
            for (auto p : { region.local.topLeft(), region.local.topRight(),
                            region.local.bottomRight(), region.local.bottomLeft() }) {
                const auto o = c["origin"].toArray();
                const QVector3D world(float(p.x() + o[0].toDouble()),
                                      float(p.y() + o[1].toDouble()),
                                      float((region.name == QString("background") ? -.14 : .07)
                                            + o[2].toDouble()));
                const auto qt = project(view, world), cpu = scene.projectWorldPoint(world);
                const double err = std::hypot(qt.x() - cpu.x(), qt.y() - cpu.y());
                maxProjection = std::max(maxProjection, err);
                valid = valid && err <= 2;
                polygon << QPointF(qt.x() * window.devicePixelRatio(),
                                   qt.y() * window.devicePixelRatio());
            }
            auto metric = compareImages(actual, expected, polygon, !calibration);
            if (backendReview)
                metric.insert("edgeDistancePixels", edgeDistance(actual, expected, polygon));
            roi.insert(region.name, metric);
        }
        if (lightingReview && !occlusionReview && !postprocessReview)
            for (const auto &region : regions) {
                const auto metric = roi[region.name].toObject();
                valid = valid && metric["pixels"].toInt() > 0
                        && metric["meanAbsoluteRgb"].toDouble() <= 6
                        && (metric["ssimWindows"].toInt() == 0 || metric["ssim"].toDouble() >= .97);
            }
        QImage diff;
        if (valid && !calibration)
            diff = absoluteDifference(actual, expected);
        valid = valid && (calibration || diff.save(output + "/diff-" + name));
        if (occlusionReview || postprocessReview) {
            roi.insert("full",
                       compareImages(actual, expected, QPolygonF(QRectF(actual.rect())), false));
            const auto dimensions = scene.quality()->targetSize();
            valid = valid && view->property("effectiveTextureSize").toSize() == dimensions;
        }
        const auto *bokeh = window.findChild<QObject *>("bokehEffect");
        const double focus = bokeh->property("focus").toDouble(),
                     aperture = bokeh->property("aperture").toDouble();
        if (postprocessReview)
            valid = valid && std::abs(focus - c["focus"].toDouble()) < 1e-4
                    && std::abs(aperture - c["aperture"].toDouble()) < 1e-10;
        pass = pass && valid;
        results.append(QJsonObject{
                { "instancePool", c["instancePool"] },
                { "depthSort", c["depthSort"] },
                { "atlas", c["atlas"] },
                { "atlasPhase", atlas.phase() },
                { "atlasSubmissions", atlas.submissions() },
                { "atlasCpuPayloadBytes", RenderProbeAtlas::pageBytes },
                { "atlasModifiedTileBytes", RenderProbeAtlas::tileBytes },
                { "atlasGpuUploadBytes", QJsonValue(QJsonValue::Null) },
                { "index", postprocessReview ? c["index"].toInt() : index },
                { "focus", focus },
                { "aperture", aperture },
                { "bokehActive", view->property("bokehActive").toBool() },
                { "smaaActive", view->property("smaaActive").toBool() },
                { "aaMode", c.value("aaMode").toInt(1) },
                { "width", window.width() },
                { "height", window.height() },
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
                { "factors", c["factors"] },
                { "theme", c["day"].toBool() ? "day" : "night" },
                { "angle", c["angle"] },
                { "cover", c["cover"] },
                { "layer", c["layer"] },
                { "mode", c["mode"] },
                { "origin", c["origin"] },
                { "roi", roi },
                { "passed", valid },
                { "frames", frames->load() - start },
                { "screenshot", name } });
        if (index + 1 < cases.size()) {
            next();
            return;
        }
        poll.stop();
        QSaveFile file(report);
        bool saved = file.open(QIODevice::WriteOnly);
        if (saved) {
            file.write(
                    QJsonDocument(
                            QJsonObject{
                                    { "renderBackendContract",
                                      QJsonObject::fromVariantMap(scene.renderBackendContract()) },
                                    { "passed", pass },
                                    { "qtVersion", qVersion() },
                                    { "graphicsApi",
                                      int(window.rendererInterface()->graphicsApi()) },
                                    { "dpr", window.devicePixelRatio() },
                                    { "width", window.width() },
                                    { "height", window.height() },
                                    { "baseline", oracle["baseline"] },
                                    { "fixtureSha256",
                                      QString::fromLatin1(QCryptographicHash::hash(
                                                                  bytes, QCryptographicHash::Sha256)
                                                                  .toHex()) },
                                    { "maxProjectionPixels", maxProjection },
                                    { "cases", results } })
                            .toJson());
            saved = file.commit();
        }
        app.exit(pass && saved ? 0 : 13);
    });
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &app, [&] {
        app.exit(14);
    });
    next();
    poll.start(10);
    timeout.start(calibration ? 1200000 : postprocessReview ? 600000 : 120000);
    const int code = app.exec();
    QObject::disconnect(connection);
    return code;
}
}
