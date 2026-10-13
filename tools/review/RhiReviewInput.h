#pragma once
#include "RhiItem.h"
#include "SceneAdapter.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QCryptographicHash>
namespace yalahoo::review {
using namespace yalahoo::rhi;
inline QVector3D vector(const QJsonArray &v)
{
    return { float(v[0].toDouble()), float(v[1].toDouble()), float(v[2].toDouble()) };
}
inline Input fixture(const QJsonObject &c)
{
    const QSize size(c["width"].toInt(), c["height"].toInt());
    SceneAdapter scene;
    scene.layout()->setViewport(size.width(), size.height());
    scene.setReducedMotion(true);
    auto *l = scene.lighting();
    l->setReducedMotion(true);
    l->setNight(!c["day"].toBool());
    auto settings = l->ribbon.settings;
    const auto mode = c["mode"].toString();
    settings.mode = mode == "baseline" ? RowLightingMode::Baseline
            : mode == "area"           ? RowLightingMode::Area
            : mode == "hybrid"         ? RowLightingMode::Hybrid
                                       : RowLightingMode::Guided;
    l->setSettings(settings);
    Input in;
    in.scene = scene.frame();
    const auto origin = vector(c["origin"].toArray()), position = vector(c["position"].toArray());
    in.scene.camera.position = { position.x(), position.y(), position.z() };
    in.scene.camera.aim = { origin.x(), origin.y() + 1.85, origin.z() };
    in.scene.view.setToIdentity();
    in.scene.view.lookAt(position, in.scene.camera.aim.qt(), { 0, 1, 0 });
    in.scene.cameraRotation =
            QQuaternion::fromRotationMatrix(in.scene.view.inverted().normalMatrix());
    in.scene.nearPlane = c["materialBaseline"].toBool() ? .1 : c.value("clipNear").toDouble(5);
    in.scene.farPlane = c["materialBaseline"].toBool() ? 100 : c.value("clipFar").toDouble(300);
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
              c["detail"].toDouble(), 0);
    const auto &p = l->palette();
    in.background = p.background.qt();
    in.keyColor = p.keyColor.qt();
    in.skyColor = p.skyColor.qt();
    in.groundColor = p.groundColor.qt();
    in.lightIntensities = l->lightIntensities();
    in.spotPosition = l->spotPosition();
    in.spotTarget = l->spotTarget();
    in.spotColor = l->spotColor();
    in.spotIntensity = float(l->spotIntensity());
    in.environmentIntensity = float(l->environmentIntensity());
    in.rowSettings = l->rowSettings();
    in.rowColumn = l->rowColumn();
    in.rowDetail = float(l->rowDetail());
    in.rowFill = float(l->rowFill());
    in.lightColumn = l->lightColumn();
    in.printAmbient = float(l->printAmbient());
    in.scatterColor = l->scatterColor();
    in.scatterStrength = float(l->scatterStrength());
    in.areaPosition = l->areaPosition();
    in.areaTarget = l->areaTarget();
    in.areaIntensity = float(l->areaIntensity());
    in.warmth = float(l->warmth());
    in.exposure = float(l->exposure());
    in.clarity = float(c["clarity"].toDouble());
    in.lightingEnabled = !c["materialBaseline"].toBool();
    in.rawOutput = in.lightingEnabled;
    if (!in.lightingEnabled)
        in.environmentIntensity = .68f - .20f * in.warmth;
    in.cover = QImage(":/assets/demo-covers/" + c["cover"].toString() + ".png");
    in.shadows = c["shadows"].toBool();
    in.shadowSize = c.value("shadowSize").toInt(2048);
    in.shadowDiagnostic = c["shadowProbe"].toBool();
    in.showPaper = c["layer"].toString() == "all" || c["layer"].toString() == "paper";
    in.shells = c["layer"].toString() != "paper";
    in.captureSamples = 4;
    QJsonArray entities;
    if (c["instancePool"].toBool())
        entities = c["instances"].toArray();
    else {
        entities.append(c["origin"]);
        for (const auto &n : c["neighbors"].toArray())
            entities.append(n);
    }
    for (auto e : entities) {
        QMatrix4x4 m;
        m.translate(vector(e.toArray()));
        in.entities.append(m);
        const auto hash =
                QCryptographicHash::hash(QJsonDocument(e.toArray()).toJson(QJsonDocument::Compact),
                                         QCryptographicHash::Sha256);
        in.entityIds.append(QString::fromLatin1(hash.toHex().left(15)).toULongLong(nullptr, 16));
    }
    return in;
}
}
