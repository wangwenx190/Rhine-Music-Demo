#include "CameraFraming.h"
#include "ReviewSupport.h"
#include "ColorTransfer.h"
#include "Units.h"
#include "RowLightController.h"
#include "Logging.h"
#include "MaterialReview.h"
#include "OffscreenSurface.h"
#include "ReviewGlCapabilities.h"
#include "VerificationWindow.h"
#include <QQmlComponent>
#include <QQmlEngine>
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
#include "JsonFile.h"
#include <QSGRendererInterface>
#include <QTimer>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>

using namespace Qt::StringLiterals;

namespace yalahoo {
namespace {
// Source parameter/projection expectations are independent from capture coverage and readback success.
constexpr auto DefaultWidthPixels = 960;
constexpr auto DefaultHeightPixels = 720;
constexpr auto SourceMaterialCaseCount = 90;
constexpr auto LifecycleRestoreCount = 1;
constexpr auto ProbeFovDegrees = 32;
constexpr auto DegreesPerTurn = 360;
constexpr auto ForegroundDifferenceThreshold = 9;
constexpr auto ParameterTolerance = 1e-6;
constexpr auto MaterialDeadlineMilliseconds = 120000;

// Nonzero process results are part of the standalone review command protocol.
enum ReviewExit {
    InvalidReviewInput = 12,
    ReviewFailed = 13,
    ReviewTimedOut = 14,
    CaptureFailed = 15
};

QVector3D project(QQuickItem *view, QVector3D world)
{
    QVector3D result;
    QMetaObject::invokeMethod(view, "qtProjection", Q_RETURN_ARG(QVector3D, result),
                              Q_ARG(QVector3D, world));
    return result;
}

// Shared references remain valid until this review event loop has stopped.
struct MaterialReviewState final
{
    quint64 &windowEpoch;
    std::unique_ptr<QQuickWindow> &window;
    QQuickRenderControl &control;
    QGuiApplication &app;
    std::atomic<int> &invalidations;
    QMutex &targetMutex;
    QJsonObject &latestQuick3DTarget;
    std::atomic<bool> &multisampleBufferAdaptation;
    QJsonArray &cases;
    const bool &lightingReview;
    SceneAdapter &scene;
    std::unique_ptr<OffscreenSurface> &surface;
    const QString &report;
    const QJsonObject &oracle;
    const QByteArray &bytes;
};
auto recordMaterialSceneGraph(MaterialReviewState &state)
{
    ++state.windowEpoch;
    state.window =
            std::make_unique<QQuickWindow>(OffscreenSurface::Enabled ? &state.control : nullptr);
    verification::prepareWindow(*state.window, true);
    state.window->resize(DefaultWidthPixels, DefaultHeightPixels);
    state.window->setPersistentSceneGraph(false);
    state.window->setPersistentGraphics(false);
    auto nextSurface = std::make_unique<OffscreenSurface>(*state.window, state.control);
    QObject::connect(
            state.window.get(), &QQuickWindow::sceneGraphInvalidated, &state.app,
            [&] {
                ++state.invalidations;
            },
            Qt::DirectConnection);
    QObject::connect(
            state.window.get(), &QQuickWindow::afterRendering, state.window.get(),
            [&, targetWindow = state.window.get()] {
                const auto target = review::quick3DTarget(*targetWindow);
                const QMutexLocker lock(&state.targetMutex);
                state.latestQuick3DTarget = target;
            },
            Qt::DirectConnection);
    QObject::connect(
            state.window.get(), &QQuickWindow::sceneGraphInitialized, state.window.get(),
            [&, targetSurface = nextSurface.get()] {
                state.multisampleBufferAdaptation =
                        review::useReviewGlMultisampleBuffers(targetSurface->rhi());
            },
            Qt::DirectConnection);
    return nextSurface;
}

auto captureMaterialReview(MaterialReviewState &state, auto &index, auto &view, auto &start,
                           auto &frames)
{
    const auto c = state.cases[index].toObject();
    view->setProperty("warmth", c[u"day"_s].toBool() ? 1.0 : 0.0);
    view->setProperty("clarity", c[u"clarity"_s].toDouble());
    view->setProperty("reviewLayer", c[u"layer"_s].toString());
    view->setProperty("coverSource",
                      c[u"paperProbe"_s].toBool() ? QUrl(u"qrc:/review/paper-probe.png"_s)
                                                  : QUrl("qrc:/assets/demo-covers/"_L1
                                                         + c[u"cover"_s].toString() + ".png"_L1));
    const auto origin = c[u"origin"_s].toArray();
    const SceneVector caseOrigin{ origin.size() == 3 ? origin[0].toDouble() : 0,
                                  origin.size() == 3 ? origin[1].toDouble() : 0,
                                  origin.size() == 3 ? origin[2].toDouble() : 0 };
    view->setProperty("reviewCasePosition", QVariant::fromValue(caseOrigin.qt()));
    view->setProperty("lightingEnabled",
                      state.lightingReview && !c[u"materialBaseline"_s].toBool());
    const int w = c.value(u"width"_s).toInt(DefaultWidthPixels),
              h = c.value(u"height"_s).toInt(DefaultHeightPixels);
    state.window->resize(w, h);
    if (state.lightingReview) {
        state.scene.lighting()->setReducedMotion(true);
        state.scene.lighting()->setNight(!c[u"day"_s].toBool());
        auto settings = state.scene.lighting()->ribbon.settings;
        const auto mode = c[u"mode"_s].toString();
        settings.mode = mode == "baseline"_L1 ? RowLightingMode::Baseline
                : mode == "area"_L1           ? RowLightingMode::Area
                : mode == "hybrid"_L1         ? RowLightingMode::Hybrid
                                              : RowLightingMode::Guided;
        state.scene.lighting()->setSettings(settings);
    }
    const auto p = c[u"position"_s].toArray();
    const SceneVector pos{ p[0].toDouble(), p[1].toDouble(), p[2].toDouble() },
            aim = caseOrigin + SceneVector{ 0, 1.85, 0 };
    state.scene.layout()->setViewport(c.value(u"width"_s).toInt(DefaultWidthPixels),
                                      c.value(u"height"_s).toInt(DefaultHeightPixels));
    state.scene.observe({ pos, aim,
                          2 * (pos - aim).length()
                                  * std::tan(ProbeFovDegrees * std::acos(-1.0) / DegreesPerTurn) },
                        0);
    if (state.lightingReview) {
        const auto offset = state.scene.cameraRotation().rotatedVector(
                { -yalahoo::cameraFraming::LightOffsetX, -yalahoo::cameraFraming::LightOffsetY,
                  yalahoo::cameraFraming::LightOffsetZ });
        RowLightingInput i;
        i.reduced = true;
        state.scene.lighting()->update(i,
                                       { -yalahoo::cameraFraming::LocalLightAimX,
                                         yalahoo::cameraFraming::LocalLightAimY, 0 },
                                       { offset.x(), offset.y(), offset.z() },
                                       c[u"detail"_s].toDouble(), 0);
    }
    double light = c.value(u"paperLight"_s).toDouble(1.0);
    view->setProperty("paperIrradiance",
                      QVariant::fromValue(QVector3D(float(light), float(light), float(light))));
    view->setSize(QSizeF(w, h));
    start = frames;
    state.surface->requestFrame();
}

auto finishMaterialReview(MaterialReviewState &state, auto &frames, auto &start, auto &index,
                          auto &restore, auto &view, auto &output, auto &maxProjection, auto &pass,
                          auto &results, auto &previousInvalidations, auto &restores, auto &next,
                          auto &restoreCount, auto &object, auto &makeWindow, auto &bindFrames,
                          auto &component, auto &applyState, auto &warnings)
{
    ++frames;
    if (frames - start < 4) {
        state.surface->requestFrame();
        return;
    }
    const auto c = state.cases[index].toObject();
    const bool paperProbe = c[u"paperProbe"_s].toBool();
    const QString name =
            restore == 0 ? u"case-%1.png"_s.arg(index) : u"restore-%1.png"_s.arg(restore);
    const auto actual = state.surface->grab().convertToFormat(QImage::Format_RGB32);
    if (actual.isNull()
        || actual.size() != state.window->size() * state.window->devicePixelRatio()) {
        qCWarning(yalahoo::logRender) << "Material output/readback size mismatch" << actual.size();
        state.app.exit(CaptureFailed);
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
            s /= yalahoo::units::MaximumChannelByte;
            double l = colorTransfer::decode(s);
            l *= std::min(1.0, light);
            return qRound(yalahoo::units::MaximumChannelByte * colorTransfer::encode(l));
        };
        const auto actualCenter =
                actual.pixelColor(qRound(center.x() * state.window->devicePixelRatio()),
                                  qRound(center.y() * state.window->devicePixelRatio()));
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
                > ForegroundDifferenceThreshold)
                ++foreground;
        }
    }
    valid = valid && foreground > yalahoo::review::MinimumForegroundPixels;
    double webProjection = 0;
    for (auto value : c[u"projections"_s].toArray()) {
        const auto p = value.toObject();
        const auto w = p[u"world"_s].toArray(), pixel = p[u"pixel"_s].toArray();
        const auto qt = project(
                view, { float(w[0].toDouble()), float(w[1].toDouble()), float(w[2].toDouble()) });
        webProjection =
                std::max(webProjection,
                         std::hypot(qt.x() - pixel[0].toDouble(), qt.y() - pixel[1].toDouble()));
    }
    valid = valid && webProjection <= 2;
    bool parameters = true;
    for (auto value : c[u"parameters"_s].toArray()) {
        const auto p = value.toObject();
        int surface = p[u"surface"_s] == u"Frosted_Polymer"_s ? 0
                : p[u"surface"_s] == u"Ivory_Edges"_s         ? 1
                                                              : 2;
        const auto *material = state.window->findChild<QObject *>(u"caseMaterial%1"_s.arg(surface));
        if (!material) {
            parameters = false;
            continue;
        }
        for (auto name : { "transmission", "roughness", "thickness", "attenuationDistance" }) {
            parameters = parameters
                    && std::abs(material->property(name).toDouble()
                                - p[QString::fromUtf8(name)].toDouble())
                            <= ParameterTolerance;
        }
    }
    valid = valid && parameters;
    // Compare independent CPU/Quick3D projections, never image edges or ROI scores.
    for (const auto point : { QVector3D(-2.225f, .175f, .07f), QVector3D(2.225f, 3.525f, .07f) }) {
        const auto origin = view->property("reviewCasePosition").template value<QVector3D>();
        const auto world = point + origin;
        const auto qt = project(view, world);
        const auto cpu = state.scene.projectWorldPoint(world);
        if (!cpu) {
            valid = false;
            continue;
        }
        const double error = std::hypot(qt.x() - cpu->x(), qt.y() - cpu->y());
        maxProjection = std::max(maxProjection, error);
        valid = valid && std::isfinite(error) && error <= 2;
    }
    QJsonObject quick3DTarget;
    {
        const QMutexLocker lock(&state.targetMutex);
        quick3DTarget = state.latestQuick3DTarget;
    }
    if (state.multisampleBufferAdaptation.load())
        valid = valid && quick3DTarget[u"samples"_s].toInt() == 4
                && quick3DTarget[u"renderbuffer"_s].toBool()
                && quick3DTarget[u"resolved"_s].toBool()
                && quick3DTarget[u"formatPreserved"_s].toBool();
    pass = pass && valid;
    QJsonObject result{ { u"quick3DTarget"_s, quick3DTarget },
                        { u"multisampleBufferAdaptation"_s,
                          state.multisampleBufferAdaptation.load() },
                        { u"index"_s, index },
                        { u"width"_s, state.window->width() },
                        { u"height"_s, state.window->height() },
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
                        { u"screenshot"_s, name } };
    if (restore == 0)
        results.append(result);
    else {
        const bool recreated = state.invalidations > previousInvalidations;
        result.insert(u"sceneGraphRecreated"_s, recreated);
        result.insert(u"qmlRootRecreated"_s, true);
        restores.append(result);
        pass = pass && recreated;
    }
    if (index + 1 < state.cases.size()) {
        next();
        return;
    }
    if (restore < restoreCount) {
        ++restore;
        previousInvalidations = state.invalidations.load();
        // Rebuild the complete Review scene, including Quick3D scene-manager ownership.
        // Reusing a View3D after this manually driven context loss can leave it blank.
        view->setParentItem(nullptr);
        object.reset();
        state.surface->hide();
        state.window->releaseResources();
        if (!OffscreenSurface::Enabled) {
            auto previousSurface = std::move(state.surface);
            state.window.reset();
            previousSurface.reset();
            state.surface = makeWindow();
            bindFrames();
        }
        object.reset(component.createWithInitialProperties(
                { { u"adapter"_s, QVariant::fromValue(&state.scene) } }));
        view = qobject_cast<QQuickItem *>(object.get());
        if (!view) {
            state.app.exit(InvalidReviewInput);
            return;
        }
        view->setParent(state.window->contentItem());
        view->setParentItem(state.window->contentItem());
        applyState();
        state.surface->show();
        return;
    }
    pass = pass && warnings.isEmpty() && verification::windowPolicySatisfied(*state.window);
    const bool saved = yalahoo::writeJsonFile(
            state.report,
            QJsonDocument(QJsonObject{
                    { u"renderBackendContract"_s,
                      QJsonObject::fromVariantMap(state.scene.renderBackendContract()) },
                    { u"passed"_s, pass },
                    { u"validation"_s, u"source-state-and-own-output"_s },
                    { u"warnings"_s, QJsonArray::fromStringList(warnings) },
                    { u"offscreen"_s, state.surface->diagnostics() },
                    { u"windowPolicy"_s, verification::windowPolicy(*state.window) },
                    { u"qtVersion"_s, QString::fromUtf8(qVersion()) },
                    { u"graphicsApi"_s, int(state.window->rendererInterface()->graphicsApi()) },
                    { u"dpr"_s, state.window->devicePixelRatio() },
                    { u"width"_s, state.window->width() },
                    { u"height"_s, state.window->height() },
                    { u"baseline"_s, state.oracle[u"baseline"_s] },
                    { u"fixtureSha256"_s,
                      QString::fromUtf8(
                              QCryptographicHash::hash(state.bytes, QCryptographicHash::Sha256)
                                      .toHex()) },
                    { u"maxProjectionPixels"_s, maxProjection },
                    { u"cases"_s, results },
                    { u"restores"_s, restores } }));
    state.app.exit(pass && saved ? 0 : ReviewFailed);
}

auto bindMaterialFrames(quint64 &windowEpoch, std::unique_ptr<OffscreenSurface> &surface,
                        QObject &reviewLifetime, std::function<void()> &collectFrame)
{
    const auto epoch = windowEpoch;
    surface->connectFrames(&reviewLifetime, [&, epoch] {
        if (epoch == windowEpoch)
            collectFrame();
    });
}
}

int reviewMaterials(QGuiApplication &app, SceneAdapter &scene, const QString &fixture,
                    const QString &report)
{
    QFile input(fixture);
    if (!input.open(QIODevice::ReadOnly))
        return InvalidReviewInput;
    const auto bytes = input.readAll();
    const auto oracle = QJsonDocument::fromJson(bytes).object();
    const bool lightingReview = oracle[u"lighting"_s].toBool();
    auto cases = oracle[u"cases"_s].toArray();
    for (const auto &value : cases) {
        const auto state = value.toObject();
        if (state[u"position"_s].toArray().size() != 3)
            return InvalidReviewInput;
        if (state[u"parameters"_s].toArray().size() != 3
            || state[u"projections"_s].toArray().size() != 5)
            return InvalidReviewInput;
    }
    if (cases.isEmpty() || (!lightingReview && cases.size() != SourceMaterialCaseCount))
        return InvalidReviewInput;
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
    MaterialReviewState state{ windowEpoch,
                               window,
                               control,
                               app,
                               invalidations,
                               targetMutex,
                               latestQuick3DTarget,
                               multisampleBufferAdaptation,
                               cases,
                               lightingReview,
                               scene,
                               surface,
                               report,
                               oracle,
                               bytes };
    const auto makeWindow = [&] {
        return recordMaterialSceneGraph(state);
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
            { { u"adapter"_s, QVariant::fromValue(&scene) } }));
    auto *view = qobject_cast<QQuickItem *>(object.get());
    if (!view) {
        qCWarning(yalahoo::logRender) << component.errors();
        return InvalidReviewInput;
    }
    view->setParent(window->contentItem());
    view->setParentItem(window->contentItem());
    const QString output = QFileInfo(report).absolutePath();
    QDir().mkpath(output);
    int frames = 0;
    int index = -1, start = 0;
    QJsonArray results, restores;
    int restore = 0, previousInvalidations = 0;
    const int restoreCount = LifecycleRestoreCount;
    bool pass = true;
    double maxProjection = 0;
    const auto applyState = [&] {
        return captureMaterialReview(state, index, view, start, frames);
    };
    const auto next = [&] {
        ++index;
        applyState();
    };
    std::function<void()> collectFrame;
    QObject reviewLifetime;
    const auto bindFrames = [&] {
        return bindMaterialFrames(windowEpoch, surface, reviewLifetime, collectFrame);
    };
    collectFrame = [&] {
        return finishMaterialReview(state, frames, start, index, restore, view, output,
                                    maxProjection, pass, results, previousInvalidations, restores,
                                    next, restoreCount, object, makeWindow, bindFrames, component,
                                    applyState, warnings);
    };
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &app, [&] {
        app.exit(ReviewTimedOut);
    });
    bindFrames();
    next();
    timeout.start(MaterialDeadlineMilliseconds);
    surface->show();
    const int code = app.exec();
    surface->hide();
    object.reset();
    auto previousSurface = std::move(surface);
    window.reset();
    previousSurface.reset();
    app.setQuitOnLastWindowClosed(quitOnClose);
    return code;
}
}
