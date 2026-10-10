import QtQuick
import QtQuick3D

Node {
    id: root
    objectName: "Root"

    // Resources
    PrincipledMaterial {
        id: frosted_Polymer_material
        objectName: "Frosted_Polymer"
        baseColor: "#fffffdfa"
        roughness: 0.3400000035762787
        alphaMode: PrincipledMaterial.Opaque
        clearcoatAmount: 0.07999999821186066
        clearcoatRoughnessAmount: 0.3799999952316284
        transmissionFactor: 0.9599999785423279
        thicknessFactor: 0.017999999225139618
        attenuationDistance: 5
        attenuationColor: "#ffe5d0b5"
        indexOfRefraction: 1.4600000381469727
    }
    PrincipledMaterial {
        id: ivory_Edges_material
        objectName: "Ivory_Edges"
        baseColor: "#fffffdfa"
        roughness: 0.25
        alphaMode: PrincipledMaterial.Opaque
        clearcoatAmount: 0.1599999964237213
        clearcoatRoughnessAmount: 0.20000000298023224
        transmissionFactor: 0.8799999952316284
        thicknessFactor: 0.05999999865889549
        attenuationDistance: 4.5
        attenuationColor: "#ffe5d0b5"
        indexOfRefraction: 1.4600000381469727
    }
    PrincipledMaterial {
        id: optical_Diffuser_material
        objectName: "Optical_Diffuser"
        baseColor: "#fffffdfa"
        roughness: 0.3799999952316284
        alphaMode: PrincipledMaterial.Opaque
        clearcoatAmount: 0.07999999821186066
        clearcoatRoughnessAmount: 0.3799999952316284
        transmissionFactor: 0.800000011920929
        thicknessFactor: 0.03799999877810478
        attenuationDistance: 4.5
        attenuationColor: "#ffe5d0b5"
        indexOfRefraction: 1.4600000381469727
    }

    // Nodes:
    Node {
        id: rhine_Music_Case_V040
        objectName: "Rhine_Music_Case_V040"
        Model {
            id: music_Case_V040__Frosted_Polymer
            objectName: "Music_Case_V040__Frosted_Polymer"
            source: "meshes/meshes_0__mesh.mesh"
            materials: [
                frosted_Polymer_material
            ]
        }
        Model {
            id: music_Case_V040__Ivory_Edges
            objectName: "Music_Case_V040__Ivory_Edges"
            source: "meshes/meshes_1__mesh.mesh"
            materials: [
                ivory_Edges_material
            ]
        }
        Model {
            id: music_Case_V040__Optical_Diffuser
            objectName: "Music_Case_V040__Optical_Diffuser"
            source: "meshes/meshes_2__mesh.mesh"
            materials: [
                optical_Diffuser_material
            ]
        }
    }

    // Animations:
}
