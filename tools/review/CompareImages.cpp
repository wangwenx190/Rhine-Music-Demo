#include "ImageComparison.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.size() != 6) return 2; // actual expected polygon.json metrics.json diff.png
    QFile input(args[3]);
    if (!input.open(QIODevice::ReadOnly)) return 3;
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(input.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isArray()) return 3;
    QPolygonF polygon;
    for (auto value : document.array()) {
        const auto point = value.toArray();
        if (point.size() != 2 || !point[0].isDouble() || !point[1].isDouble()) return 3;
        polygon << QPointF(point[0].toDouble(), point[1].toDouble());
    }
    const QImage a(args[1]), b(args[2]);
    auto result = yalahoo::compareImages(a,b,polygon);
    if (!result["valid"].toBool()) return 4;
    result.insert("edgeDistancePixels", yalahoo::edgeDistance(a,b,polygon));
    if (!yalahoo::absoluteDifference(a,b).save(args[5])) return 5;
    QSaveFile output(args[4]);
    if (!output.open(QIODevice::WriteOnly)) return 5;
    output.write(QJsonDocument(result).toJson());
    return output.commit() ? 0 : 5;
}
