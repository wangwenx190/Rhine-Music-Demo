import QtQuick
import QtQuick3D
import Yalahoo.Music
Node {
    id: pool
    required property SceneAdapter adapter
    required property Instancing table
    required property TextureData atlas
    property bool atlasEnabled: false
    property url coverSource: "qrc:/assets/demo-covers/square.png"
    Component.onCompleted: {
        materials.lid.vertexShader = "qrc:/review/instanced-glass.vert"
        materials.spine.vertexShader = "qrc:/review/instanced-glass.vert"
        materials.tray.vertexShader = "qrc:/review/instanced-glass.vert"
        materials.paper.vertexShader = "qrc:/review/instanced-paper.vert"
    }
    CaseMaterials {
        id: materials
        lighting: pool.adapter.lighting
        compositorOutput: true
        clarity: .65
        coverSource: pool.coverSource
    }
    CustomMaterial {
        id: atlasMaterial
        shadingMode: CustomMaterial.Unshaded
        property TextureInput atlasMap: TextureInput {
            texture: Texture {
                textureData: pool.atlas
                generateMipmaps: false
                minFilter: Texture.Nearest; magFilter: Texture.Nearest
                tilingModeHorizontal: Texture.ClampToEdge
                tilingModeVertical: Texture.ClampToEdge
            }
        }
        vertexShader: "qrc:/review/probe-atlas.vert"
        fragmentShader: "qrc:/review/probe-atlas.frag"
    }
    Model { source: "qrc:/assets/models/meshes/meshes_0__mesh.mesh"; materials: materials.lid; instancing: pool.table; castsShadows: false }
    Model { source: "qrc:/assets/models/meshes/meshes_1__mesh.mesh"; materials: materials.spine; instancing: pool.table; castsShadows: false }
    Model { source: "qrc:/assets/models/meshes/meshes_2__mesh.mesh"; materials: materials.tray; instancing: pool.table; castsShadows: true }
    Model {
        source: "#Rectangle"
        position: Qt.vector3d(.17,1.85,.027); scale: Qt.vector3d(.0306,.0306,1)
        instanceRoot: pool; instancing: pool.table
        materials: pool.atlasEnabled ? atlasMaterial : materials.paper
        castsShadows: false
    }
}
