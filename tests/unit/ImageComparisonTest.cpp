#include "ImageComparison.h"
#include <QTest>
class ImageComparisonTest : public QObject {
    Q_OBJECT
private slots:
    void metrics() {
        QImage a(24,24,QImage::Format_RGB32);a.fill(QColor(90,110,130));
        QImage b=a;const QPolygonF roi(QRectF(0,0,24,24));
        auto m=yalahoo::compareImages(a,b,roi);
        QVERIFY(m["valid"].toBool());QCOMPARE(m["ssim"].toDouble(),1.0);
        QCOMPARE(m["meanAbsoluteRgb"].toDouble(),0.0);QCOMPARE(m["ssimWindows"].toInt(),196);
        b.fill(QColor(100,120,140));m=yalahoo::compareImages(a,b,roi);
        QCOMPARE(m["meanAbsoluteRgb"].toDouble(),10.0);
        QVERIFY(m["ssim"].toDouble()>0&&m["ssim"].toDouble()<1);
        QCOMPARE(yalahoo::absoluteDifference(a,b).pixelColor(0,0),QColor(10,10,10));
        QVERIFY(!yalahoo::compareImages({},b,roi)["valid"].toBool());
        QVERIFY(!yalahoo::compareImages(a,b,QPolygonF(QRectF(30,30,5,5)))["valid"].toBool());
        QVERIFY(yalahoo::compareImages(a,b,QPolygonF(QRectF(2,2,5,5)))["ssim"].isNull());
        QVERIFY(yalahoo::edgeDistance(a,b,roi).isNull());
    }
    void displacedEdges() {
        QImage a(24,24,QImage::Format_RGB32),b(24,24,QImage::Format_RGB32);a.fill(Qt::black);b.fill(Qt::black);
        for(int y=0;y<24;++y)for(int x=12;x<24;++x)a.setPixelColor(x,y,Qt::white);
        for(int y=0;y<24;++y)for(int x=14;x<24;++x)b.setPixelColor(x,y,Qt::white);
        const QPolygonF roi(QRectF(0,0,24,24));
        QCOMPARE(yalahoo::edgeDistance(a,a,roi).toDouble(),0.0);
        QCOMPARE(yalahoo::edgeDistance(a,b,roi).toDouble(),1.5);
    }
};
QTEST_GUILESS_MAIN(ImageComparisonTest)
#include "ImageComparisonTest.moc"
