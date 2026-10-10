import QtQuick
import QtQuick3D

CustomMaterial {
    id: glass
    objectName: "caseMaterial"+surface
    property real exposureScale: 1
    property real environmentScale: 1.01
    property real transmissionScale: 1
    property real roughnessScale: 1.01
    property real thicknessScale: 0.91809
    property real attenuationScale: 1.0201
    property int surface: 0
    property real warmth: 1
    property real clarity: 0
    property color nightTint: surface === 2 ? "#c6d6e5" : "#f6fbff"
    property color dayTint: surface === 0 ? "#fffdf8" : surface === 1 ? "#e7e1d8" : "#ece6dc"
    property color nightAttenuation: surface === 2 ? "#f3e9db" : "#dceafd"
    property color dayAttenuation: "#f3e9db"
    property real transmission: Math.max(0,Math.min(1,(surface === 0 ? 0.96-0.02*warmth : surface === 1 ? 0.88-0.04*warmth : 0.8-0.1*warmth)*transmissionScale))
    property real thickness: (surface === 0 ? 0.018+0.004*warmth : surface === 1 ? 0.06+0.02*warmth : 0.038+0.012*warmth)*thicknessScale
    property real roughness: Math.max(0,Math.min(1,(surface === 0 ? (0.34+0.06*warmth)*(1-clarity)+(0.27+0.07*warmth)*clarity : surface === 1 ? 0.25+0.05*warmth : 0.38+0.08*warmth)*roughnessScale))
    property real attenuationDistance: (surface === 0 ? 5-1.2*warmth : surface === 1 ? 4.5-1.3*warmth : 4.5-2.5*warmth)*attenuationScale
    property real clearcoat: surface === 1 ? 0.16 : 0.08
    property real coatRoughness: surface === 1 ? 0.2 : 0.38
    property real environmentIntensity: (0.68-0.20*warmth)*environmentScale
    property real exposure: (1.08-0.03*warmth)*exposureScale
    property vector3d paperIrradiance: Qt.vector3d(1,1,1)
    required property Texture roomTexture
    required property Texture integrationTexture
    property TextureInput environmentMap: TextureInput { texture: glass.roomTexture }
    property TextureInput dfgMap: TextureInput { texture: glass.integrationTexture }
    shadingMode: CustomMaterial.Unshaded
    cullMode: Material.BackFaceCulling
    vertexShader: "qrc:/assets/shaders/case-glass.vert"
    fragmentShader: "qrc:/assets/shaders/case-glass.frag"
}
