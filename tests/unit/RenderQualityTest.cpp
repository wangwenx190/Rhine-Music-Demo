#include "RenderQuality.h"
#include "SceneAdapter.h"
#include <QtTest>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <cmath>
using namespace yalahoo;
class RenderQualityTest:public QObject {
    Q_OBJECT
    QJsonObject load(const char *name) {
        QFile f(QString(FIXTURE_ROOT "/tests/fixtures/occlusion/")+name);if(!f.open(QIODevice::ReadOnly))return {};
        return QJsonDocument::fromJson(f.readAll()).object();
    }
private slots:
    void frozenQuality() {
        const auto oracle=load("quality.json");QVERIFY(!oracle.isEmpty());RenderQuality q;
        for(auto value:oracle["normalized"].toArray()) {
            auto c=value.toObject();q.configure(c["input"].toObject().toVariantMap(),c["legacyHigh"].toBool());
            const auto expected=c["expected"].toObject().toVariantMap();
            for(auto i=expected.begin();i!=expected.end();++i)QCOMPARE(q.values()[i.key()],i.value());
            QCOMPARE(q.preset(),c["preset"].toString());
        }
        for(auto value:oracle["dimensions"].toArray()) {
            const auto c=value.toObject(),e=c["expected"].toObject();q.setPreset(c["preset"].toString());
            q.resize(c["width"].toDouble(),c["height"].toDouble(),c["stageScale"].toDouble(),c["dpr"].toDouble(),c["maximum"].toInt());
            QCOMPARE(q.targetSize(),QSize(e["width"].toInt(),e["height"].toInt()));QCOMPARE(q.limited(),e["limited"].toBool());
            QVERIFY(qint64(q.targetSize().width())*q.targetSize().height()<=8294400);
            QVERIFY(q.targetSize().width()<=c["maximum"].toInt());QVERIFY(q.targetSize().height()<=c["maximum"].toInt());
            QCOMPARE(q.preset(),c["preset"].toString());
        }
    }
    void seededKernel() {
        const auto oracle=load("kernel.json");QVERIFY(!oracle.isEmpty());
        for(int count:{16,32,64}) {
            quint32 state=417;auto random=[&]{state=state*1664525u+1013904223u;return double(state)/4294967296.0;};
            const auto samples=oracle["kernels"].toObject()[QString::number(count)].toObject()["samples"].toArray();QCOMPARE(samples.size(),count);
            for(int i=0;i<count;++i){double x=2*random()-1,y=2*random()-1,z=random(),length=std::sqrt(x*x+y*y+z*z),scale=.1+.9*std::pow(double(i)/count,2);const auto s=samples[i].toArray();
                QVERIFY(std::abs(x/length*scale-s[0].toDouble())<1e-12);QVERIFY(std::abs(y/length*scale-s[1].toDouble())<1e-12);QVERIFY(std::abs(z/length*scale-s[2].toDouble())<1e-12);
            }
        }
    }
    void invalidAndIdempotent() {
        RenderQuality q;QSignalSpy changes(&q,&RenderQuality::changed);q.setPreset("original");QCOMPARE(changes.size(),0);
        const auto size=q.targetSize();q.resize(-1,10,1,1,4096);QCOMPARE(q.targetSize(),size);
        q.setPreset("ultra");q.resize(8000,8000,1,3,16384);QVERIFY(q.limited());QCOMPARE(q.values()["scale"].toInt(),150);
        auto values=q.values();values["aoResolution"]=.5;q.configure(values);QCOMPARE(q.aoSize(),QSize(q.targetSize().width()/2,q.targetSize().height()/2));
    }
    void preservesSceneState() {
        SceneAdapter scene;scene.setReducedMotion(true);scene.setDetail(1);
        const auto revision=scene.frameRevision();const auto position=scene.cameraPosition();const auto *lighting=scene.lighting();
        for(auto preset:{"performance","high","ultra","original"}){
            scene.quality()->setPreset(preset);QCOMPARE(scene.frameRevision(),revision);
            QCOMPARE(scene.cameraPosition(),position);QCOMPARE(scene.detail(),1.0);QCOMPARE(scene.lighting(),lighting);
        }
    }
    void focalPlaneFollowsCamera() {
        SceneAdapter scene;
        scene.layout()->setViewport(960,720);
        scene.observe({{0,1.85,12},{0,1.85,0},7.33},0);
        QCOMPARE(scene.focalDepth({0,0,0}),12.0);
        QCOMPARE(scene.focalDepth({0,0,4}),8.0);
        QCOMPARE(scene.focalDepth({0,0,-4}),16.0);
        scene.observe({{6,3.8,6},{0,1.85,0},7.33},0);
        const auto direction=QVector3D(-6,-1.95f,-6).normalized();
        QVERIFY(std::abs(scene.focalDepth({0,0,0})-QVector3D::dotProduct(QVector3D(-6,-1.8f,-6),direction))<1e-5);
        // Rebase both entity and camera: optical depth is translation invariant.
        const auto before=scene.focalDepth({0,0,0});
        scene.observe({{106,23.8,36},{100,21.85,30},7.33},0);
        QVERIFY(std::abs(scene.focalDepth({100,20,30})-before)<1e-4);
    }
};
QTEST_GUILESS_MAIN(RenderQualityTest)
#include "RenderQualityTest.moc"
