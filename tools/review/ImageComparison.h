#pragma once
#include <QImage>
#include <QJsonObject>
#include <QPolygonF>
namespace yalahoo {
QJsonObject compareImages(const QImage &actual, const QImage &expected, const QPolygonF &region,
                          bool ssim = true);
QImage absoluteDifference(const QImage &actual, const QImage &expected);
QJsonObject compareAntialiasBand(const QImage &actual, const QImage &expected,
                                 const QImage &referenceAlpha, const QPolygonF &region,
                                 int opaqueAlpha = 255, int referenceScale = 1);
// Symmetric distance between Sobel edges (threshold 48/255), in physical pixels.
// Null when one side has no edges: absence must not be scored as perfect parity.
QJsonValue edgeDistance(const QImage &actual, const QImage &expected, const QPolygonF &region);
}
