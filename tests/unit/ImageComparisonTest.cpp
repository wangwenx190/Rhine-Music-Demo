#include "ImageComparison.h"
#include <QTest>
class ImageComparisonTest : public QObject
{
    Q_OBJECT
private slots:
    void highResolutionContourPreservesOpaqueInterior()
    {
        QImage expected(40, 40, QImage::Format_ARGB32);
        expected.fill(Qt::white);
        for (int y = 8; y < 32; ++y) {
            for (int x = 8; x < 32; ++x) {
                expected.setPixelColor(x, y, Qt::black);
            }
        }
        const QPolygonF roi(QRectF(0, 0, 40, 40));
        const auto outline =
                expected.scaled(320, 320, Qt::IgnoreAspectRatio, Qt::FastTransformation);
        const auto same = yalahoo::compareAntialiasBand(expected, expected, outline, roi, 255, 8);
        QVERIFY(same["valid"].toBool());
        QCOMPARE(same["ssim"].toDouble(), 1.0);
        QVERIFY(same["bandFraction"].toDouble() < 0.2);
        QImage missing = expected;
        missing.fill(Qt::white);
        const auto changed = yalahoo::compareAntialiasBand(missing, expected, outline, roi, 255, 8);
        QVERIFY(changed["nonBandMeanAbsoluteRgb"].toDouble() > 10);
        QVERIFY(changed["ssim"].toDouble() < 0.98);
        QVERIFY(!yalahoo::compareAntialiasBand(expected, expected, outline, roi, 255, 2)["valid"]
                         .toBool());
        QVERIFY(!yalahoo::compareAntialiasBand(expected, expected, outline, roi, 0, 8)["valid"]
                         .toBool());
    }
    void fixedReferenceBandKeepsDisplacement()
    {
        QImage expected(40, 40, QImage::Format_ARGB32), alpha(40, 40, QImage::Format_ARGB32);
        expected.fill(Qt::white);
        alpha.fill(Qt::transparent);
        for (int y = 8; y < 28; ++y) {
            for (int x = 8; x < 20; ++x) {
                expected.setPixelColor(x, y, Qt::black);
                alpha.setPixelColor(x, y, Qt::black);
            }
        }
        const QPolygonF roi(QRectF(0, 0, 40, 40));
        auto same = yalahoo::compareAntialiasBand(expected, expected, alpha, roi);
        QCOMPARE(same["ssim"].toDouble(), 1.0);
        QVERIFY(same["bandFraction"].toDouble() < 0.2);
        QImage displaced(40, 40, QImage::Format_ARGB32);
        displaced.fill(Qt::white);
        for (int y = 8; y < 28; ++y)
            for (int x = 18; x < 30; ++x)
                displaced.setPixelColor(x, y, Qt::black);
        auto changed = yalahoo::compareAntialiasBand(displaced, expected, alpha, roi);
        QVERIFY(changed["nonBandMeanAbsoluteRgb"].toDouble() > 10);
        QVERIFY(changed["ssim"].toDouble() < 0.98);
        QVERIFY(!yalahoo::compareAntialiasBand(displaced, expected, QImage{ }, roi)["valid"]
                         .toBool());
    }
    void metrics()
    {
        QImage a(24, 24, QImage::Format_RGB32);
        a.fill(QColor(90, 110, 130));
        QImage b = a;
        const QPolygonF roi(QRectF(0, 0, 24, 24));
        auto m = yalahoo::compareImages(a, b, roi);
        QVERIFY(m["valid"].toBool());
        QCOMPARE(m["ssim"].toDouble(), 1.0);
        QCOMPARE(m["meanAbsoluteRgb"].toDouble(), 0.0);
        QCOMPARE(m["ssimWindows"].toInt(), 196);
        b.fill(QColor(100, 120, 140));
        m = yalahoo::compareImages(a, b, roi);
        QCOMPARE(m["meanAbsoluteRgb"].toDouble(), 10.0);
        QVERIFY(m["ssim"].toDouble() > 0 && m["ssim"].toDouble() < 1);
        QCOMPARE(yalahoo::absoluteDifference(a, b).pixelColor(0, 0), QColor(10, 10, 10));
        QVERIFY(!yalahoo::compareImages({ }, b, roi)["valid"].toBool());
        QVERIFY(!yalahoo::compareImages(a, b, QPolygonF(QRectF(30, 30, 5, 5)))["valid"].toBool());
        QVERIFY(yalahoo::compareImages(a, b, QPolygonF(QRectF(2, 2, 5, 5)))["ssim"].isNull());
        QVERIFY(yalahoo::edgeDistance(a, b, roi).isNull());
    }
    void displacedEdges()
    {
        QImage a(24, 24, QImage::Format_RGB32), b(24, 24, QImage::Format_RGB32);
        a.fill(Qt::black);
        b.fill(Qt::black);
        for (int y = 0; y < 24; ++y)
            for (int x = 12; x < 24; ++x)
                a.setPixelColor(x, y, Qt::white);
        for (int y = 0; y < 24; ++y)
            for (int x = 14; x < 24; ++x)
                b.setPixelColor(x, y, Qt::white);
        const QPolygonF roi(QRectF(0, 0, 24, 24));
        QCOMPARE(yalahoo::edgeDistance(a, a, roi).toDouble(), 0.0);
        QCOMPARE(yalahoo::edgeDistance(a, b, roi).toDouble(), 1.5);
    }
};
QTEST_GUILESS_MAIN(ImageComparisonTest)
#include "ImageComparisonTest.moc"
