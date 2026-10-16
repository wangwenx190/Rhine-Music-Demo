pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import Yalahoo.Music

Item {
    id: root
    required property DesignTokens tokens
    required property PlaybackController playback
    property bool navigationInputOwner: true
    implicitHeight: 38
    onVisibleChanged: if (!visible)
                          playback.cancelSeek()
    Component.onDestruction: playback.cancelSeek()
    function time(value) {
        const seconds = Math.max(0, Math.floor(value / 1000));
        return Math.floor(seconds / 60) + ":" + (seconds % 60).toString().padStart(2, "0");
    }
    Row {
        id: buttons
        anchors.verticalCenter: parent.verticalCenter
        spacing: 12
        ArchiveButton {
            tokens: root.tokens
            text: "上一首"
            cssPixelSize: 10
            enabled: root.playback.canPrevious
            onClicked: root.playback.previous()
        }
        ArchiveButton {
            tokens: root.tokens
            text: "下一首"
            cssPixelSize: 10
            enabled: root.playback.canNext
            onClicked: root.playback.next()
        }
    }
    Slider {
        id: progress
        objectName: "songProgress"
        x: buttons.width + 12
        width: Math.max(0, root.width - x - 88)
        anchors.verticalCenter: parent.verticalCenter
        from: 0
        to: Math.max(1, root.playback.duration)
        value: root.playback.position
        enabled: root.playback.seekable && root.playback.duration > 0
        live: true
        stepSize: 1000
        palette.highlight: root.tokens.accent
        Keys.onEscapePressed: root.playback.cancelSeek()
        onPressedChanged: {
            if (pressed)
                root.playback.beginSeek();
            else
                root.playback.commitSeek();
        }
        onMoved: {
            if (pressed)
                root.playback.previewSeek(value);
            else
                root.playback.seek(value);
        }
        Accessible.name: "歌曲进度"
    }
    ArchiveText {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        tokens: root.tokens
        text: root.time(root.playback.position) + " / " + (root.playback.duration > 0 ? root.time(
                                                                                            root.playback.duration) :
                                                                                        "—")
        cssPixelSize: 9
        color: root.tokens.muted
    }
}
