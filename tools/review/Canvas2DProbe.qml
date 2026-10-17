pragma ComponentBehavior: Bound
import QtQuick
import QtCanvas2D
import Yalahoo.Music

Item {
    id: root
    property bool night: false
    property int paintCount: 0
    property real scriptMilliseconds: 0
    property bool surfaceReady: true
    property bool rebuilding: false
    property bool offscreenRendering: false
    function resumeSurface() {
        rebuilding = false;
    }
    function rebuildSurface() {
        if (surfaceLoader.item) {
            rebuilding = true;
            Qt.callLater(resumeSurface);
        }
    }
    onWidthChanged: Qt.callLater(rebuildSurface)
    onHeightChanged: Qt.callLater(rebuildSurface)
    Connections {
        target: root.Window.window
        function onSceneGraphInvalidated() {
            root.surfaceReady = false;
        }
        function onSceneGraphInitialized() {
            root.surfaceReady = true;
        }
    }
    Loader {
        id: surfaceLoader
        anchors.fill: parent
        // The preview Canvas2D context cannot replay into a replaced surface.
        // Preserve business state here, and own one disposable drawing context.
        active: root.visible && root.surfaceReady && !root.rebuilding && root.Window.window && (
                    root.offscreenRendering || root.Window.window.visible)
        sourceComponent: Component {
            Canvas2D {
                id: canvas
                property path2d curve
                function invalidate() {
                    if (available && visible) {
                        requestPaint();
                    }
                }
                Component.onCompleted: {
                    curve.addPath("M16 60 C40 12 76 108 112 60");
                    Qt.callLater(invalidate);
                }
                fillColor: "transparent"
                alphaBlending: true
                sampleCount: 1
                onAvailableChanged: Qt.callLater(invalidate)
                Connections {
                    target: root
                    function onNightChanged() {
                        Qt.callLater(canvas.invalidate);
                    }
                }
                onPaint: {
                    const start = Date.now();
                    const ctx = getContext("2d");
                    const ink = root.night ? "#e9eef2" : "#232722";
                    ctx.reset();
                    ctx.resetClipping();
                    ctx.strokeStyle = ink;
                    ctx.lineWidth = 3;
                    ctx.lineCap = "round";
                    ctx.stroke(curve);
                    const gradient = ctx.createLinearGradient(136, 20, 232, 80);
                    gradient.addColorStop(0, "#b56834");
                    gradient.addColorStop(1, "#64d9ad7d");
                    ctx.fillStyle = gradient;
                    ctx.fillRect(136, 20, 96, 60);
                    ctx.save();
                    ctx.setClipRect(256, 20, 80, 60);
                    ctx.fillStyle = ink;
                    ctx.globalAlpha = 128 / 255;
                    ctx.fillRect(244, 10, 110, 100);
                    ctx.restore();
                    ctx.resetClipping();
                    ctx.fillStyle = ink;
                    ctx.font = "20px '" + MusicFonts.regular.family + "'";
                    ctx.textBaseline = "alphabetic";
                    ctx.fillText("\u97f3\u4e50\u6536\u85cf  gyp  e\u0301  \ud83c\udfb5", 16, 128);
                    root.paintCount++;
                    root.scriptMilliseconds += Date.now() - start;
                }
            }
        }
    }
}
