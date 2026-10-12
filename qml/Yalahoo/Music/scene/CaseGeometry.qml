import QtQuick
import QtQuick3D

Node {
    id: caseRoot
    required property Material lid
    required property Material spine
    required property Material tray
    required property Material paper
    property bool showPaper: true
    property string layer: "all"
    Model { source: "qrc:/assets/models/meshes/meshes_0__mesh.mesh"; materials: caseRoot.lid; castsShadows: false; visible: caseRoot.layer === "all" || caseRoot.layer === "Frosted_Polymer" }
    Model { source: "qrc:/assets/models/meshes/meshes_1__mesh.mesh"; materials: caseRoot.spine; castsShadows: false; visible: caseRoot.layer === "all" || caseRoot.layer === "Ivory_Edges" }
    Model { source: "qrc:/assets/models/meshes/meshes_2__mesh.mesh"; materials: caseRoot.tray; castsShadows: true; visible: caseRoot.layer === "all" || caseRoot.layer === "Optical_Diffuser" }
    Model {
        source: "#Rectangle"
        position: Qt.vector3d(.17,1.85,.027)
        scale: Qt.vector3d(.0306,.0306,1)
        materials: caseRoot.paper
        visible: caseRoot.showPaper
        castsShadows: false
    }
}
