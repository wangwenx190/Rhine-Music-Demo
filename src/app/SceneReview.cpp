#include "SceneReview.h"
#include "SceneAdapter.h"
#include <QDir>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSaveFile>
#include <QSGRendererInterface>
#include <QTimer>
#include <atomic>
#include <memory>
namespace yalahoo {
int reviewScene(QGuiApplication &app, QQuickWindow &window, SceneAdapter &scene, const QString &fixture, const QString &report)
{
    QFile input(fixture);
    if (!input.open(QIODevice::ReadOnly)) return 9;
    const auto fixtureBytes=input.readAll();
    const auto oracle=QJsonDocument::fromJson(fixtureBytes).object();
    auto cases=oracle["cases"].toArray();
    if (cases.size()!=48) return 9;
    // Render every 1/120-second state of the interrupted dolly on Windows too.
    for(auto value:oracle["sequences"].toArray()) {
        const auto sequence=value.toObject();
        if(sequence["fps"].toInt()!=120) continue;
        for(auto sample:sequence["samples"].toArray()) {
            auto c=sample.toObject(); c.insert("dynamic",true); cases.append(c);
        }
    }
    window.setProperty("sceneReview",true);
    window.setFlags(Qt::Window | Qt::FramelessWindowHint);
    window.setPosition(0,0);
    window.show();
    scene.setReducedMotion(true); // exact state; acquisition waits for rendered frames, never wall time pose
    auto *view=window.findChild<QQuickItem *>(QStringLiteral("quick3DProbe"));
    auto *camera=window.findChild<QObject *>(QStringLiteral("sceneCamera"));
    auto *layoutLabel=window.findChild<QObject *>(QStringLiteral("layoutProbe"));
    if (!view || !camera || !layoutLabel) return 9;
    const QString folder=QFileInfo(report).absolutePath(); QDir().mkpath(folder);
    const auto frames=std::make_shared<std::atomic<int>>(0);
    const auto renderedConnection=QObject::connect(&window,&QQuickWindow::afterRendering,&window,[frames] { ++*frames; },Qt::DirectConnection);
    QJsonArray results;
    int index=-1, startFrame=0;
    bool pass=true;
    double maxQt=0, maxWeb=0;
    const auto next=[&] {
        ++index;
        if(index>=cases.size()) return;
        const auto c=cases[index].toObject();
        if(index==48) {
            scene.setReducedMotion(false);
            scene.observe({{-62.26,35.98,43.28},{-.5,1.1,.4},
                2*SceneVector{-62.26+.5,35.98-1.1,43.28-.4}.length()*std::tan(6.15*std::acos(-1.0)/360)},0);
        }
        window.resize(c["width"].toInt(),c["height"].toInt());
        scene.layout()->setViewport(window.width(),window.height());
        scene.setDetail(c["detail"].toDouble());
        if(c["dynamic"].toBool()) {
            scene.setReducedMotion(c["reduced"].toBool()); scene.advance(c["dt"].toDouble());
        } else scene.snapToTarget();
        startFrame=frames->load(); window.update();
    };
    QTimer poll;
    QObject::connect(&poll,&QTimer::timeout,&window,[&] {
        if (frames->load()-startFrame<3) { window.update(); return; }
        const auto c=cases[index].toObject();
        const bool dynamic=c["dynamic"].toBool();
        const bool capture=!dynamic || index==48 || index==108 || index==198 || index==288 || index==408;
        const QImage image=capture ? window.grabWindow() : QImage{};
        const bool bindings=window.width()==c["width"].toInt() && window.height()==c["height"].toInt()
            && view->width()==window.width() && view->height()==window.height()
            && layoutLabel->property("text").toString().startsWith(scene.layout()->kind())
            && (camera->property("position").value<QVector3D>()-scene.cameraPosition()).length()<1e-3
            && std::abs(camera->property("fieldOfView").toDouble()-scene.fieldOfView())<1e-4;
        bool points=true;
        QJsonArray projections;
        for(auto value:c["projections"].toArray()) {
            const auto p=value.toObject();
            const auto a=p["world"].toArray(), expected=p["pixel"].toArray();
            const QVector3D world(float(a[0].toDouble()),float(a[1].toDouble()),float(a[2].toDouble()));
            QVector3D qt;
            const bool invoked=QMetaObject::invokeMethod(view,"qtProjection",Q_RETURN_ARG(QVector3D,qt),Q_ARG(QVector3D,world));
            const auto cpu=scene.projectWorldPoint(world);
            const double qtError=std::hypot(qt.x()-cpu.x(),qt.y()-cpu.y());
            const double webError=std::hypot(qt.x()-expected[0].toDouble(),qt.y()-expected[1].toDouble());
            maxQt=std::max(maxQt,qtError); maxWeb=std::max(maxWeb,webError);
            points=points && invoked && qtError<=2 && webError<=2;
            projections.append(QJsonObject{{"qtPixel",QJsonArray{qt.x(),qt.y()}},{"qtCpuError",qtError},{"webError",webError}});
        }
        // Actual no-light mesh pixels and client dimensions/DPR, in addition to public View3D mapping.
        int pixels=0;
        const auto rgb=image.convertToFormat(QImage::Format_RGB32);
        for(int y=0;y<rgb.height();++y) for(int x=0;x<rgb.width();++x) {
            const auto color=reinterpret_cast<const QRgb *>(rgb.constScanLine(y))[x];
            if(qGreen(color)>qRed(color)+8 && qGreen(color)<165 && qRed(color)>15) ++pixels;
        }
        const QString file=QString("case-%1-%2x%3-detail-%4.png").arg(index,2,10,QChar('0')).arg(window.width()).arg(window.height()).arg(scene.detail());
        const bool saved=!capture || (!image.isNull() && image.save(folder+"/"+file));
        const bool passed=bindings && points && saved && (!capture || pixels>100);
        pass=pass && passed;
        results.append(QJsonObject{{"index",index},{"width",window.width()},{"height",window.height()},
            {"detail",scene.detail()},{"dynamic",dynamic},{"time",c["time"]},{"portrait",scene.layout()->portrait()},{"kind",scene.layout()->kind()},
            {"frameRevision",qint64(scene.frameRevision())},{"renderedFrames",frames->load()-startFrame},
            {"imageWidth",image.width()},{"imageHeight",image.height()},{"dpr",window.devicePixelRatio()},
            {"modelPixels",pixels},{"bindings",bindings},{"passed",passed},{"screenshot",capture ? file : QString{}},{"projections",projections}});
        if(index+1<cases.size()) { next(); return; }
        poll.stop();
        const auto api=window.rendererInterface()->graphicsApi();
        pass=pass && QSGRendererInterface::isApiRhiBased(api);
        QSaveFile output(report);
        bool committed=output.open(QIODevice::WriteOnly);
        if(committed) {
            output.write(QJsonDocument(QJsonObject{{"passed",pass},{"qtVersion",qVersion()},
                {"platform",QGuiApplication::platformName()},{"graphicsApi",int(api)},
                {"baseline",oracle["baseline"]},{"fixtureSha256",QString::fromLatin1(QCryptographicHash::hash(fixtureBytes,QCryptographicHash::Sha256).toHex())},
                {"maxQtCpuPixels",maxQt},{"maxWebPixels",maxWeb},{"cases",results}}).toJson());
            committed=output.commit();
        }
        app.exit(pass && committed?0:10);
    });
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout,&QTimer::timeout,&app,[&] { app.exit(11); });
    next(); poll.start(10); timeout.start(80000);
    const int code=app.exec();
    // The shared counter also covers a callback already in progress during exit.
    QObject::disconnect(renderedConnection);
    return code;
}
}
