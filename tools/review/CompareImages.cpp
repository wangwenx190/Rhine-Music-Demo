#include "ImageComparison.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.size() < 6 || args.size() > 9)
        return 2; // actual expected polygon.json metrics.json diff.png
    QFile input(args[3]);
    if (!input.open(QIODevice::ReadOnly))
        return 3;
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(input.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isArray())
        return 3;
    QPolygonF polygon;
    for (auto value : document.array()) {
        const auto point = value.toArray();
        if (point.size() != 2 || !point[0].isDouble() || !point[1].isDouble())
            return 3;
        polygon << QPointF(point[0].toDouble(), point[1].toDouble());
    }
    const QImage a(args[1]), b(args[2]);
    auto result = yalahoo::compareImages(a, b, polygon);
    if (args.size() >= 7) {
        const int opaqueAlpha = args.size() >= 8 ? args[7].toInt() : 255;
        const int scale = args.size() == 9 ? args[8].toInt() : 1;
        if (opaqueAlpha < 1 || opaqueAlpha > 255)
            return 4;
        if (scale < 1 || scale > 16)
            return 4;
        const auto nonEdge =
                yalahoo::compareAntialiasBand(a, b, QImage(args[6]), polygon, opaqueAlpha, scale);
        if (!nonEdge["valid"].toBool())
            return 4;
        result.insert("nonEdge", nonEdge);
    }
    if (!result["valid"].toBool())
        return 4;
    result.insert("edgeDistancePixels", yalahoo::edgeDistance(a, b, polygon));
    if (!yalahoo::absoluteDifference(a, b).save(args[5]))
        return 5;
    QSaveFile output(args[4]);
    if (!output.open(QIODevice::WriteOnly))
        return 5;
    output.write(QJsonDocument(result).toJson());
    return output.commit() ? 0 : 5;
}
