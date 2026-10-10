#pragma once
#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>
#include <array>

namespace yalahoo::assets {
struct Batch {
    QString name;
    QVector<std::array<float, 8>> vertices; // position, normal, UV
    QVector<quint32> indices;
};
QVector<Batch> readGlb(const QByteArray &bytes);
Batch readQtMesh(const QByteArray &bytes);
QJsonObject compareGeometry(const QVector<Batch> &original, const QVector<Batch> &imported);
QByteArray readFile(const QString &path);
void saveFile(const QString &path, const QByteArray &bytes);
}
