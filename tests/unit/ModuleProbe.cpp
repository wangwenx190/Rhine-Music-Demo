#include <QAudioOutput>
#include <QGuiApplication>
#include <QMediaPlayer>
#include <QQuick3DGeometry>
#include <QtTest>

class ModuleProbe : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void quick3DLink()
    {
        QQuick3DGeometry geometry;
        geometry.setStride(3 * sizeof(float));
        QCOMPARE(geometry.stride(), 12);
    }
    void multimediaPlugin()
    {
        QAudioOutput output;
        QMediaPlayer player;
        player.setAudioOutput(&output);
        QVERIFY(player.isAvailable());
        QCOMPARE(player.audioOutput(), &output);
        QVERIFY(player.source().isEmpty());
        QCOMPARE(player.playbackState(), QMediaPlayer::StoppedState);
    }
};
QTEST_MAIN(ModuleProbe)
#include "ModuleProbe.moc"
