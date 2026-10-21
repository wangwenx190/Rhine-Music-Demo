#include "Require.h"
#include "JsonFile.h"
#include "ReviewApplication.h"
#include "RhiShadowAudit.h"
#include "RhiReviewInput.h"
#include "GraphicsBackend.h"
#include "GeometryAudit.h"
#include <QGuiApplication>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QtEndian>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <memory>
#include <utility>

using namespace Qt::StringLiterals;
using namespace yalahoo;
using namespace yalahoo::rhi;
using namespace yalahoo::review;

namespace {
// Independent scalar transcription of locked Three PCF: compare first, then bilinear filter.
// The input is this run's R32F canonical geometry depth, never an old rendered reference.
QJsonObject validateProjection(const Input &input, const Capture &capture)
{
    static const auto geometry = assets::readGlb(assets::readFile(
            (QString::fromUtf8(FIXTURE_ROOT)) + "/resources/models/music-case-v040.glb"_L1));
    require(geometry[2].name == "Optical_Diffuser"_L1, "Shadow source caster identity changed");
    QMatrix4x4 view, projection;
    view.lookAt({ -6, 14, -5 }, { 0, 0, 0 }, { 0, 1, 0 });
    projection.ortho(-16, 16, -15, 15, .1f, 45);
    struct Triangle final
    {
        QVector3D a, b, c;
        double area;
    };
    QVector<Triangle> triangles;
    float left = 1, right = 0, bottom = 1, top = 0;
    const auto cross = [](QVector3D a, QVector3D b) {
        return double(a.x()) * b.y() - double(a.y()) * b.x();
    };
    const auto &mesh = geometry[2];
    for (const auto &model : input.entities) {
        const auto light = projection * view;
        const auto point = [&](quint32 index) {
            const auto &v = mesh.vertices[index];
            return light.map(model.map(QVector3D(v[0], v[1], v[2]))) * .5f
                    + QVector3D(.5f, .5f, .5f);
        };
        for (int i = 0; i < mesh.indices.size(); i += 3) {
            Triangle t{ point(mesh.indices[i]), point(mesh.indices[i + 1]),
                        point(mesh.indices[i + 2]), 0 };
            t.area = cross(t.b - t.a, t.c - t.a);
            if (t.area >= -1e-12)
                continue; // Source FrontSide material: render its back faces into the shadow map.
            triangles.append(t);
            for (auto p : { t.a, t.b, t.c }) {
                left = std::min(left, p.x());
                right = std::max(right, p.x());
                bottom = std::min(bottom, p.y());
                top = std::max(top, p.y());
            }
        }
    }
    const bool yUp = capture.metadata[u"shadowState"_s].toObject()[u"framebufferYUp"_s].toBool();
    int points = 0;
    double worst = 0, worstPixels = 0;
    for (int j = 0; j < 9; ++j)
        for (int i = 0; i < 9; ++i) {
            const int x = int((left + (right - left) * (i + .5) / 9) * input.shadowSize);
            const int y = int((bottom + (top - bottom) * (j + .5) / 9) * input.shadowSize);
            const QVector3D p((x + .5f) / input.shadowSize, (y + .5f) / input.shadowSize, 0);
            double expected = 1;
            const Triangle *winner = nullptr;
            for (const auto &t : triangles) {
                const double b = cross(p - t.a, t.c - t.a) / t.area;
                const double c = cross(t.b - t.a, p - t.a) / t.area;
                const double a = 1 - b - c;
                const double depth = a * t.a.z() + b * t.b.z() + c * t.c.z();
                if (a > 1e-5 && b > 1e-5 && c > 1e-5 && depth < expected) {
                    expected = depth;
                    winner = &t;
                }
            }
            if (expected == 1)
                continue;
            const int row = yUp ? y : input.shadowSize - 1 - y;
            float actual;
            std::memcpy(&actual, capture.shadowDepth.constData() + (row * input.shadowSize + x) * 4,
                        4);
            require(winner, "Missing source caster triangle");
            const auto ab = winner->b - winner->a, ac = winner->c - winner->a;
            const double dx = (ab.z() * ac.y() - ac.z() * ab.y()) / winner->area / input.shadowSize;
            const double dy = (ab.x() * ac.z() - ac.x() * ab.z()) / winner->area / input.shadowSize;
            const double slope = std::hypot(dx, dy);
            const double error = std::abs(expected - actual);
            // Tilted raster planes use subpixel vertex/interpolation precision. Express their
            // error in physical shadow texels, using the established two-pixel projection gate.
            // Constant-depth planes have no position/depth conversion and retain 2e-6 depth.
            if (slope < 1e-10)
                require(error < 2e-6, "Constant source caster depth is incorrect");
            else
                worstPixels = std::max(worstPixels, error / slope);
            worst = std::max(worst, std::abs(expected - actual));
            ++points;
        }
    std::fprintf(stderr, "Caster %d projection points=%d depth=%.9g texels=%.9g\n",
                 input.shadowSize, points, worst, worstPixels);
    require(points > 5 && worstPixels <= 2,
            "Shadow caster projection/backface depth differs from source geometry");
    return { { u"points"_s, points },
             { u"maxDepthError"_s, worst },
             { u"maxProjectionTexels"_s, worstPixels } };
}
double pcf(const Capture &capture, double u, double v, double depth, int size)
{
    const bool yUp = capture.metadata[u"shadowState"_s].toObject()[u"framebufferYUp"_s].toBool();
    const double x = u * size - .5, y = v * size - .5;
    const int ix = int(std::floor(x)), iy = int(std::floor(y));
    const double fx = x - ix, fy = y - iy;
    double sum = 0;
    for (int dy = 0; dy < 2; ++dy)
        for (int dx = 0; dx < 2; ++dx) {
            const int xx = std::clamp(ix + dx, 0, size - 1);
            int yy = std::clamp(iy + dy, 0, size - 1);
            // Canonical UV is bottom-left; retain the native readback's row order.
            if (!yUp)
                yy = size - 1 - yy;
            float stored;
            std::memcpy(&stored, capture.shadowDepth.constData() + (yy * size + xx) * 4, 4);
            require(std::isfinite(stored), "Nonfinite source shadow depth");
            sum += (depth <= stored ? 1 : 0) * (dx ? fx : 1 - fx) * (dy ? fy : 1 - fy);
        }
    return sum;
}
double shadowReference(const Capture &capture, QVector3D world, int x, int y, QSize size)
{
    const auto state = capture.metadata[u"shadowState"_s].toObject();
    // Independently construct the authored light camera, rather than reuse RhiApi helpers.
    QMatrix4x4 view, projection;
    view.lookAt({ -6, 14, -5 }, { 0, 0, 0 }, { 0, 1, 0 });
    projection.ortho(-16, 16, -15, 15, .1f, 45);
    const auto clip = projection * view * QVector4D(world + QVector3D(0, .035f, 0), 1);
    const double u = clip.x() / clip.w() * .5 + .5;
    const double v = clip.y() / clip.w() * .5 + .5;
    const double depth = clip.z() / clip.w() * .5 + .5 - .0003;
    if (state[u"enabled"_s].toInt() == 0 || u < 0 || u > 1 || v < 0 || v > 1 || depth > 1)
        return 1;
    // Match source float arithmetic for IGN's high frequency phase, scalar PCF stays double.
    const float noise = (x + .5f) * .06711056f + (size.height() - y - .5f) * .00583715f;
    const float n = 52.9829189f * (noise - std::floor(noise));
    const double phi = (n - std::floor(n)) * 6.28318530718;
    double sum = 0;
    const int mapSize = state[u"size"_s].toInt();
    for (int i = 0; i < 5; ++i) {
        const double radius = std::sqrt((i + .5) / 5) * 4 / mapSize;
        const double theta = i * 2.399963229728653 + phi;
        sum += pcf(capture, u + std::cos(theta) * radius, v + std::sin(theta) * radius, depth,
                   mapSize);
    }
    return 1 - .32 + .32 * sum * .2;
}
#include "RhiFloorAudit.h"

QJsonObject validateMath(const Input &input, const Capture &capture, QSize size)
{
    const int map = input.shadowSize;
    require(capture.shadowDepth.size() == map * map * 4, "Missing unquantized shadow readback");
    if (!input.shadows)
        for (qsizetype offset = 0; offset < capture.shadowDepth.size(); offset += 4) {
            float value;
            std::memcpy(&value, capture.shadowDepth.constData() + offset, 4);
            require(value == 1, "Disabled caster cache did not clear canonical depth");
        }
    const auto inverse = (input.scene.projection * input.scene.view).inverted();
    int points = 0, shadowed = 0, filtered = 0;
    double worst = 0;
    for (int y = 3; y < size.height(); y += 7)
        for (int x = 3; x < size.width(); x += 7) {
            const float nx = (x + .5f) / size.width() * 2 - 1;
            const float ny = 1 - (y + .5f) / size.height() * 2;
            const auto a = inverse.map(QVector3D(nx, ny, -1));
            const auto b = inverse.map(QVector3D(nx, ny, 1));
            const auto delta = b - a;
            if (std::abs(delta.y()) < 1e-7)
                continue;
            const float t = (-.05f - a.y()) / delta.y();
            const auto world = a + delta * t;
            if (t < 0 || t > 1 || std::abs(world.x()) >= 5.95 || std::abs(world.z()) >= 5.95)
                continue;
            const double expected = shadowReference(capture, world, x, y, size);
            const double actual = capture.color.pixelColor(x, y).redF();
            worst = std::max(worst, std::abs(expected - actual));
            ++points;
            shadowed += expected < .99;
            filtered += expected > .69 && expected < .99;
        }
    std::fprintf(stderr, "Shadow %d math points=%d occluded=%d filtered=%d max=%.9f\n", map, points,
                 shadowed, filtered, worst);
    // RGBA16F -> RGBA8 diagnostics plus float matrix/IGN and hardware PCF rounding.
    require(points > 1000 && worst <= 2.0 / 255, "Shadow PCF differs from source scalar math");
    if (input.shadows)
        require(shadowed > 20 && filtered > 5, "Shadow math did not exercise PCF edges");
    return { { u"points"_s, points },
             { u"occluded"_s, shadowed },
             { u"filtered"_s, filtered },
             { u"maxError"_s, worst } };
}

struct ShadowSubmission final
{
    QJsonObject profile;
    QImage color;
};
ShadowSubmission submitDepthOnly(RhiCaptureHarness &driver, Input input, QSize size)
{
    struct Mailbox final
    {
        QMutex mutex;
        QJsonObject profile;
    };
    const auto mailbox = std::make_shared<Mailbox>();
    auto captured = std::move(driver.item->captured);
    driver.item->captured = { };
    driver.item->profiled = [mailbox](QJsonObject profile) {
        const QMutexLocker lock(&mailbox->mutex);
        mailbox->profile = std::move(profile);
        return true;
    };
    input.profile = true;
    driver.replay(1, [&](int) {
        driver.submitWithoutCapture(input, size);
    });
    driver.item->profiled = { };
    driver.item->captured = std::move(captured);
    QJsonObject profile;
    {
        const QMutexLocker lock(&mailbox->mutex);
        profile = mailbox->profile;
    }
    require(!profile.isEmpty(), "Missing depth-only shadow resource snapshot");
    require(verification::windowPolicySatisfied(*driver.window), "Depth-only window policy failed");
    return { profile, driver.surface->grab() };
}
qint64 checkShadowAttachments(const QJsonObject &resources, int size, bool diagnostic)
{
    const auto checkEntries = [&](const QJsonArray &entries, const QString &depthName,
                                  const QString &colorName) {
        int depths = 0, colors = 0;
        for (const auto &value : entries) {
            const auto row = value.toObject();
            const auto name = row[u"name"_s].toString();
            if (name != depthName && name != colorName)
                continue;
            const bool depth = name == depthName;
            depths += int(depth);
            colors += int(!depth);
            require(row[u"format"_s].toInt() == int(depth ? QRhiTexture::D32F : QRhiTexture::R32F)
                            && row[u"width"_s].toInt() == size && row[u"height"_s].toInt() == size
                            && row[u"samples"_s].toInt() == 1
                            && row[u"estimatedBytes"_s].toInteger() == qint64(size) * size * 4,
                    "Shadow attachment descriptor or byte estimate is incorrect");
        }
        require(depths == 1 && colors == int(diagnostic),
                "Shadow diagnostic color escaped its capture lifetime");
    };
    checkEntries(resources[u"entries"_s].toArray(), u"shadow-hardware-depth"_s,
                 u"shadow-color-resolve"_s);
    checkEntries(resources[u"preallocationPlan"_s].toObject()[u"entries"_s].toArray(),
                 u"shadow-depth"_s, u"shadow-readback-color"_s);
    const auto id = resources[u"shadowDepthResourceId"_s].toInteger();
    require(id > 0, "Missing actual shadow depth resource identity");
    return id;
}
QJsonArray checkShadowAttachmentLifetime(RhiCaptureHarness &driver, const QJsonObject &source)
{
    QJsonArray states;
    const QSize size(640, 480);
    for (int count : { 1, 8 }) {
        auto input = fixture(source);
        input.entities.clear();
        input.entityIds.clear();
        for (int index = 0; index < count; ++index) {
            QMatrix4x4 model;
            model.translate((index % 3 - 1) * 1.5f, 0, (index / 3 - 1) * .62f);
            input.entities.append(model);
            input.entityIds.append(index + 1);
        }
        input.shadowDiagnostic = true;
        input.captureShadowDepth = true;
        input.captureSamples = 1;
        input.shadowSize = 1024;
        input.shadows = true;
        const auto production = submitDepthOnly(driver, input, size);
        const auto depthId = checkShadowAttachments(production.profile[u"resources"_s].toObject(),
                                                    input.shadowSize, false);
        require(production.profile[u"shadowRendered"_s].toBool(),
                "Depth-only shadow did not rasterize its initial caster state");
        auto reference = driver.capture(input, size);
        checkSubmittedShadow(input, reference);
        require(checkShadowAttachments(reference.metadata[u"resources"_s].toObject(),
                                       input.shadowSize, true)
                        == depthId,
                "Enabling diagnostic color replaced the sampled shadow depth");
        validateProjection(input, reference);
        validateMath(input, reference, size);
        // The same-run canonical map supplies compare-first PCF math, independently checked
        // against source triangles above. Inspect the actual depth-only Qt output with it.
        reference.color = production.color;
        const auto math = validateMath(input, reference, size);
        const auto restored = submitDepthOnly(driver, input, size);
        require(checkShadowAttachments(restored.profile[u"resources"_s].toObject(),
                                       input.shadowSize, false)
                                == depthId
                        && !restored.profile[u"shadowRendered"_s].toBool(),
                "Disabling capture replaced or redrew cached shadow depth");
        reference.color = restored.color;
        validateMath(input, reference, size);
        input.entities[0].translate(.1f, 0, 0);
        const auto moved = submitDepthOnly(driver, input, size);
        require(moved.profile[u"shadowRendered"_s].toBool(),
                "Depth-only caster motion did not update shadow depth");
        reference = driver.capture(input, size);
        reference.color = moved.color;
        validateMath(input, reference, size);
        input.shadows = false;
        const auto disabled = submitDepthOnly(driver, input, size);
        require(disabled.profile[u"shadowRendered"_s].toBool(),
                "Disabling depth-only shadows did not clear the map");
        input.entities[0].translate(.1f, 0, 0);
        const auto cached = submitDepthOnly(driver, input, size);
        require(!cached.profile[u"shadowRendered"_s].toBool(),
                "Disabled depth-only caster motion redrew the map");
        input.shadows = true;
        input.shadowSize = 512;
        const auto resized = submitDepthOnly(driver, input, size);
        require(resized.profile[u"shadowRendered"_s].toBool()
                        && checkShadowAttachments(resized.profile[u"resources"_s].toObject(),
                                                  input.shadowSize, false)
                                != depthId,
                "Depth-only shadow resize did not replace and render its depth target");
        reference = driver.capture(input, size);
        validateProjection(input, reference);
        reference.color = resized.color;
        validateMath(input, reference, size);
        states.append(QJsonObject{ { u"entities"_s, count },
                                   { u"depthOnly"_s, production.profile },
                                   { u"captureDisabled"_s, restored.profile },
                                   { u"resized"_s, resized.profile },
                                   { u"sourcePcf"_s, math } });
    }
    return states;
}

struct ShadowTransition final
{
    const char *label;
    bool redraw, replaceShadow, replaceMain;
};
Capture checkShadowTransition(RhiCaptureHarness &driver, const Input &input, QSize size,
                              const Capture &previous, ShadowTransition transition)
{
    const auto current = driver.capture(input, size);
    checkSubmittedShadow(input, current);
    const auto state = current.metadata[u"shadowState"_s].toObject();
    const auto oldState = previous.metadata[u"shadowState"_s].toObject();
    if (state[u"rendered"_s].toBool() != transition.redraw || !state[u"bindingMatches"_s].toBool())
        std::fprintf(stderr, "Shadow transition %s expected redraw=%d actual=%s\n",
                     transition.label, transition.redraw,
                     QJsonDocument(state).toJson(QJsonDocument::Compact).constData());
    require(state[u"rendered"_s].toBool() == transition.redraw
                    && state[u"bindingMatches"_s].toBool(),
            "Shadow dirty state or sampled attachment is incorrect");
    require((state[u"resourceRevision"_s] != oldState[u"resourceRevision"_s])
                    == transition.replaceShadow,
            "Shadow resource replacement escaped its compatibility key");
    require((current.metadata[u"resourceGeneration"_s]
             != previous.metadata[u"resourceGeneration"_s])
                    == transition.replaceMain,
            "Screen target replacement escaped its compatibility key");
    if (!transition.replaceMain)
        require(current.metadata[u"postprocessRevision"_s]
                        == previous.metadata[u"postprocessRevision"_s],
                "A shadow-only change rebuilt the postprocess graph");
    if (!transition.redraw) {
        require(state[u"separateCasterUniforms"_s].toInt() == 0,
                "A clean shadow uploaded separate caster uniforms");
        require(!current.shadowDepth.isEmpty() && current.shadowDepth == previous.shadowDepth,
                "A clean shadow lost its retained depth contents");
    }
    if (!input.shadows && transition.redraw)
        for (qsizetype offset = 0; offset < current.shadowDepth.size(); offset += sizeof(float)) {
            float depth;
            std::memcpy(&depth, current.shadowDepth.constData() + offset, sizeof(depth));
            require(depth == 1, "A disabled shadow target retained caster depth");
        }
    if (QByteArray(transition.label) == "caster")
        require(current.shadowDepth != previous.shadowDepth,
                "A moving caster did not change actual shadow depth");
    return current;
}
QJsonArray checkShadowInvalidation(RhiCaptureHarness &driver, const QJsonObject &source,
                                   const QString &output)
{
    QJsonArray states;
    for (int count : { 1, 8 }) {
        auto input = fixture(source);
        input.entities.clear();
        input.entityIds.clear();
        for (int index = 0; index < count; ++index) {
            QMatrix4x4 model;
            model.translate(index * .4f, 0, 0);
            input.entities.append(model);
            input.entityIds.append(index + 1);
        }
        input.shells = true;
        input.postprocess = true;
        input.aoSamples = 64;
        input.bokeh = true;
        input.smaa = true;
        input.shadows = true;
        input.captureShadowDepth = true;
        input.shadowSize = 256;
        input.captureSamples = 1;
        QSize size(193, 129);
        auto previous = driver.capture(input, size);
        // Establish the current caster state before comparing later transitions.
        require(previous.metadata[u"shadowState"_s].toObject()[u"bindingMatches"_s].toBool(),
                "Initial shadow attachment is not bound to the scene");
        const auto capture = [&](ShadowTransition transition) {
            previous = checkShadowTransition(driver, input, size, previous, transition);
            states.append(QJsonObject{
                    { u"transition"_s, QString::fromUtf8(transition.label) },
                    { u"entities"_s, count },
                    { u"resourceGeneration"_s, previous.metadata[u"resourceGeneration"_s] },
                    { u"postprocessRevision"_s, previous.metadata[u"postprocessRevision"_s] },
                    { u"shadow"_s, previous.metadata[u"shadowState"_s] },
                    { u"resources"_s, previous.metadata[u"resources"_s] } });
        };
        capture({ "unchanged", false, false, false });
        size = { 257, 193 };
        capture({ "resize", false, false, true });
        input.shells = false;
        capture({ "opaque-layout", false, false, true });
        input.shells = true;
        capture({ "shell-layout", false, false, true });
        input.captureSamples = 4;
        capture({ "msaa", false, false, true });
        input.scene.view.rotate(.5f, 0, 1, 0);
        input.keyColor *= .9f;
        input.focus += 1;
        capture({ "camera-light-focus", false, false, false });
        input.cover.fill(Qt::blue);
        capture({ "cover", false, false, false });
        input.shadowSize = 512;
        capture({ "shadow-size", true, true, false });
        capture({ "shadow-size-stable", false, false, false });
        input.entities[0].translate(.1f, 0, 0);
        capture({ "caster", true, false, false });
        input.shadows = false;
        capture({ "disable", true, false, false });
        input.entities[0].translate(.1f, 0, 0);
        capture({ "disabled-caster", false, false, false });
        input.shadowSize = 256;
        capture({ "disabled-shadow-size", true, true, false });
        capture({ "disabled-shadow-size-stable", false, false, false });
        size = { 193, 129 };
        capture({ "disabled-resize", false, false, true });
        input.shadows = true;
        capture({ "enable", true, false, false });
        capture({ "enabled-stable", false, false, false });
        require(previous.color.save(output + u"/casters-%1.png"_s.arg(count)),
                "Cannot save own shadow invalidation output");
    }
    return states;
}
auto runShadowAudit(int &argc, char **&argv)
{
    if (argc != 2 && argc != 3)
        return 2;
    try {
        const QString output = QString::fromLocal8Bit(argv[1]);
        require(QDir().mkpath(output), "Cannot create shadow output");
        QFile file((QString::fromUtf8(FIXTURE_ROOT)) + "/tests/fixtures/rhi-shell/visual.json"_L1);
        require(file.open(QIODevice::ReadOnly), "Missing anonymous shadow fixture");
        auto c = QJsonDocument::fromJson(file.readAll())
                         .object()[u"cases"_s]
                         .toArray()[0]
                         .toObject();
        c[u"position"_s] = QJsonArray{ 6, 4, 6 };
        c[u"clipNear"_s] = .1;
        c[u"clipFar"_s] = 100;
        c[u"layer"_s] = u"paper"_s;
        const QByteArray option = argc == 3 ? QByteArray(argv[2]) : QByteArray();
        require(option.isEmpty() || option == "--invalidation-only"
                        || option == "--attachments-only",
                "Unsupported shadow audit option");
        RhiCaptureHarness driver;
        const auto invalidation = option == "--attachments-only"
                ? checkShadowAttachmentLifetime(driver, c)
                : checkShadowInvalidation(driver, c, output);
        if (argc == 3) {
            require(yalahoo::writeJsonFile(
                            output + "/runtime.json"_L1,
                            QJsonDocument(
                                    QJsonObject{ { u"passed"_s, true },
                                                 { u"states"_s, invalidation },
                                                 { u"offscreen"_s, driver.surface->diagnostics() },
                                                 { u"windowPolicy"_s,
                                                   verification::windowPolicy(*driver.window) } })),
                    "Cannot write shadow invalidation report");
            return 0;
        }
        const auto floors = runFloorAudit(driver, c, output);
        QJsonArray results, lifecycle;
        Input last;
        const QSize size(640, 480);
        c[u"width"_s] = size.width();
        c[u"height"_s] = size.height();
        for (bool night : { false, true }) {
            c[u"day"_s] = !night;
            for (int count : { 1, 7, 8, 9 }) {
                if (night && count != 9)
                    continue;
                auto input = fixture(c);
                input.entities.clear();
                for (int i = 0; i < count; ++i) {
                    QMatrix4x4 model;
                    model.translate((i % 3 - 1) * 1.5f, 0, (i / 3 - 1) * .62f);
                    model.rotate(i * 2.3f, 0, 1, 0);
                    input.entities.append(model);
                }
                input.shadowDiagnostic = true;
                input.captureShadowDepth = true;
                input.captureSamples = 1;
                for (int map : { 1024, 2048, 4096 }) {
                    if (count != 9 && map != 1024)
                        continue;
                    input.shadowSize = map;
                    input.shadows = true;
                    const auto capture = driver.capture(input, size);
                    checkSubmittedShadow(input, capture);
                    auto math = validateMath(input, capture, size);
                    math[u"projection"_s] = validateProjection(input, capture);
                    auto same = driver.capture(input, size);
                    require(!same.metadata[u"shadowState"_s].toObject()[u"rendered"_s].toBool()
                                    && same.metadata[u"shadowState"_s]
                                                    .toObject()[u"separateCasterUniforms"_s]
                                                    .toInt()
                                            == 0,
                            "Unchanged caster pool redrew or reuploaded shadow uniforms");
                    math[u"initialUniformUploadBytes"_s] =
                            capture.metadata[u"uniformUploadBytes"_s];
                    math[u"cachedUniformUploadBytes"_s] = same.metadata[u"uniformUploadBytes"_s];
                    const auto generation = same.metadata[u"resourceGeneration"_s];
                    input.scene.view.rotate(.5f, 0, 1, 0);
                    input.keyColor *= .9f;
                    same = driver.capture(input, size);
                    require(!same.metadata[u"shadowState"_s].toObject()[u"rendered"_s].toBool()
                                    && same.metadata[u"resourceGeneration"_s] == generation,
                            "Receiver camera/light color invalidated shadow resources");
                    input.entities[0].translate(.1f, 0, 0);
                    same = driver.capture(input, size);
                    require(same.metadata[u"shadowState"_s].toObject()[u"rendered"_s].toBool()
                                    && same.metadata[u"resourceGeneration"_s] == generation,
                            "Animated caster failed to update cached shadow");
                    input.shadows = false;
                    const auto off = driver.capture(input, size);
                    checkSubmittedShadow(input, off);
                    validateMath(input, off, size);
                    input.shadows = true;
                    const auto restored = driver.capture(input, size);
                    checkSubmittedShadow(input, restored);
                    validateMath(input, restored, size);
                    math[u"night"_s] = night;
                    math[u"count"_s] = count;
                    math[u"metadata"_s] = capture.metadata;
                    results.append(math);
                    if (count == 9 && map == 2048)
                        require(capture.color.save(output
                                                   + (night ? "/night.png"_L1 : "/day.png"_L1)),
                                "Cannot save own shadow output");
                    last = input;
                }
            }
        }
        last.shadowSize = 1024;
        for (int cycle = 0; cycle < 10; ++cycle) {
            driver.rebuild();
            const auto capture = driver.capture(last, size);
            require(driver.rebuilt(), "Shadow lifecycle did not invalidate real scene graph");
            checkSubmittedShadow(last, capture);
            lifecycle.append(validateMath(last, capture, size));
        }
        require(yalahoo::writeJsonFile(output + "/runtime.json"_L1,
                                       QJsonDocument(QJsonObject{
                                               { u"round"_s, u"R19D3A"_s },
                                               { u"passed"_s, true },
                                               { u"qt"_s, QString::fromUtf8(qVersion()) },
                                               { u"cases"_s, results },
                                               { u"floors"_s, floors },
                                               { u"invalidation"_s, invalidation },
                                               { u"rebuilds"_s, lifecycle },
                                               { u"offscreen"_s, driver.surface->diagnostics() },
                                               { u"windowPolicy"_s,
                                                 verification::windowPolicy(*driver.window) } })),
                "Cannot write shadow report");
        std::printf("Shadow source math/cache passed %lld states and ten rebuilds\n",
                    results.size());
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
}
int main(int argc, char **argv)
{
    return yalahoo::review::runApplication(argc, argv, [&](QGuiApplication &) {
        return runShadowAudit(argc, argv);
    });
}
