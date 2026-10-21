#include "ReviewSupport.h"
#include "Units.h"
#include "SceneReview.h"
#include "SceneAdapter.h"
#include "SceneItem.h"
#include "VerificationWindow.h"
#include <QDir>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSaveFile>
#include <QSGRendererInterface>
#include <QTimer>
#include <atomic>
#include <memory>

namespace {
// Static fixture count and scalar tolerances validate source geometry independently of actual pixel coverage.
constexpr auto StaticPoseCount = 48;
constexpr auto DynamicFramesPerSecond = 120;
constexpr auto PositionTolerance = 1e-3;
constexpr auto FovTolerance = 1e-4;
constexpr auto CaptureInsetPixels = 40;
constexpr auto UnitCubeDiagonal = 1.7320508075688772;
constexpr auto DepthPackingErrorUnits = 3.0;
constexpr auto ReviewDeadlineMilliseconds = 80000;
constexpr auto PollIntervalMilliseconds = 10;

// Nonzero process results are part of the standalone review command protocol.
enum ReviewExit {
    InvalidReviewInput = 9,
    ReviewFailed = 10,
    ReviewTimedOut = 11
};
}

using namespace Qt::StringLiterals;

namespace yalahoo {

namespace {
// Shared references remain valid until this review event loop has stopped.
struct SceneReviewState final
{
    int &index;
    QJsonArray &cases;
    SceneAdapter &scene;
    QQuickWindow &window;
    int &startFrame;
    const std::shared_ptr<std::atomic<int>> &frames;
    yalahoo::rhi::SceneItem *&view;
    QObject *&layoutLabel;
    double &maxQt;
    double &maxWeb;
    const QString &folder;
    bool &pass;
    QJsonArray &results;
    const QString &report;
    const QJsonObject &oracle;
    const QByteArray &fixtureBytes;
    QGuiApplication &app;
};
auto captureSceneReview(SceneReviewState &state)
{
    ++state.index;
    if (state.index >= state.cases.size())
        return;
    const auto c = state.cases[state.index].toObject();
    if (state.index == StaticPoseCount) {
        state.scene.setReducedMotion(false);
        state.scene.observe({ { -62.26, 35.98, 43.28 },
                              { -.5, 1.1, .4 },
                              2 * SceneVector{ -62.26 + .5, 35.98 - 1.1, 43.28 - .4 }.length()
                                      * std::tan(6.15 * std::acos(-1.0) / 360) },
                            0);
    }
    state.window.resize(c[u"width"_s].toInt(), c[u"height"_s].toInt());
    state.scene.layout()->setViewport(state.window.width(), state.window.height());
    state.scene.setDetail(c[u"detail"_s].toDouble());
    if (c[u"dynamic"_s].toBool()) {
        state.scene.setReducedMotion(c[u"reduced"_s].toBool());
        state.scene.advance(c[u"dt"_s].toDouble());
    } else
        state.scene.snapToTarget();
    state.startFrame = state.frames->load();
    state.window.update();
}

auto finishSceneReview(SceneReviewState &state, auto &next, auto &poll)
{
    if (state.frames->load() - state.startFrame < 3) {
        state.window.update();
        return;
    }
    const auto c = state.cases[state.index].toObject();
    const bool dynamic = c[u"dynamic"_s].toBool();
    const bool capture = !dynamic || state.index == StaticPoseCount || state.index == 108
            || state.index == 198 || state.index == 288 || state.index == 408;
    const QImage image = capture ? state.window.grabWindow() : QImage{ };
    const bool bindings = state.window.width() == c[u"width"_s].toInt()
            && state.window.height() == c[u"height"_s].toInt()
            && state.view->width() == state.window.width()
            && state.view->height() == state.window.height()
            && state.layoutLabel->property("text").toString().startsWith(
                    state.scene.layout()->kind())
            && (state.view->input().scene.camera.position.qt() - state.scene.cameraPosition())
                            .length()
                    < PositionTolerance
            && std::abs(state.view->input().scene.fieldOfView - state.scene.fieldOfView())
                    < FovTolerance;
    bool points = true;
    QJsonArray projections;
    for (auto value : c[u"projections"_s].toArray()) {
        const auto p = value.toObject();
        const auto a = p[u"world"_s].toArray(), expected = p[u"pixel"_s].toArray();
        const QVector3D world(float(a[0].toDouble()), float(a[1].toDouble()),
                              float(a[2].toDouble()));
        QVector3D qt;
        const bool invoked = QMetaObject::invokeMethod(
                state.view, "qtProjection", Q_RETURN_ARG(QVector3D, qt), Q_ARG(QVector3D, world));
        const auto cpu = state.scene.projectWorldPoint(world);
        const double qtError = std::hypot(qt.x() - cpu.x(), qt.y() - cpu.y());
        const double webError =
                std::hypot(qt.x() - expected[0].toDouble(), qt.y() - expected[1].toDouble());
        state.maxQt = std::max(state.maxQt, qtError);
        state.maxWeb = std::max(state.maxWeb, webError);
        points = points && invoked && qtError <= 2 && webError <= 2;
        projections.append(QJsonObject{ { u"qtPixel"_s, QJsonArray{ qt.x(), qt.y() } },
                                        { u"qtCpuError"_s, qtError },
                                        { u"webError"_s, webError } });
    }
    // Actual no-light mesh pixels and client dimensions/DPR, in addition to the submitted RHI snapshot mapping.
    int pixels = 0;
    const auto rgb = image.convertToFormat(QImage::Format_RGB32);
    // Diagnostic labels must not stand in for missing mesh pixels.
    const int inset = qRound(CaptureInsetPixels * state.window.devicePixelRatio());
    const auto background = image.isNull() ? QColor() : image.pixelColor(image.width() - 1, 0);
    for (int y = inset; y < rgb.height() - inset; ++y)
        for (int x = 0; x < rgb.width(); ++x) {
            const auto color = reinterpret_cast<const QRgb *>(rgb.constScanLine(y))[x];
            // geometryDiagnostic writes a packed unit view normal for every face.
            // A green-only test loses valid purple/front-facing normals during the dolly.
            const double nx = 2.0 * qRed(color) / yalahoo::units::MaximumChannelByte - 1;
            const double ny = 2.0 * qGreen(color) / yalahoo::units::MaximumChannelByte - 1;
            const double nz = 2.0 * qBlue(color) / yalahoo::units::MaximumChannelByte - 1;
            const int difference = std::abs(qRed(color) - background.red())
                    + std::abs(qGreen(color) - background.green())
                    + std::abs(qBlue(color) - background.blue());
            constexpr double packedNormalTolerance =
                    2 * UnitCubeDiagonal / yalahoo::units::MaximumChannelByte
                    + DepthPackingErrorUnits
                            / (yalahoo::units::MaximumChannelByte
                               * yalahoo::units::MaximumChannelByte);
            if (difference > 9
                && std::abs(nx * nx + ny * ny + nz * nz - 1) <= packedNormalTolerance)
                ++pixels;
        }
    const QString file = u"case-%1-%2x%3-detail-%4.png"_s
                                 .arg(state.index, 2, yalahoo::units::DecimalBase, '0'_L1)
                                 .arg(state.window.width())
                                 .arg(state.window.height())
                                 .arg(state.scene.detail());
    const bool saved = !capture || (!image.isNull() && image.save(state.folder + "/"_L1 + file));
    const bool passed = bindings && points && saved && state.view->errorText().isEmpty()
            && (!capture || pixels > yalahoo::review::MinimumForegroundPixels);
    state.pass = state.pass && passed;
    state.results.append(
            QJsonObject{ { u"index"_s, state.index },
                         { u"width"_s, state.window.width() },
                         { u"height"_s, state.window.height() },
                         { u"detail"_s, state.scene.detail() },
                         { u"dynamic"_s, dynamic },
                         { u"time"_s, c[u"time"_s] },
                         { u"portrait"_s, state.scene.layout()->portrait() },
                         { u"kind"_s, state.scene.layout()->kind() },
                         { u"frameRevision"_s, qint64(state.scene.frameRevision()) },
                         { u"renderedFrames"_s, state.frames->load() - state.startFrame },
                         { u"imageWidth"_s, image.width() },
                         { u"imageHeight"_s, image.height() },
                         { u"dpr"_s, state.window.devicePixelRatio() },
                         { u"modelPixels"_s, pixels },
                         { u"renderTargetWidth"_s, state.view->fixedColorBufferWidth() },
                         { u"renderTargetHeight"_s, state.view->fixedColorBufferHeight() },
                         { u"renderError"_s, state.view->errorText() },
                         { u"bindings"_s, bindings },
                         { u"passed"_s, passed },
                         { u"screenshot"_s, capture ? file : QString{ } },
                         { u"projections"_s, projections } });
    if (state.index + 1 < state.cases.size()) {
        next();
        return;
    }
    poll.stop();
    const auto api = state.window.rendererInterface()->graphicsApi();
    state.pass = state.pass && QSGRendererInterface::isApiRhiBased(api);
    state.pass = state.pass && verification::windowPolicySatisfied(state.window);
    QSaveFile output(state.report);
    bool committed = output.open(QIODevice::WriteOnly);
    if (committed) {
        output.write(
                QJsonDocument(QJsonObject{ { u"passed"_s, state.pass },
                                           { u"windowPolicy"_s,
                                             verification::windowPolicy(state.window) },
                                           { u"qtVersion"_s, QString::fromUtf8(qVersion()) },
                                           { u"platform"_s, QGuiApplication::platformName() },
                                           { u"graphicsApi"_s, int(api) },
                                           { u"baseline"_s, state.oracle[u"baseline"_s] },
                                           { u"fixtureSha256"_s,
                                             QString::fromUtf8(QCryptographicHash::hash(
                                                                       state.fixtureBytes,
                                                                       QCryptographicHash::Sha256)
                                                                       .toHex()) },
                                           { u"maxQtCpuPixels"_s, state.maxQt },
                                           { u"maxWebPixels"_s, state.maxWeb },
                                           { u"cases"_s, state.results } })
                        .toJson());
        committed = output.commit();
    }
    state.app.exit(state.pass && committed ? 0 : ReviewFailed);
}

}
int reviewScene(QGuiApplication &app, QQuickWindow &window, SceneAdapter &scene,
                const QString &fixture, const QString &report)
{
    QFile input(fixture);
    if (!input.open(QIODevice::ReadOnly))
        return InvalidReviewInput;
    const auto fixtureBytes = input.readAll();
    const auto oracle = QJsonDocument::fromJson(fixtureBytes).object();
    auto cases = oracle[u"cases"_s].toArray();
    if (cases.size() != StaticPoseCount)
        return InvalidReviewInput;
    // Render every 1/120-second state of the interrupted dolly on Windows too.
    for (auto value : oracle[u"sequences"_s].toArray()) {
        const auto sequence = value.toObject();
        if (sequence[u"fps"_s].toInt() != DynamicFramesPerSecond)
            continue;
        for (auto sample : sequence[u"samples"_s].toArray()) {
            auto c = sample.toObject();
            c.insert(u"dynamic"_s, true);
            cases.append(c);
        }
    }
    window.setProperty("sceneReview", true);
    verification::prepareWindow(window, true);
    window.setPosition(0, 0);
    window.show();
    scene.setReducedMotion(
            true); // exact state; acquisition waits for rendered frames, never wall time pose
    auto *view = window.findChild<rhi::SceneItem *>(u"nativeScene"_s);
    auto *layoutLabel = window.findChild<QObject *>(u"layoutProbe"_s);
    if (!view || !layoutLabel)
        return InvalidReviewInput;
    const QString folder = QFileInfo(report).absolutePath();
    QDir().mkpath(folder);
    const auto frames = std::make_shared<std::atomic<int>>(0);
    const auto renderedConnection = QObject::connect(
            &window, &QQuickWindow::afterRendering, &window,
            [frames] {
                ++*frames;
            },
            Qt::DirectConnection);
    QJsonArray results;
    int index = -1, startFrame = 0;
    bool pass = true;
    double maxQt = 0, maxWeb = 0;
    SceneReviewState state{ index,   cases,       scene,  window,       startFrame, frames,
                            view,    layoutLabel, maxQt,  maxWeb,       folder,     pass,
                            results, report,      oracle, fixtureBytes, app };
    const auto next = [&] {
        return captureSceneReview(state);
    };
    QTimer poll;
    QObject::connect(&poll, &QTimer::timeout, &window, [&] {
        return finishSceneReview(state, next, poll);
    });
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &app, [&] {
        app.exit(ReviewTimedOut);
    });
    next();
    poll.start(PollIntervalMilliseconds);
    timeout.start(ReviewDeadlineMilliseconds);
    const int code = app.exec();
    // The shared counter also covers a callback already in progress during exit.
    QObject::disconnect(renderedConnection);
    return code;
}
}
