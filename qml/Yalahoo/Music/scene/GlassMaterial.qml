// Quick 3D review material exposes the source optical endpoints. Surface identity, warmth and
// per-case clarity are independent shader inputs.

import QtQuick
import QtQuick3D

LightingMaterial {
    id: glass
    // Authored optical endpoints use the same units as ShellMaterial; clarity affects only the lid.
    readonly property real lidTransmission: 0.96
    readonly property real lidTransmissionWarmthLoss: 0.02
    readonly property real spineTransmission: 0.88
    readonly property real spineTransmissionWarmthLoss: 0.04
    readonly property real trayTransmission: 0.8
    readonly property real trayTransmissionWarmthLoss: 0.1
    readonly property real lidThickness: 0.018
    readonly property real lidThicknessWarmthGain: 0.004
    readonly property real spineThickness: 0.06
    readonly property real frostedLidWarmthGain: 0.06
    readonly property real spineThicknessWarmthGain: 0.02
    readonly property real otherCoat: 0.08
    readonly property real otherCoatRoughness: 0.38
    readonly property real lidAttenuationDistance: 5
    readonly property real trayThickness: 0.038
    readonly property real trayThicknessWarmthGain: 0.012
    readonly property real frostedLidRoughness: 0.34
    readonly property real clearLidRoughness: 0.27
    readonly property real clearLidRoughnessWarmthGain: 0.07
    readonly property real spineRoughness: 0.25
    readonly property real spineRoughnessWarmthGain: 0.05
    readonly property real trayRoughness: 0.38
    readonly property real trayRoughnessWarmthGain: 0.08
    readonly property real lidDistanceWarmthLoss: 1.2
    readonly property real edgeAttenuationDistance: 4.5
    readonly property real spineDistanceWarmthLoss: 1.3
    readonly property real trayDistanceWarmthLoss: 2.5
    readonly property real spineCoat: 0.16
    readonly property real spineCoatRoughness: 0.2
    readonly property real fallbackEnvironment: 0.68
    readonly property real fallbackEnvironmentWarmthLoss: 0.20
    readonly property real fallbackExposure: 1.08
    readonly property real fallbackExposureWarmthLoss: 0.03

    objectName: "caseMaterial" + surface
    property int surface: 0
    property real warmth: 1
    property real clarity: 0
    property bool compositorOutput: false
    property color nightTint: surface === 2 ? "#c6d6e5" : "#f6fbff"
    property color dayTint: surface === 0 ? "#fffdf8" : surface === 1 ? "#e7e1d8" : "#ece6dc"
    property color nightAttenuation: surface === 2 ? "#f3e9db" : "#dceafd"
    property color dayAttenuation: "#f3e9db"
    property real transmission: Math.max(0, Math.min(1, (surface === 0 ? glass.lidTransmission
                                                                         - glass.lidTransmissionWarmthLoss
                                                                         * warmth : surface === 1
                                                                         ? glass.spineTransmission
                                                                           - glass.spineTransmissionWarmthLoss
                                                                           * warmth :
                                                                           glass.trayTransmission
                                                                           - glass.trayTransmissionWarmthLoss
                                                                           * warmth)))
    property real thickness: (surface === 0 ? glass.lidThickness + glass.lidThicknessWarmthGain
                                              * warmth : surface === 1 ? glass.spineThickness
                                                                         + glass.spineThicknessWarmthGain
                                                                         * warmth :
                                                                         glass.trayThickness
                                                                         + glass.trayThicknessWarmthGain
                                                                         * warmth)
    property real roughness: Math.max(0, Math.min(1, (surface === 0 ? (glass.frostedLidRoughness
                                                                       + glass.frostedLidWarmthGain
                                                                       * warmth) * (1 - clarity) + (
                                                                          glass.clearLidRoughness
                                                                          + glass.clearLidRoughnessWarmthGain
                                                                          * warmth) * clarity :
                                                                      surface === 1
                                                                      ? glass.spineRoughness
                                                                        + glass.spineRoughnessWarmthGain
                                                                        * warmth :
                                                                        glass.trayRoughness
                                                                        + glass.trayRoughnessWarmthGain
                                                                        * warmth)))
    property real attenuationDistance: (surface === 0 ? glass.lidAttenuationDistance
                                                        - glass.lidDistanceWarmthLoss * warmth :
                                                        surface === 1
                                                        ? glass.edgeAttenuationDistance
                                                          - glass.spineDistanceWarmthLoss * warmth :
                                                          glass.edgeAttenuationDistance
                                                          - glass.trayDistanceWarmthLoss * warmth)
    property real clearcoat: surface === 1 ? glass.spineCoat : glass.otherCoat
    property real coatRoughness: surface === 1 ? glass.spineCoatRoughness : glass.otherCoatRoughness
    property real environmentIntensity: (lightingEnabled ? lighting.environmentIntensity :
                                                           glass.fallbackEnvironment
                                                           - glass.fallbackEnvironmentWarmthLoss
                                                           * warmth)
    property real exposure: (lightingEnabled ? lighting.exposure : glass.fallbackExposure
                                               - glass.fallbackExposureWarmthLoss * warmth)
    property vector3d paperIrradiance: Qt.vector3d(1, 1, 1)
    required property Texture roomTexture
    required property Texture ltc1Texture
    required property Texture ltc2Texture
    property TextureInput ltc1Map: TextureInput {
        texture: glass.ltc1Texture
    }
    property TextureInput ltc2Map: TextureInput {
        texture: glass.ltc2Texture
    }
    required property Texture integrationTexture
    property TextureInput environmentMap: TextureInput {
        texture: glass.roomTexture
    }
    property TextureInput dfgMap: TextureInput {
        texture: glass.integrationTexture
    }
    shadingMode: CustomMaterial.Unshaded
    cullMode: Material.BackFaceCulling
    vertexShader: "qrc:/assets/shaders/case-glass.vert"
    fragmentShader: "qrc:/assets/shaders/case-glass.frag"
}
