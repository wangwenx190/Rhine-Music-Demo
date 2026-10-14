#include "RhiItem.h"
#include "SceneAdapter.h"
#include "FixedTextureData.h"
#include "ImageComparison.h"
#include "VerificationWindow.h"
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QEventLoop>
#include <QTimer>
#include <QElapsedTimer>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSaveFile>
#include <QPointer>
#include <QCryptographicHash>
#include <atomic>
#include <optional>
#include <stdexcept>
#include <cstdio>
#include <cmath>
using namespace yalahoo;
using namespace yalahoo::rhi;
namespace {
void check(bool ok, const char *reason)
{
    if (!ok)
        throw std::runtime_error(reason);
}
QPolygonF full(QSize s)
{
    return { { 0, 0 },
             { double(s.width()), 0 },
             { double(s.width()), double(s.height()) },
             { 0, double(s.height()) } };
}
void await(const std::function<bool()> &done, QQuickWindow &window)
{
    QElapsedTimer timeout;
    timeout.start();
    while (!done() && timeout.elapsed() < 15000) {
        window.update();
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    check(done(), "GPU capture timeout");
}
void frames(QQuickWindow &w, int count = 3)
{
    std::atomic<int> received = 0;
    auto c = QObject::connect(
            &w, &QQuickWindow::afterRendering, &w,
            [&] {
                ++received;
            },
            Qt::DirectConnection);
    await(
            [&] {
                return received >= count;
            },
            w);
    QObject::disconnect(c);
}
Input fixture(QSize size, bool night, bool low, const QString &cover)
{
    SceneAdapter scene;
    scene.layout()->setViewport(size.width(), size.height());
    scene.setReducedMotion(true);
    scene.lighting()->setReducedMotion(true);
    scene.lighting()->setNight(night);
    Input in;
    in.scene = scene.frame();
    in.scene.nearPlane = .1;
    in.scene.farPlane = 100;
    in.scene.fieldOfView = 32;
    in.scene.camera.position = low ? SceneVector{ 8, 6, 15 } : SceneVector{ 0, 1.85, 12 };
    in.scene.camera.aim = { 0, 1.85, 0 };
    in.scene.view.setToIdentity();
    in.scene.view.lookAt(in.scene.camera.position.qt(), in.scene.camera.aim.qt(), { 0, 1, 0 });
    in.scene.cameraRotation =
            QQuaternion::fromRotationMatrix(in.scene.view.inverted().normalMatrix());
    in.scene.projection.setToIdentity();
    in.scene.projection.perspective(32, float(size.width()) / size.height(), .1f, 100);
    in.scene.caseTransform.setToIdentity();
    in.scene.casePosition = { 0, 0, 0 };
    scene.setLightingInput({ });
    scene.updateLighting(0);
    const auto &p = scene.lighting()->palette();
    auto *l = scene.lighting();
    in.background = p.background.qt();
    in.keyColor = p.keyColor.qt();
    in.skyColor = p.skyColor.qt();
    in.groundColor = p.groundColor.qt();
    in.lightIntensities = l->lightIntensities();
    in.spotPosition = l->spotPosition();
    in.spotTarget = l->spotTarget();
    in.spotColor = l->spotColor();
    in.spotIntensity = l->spotIntensity();
    in.environmentIntensity = l->environmentIntensity();
    in.rowSettings = l->rowSettings();
    in.rowColumn = l->rowColumn();
    in.rowDetail = l->rowDetail();
    in.rowFill = l->rowFill();
    in.lightColumn = l->lightColumn();
    in.printAmbient = l->printAmbient();
    in.cover = QImage(":/assets/demo-covers/" + cover + ".png");
    in.shadows = false;
    return in;
}
QVariantMap oracleValues(const Input &i)
{
    auto encode = [](float c) {
        return c <= .0031308f ? 12.92f * c : 1.055f * std::pow(c, 1.0f / 2.4f) - .055f;
    };
    const auto bg = QColor::fromRgbF(encode(i.background.x()), encode(i.background.y()),
                                     encode(i.background.z()));
    return { { "position", QVariant::fromValue(i.scene.camera.position.qt()) },
             { "rotation", QVariant::fromValue(i.scene.cameraRotation) },
             { "origin", QVariant::fromValue(i.scene.casePosition.qt()) },
             { "fov", i.scene.fieldOfView },
             { "near", i.scene.nearPlane },
             { "far", i.scene.farPlane },
             { "background", QVariant::fromValue(i.background) },
             { "backgroundSrgb", bg },
             { "environmentIntensity", i.environmentIntensity },
             { "rowColumn", QVariant::fromValue(i.rowColumn) },
             { "rowSettings", QVariant::fromValue(i.rowSettings) },
             { "rowDetail", i.rowDetail },
             { "rowFill", i.rowFill },
             { "lightColumn", QVariant::fromValue(i.lightColumn) },
             { "printAmbient", i.printAmbient },
             { "spotPosition", QVariant::fromValue(i.spotPosition) },
             { "spotTarget", QVariant::fromValue(i.spotTarget) },
             { "spotColor", QVariant::fromValue(i.spotColor) },
             { "spotIntensity", i.spotIntensity },
             { "groundColor", QVariant::fromValue(i.groundColor) },
             { "skyColor", QVariant::fromValue(i.skyColor) },
             { "keyColor", QVariant::fromValue(i.keyColor) },
             { "lightIntensities", QVariant::fromValue(i.lightIntensities) } };
}
double depthAt(const QImage &i, int x, int y)
{
    const auto p = i.pixelColor(x, y);
    return (p.red() * 65536 + p.green() * 256 + p.blue()) / 16777215.0;
}
double cornerError(const Input &in, const QImage &image)
{
    const auto bg = image.pixelColor(0, 0);
    double worst = 0;
    auto inside = [&](int x, int y) {
        return image.rect().contains(x, y) && image.pixelColor(x, y) != bg;
    };
    for (float x : { -2.225f, 2.225f })
        for (float y : { .175f, 3.525f }) {
            const auto c = in.scene.projection * in.scene.view * QVector4D(x, y, .07f, 1);
            const QPointF p((c.x() / c.w() + 1) * .5 * image.width(),
                            (1 - c.y() / c.w()) * .5 * image.height());
            double best = 100;
            for (int yy = int(p.y()) - 4; yy <= int(p.y()) + 4; ++yy)
                for (int xx = int(p.x()) - 4; xx <= int(p.x()) + 4; ++xx) {
                    if (inside(xx, yy)
                        && (!inside(xx - 1, yy) || !inside(xx + 1, yy) || !inside(xx, yy - 1)
                            || !inside(xx, yy + 1)))
                        best = std::min(best, std::hypot(xx + .5 - p.x(), yy + .5 - p.y()));
                }
            worst = std::max(worst, best);
        }
    return worst;
}
}
int main(int argc, char **argv)
{
    yalahoo::verification::prepareApplication();
#if defined(Q_OS_WIN)
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
#elif defined(Q_OS_MACOS)
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Metal);
#else
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
#endif
    QGuiApplication app(argc, argv);
    if (argc != 2)
        return 2;
    try {
        const QString output = QString::fromLocal8Bit(argv[1]);
        QDir().mkpath(output);
        qmlRegisterType<FixedTextureData>("Yalahoo.RhiReview", 1, 0, "FixedTextureData");
        // Window outlives the engine/items and guarded queued readback deliveries.
        QQuickWindow window;
        window.setTitle("yalahOO!!! · R10A GPU review");
        verification::prepareWindow(window, true);
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl("qrc:/review/RhiOracle.qml"),
                                QQmlComponent::PreferSynchronous);
        while (component.isLoading())
            QCoreApplication::processEvents();
        std::unique_ptr<QObject> oracleObject(component.createWithInitialProperties(
                { { "fixture", oracleValues(fixture({ 960, 720 }, false, false, "square")) } }));
        if (!oracleObject)
            for (const auto &e : component.errors())
                std::fprintf(stderr, "%s\n", qPrintable(e.toString()));
        check(bool(oracleObject), "Quick3D oracle failed to load");
        auto *oracle = qobject_cast<QQuickItem *>(oracleObject.get());
        check(oracle, "Invalid oracle item");
        oracle->setParentItem(window.contentItem());
        auto *item = new RhiItem(window.contentItem());
        item->setVisible(false);
        auto result = std::make_shared<std::optional<Capture>>();
        QPointer<QQuickWindow> guard(&window);
        item->captured = [result, guard](Capture capture) {
            if (guard)
                QMetaObject::invokeMethod(
                        guard,
                        [result, capture = std::move(capture)]() mutable {
                            if (!*result || capture.request >= (**result).request)
                                *result = std::move(capture);
                        },
                        Qt::QueuedConnection);
        };
        quint64 request = 0;
        QJsonArray cases;
        double worstMAE = 0, worstDepth = 0, worstProjection = 0;
        auto capture = [&](Input in, QSize size) {
            result->reset();
            in.request = ++request;
            window.resize(size);
            item->setSize(size);
            oracle->setSize(size);
            oracle->setVisible(false);
            item->submit(in, size);
            item->setVisible(true);
            window.show();
            await(
                    [&] {
                        return *result && (**result).request == request;
                    },
                    window);
            check((**result).color.size() == size && (**result).depth.size() == size,
                  "Stale target dimensions after resize");
#if defined(Q_OS_WIN)
            check((**result).metadata["backend"].toInt() == 3, "Windows probe did not use D3D11");
#endif
            check((**result).metadata["renderThreadDistinct"].toBool(),
                  "Review did not exercise GUI/render thread isolation");
            auto gpu = **result;
            const auto v = [](QVector3D p) {
                return QJsonArray{ p.x(), p.y(), p.z() };
            };
            const auto matrix = [](const QMatrix4x4 &m) {
                QJsonArray a;
                for (int n = 0; n < 16; ++n)
                    a.append(m.constData()[n]);
                return a;
            };
            QJsonArray entities;
            for (const auto &m : in.entities)
                entities.append(matrix(m));
            const auto rgba = in.cover.convertToFormat(QImage::Format_RGBA8888);
            const auto coverHash =
                    QCryptographicHash::hash(
                            QByteArrayView(reinterpret_cast<const char *>(rgba.constBits()),
                                           rgba.sizeInBytes()),
                            QCryptographicHash::Sha256)
                            .toHex();
            QJsonObject witness{ { "width", size.width() },
                                 { "height", size.height() },
                                 { "dpr", window.devicePixelRatio() },
                                 { "quality", "R10A single-sample opaque channel diagnostic" },
                                 { "reviewTime", 0 },
                                 { "cameraProjection", matrix(in.scene.projection) },
                                 { "cameraView", matrix(in.scene.view) },
                                 { "entity", matrix(in.scene.caseTransform) },
                                 { "entities", entities },
                                 { "coverRgbaSha256", QString::fromLatin1(coverHash) },
                                 { "rowSettings",
                                   QJsonArray{ in.rowSettings.x(), in.rowSettings.y(),
                                               in.rowSettings.z(), in.rowSettings.w() } },
                                 { "rowColumn", v(in.rowColumn) },
                                 { "lightColumn", v(in.lightColumn) },
                                 { "background", v(in.background) },
                                 { "spotPosition", v(in.spotPosition) },
                                 { "spotTarget", v(in.spotTarget) },
                                 { "spotIntensity", in.spotIntensity },
                                 { "shadowSize", in.shadowSize },
                                 { "shadows", in.shadows },
                                 { "geometryDiagnostic", in.geometryDiagnostic },
                                 { "shadowDiagnostic", in.shadowDiagnostic } };
            gpu.metadata["input"] = witness;
            gpu.metadata["fixtureSha256"] = QString::fromLatin1(
                    QCryptographicHash::hash(QJsonDocument(witness).toJson(QJsonDocument::Compact),
                                             QCryptographicHash::Sha256)
                            .toHex());
            gpu.metadata["baseline"] = "57f4f845c20cce646a15dbf4008c2a162aaaf4e2";
            check(window.devicePixelRatio() == 1, "Review requires actual DPR1");
            return gpu;
        };
        // Actual current Quick3D shader/mesh oracle; independent of RHI matrix correction and qsb.
        for (bool night : { false, true })
            for (bool low : { false, true })
                for (QString cover : { "square", "portrait", "landscape" })
                    for (int channel : { 0, 1, 2 }) {
                        const QSize size(960, 720);
                        auto in = fixture(size, night, low, cover);
                        in.geometryDiagnostic = channel != 0;
                        auto gpu = capture(in, size);
                        item->setVisible(false);
                        oracle->setProperty("fixture", oracleValues(in));
                        oracle->setProperty("channel", channel);
                        oracle->setProperty("cover", cover);
                        oracle->setProperty("geometry", in.geometryDiagnostic);
                        oracle->setVisible(true);
                        frames(window);
                        auto expected =
                                window.grabWindow().convertToFormat(QImage::Format_RGBA8888);
                        if (expected.size() != size)
                            std::fprintf(stderr, "oracle=%dx%d window=%dx%d dpr=%g\n",
                                         expected.width(), expected.height(), window.width(),
                                         window.height(), window.devicePixelRatio());
                        check(expected.size() == size, "Oracle image dimensions changed");
                        const auto actual = channel == 0 ? gpu.color
                                : channel == 1           ? gpu.normal
                                                         : gpu.depth;
                        const auto metrics = compareImages(actual, expected, full(size), false);
                        const double mae = metrics["meanAbsoluteRgb"].toDouble(1e9);
                        worstMAE = std::max(worstMAE, mae);
                        const auto label =
                                QString("%1-%2-%3-%4")
                                        .arg(night ? "night" : "day", low ? "low" : "front", cover)
                                        .arg(channel);
                        check(actual.save(output + "/" + label + "-rhi.png")
                                      && expected.save(output + "/" + label + "-quick3d.png"),
                              "Cannot save paired GPU images");
                        cases.append(QJsonObject{ { "scene", label },
                                                  { "channel", channel },
                                                  { "metrics", metrics },
                                                  { "metadata", gpu.metadata } });
                        // 0.05 normalized full-image MAE contract; the stronger measured value is retained.
                        check(mae <= 6, "RHI/Quick3D opaque channel mismatch");
                        if (channel == 1 && !low) {
                            const auto error = cornerError(in, gpu.normal);
                            worstProjection = std::max(worstProjection, error);
                            check(error <= 2, "Authored corner GPU projection exceeds 2 pixels");
                        }
                        if (channel == 2 && !low) {
                            const double observed = depthAt(gpu.depth, 480, 360);
                            const double expectedDepth = (12 - .070 - .1)
                                    / (100 - .1); // nearest authored lid front face
                            const double error = std::abs(observed - expectedDepth);
                            worstDepth = std::max(worstDepth, error);
                            check(error < 2e-6,
                                  "GPU depth does not match known authored vertex plane");
                        }
                    }
        // Same item repeatedly crosses target-size/aspect boundaries, including restoring earlier sizes.
        QJsonArray sizes;
        for (auto size : { QSize(1920, 1080), QSize(1280, 720), QSize(995, 858), QSize(960, 900),
                           QSize(930, 900), QSize(390, 844), QSize(320, 720), QSize(2560, 1080),
                           QSize(3440, 1440), QSize(960, 720), QSize(320, 720), QSize(960, 720) }) {
            auto gpu = capture(fixture(size, false, true, "square"), size);
            sizes.append(gpu.metadata);
        }
        // Issue new targets while earlier readbacks are still queued. No GUI/render shared resource pointers.
        for (int n = 0; n < 30; ++n) {
            const QSize size(n % 2 ? 930 : 390, n % 2 ? 900 : 844);
            auto in = fixture(size, false, true, "portrait");
            in.request = ++request;
            window.resize(size);
            item->setSize(size);
            item->submit(in, size);
            item->setVisible(true);
            oracle->setVisible(false);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
            window.update();
        }
        auto finalResize = capture(fixture({ 960, 720 }, false, false, "square"), { 960, 720 });
        // Cover revision is independent of the optional capture request number.
        auto changed = fixture({ 960, 720 }, false, false, "square");
        changed.request = request;
        changed.cover.fill(Qt::red);
        item->submit(changed, { 960, 720 });
        frames(window);
        const auto red = window.grabWindow().pixelColor(480, 360);
        changed.cover.fill(Qt::blue);
        item->submit(changed, { 960, 720 });
        frames(window);
        const auto blue = window.grabWindow().pixelColor(480, 360);
        check(red.red() > red.blue() + 20 && blue.blue() > blue.red() + 20,
              "Same-size cover change was tied to capture request");
        // R10 source shadow fixture: receiver excludes transmission shells, which belong to R10B.
        QJsonArray shadows;
        for (bool night : { false, true })
            for (int map : { 1024, 2048, 4096 }) {
                auto in = fixture({ 960, 720 }, night, true, "square");
                in.shadowDiagnostic = true;
                in.shadowSize = map;
                in.scene.camera.position = { 6, 4, 6 };
                in.scene.view.setToIdentity();
                in.scene.view.lookAt({ 6, 4, 6 }, { 0, 1.85f, 0 }, { 0, 1, 0 });
                for (float z : { 0.0f, .62f, -.62f }) {
                    QMatrix4x4 m;
                    m.translate(0, 0, z);
                    in.entities.append(m);
                }
                in.shadows = false;
                auto off = capture(in, { 960, 720 });
                in.shadows = true;
                auto on = capture(in, { 960, 720 });
                int pixels = 0;
                double minimum = 1;
                for (int y = 0; y < 720; ++y)
                    for (int x = 0; x < 960; ++x) {
                        const auto a = off.color.pixelColor(x, y), b = on.color.pixelColor(x, y);
                        if (a.redF() > .99 && a.greenF() > .99 && a.blueF() > .99
                            && a.red() - b.red() > 2) {
                            ++pixels;
                            minimum = std::min(minimum, double(b.redF()));
                        }
                    }
                check(pixels > 100 && minimum >= .675 && minimum <= .70,
                      "Source shadow strength/caster projection failed");
                const auto label = QString("shadow-%1-%2").arg(night ? "night" : "day").arg(map);
                off.color.save(output + "/" + label + "-off.png");
                on.color.save(output + "/" + label + "-on.png");
                on.shadow.save(output + "/" + label + "-target.png");
                QJsonObject shadowReport{ { "scene", label },
                                          { "changedReceiverPixels", pixels },
                                          { "minimumFactor", minimum },
                                          { "metadata", on.metadata } };
                const int sourceCase = (night ? 15 : 9) + (map == 1024 ? 0 : map == 2048 ? 2 : 4);
                const auto path = QString(FIXTURE_ROOT) + "/docs/media/render-backend/web/";
                QImage webOff(path + QString("%1.png").arg(sourceCase, 3, 10, QChar('0'))),
                        webOn(path + QString("%1.png").arg(sourceCase + 1, 3, 10, QChar('0')));
                check(webOff.size() == off.color.size() && webOn.size() == on.color.size(),
                      "Missing same-input frozen shadow witness");
                double error = 0;
                int count = 0, webChanged = 0;
                for (int y = 0; y < 720; ++y)
                    for (int x = 0; x < 960; ++x) {
                        // Only the isolated white receiver common to both captures. Shell shading is R10B.
                        if (off.color.pixelColor(x, y).red() < 254
                            || webOff.pixelColor(x, y) != QColor(Qt::white))
                            continue;
                        const int a = on.color.pixelColor(x, y).red(),
                                  b = webOn.pixelColor(x, y).red();
                        error += std::abs(a - b);
                        ++count;
                        if (b < 252)
                            ++webChanged;
                    }
                check(count > 1000 && webChanged > 100, "Empty frozen receiver comparison");
                shadowReport["sourceReceiverMAE"] = error / count;
                shadowReport["sourceReceiverPixels"] = count;
                shadowReport["sourceCase"] = sourceCase + 1;
                check(error / count <= 6, "Source shadow receiver does not match effective filter");
                shadows.append(shadowReport);
            }
        item->captured = { };
        item->setVisible(false);
        window.hide();
        frames(window, 0);
        QJsonObject report{ { "round", "R10A" },
                            { "windowPolicy", verification::windowPolicy(window) },
                            { "configuration", "Release" },
                            { "passed", true },
                            { "qt", qVersion() },
                            { "cases", cases },
                            { "resize", sizes },
                            { "resizeBurst", 30 },
                            { "sameRequestCoverReplacement", true },
                            { "finalResize", finalResize.metadata },
                            { "shadows", shadows },
                            { "worstChannelMAE", worstMAE },
                            { "knownPlaneDepthError", worstDepth },
                            { "maxCornerProjectionPixels", worstProjection },
                            { "productImplementationReady", false },
                            { "materialAcceptanceRound", "R10B" },
                            { "postprocessAndTileBudgetRound", "R10C" } };
        QSaveFile file(output + "/runtime.json");
        check(verification::windowPolicySatisfied(window),
              "GPU review window violated bottom/no-focus policy");
        check(file.open(QIODevice::WriteOnly), "Cannot open report");
        file.write(QJsonDocument(report).toJson());
        check(file.commit(), "Cannot commit report");
        std::puts("R10A D3D11 opaque/normal/depth, source shadow and resize checks passed.");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "R10A: %s\n", e.what());
        return 1;
    }
}
