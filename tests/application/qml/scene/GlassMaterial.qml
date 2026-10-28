// Quick 3D review material exposes the source optical endpoints. Surface identity, warmth and
// per-case clarity are independent shader inputs.

import QtQuick
import QtQuick3D
import Yalahoo.Music

LightingMaterial {
    id: glass
    SourceOptics {
        id: optics
        surface: glass.surface
        warmth: glass.warmth
        clarity: glass.clarity
    }

    objectName: "caseMaterial" + surface
    property int surface: 0
    property real warmth: 1
    property real clarity: 0
    property bool compositorOutput: false
    property color nightTint: optics.nightTint
    property color dayTint: optics.dayTint
    property color nightAttenuation: optics.nightAttenuation
    property color dayAttenuation: optics.dayAttenuation
    property real transmission: optics.transmission
    property real thickness: optics.thickness
    property real roughness: optics.roughness
    property real attenuationDistance: optics.attenuationDistance
    property real clearcoat: optics.clearcoat
    property real coatRoughness: optics.coatRoughness
    property real environmentIntensity: lightingEnabled ? lighting.environmentIntensity :
                                                          optics.fallbackEnvironment
    property real exposure: lightingEnabled ? lighting.exposure : optics.fallbackExposure
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
