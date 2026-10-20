#include <QAudioOutput>
#include <QGuiApplication>
#include <QMediaPlayer>
#include <QQuick3DGeometry>
#include <QtTest>

class ModuleProbe final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ModuleProbe)
public:
    ModuleProbe() = default;

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
