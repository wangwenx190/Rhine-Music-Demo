import QtQuick
import QtQuick3D
import Yalahoo.Music

View3D {
    id: view
    required property SceneAdapter adapter
    property bool geometryOnly: false
    property bool materialReview: false
    property vector3d reviewCasePosition: Qt.vector3d(0,0,0)
    property bool lightingEnabled: !materialReview
    property real warmth: adapter.lighting.warmth
    property real clarity: adapter.detail
    property string reviewLayer: "all"
    property url coverSource: "qrc:/assets/demo-covers/square.png"
    property vector3d paperIrradiance: Qt.vector3d(1,1,1)
    objectName: "quick3DProbe"
    camera: sceneCamera
    environment: SceneEnvironment {
        backgroundMode: view.geometryOnly ? SceneEnvironment.Transparent : SceneEnvironment.Color
        clearColor: view.adapter.lighting.backgroundColor
        antialiasingMode: SceneEnvironment.MSAA
        antialiasingQuality: SceneEnvironment.High
        tonemapMode: SceneEnvironment.TonemapModeNone
        effects: view.geometryOnly ? [] : [materialOutput]
    }
    Effect {
        id: materialOutput
        passes: Pass {
            shaders: Shader {
                stage: Shader.Fragment
                shader: "qrc:/assets/shaders/material-output.frag"
            }
        }
    }
    Model {
        visible: !view.geometryOnly
        source: "#Rectangle"
        position: view.materialReview ? view.reviewCasePosition.plus(Qt.vector3d(0,1.85,-0.25)) : view.adapter.casePosition.plus(Qt.vector3d(0,1.85,-0.25))
        scale: Qt.vector3d(10,10,1)
        materials: CustomMaterial {
            property bool lightingEnabled: view.lightingEnabled
            property vector3d backgroundLinear: view.adapter.lighting.backgroundLinear
            property vector3d backgroundSrgb: Qt.vector3d((7+227*view.warmth)/255,(17+212*view.warmth)/255,(31+194*view.warmth)/255)
            shadingMode: CustomMaterial.Unshaded
            cullMode: Material.NoCulling
            fragmentShader: "qrc:/assets/shaders/material-background.frag"
        }
    }
    // All authored units, aim/orientation and optical values belong to SceneFrame.
    PerspectiveCamera {
        id: sceneCamera
        objectName: "sceneCamera"
        position: view.adapter.cameraPosition
        rotation: view.adapter.cameraRotation
        fieldOfView: view.adapter.fieldOfView
        fieldOfViewOrientation: PerspectiveCamera.Vertical
        clipNear: view.adapter.clipNear
        clipFar: view.adapter.clipFar
    }
    PrincipledMaterial {
        id: geometryMaterial
        lighting: PrincipledMaterial.NoLighting
        baseColor: "#668674"
        cullMode: Material.NoCulling
    }
    CaseMaterials {
        id: caseMaterials
        lighting: view.adapter.lighting
        lightingEnabled: view.lightingEnabled
        warmth: view.warmth
        clarity: view.clarity
        coverSource: view.coverSource
        paperIrradiance: view.paperIrradiance
    }
    Node {
        objectName: "inspectionCase"
        position: view.materialReview ? view.reviewCasePosition : view.adapter.casePosition
        Model {
            objectName: "caseLidProbe"
            source: "qrc:/assets/models/meshes/meshes_0__mesh.mesh"
            visible: !view.materialReview || view.reviewLayer === "all" || view.reviewLayer === "Frosted_Polymer"
            materials: view.geometryOnly ? geometryMaterial : caseMaterials.lid
        }
        Model {
            objectName: "caseSpineProbe"
            source: "qrc:/assets/models/meshes/meshes_1__mesh.mesh"
            visible: !view.materialReview || view.reviewLayer === "all" || view.reviewLayer === "Ivory_Edges"
            materials: view.geometryOnly ? geometryMaterial : caseMaterials.spine
        }
        Model {
            objectName: "caseTrayProbe"
            source: "qrc:/assets/models/meshes/meshes_2__mesh.mesh"
            visible: !view.materialReview || view.reviewLayer === "all" || view.reviewLayer === "Optical_Diffuser"
            materials: view.geometryOnly ? geometryMaterial : caseMaterials.tray
        }
    }
    Model {
        objectName: "albumPaper"
        visible: !view.geometryOnly && (!view.materialReview || view.reviewLayer === "all" || view.reviewLayer === "paper")
        source: "#Rectangle"
        position: view.materialReview ? view.reviewCasePosition.plus(Qt.vector3d(0.17,1.85,0.027)) : view.adapter.casePosition.plus(Qt.vector3d(0.17,1.85,0.027))
        scale: Qt.vector3d(0.0306,0.0306,1)
        materials: caseMaterials.paper
    }
    function qtProjection(point: vector3d): vector3d { return mapFrom3DScene(point) }
}
