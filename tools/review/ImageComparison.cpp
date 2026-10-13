#include "ImageComparison.h"
#include <QJsonArray>
#include <algorithm>
#include <cmath>
#include <limits>
namespace yalahoo {
QJsonObject compareImages(const QImage &actual, const QImage &expected, const QPolygonF &polygon,
                          bool full)
{
    if (actual.isNull() || expected.isNull() || actual.size() != expected.size()
        || polygon.size() < 3)
        return { { "valid", false } };
    const auto bounds = polygon.boundingRect()
                                .toAlignedRect()
                                .intersected(actual.rect())
                                .intersected(expected.rect());
    double error = 0, delta[3] = { 0, 0, 0 }, ssim = 0;
    int count = 0, windows = 0;
    // 11x11 Gaussian sigma=1.5, channels independently, source contract constants.
    double weights[11][11], total = 0;
    for (int y = -5; y <= 5; ++y)
        for (int x = -5; x <= 5; ++x) {
            double w = std::exp(-(x * x + y * y) / 4.5);
            weights[y + 5][x + 5] = w;
            total += w;
        }
    for (int y = bounds.top(); y <= bounds.bottom(); ++y)
        for (int x = bounds.left(); x <= bounds.right(); ++x) {
            if (!polygon.containsPoint({ x + .5, y + .5 }, Qt::OddEvenFill))
                continue;
            const auto a = actual.pixelColor(x, y), b = expected.pixelColor(x, y);
            double av[3] = { double(a.red()), double(a.green()), double(a.blue()) },
                   bv[3] = { double(b.red()), double(b.green()), double(b.blue()) };
            for (int c = 0; c < 3; ++c) {
                error += std::abs(av[c] - bv[c]);
                delta[c] += av[c] - bv[c];
            }
            ++count;
            if (!full)
                continue;
            if (x < 5 || y < 5 || x + 5 >= actual.width() || y + 5 >= actual.height())
                continue;
            // A window crossing a ROI boundary is not included in the region's SSIM.
            bool contained = true;
            for (int dy : { -5, 5 })
                for (int dx : { -5, 5 })
                    contained = contained
                            && polygon.containsPoint({ x + dx + .5, y + dy + .5 }, Qt::OddEvenFill);
            if (!contained)
                continue;
            for (int c = 0; c < 3; ++c) {
                double ma = 0, mb = 0, va = 0, vb = 0, cov = 0;
                for (int dy = -5; dy <= 5; ++dy)
                    for (int dx = -5; dx <= 5; ++dx) {
                        auto aa = actual.pixelColor(x + dx, y + dy),
                             bb = expected.pixelColor(x + dx, y + dy);
                        double a0 = c == 0 ? aa.red()
                                : c == 1   ? aa.green()
                                           : aa.blue(),
                               b0 = c == 0 ? bb.red()
                                : c == 1   ? bb.green()
                                           : bb.blue();
                        double w = weights[dy + 5][dx + 5] / total;
                        ma += w * a0;
                        mb += w * b0;
                        va += w * a0 * a0;
                        vb += w * b0 * b0;
                        cov += w * a0 * b0;
                    }
                va = std::max(0.0, va - ma * ma);
                vb = std::max(0.0, vb - mb * mb);
                cov -= ma * mb;
                ssim += ((2 * ma * mb + 6.5025) * (2 * cov + 58.5225))
                        / ((ma * ma + mb * mb + 6.5025) * (va + vb + 58.5225));
            }
            ++windows;
        }
    QJsonArray polygonJson;
    for (auto p : polygon)
        polygonJson.append(QJsonArray{ p.x(), p.y() });
    return { { "valid", count > 0 },
             { "pixels", count },
             { "meanAbsoluteRgb",
               count ? QJsonValue(error / (3 * count)) : QJsonValue(QJsonValue::Null) },
             { "meanSignedRgb",
               QJsonArray{ count ? delta[0] / count : 0, count ? delta[1] / count : 0,
                           count ? delta[2] / count : 0 } },
             { "ssim", windows ? QJsonValue(ssim / (3 * windows)) : QJsonValue(QJsonValue::Null) },
             { "ssimWindows", windows },
             { "polygon", polygonJson } };
}
QImage absoluteDifference(const QImage &a, const QImage &b)
{
    if (a.isNull() || a.size() != b.size())
        return { };
    QImage result(a.size(), QImage::Format_RGB32);
    for (int y = 0; y < a.height(); ++y)
        for (int x = 0; x < a.width(); ++x) {
            const auto p = a.pixelColor(x, y), q = b.pixelColor(x, y);
            result.setPixelColor(x, y,
                                 QColor(std::abs(p.red() - q.red()),
                                        std::abs(p.green() - q.green()),
                                        std::abs(p.blue() - q.blue())));
        }
    return result;
}
QJsonObject compareAntialiasBand(const QImage &actual, const QImage &expected,
                                 const QImage &referenceAlpha, const QPolygonF &region,
                                 int opaqueAlpha, int referenceScale)
{
    if (actual.isNull() || actual.size() != expected.size() || opaqueAlpha < 1 || opaqueAlpha > 255
        || referenceScale < 1 || referenceScale > 16
        || (referenceScale == 1 && referenceAlpha.size() != expected.size())
        || referenceAlpha.isNull() || !referenceAlpha.hasAlphaChannel())
        return { { "valid", false } };
    if (referenceScale > 1
        && (std::abs(referenceAlpha.width() - region.boundingRect().width() * referenceScale) > 2
            || std::abs(referenceAlpha.height() - region.boundingRect().height() * referenceScale)
                    > 2))
        return { { "valid", false } };
    QImage normalized = actual;
    const QRect bounds = region.boundingRect().toAlignedRect().intersected(actual.rect());
    qint64 bandPixels = 0, otherPixels = 0;
    double bandError = 0, otherError = 0;
    double maxOtherError = 0;
    QJsonObject worstPixel;
    for (int y = bounds.top(); y <= bounds.bottom(); ++y) {
        for (int x = bounds.left(); x <= bounds.right(); ++x) {
            if (!region.containsPoint({ x + .5, y + .5 }, Qt::OddEvenFill))
                continue;
            const int alpha = referenceScale == 1 ? referenceAlpha.pixelColor(x, y).alpha() : 0;
            const bool foreground = alpha >= (opaqueAlpha + 1) / 2;
            bool band = alpha > 0 && alpha < opaqueAlpha;
            // The 0.5-alpha contour is reference-only. Adjacent pixel centres are
            // within one physical pixel of it; there is no extra dilation.
            for (int dy = -1; dy <= 1 && referenceScale == 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (x + dx < 0 || y + dy < 0 || x + dx >= actual.width()
                        || y + dy >= actual.height())
                        continue;
                    band |= (referenceAlpha.pixelColor(x + dx, y + dy).alpha()
                             >= (opaqueAlpha + 1) / 2)
                            != foreground;
                }
            }
            if (referenceScale > 1) {
                const auto origin = region.boundingRect().topLeft();
                const int sx = int((x + .5 - origin.x()) * referenceScale);
                const int sy = int((y + .5 - origin.y()) * referenceScale);
                const auto inside = [&](int px, int py) {
                    return referenceAlpha.rect().contains(px, py)
                            && referenceAlpha.pixelColor(px, py).alpha() >= (opaqueAlpha + 1) / 2;
                };
                const bool centre = inside(sx, sy);
                const QColor centreColor = referenceAlpha.rect().contains(sx, sy)
                        ? referenceAlpha.pixelColor(sx, sy)
                        : QColor(Qt::transparent);
                band = false;
                for (int dy = -referenceScale; dy <= referenceScale; ++dy) {
                    for (int dx = -referenceScale; dx <= referenceScale; ++dx) {
                        if (dx * dx + dy * dy <= referenceScale * referenceScale) {
                            // A cropped reference has no evidence outside its bounds;
                            // the crop boundary must not become a synthetic alpha edge.
                            if (!referenceAlpha.rect().contains(sx + dx, sy + dy)) {
                                continue;
                            }
                            band |= inside(sx + dx, sy + dy) != centre;
                            if (centre && inside(sx + dx, sy + dy)) {
                                const auto neighbor = referenceAlpha.pixelColor(sx + dx, sy + dy);
                                // Text on the opaque selection pill has no alpha contour.
                                // Its reference color boundary is sampled at the same 1px radius.
                                band |= std::abs(neighbor.red() - centreColor.red())
                                                + std::abs(neighbor.green() - centreColor.green())
                                                + std::abs(neighbor.blue() - centreColor.blue())
                                        > 80;
                            }
                        }
                    }
                }
            }
            const auto a = actual.pixelColor(x, y), b = expected.pixelColor(x, y);
            const double error = (std::abs(a.red() - b.red()) + std::abs(a.green() - b.green())
                                  + std::abs(a.blue() - b.blue()))
                    / 3.0;
            if (band) {
                ++bandPixels;
                bandError += error;
                normalized.setPixelColor(x, y, b);
            } else {
                ++otherPixels;
                otherError += error;
                if (error > maxOtherError) {
                    maxOtherError = error;
                    worstPixel = {
                        { "x", x }, { "y", y }, { "actual", a.name() }, { "expected", b.name() }
                    };
                }
            }
        }
    }
    auto result = compareImages(normalized, expected, region);
    result.insert("bandPixels", bandPixels);
    result.insert("nonBandPixels", otherPixels);
    result.insert("bandFraction",
                  bandPixels + otherPixels ? double(bandPixels) / (bandPixels + otherPixels) : 0);
    result.insert("bandMeanAbsoluteRgb", bandPixels ? bandError / bandPixels : 0);
    result.insert("nonBandMeanAbsoluteRgb", otherPixels ? otherError / otherPixels : 0);
    result.insert("worstNonBandPixel", worstPixel);
    result.insert("referenceScale", referenceScale);
    result.insert(
            "method",
            "Unmodified 11x11 Gaussian SSIM after reference-only one-physical-pixel contour normalization (alpha and internal opaque-pill color boundaries); raw and band error are separate. Displaced/missing ink outside the reference band is never normalized.");
    return result;
}
QJsonValue edgeDistance(const QImage &a, const QImage &b, const QPolygonF &region)
{
    if (a.isNull() || a.size() != b.size())
        return QJsonValue::Null;
    const QRect bounds =
            region.boundingRect().toAlignedRect().intersected(a.rect().adjusted(1, 1, -1, -1));
    if (bounds.isEmpty())
        return QJsonValue::Null;
    auto edges = [&](const QImage &image) {
        QList<QPoint> points;
        auto gray = [&](int x, int y) {
            const auto c = image.pixelColor(x, y);
            return .2126 * c.red() + .7152 * c.green() + .0722 * c.blue();
        };
        for (int y = bounds.top(); y <= bounds.bottom(); ++y)
            for (int x = bounds.left(); x <= bounds.right(); ++x) {
                if (!region.containsPoint({ x + .5, y + .5 }, Qt::OddEvenFill))
                    continue;
                const double gx = gray(x + 1, y - 1) + 2 * gray(x + 1, y) + gray(x + 1, y + 1)
                        - gray(x - 1, y - 1) - 2 * gray(x - 1, y) - gray(x - 1, y + 1);
                const double gy = gray(x - 1, y + 1) + 2 * gray(x, y + 1) + gray(x + 1, y + 1)
                        - gray(x - 1, y - 1) - 2 * gray(x, y - 1) - gray(x + 1, y - 1);
                if (std::hypot(gx, gy) >= 48)
                    points.append({ x, y });
            }
        return points;
    };
    const auto first = edges(a), second = edges(b);
    if (first.isEmpty() || second.isEmpty())
        return QJsonValue::Null;
    // Exact Euclidean nearest distance. ROIs are small; no approximate chamfer metric.
    auto distance = [](const QList<QPoint> &from, const QList<QPoint> &to) {
        double sum = 0;
        for (auto p : from) {
            double closest = std::numeric_limits<double>::max();
            for (auto q : to) {
                const double dx = p.x() - q.x(), dy = p.y() - q.y();
                closest = std::min(closest, dx * dx + dy * dy);
                if (closest == 0)
                    break;
            }
            sum += std::sqrt(closest);
        }
        return sum / from.size();
    };
    return .5 * (distance(first, second) + distance(second, first));
}
}
