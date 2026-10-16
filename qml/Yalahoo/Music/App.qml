pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import Yalahoo.Music
import "pages"

ApplicationWindow {
    id: window
    required property LibrarySession librarySession
    required property MotionController motionController
    required property SceneAdapter sceneAdapter
    property OpeningController openingController: null
    property InputRouter inputRouter: null
    property RulerController rulerController: null
    property PresentationController presentationController: null
    property DetailModel detailModel: null
    property OverviewController overviewController: null
    property string reviewPlayingTitle: ""
    property string preferencesError: ""
    property bool geometryProbe: false
    property bool verificationRun: false
    property int verificationFlags: Qt.Window
    width: 1280
    height: 720
    minimumWidth: 320
    minimumHeight: 240
    visible: !verificationRun
    flags: verificationRun ? verificationFlags : Qt.Window
    title: "yalahOO!!!"
    color: sceneAdapter.lighting.night ? "#08121f" : "#e8e5e1"
    MusicStage {
        anchors.fill: parent
        librarySession: window.librarySession
        motionController: window.motionController
        sceneAdapter: window.sceneAdapter
        openingController: window.openingController
        inputRouter: window.inputRouter
        rulerController: window.rulerController
        presentationController: window.presentationController
        detailModel: window.detailModel
        overviewController: window.overviewController
        reviewPlayingTitle: window.reviewPlayingTitle
        preferencesError: window.preferencesError
        geometryProbe: window.geometryProbe
    }
}
