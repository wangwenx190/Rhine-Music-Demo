import QtQuick
import QtQuick3D
import Yalahoo.Music

View3D {
    id: view
    required property SceneAdapter adapter
    objectName: "quick3DProbe"
    camera: sceneCamera
    environment: SceneEnvironment {
        backgroundMode: SceneEnvironment.Transparent
        antialiasingMode: SceneEnvironment.MSAA
        antialiasingQuality: SceneEnvironment.High
        tonemapMode: SceneEnvironment.TonemapModeNone
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
    Node {
        objectName: "inspectionCase"
        position: view.adapter.casePosition
        Model {
            objectName: "caseLidProbe"
            source: "qrc:/assets/models/meshes/meshes_0__mesh.mesh"
            materials: geometryMaterial
        }
        Model {
            objectName: "caseSpineProbe"
            source: "qrc:/assets/models/meshes/meshes_1__mesh.mesh"
            materials: geometryMaterial
        }
        Model {
            objectName: "caseTrayProbe"
            source: "qrc:/assets/models/meshes/meshes_2__mesh.mesh"
            materials: geometryMaterial
        }
    }
    function qtProjection(point: vector3d): vector3d { return mapFrom3DScene(point) }
}
