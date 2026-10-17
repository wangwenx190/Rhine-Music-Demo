#include "RhiShadowAudit.h"
#include "RhiReviewInput.h"
#include "GraphicsBackend.h"
#include "GeometryAudit.h"
#include <QGuiApplication>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QtEndian>
#include <cmath>
#include <cstring>
#include <cstdio>

using namespace Qt::StringLiterals;
using namespace yalahoo;
using namespace yalahoo::rhi;
using namespace yalahoo::review;
namespace {
void check(bool ok, const char *why)
{
    RhiCaptureHarness::require(ok, why);
}
// Independent scalar transcription of locked Three PCF: compare first, then bilinear filter.
// The input is this run's R32F canonical geometry depth, never an old rendered reference.
QJsonObject validateProjection(const Input &input, const Capture &capture)
{
    static const auto geometry = assets::readGlb(assets::readFile(
            (QString::fromUtf8(FIXTURE_ROOT)) + "/resources/models/music-case-v040.glb"_L1));
    check(geometry[2].name == "Optical_Diffuser"_L1, "Shadow source caster identity changed");
    QMatrix4x4 view, projection;
    view.lookAt({ -6, 14, -5 }, { 0, 0, 0 }, { 0, 1, 0 });
    projection.ortho(-16, 16, -15, 15, .1f, 45);
    struct Triangle
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
            check(winner, "Missing source caster triangle");
            const auto ab = winner->b - winner->a, ac = winner->c - winner->a;
            const double dx = (ab.z() * ac.y() - ac.z() * ab.y()) / winner->area / input.shadowSize;
            const double dy = (ab.x() * ac.z() - ac.x() * ab.z()) / winner->area / input.shadowSize;
            const double slope = std::hypot(dx, dy);
            const double error = std::abs(expected - actual);
            // Tilted raster planes use subpixel vertex/interpolation precision. Express their
            // error in physical shadow texels, using the established two-pixel projection gate.
            // Constant-depth planes have no position/depth conversion and retain 2e-6 depth.
            if (slope < 1e-10)
                check(error < 2e-6, "Constant source caster depth is incorrect");
            else
                worstPixels = std::max(worstPixels, error / slope);
            worst = std::max(worst, std::abs(expected - actual));
            ++points;
        }
    std::fprintf(stderr, "Caster %d projection points=%d depth=%.9g texels=%.9g\n",
                 input.shadowSize, points, worst, worstPixels);
    check(points > 5 && worstPixels <= 2,
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
            check(std::isfinite(stored), "Nonfinite source shadow depth");
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
    check(capture.shadowDepth.size() == map * map * 4, "Missing unquantized shadow readback");
    if (!input.shadows)
        for (qsizetype offset = 0; offset < capture.shadowDepth.size(); offset += 4) {
            float value;
            std::memcpy(&value, capture.shadowDepth.constData() + offset, 4);
            check(value == 1, "Disabled caster cache did not clear canonical depth");
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
    check(points > 1000 && worst <= 2.0 / 255, "Shadow PCF differs from source scalar math");
    if (input.shadows)
        check(shadowed > 20 && filtered > 5, "Shadow math did not exercise PCF edges");
    return { { u"points"_s, points },
             { u"occluded"_s, shadowed },
             { u"filtered"_s, filtered },
             { u"maxError"_s, worst } };
}
}
int main(int argc, char **argv)
{
    verification::prepareApplication();
    if (!selectGraphicsBackend())
        return 2;
    QGuiApplication app(argc, argv);
    yalahoo::probeOptionalGraphicsBackend();
    if (argc != 2)
        return 2;
    try {
        const QString output = QString::fromLocal8Bit(argv[1]);
        check(QDir().mkpath(output), "Cannot create shadow output");
        QFile file((QString::fromUtf8(FIXTURE_ROOT)) + "/tests/fixtures/rhi-shell/visual.json"_L1);
        check(file.open(QIODevice::ReadOnly), "Missing anonymous shadow fixture");
        auto c = QJsonDocument::fromJson(file.readAll())
                         .object()[u"cases"_s]
                         .toArray()[0]
                         .toObject();
        c[u"position"_s] = QJsonArray{ 6, 4, 6 };
        c[u"clipNear"_s] = .1;
        c[u"clipFar"_s] = 100;
        c[u"layer"_s] = u"paper"_s;
        RhiCaptureHarness driver;
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
                    check(!same.metadata[u"shadowState"_s].toObject()[u"rendered"_s].toBool()
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
                    check(!same.metadata[u"shadowState"_s].toObject()[u"rendered"_s].toBool()
                                  && same.metadata[u"resourceGeneration"_s] == generation,
                          "Receiver camera/light color invalidated shadow resources");
                    input.entities[0].translate(.1f, 0, 0);
                    same = driver.capture(input, size);
                    check(same.metadata[u"shadowState"_s].toObject()[u"rendered"_s].toBool()
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
                        check(capture.color.save(output
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
            check(driver.rebuilt(), "Shadow lifecycle did not invalidate real scene graph");
            checkSubmittedShadow(last, capture);
            lifecycle.append(validateMath(last, capture, size));
        }
        QSaveFile report(output + "/runtime.json"_L1);
        check(report.open(QIODevice::WriteOnly), "Cannot open shadow report");
        report.write(QJsonDocument(QJsonObject{ { u"round"_s, u"R19D3A"_s },
                                                { u"passed"_s, true },
                                                { u"qt"_s, QString::fromUtf8(qVersion()) },
                                                { u"cases"_s, results },
                                                { u"floors"_s, floors },
                                                { u"rebuilds"_s, lifecycle },
                                                { u"offscreen"_s, driver.surface->diagnostics() },
                                                { u"windowPolicy"_s,
                                                  verification::windowPolicy(*driver.window) } })
                             .toJson());
        check(report.commit(), "Cannot write shadow report");
        std::printf("Shadow source math/cache passed %lld states and ten rebuilds\n",
                    results.size());
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
