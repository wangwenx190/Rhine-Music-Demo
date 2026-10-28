// Decodes the bundled GLB geometry used by the product renderer.
#pragma once
#include <QByteArray>
#include <QString>
#include <QVector>
#include <array>
namespace yalahoo::assets {
struct Batch final
{
    QString name;
    QVector<std::array<float, 8>> vertices; // position, normal, UV
    QVector<quint32> indices;
};
QVector<Batch> readGlb(const QByteArray &bytes);
}
