// Checks production floor projection, lighting and shadow reception. Analytic expectations use
// the actual camera and distinguish world-space from pixel tolerances.

// Included by RhiShadowAudit after its independent source PCF reference.
// All values below derive from Three 0.183.2 MeshStandardMaterial, not rendered references.
struct FloorLookup final
{
    QByteArray pixels;
    int width, height;
    FloorLookup(const char *path, int w, int h) : width(w), height(h)
    {
        QFile file(QString::fromUtf8(path));
        check(file.open(QIODevice::ReadOnly), "Missing floor source lookup");
        pixels = file.readAll();
        check(pixels.size() == w * h * 16, "Invalid floor source lookup");
    }
    QVector3D sample(double u, double v) const
    {
        const double x = u * width - .5, y = v * height - .5;
        const int ix = int(std::floor(x)), iy = int(std::floor(y));
        const double fx = x - ix, fy = y - iy;
        QVector3D result;
        for (int j = 0; j < 2; ++j)
            for (int i = 0; i < 2; ++i) {
                const int xx = std::clamp(ix + i, 0, width - 1);
                const int yy = std::clamp(iy + j, 0, height - 1);
                for (int c = 0; c < 3; ++c) {
                    const auto bits = qFromLittleEndian<quint32>(pixels.constData()
                                                                 + ((yy * width + xx) * 4 + c) * 4);
                    float value;
                    std::memcpy(&value, &bits, 4);
                    result[c] += float(value * (i ? fx : 1 - fx) * (j ? fy : 1 - fy));
                }
            }
        return result;
    }
};
QVector3D floorEnvironment(QVector3D direction, double roughness)
{
    static const FloorLookup room(":/rhi/environment/room-pmrem.rgba32f", 768, 1024);
    const auto a =
            QVector3D(std::abs(direction.x()), std::abs(direction.y()), std::abs(direction.z()));
    const int face = a.x() > a.z()
            ? (a.x() > a.y() ? (direction.x() > 0 ? 0 : 3) : (direction.y() > 0 ? 1 : 4))
            : (a.z() > a.y() ? (direction.z() > 0 ? 2 : 5) : (direction.y() > 0 ? 1 : 4));
    double u = 0, v = 0;
    switch (face) {
    case 0:
        u = direction.z() / a.x();
        v = direction.y() / a.x();
        break;
    case 1:
        u = -direction.x() / a.y();
        v = -direction.z() / a.y();
        break;
    case 2:
        u = -direction.x() / a.z();
        v = direction.y() / a.z();
        break;
    case 3:
        u = -direction.z() / a.x();
        v = direction.y() / a.x();
        break;
    case 4:
        u = -direction.x() / a.y();
        v = direction.z() / a.y();
        break;
    case 5:
        u = direction.x() / a.z();
        v = direction.y() / a.z();
        break;
    }
    // Floor only uses roughness .95 (reflection) and 1 (diffuse), the first source segment.
    check(roughness >= .8 && roughness <= 1, "Unexpected floor PMREM roughness");
    const double mip = (1 - roughness) * 5 - 2;
    const auto level = [&](int m) {
        const double filter = std::max(4 - m, 0), side = std::exp2(std::max(m, 4));
        double x = (.5 * u + .5) * (side - 2) + 1;
        double y = (.5 * v + .5) * (side - 2) + 1;
        y += face > 2 ? side : 0;
        x += (face % 3) * side + filter * 48;
        y += 4 * (256 - side);
        return room.sample(x / 768, y / 1024);
    };
    const int m = int(std::floor(mip));
    return level(m) * float(1 - (mip - m)) + level(m + 1) * float(mip - m);
}
QVector3D floorPoint(const Input &input, int x, int y, QSize size)
{
    // Analytic camera ray avoids subtracting two float inverse-projection endpoints.
    const auto camera = input.scene.camera.position;
    const auto back = (camera - input.scene.camera.aim).normalized();
    const auto right = cross({ 0, 1, 0 }, back).normalized();
    const auto up = cross(back, right);
    const double nx = (x + .5) * 2 / size.width() - 1;
    const double ny = 1 - (y + .5) * 2 / size.height();
    const auto ray = right * (nx / input.scene.projection(0, 0))
            + up * (ny / input.scene.projection(1, 1)) - back;
    return (camera + ray * ((-4.63 - camera.y) / ray.y)).qt();
}
QVector3D floorReference(const Input &input, QVector3D world, double shadow)
{
    static const FloorLookup dfg(":/rhi/environment/dfg.rgba32f", 16, 16);
    constexpr double pi = 3.141592653589793, r = .95, f0 = .04;
    const auto view = (input.scene.camera.position.qt() - world).normalized();
    const double nv = std::clamp(double(view.y()), 0., 1.);
    const auto fv = dfg.sample(r, nv);
    const double single = f0 * fv.x() + fv.y(), ems = 1 - fv.x() - fv.y();
    const double average = f0 + (1 - f0) * .047619;
    const double multi = single * average / (1 - ems * average) * ems;
    auto outgoing = input.floorColor * input.skyColor * float(input.lightIntensities.y() / pi);
    const auto direct = [&](QVector3D light, QVector3D energy) {
        const double nl = std::clamp(double(light.y()), 0., 1.);
        if (nl == 0)
            return QVector3D();
        const auto half = (view + light).normalized();
        const double nh = std::clamp(double(half.y()), 0., 1.);
        const double vh = std::clamp(double(QVector3D::dotProduct(view, half)), 0., 1.);
        const double aa = std::pow(r, 4);
        const double visibility = .5
                / std::max(nl * std::sqrt(aa + (1 - aa) * nv * nv)
                                   + nv * std::sqrt(aa + (1 - aa) * nl * nl),
                           1e-6);
        const double distribution = aa / (pi * std::pow(nh * nh * (aa - 1) + 1, 2));
        const double fresnel = f0 + (1 - f0) * std::exp2((-5.55473 * vh - 6.98316) * vh);
        const auto fl = dfg.sample(r, nl);
        const double sl = f0 * fl.x() + fl.y();
        const double compensation = ems * (1 - fl.x() - fl.y());
        const double brdf = fresnel * visibility * distribution
                + single * sl * average / (1 - compensation * average + 1e-6) * compensation;
        return energy * float(nl)
                * (input.floorColor / float(pi) + QVector3D(1, 1, 1) * float(brdf));
    };
    outgoing += direct(QVector3D(-6, 14, -5).normalized(),
                       input.keyColor * float(input.lightIntensities.x() * shadow));
    outgoing += direct(QVector3D(7, 8, -10).normalized(),
                       QVector3D(1, 1, 1) * input.lightIntensities.z());
    const auto delta = input.spotPosition - world;
    const double angle = QVector3D::dotProduct(
            delta.normalized(), (input.spotPosition - input.spotTarget).normalized());
    const double t = std::clamp((angle - std::cos(.32)) / (std::cos(.016) - std::cos(.32)), 0., 1.);
    outgoing += direct(delta.normalized(),
                       input.spotColor
                               * float(input.spotIntensity * t * t * (3 - 2 * t)
                                       / std::max(double(delta.lengthSquared()), .01)));
    const auto irradiance = floorEnvironment({ 0, 1, 0 }, 1) * input.environmentIntensity;
    const auto reflection = QVector3D(-view.x(), view.y(), -view.z());
    const float mix = float(std::pow(r, 4));
    const auto direction = (reflection * (1 - mix) + QVector3D(0, 1, 0) * mix).normalized();
    outgoing += input.floorColor * irradiance * float(1 - single - multi)
            + floorEnvironment(direction, r) * float(input.environmentIntensity * single)
            + irradiance * float(multi);
    if (input.fogEnabled) {
        const double depth = -(input.scene.view * QVector4D(world, 1)).z();
        const double f = std::clamp(
                (depth - input.scene.fogNear) / (input.scene.fogFar - input.scene.fogNear), 0., 1.);
        const float amount = float(f * f * (3 - 2 * f));
        outgoing = outgoing * (1 - amount) + input.background * amount;
    }
    return outgoing;
}
QJsonObject validateFloor(const Input &input, const Capture &capture, QSize size, bool math)
{
    checkSubmittedShadow(input, capture);
    const auto state = capture.metadata[u"productFloor"_s].toObject();
    check(state[u"enabled"_s].toBool() && state[u"kind"_s].toInt() == 6
                  && std::abs(state[u"roughness"_s].toDouble() - .95) < 1e-7
                  && state[u"opaqueIndices"_s].toInt() == 6 && !state[u"castsShadow"_s].toBool(),
          "Missing source floor draw/material or unexpected caster");
    check(capture.metadata[u"captureDrawKinds"_s].toArray().contains(6),
          "Floor omitted from capture");
    for (int c = 0; c < 3; ++c)
        check(state[u"color"_s].toArray()[c].toDouble() == input.floorColor[c],
              "Stale floor theme");
    int points = 0, occluded = 0;
    double worst = 0, worstDepth = 0, worstProjection = 0;
    for (int y = 5; y < size.height(); y += 13)
        for (int x = 5; x < size.width(); x += 13) {
            const auto world = floorPoint(input, x, y, size);
            const double depth = -(input.scene.view * QVector4D(world, 1)).z();
            if (std::abs(world.x()) > 99 || std::abs(world.z()) > 99
                || depth <= input.scene.nearPlane || depth >= input.scene.farPlane)
                continue;
            const auto pixel = capture.depth.pixelColor(x, y);
            const double actualDepth = input.scene.nearPlane
                    + (pixel.red() * 65536. + pixel.green() * 256. + pixel.blue()) / 16777215.
                            * (input.scene.farPlane - input.scene.nearPlane);
            // Skip nearer boxes; the isolated math scenes disable their color/geometry coverage.
            if (!math && actualDepth < depth - .001)
                continue;
            const double neighborDepth =
                    -(input.scene.view * QVector4D(floorPoint(input, x, y + 1, size), 1)).z();
            const double slope = std::abs(neighborDepth - depth);
            worstDepth = std::max(worstDepth, std::abs(depth - actualDepth));
            worstProjection = std::max(worstProjection, std::abs(depth - actualDepth) / slope);
            // The source projection contract is in physical pixels, including tilted planes.
            check(slope > 1e-8 && std::abs(depth - actualDepth) / slope <= 2,
                  "Floor source plane projection differs by more than two physical pixels");
            const auto normal = capture.normal.pixelColor(x, y);
            const auto n = input.scene.view.mapVector({ 0, 1, 0 }).normalized() * .5f
                    + QVector3D(.5f, .5f, .5f);
            check(std::abs(normal.redF() - n.x()) <= 1.0 / 255
                          && std::abs(normal.greenF() - n.y()) <= 1.0 / 255
                          && std::abs(normal.blueF() - n.z()) <= 1.0 / 255,
                  "Floor source normal changed");
            if (math) {
                const double shadow = shadowReference(capture, world, x, y, size);
                occluded += shadow < .99;
                const auto expected = floorReference(input, world, shadow);
                const auto actual = capture.color.pixelColor(x, y);
                for (int c = 0; c < 3; ++c) {
                    const double value = c == 0 ? actual.redF()
                            : c == 1            ? actual.greenF()
                                                : actual.blueF();
                    worst = std::max(worst,
                                     std::abs(value - std::clamp(double(expected[c]), 0., 1.)));
                }
            }
            ++points;
        }
    std::fprintf(stderr, "Floor %dx%d points=%d occluded=%d math=%d error=%.9g\n", size.width(),
                 size.height(), points, occluded, math, worst);
    if (math && input.shadows)
        check(occluded > 0, "Floor math did not exercise actual received shadows");
    check(points > 100 && worst <= 3.0 / 255, "Floor standard PBR/fog differs from source math");
    if (input.postprocess) {
        check(state[u"aoIndices"_s].toInt() == 6 && state[u"dofIndices"_s].toInt() == 6,
              "Floor omitted from post geometry submission");
        for (const auto *image : { &capture.aoDepth, &capture.dofDepth }) {
            check(!image->isNull(), "Missing actual floor AO/DOF attachment");
            const int x = image->width() / 2, y = image->height() * 3 / 4;
            const auto world = floorPoint(input, x, y, image->size());
            const double expected = -(input.scene.view * QVector4D(world, 1)).z();
            const auto p = image->pixelColor(x, y);
            const double actual = input.scene.nearPlane
                    + (p.red() * 65536. + p.green() * 256. + p.blue()) / 16777215.
                            * (input.scene.farPlane - input.scene.nearPlane);
            const double next =
                    -(input.scene.view * QVector4D(floorPoint(input, x, y + 1, image->size()), 1))
                             .z();
            check(std::abs(actual - expected) / std::abs(next - expected) <= 2,
                  "Floor omitted from actual AO/DOF geometry");
        }
    }
    return { { u"points"_s, points },
             { u"occluded"_s, occluded },
             { u"maxError"_s, worst },
             { u"maxDepthError"_s, worstDepth },
             { u"maxProjectionPixels"_s, worstProjection } };
}
QJsonArray runFloorAudit(RhiCaptureHarness &driver, QJsonObject c, const QString &output)
{
    QJsonArray results;
    Input last;
    for (QSize size : { QSize(320, 240), QSize(960, 720) }) {
        c[u"width"_s] = size.width();
        c[u"height"_s] = size.height();
        c[u"position"_s] = QJsonArray{ 6, 4, 9 };
        for (const auto &theme : { u"day"_s, u"night"_s, u"dusk"_s }) {
            c[u"day"_s] = theme != "night"_L1;
            auto input = fixture(c);
            input.scene.camera.aim = { 0, -4.63, 0 };
            input.scene.view.setToIdentity();
            input.scene.view.lookAt(input.scene.camera.position.qt(), { 0, -4.63f, 0 },
                                    { 0, 1, 0 });
            input.productFloor = true;
            input.floorColor = LightingPalette::floorColor(theme).qt();
            input.captureShadowDepth = true;
            input.captureFloorDepth = true;
            input.captureSamples = 1;
            input.rawOutput = true;
            input.showPaper = false;
            input.shells = false;
            input.poolFrame = true;
            input.entities.clear();
            input.entityIds.clear();
            // Real caster shadows reach the ground; color path isolates the floor.
            QMatrix4x4 caster;
            caster.translate(0, -3.5f, 0);
            input.entities.append(caster);
            input.entityIds.append(1);
            input.spotPosition = { 0, 3, 0 };
            input.spotTarget = { 0, -4.63f, 0 };
            input.spotIntensity = 8;
            input.areaIntensity = 0;
            input.shadows = true;
            input.shadowSize = 1024;
            input.fogEnabled = true;
            input.scene.fogNear = 10;
            input.scene.fogFar = 35;
            for (int map : { 1024, 2048, 4096 }) {
                if (theme == "dusk"_L1 && map != 1024)
                    continue;
                input.shadowSize = map;
                const auto capture = driver.capture(input, size);
                auto row = validateFloor(input, capture, size, true);
                row[u"map"_s] = map;
                row[u"theme"_s] = theme;
                row[u"metadata"_s] = capture.metadata;
                results.append(row);
                const auto generation = capture.metadata[u"resourceGeneration"_s];
                const auto same = driver.capture(input, size);
                check(!same.metadata[u"shadowState"_s].toObject()[u"rendered"_s].toBool(),
                      "Floor invalidated caster cache");
                input.floorColor *= .9f;
                const auto changed = driver.capture(input, size);
                check(changed.metadata[u"resourceGeneration"_s] == generation
                              && !changed.metadata[u"shadowState"_s]
                                          .toObject()[u"rendered"_s]
                                          .toBool(),
                      "Floor color rebuilt targets or shadow map");
                validateFloor(input, changed, size, true);
                input.shadows = false;
                validateFloor(input, driver.capture(input, size), size, true);
                input.shadows = true;
            }
            input.postprocess = true;
            input.aoSamples = 32;
            input.bokeh = true;
            input.captureSamples = 4;
            input.shadowSize = 1024;
            {
                auto capture = driver.capture(input, size);
                validateFloor(input, capture, size, false);
                check(capture.postprocessed.save(
                              output + u"/floor-%1-%2.png"_s.arg(size.width()).arg(theme)),
                      "Cannot save own floor output");
            }
            last = input;
        }
    }
    for (int cycle = 0; cycle < 10; ++cycle) {
        driver.rebuild();
        const auto capture = driver.capture(last, { 960, 720 });
        check(driver.rebuilt(), "Floor rebuild did not invalidate scene graph");
        validateFloor(last, capture, { 960, 720 }, false);
    }
    return results;
}
#include <QString>
using Qt::StringLiterals::operator""_s;
using Qt::StringLiterals::operator""_L1;
