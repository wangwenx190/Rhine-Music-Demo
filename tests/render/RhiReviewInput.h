// Builds bounded deterministic RHI review inputs from anonymous fixtures. Parsed options
// describe real backend capabilities rather than fabricated output.

#pragma once
#include "Require.h"
#include <QString>
using Qt::StringLiterals::operator""_s;
using Qt::StringLiterals::operator""_L1;
#include "RhiItem.h"
#include "SceneAdapter.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QFile>
namespace yalahoo::review {
using namespace yalahoo::rhi;
inline QJsonObject readFixture(const QString &path)
{
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "Missing source review JSON");
    const auto document = QJsonDocument::fromJson(file.readAll());
    require(document.isObject(), "Invalid source review JSON");
    return document.object();
}
inline QVector3D vector(const QJsonArray &v)
{
    return { float(v[0].toDouble()), float(v[1].toDouble()), float(v[2].toDouble()) };
}
inline Input fixture(const QJsonObject &c)
{
    const QSize size(c[u"width"_s].toInt(), c[u"height"_s].toInt());
    SceneAdapter scene;
    scene.layout()->setViewport(size.width(), size.height());
    scene.setReducedMotion(true);
    auto *l = scene.lighting();
    l->setReducedMotion(true);
    l->setNight(!c[u"day"_s].toBool());
    auto settings = l->ribbon.settings;
    const auto mode = c[u"mode"_s].toString();
    settings.mode = mode == "baseline"_L1 ? RowLightingMode::Baseline
            : mode == "area"_L1           ? RowLightingMode::Area
            : mode == "hybrid"_L1         ? RowLightingMode::Hybrid
                                          : RowLightingMode::Guided;
    l->setSettings(settings);
    Input in;
    in.scene = scene.frame();
    const auto origin = vector(c[u"origin"_s].toArray()),
               position = vector(c[u"position"_s].toArray());
    in.scene.camera.position = { position.x(), position.y(), position.z() };
    in.scene.camera.aim = { origin.x(), origin.y() + 1.85, origin.z() };
    in.scene.view.setToIdentity();
    in.scene.view.lookAt(position, in.scene.camera.aim.qt(), { 0, 1, 0 });
    in.scene.cameraRotation =
            QQuaternion::fromRotationMatrix(in.scene.view.inverted().normalMatrix());
    in.scene.nearPlane =
            c[u"materialBaseline"_s].toBool() ? .1 : c.value(u"clipNear"_s).toDouble(5);
    in.scene.farPlane =
            c[u"materialBaseline"_s].toBool() ? 100 : c.value(u"clipFar"_s).toDouble(300);
    in.scene.projection.setToIdentity();
    in.scene.projection.perspective(32, float(size.width()) / size.height(),
                                    float(in.scene.nearPlane), float(in.scene.farPlane));
    in.scene.caseTransform.setToIdentity();
    in.scene.caseTransform.translate(origin);
    in.scene.casePosition = { origin.x(), origin.y(), origin.z() };
    RowLightingInput lightInput;
    lightInput.reduced = true;
    const auto offset = in.scene.cameraRotation.rotatedVector({ -48, -17.6f, 36 });
    l->update(lightInput, { -2.085, 2.252, 0 }, { offset.x(), offset.y(), offset.z() },
              c[u"detail"_s].toDouble(), 0);
    in.setLighting(*l);
    in.clarity = float(c[u"clarity"_s].toDouble());
    in.lightingEnabled = !c[u"materialBaseline"_s].toBool();
    in.rawOutput = in.lightingEnabled;
    if (!in.lightingEnabled)
        in.environmentIntensity = .68f - .20f * in.warmth;
    in.cover = QImage(":/assets/demo-covers/"_L1 + c[u"cover"_s].toString() + ".png"_L1);
    in.shadows = c[u"shadows"_s].toBool();
    in.shadowSize = c.value(u"shadowSize"_s).toInt(2048);
    in.shadowDiagnostic = c[u"shadowProbe"_s].toBool();
    in.showPaper = c[u"layer"_s].toString() == "all"_L1 || c[u"layer"_s].toString() == "paper"_L1;
    in.shells = c[u"layer"_s].toString() != "paper"_L1;
    in.captureSamples = 4;
    QJsonArray entities;
    if (c[u"instancePool"_s].toBool())
        entities = c[u"instances"_s].toArray();
    else {
        entities.append(c[u"origin"_s]);
        for (const auto &n : c[u"neighbors"_s].toArray())
            entities.append(n);
    }
    for (auto e : entities) {
        QMatrix4x4 m;
        m.translate(vector(e.toArray()));
        in.entities.append(m);
        const auto hash =
                QCryptographicHash::hash(QJsonDocument(e.toArray()).toJson(QJsonDocument::Compact),
                                         QCryptographicHash::Sha256);
        in.entityIds.append(QString::fromUtf8(hash.toHex().left(15)).toULongLong(nullptr, 16));
    }
    return in;
}
}
