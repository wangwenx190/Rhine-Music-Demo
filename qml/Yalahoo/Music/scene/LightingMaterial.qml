import QtQuick
import QtQuick3D
import Yalahoo.Music

CustomMaterial {
    required property SelectionLighting lighting
    property bool lightingEnabled: true
    property vector3d rowColumn: lighting.rowColumn
    property vector4d rowSettings: lighting.rowSettings
    property real rowDetail: lighting.rowDetail
    property real rowFill: lighting.rowFill
    property vector3d lightColumn: lighting.lightColumn
    property real printAmbient: lighting.printAmbient
    property vector3d scatterColor: lighting.scatterColor
    property real scatterStrength: lighting.scatterStrength
    property vector3d spotPosition: lighting.spotPosition
    property vector3d spotTarget: lighting.spotTarget
    property vector3d spotColor: lighting.spotColor
    property real spotIntensity: lighting.spotIntensity
    property real areaIntensity: lighting.areaIntensity
    property vector3d areaPosition: lighting.areaPosition
    property vector3d areaTarget: lighting.areaTarget
    property vector3d keyColor: lighting.keyColor
    property vector3d skyColor: lighting.skyColor
    property vector3d groundColor: lighting.groundColor
    property vector3d lightIntensities: lighting.lightIntensities
}
