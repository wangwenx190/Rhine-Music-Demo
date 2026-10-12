pragma ComponentBehavior: Bound
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
    property bool occlusionEnabled: !geometryOnly && !materialReview
    property bool shadowsEnabled: occlusionEnabled
    property int occlusionChannel: 0
    property real renderNear: adapter.clipNear
    property real renderFar: adapter.clipFar
    property real aoRadius: .18
    property bool shadowProbe: false
    property var reviewNeighbors: []
    property bool postprocessEnabled: !geometryOnly && !materialReview
    property bool reviewDisableBokeh: false
    property bool reviewDisableSmaa: false
    property int reviewAntialiasing: 1 // 0 off, 1 production MSAA, 2 SSAA diagnostic
    property real reviewDpr: 0
    readonly property bool colorPipeline: postprocessEnabled || occlusionEnabled
    readonly property bool bokehActive: postprocessEnabled && !reviewDisableBokeh && occlusionChannel === 0 && adapter.quality.values.depthOfField > 0
    readonly property bool smaaActive: postprocessEnabled && !reviewDisableSmaa && occlusionChannel === 0 && adapter.quality.values.antialias === "smaa"
    readonly property real currentDpr: Screen.devicePixelRatio
    onCurrentDprChanged: updateQuality()
    readonly property vector3d entityOrigin: materialReview ? reviewCasePosition : adapter.casePosition
    explicitTextureWidth: colorPipeline ? adapter.quality.targetSize.width : 0
    explicitTextureHeight: colorPipeline ? adapter.quality.targetSize.height : 0
    objectName: "quick3DProbe"
    camera: sceneCamera
    environment: SceneEnvironment {
        backgroundMode: view.geometryOnly ? SceneEnvironment.Transparent : SceneEnvironment.Color
        clearColor: view.adapter.lighting.backgroundColor
        antialiasingMode: view.reviewAntialiasing === 0 ? SceneEnvironment.NoAA : view.reviewAntialiasing === 2 ? SceneEnvironment.SSAA : SceneEnvironment.MSAA
        antialiasingQuality: SceneEnvironment.High
        tonemapMode: SceneEnvironment.TonemapModeNone
        effects: view.geometryOnly ? [] : view.compositionEffects()
    }
    function compositionEffects(): list<Effect> {
        let result = []
        if (occlusionEnabled) result.push(ssao)
        if (bokehActive || smaaActive) result.push(bokehActive ? smaaActive ? combined : bokeh : smaa)
        result.push(materialOutput)
        return result
    }
    Effect {
        id: materialOutput
        property bool rawChannel: view.occlusionEnabled && view.occlusionChannel !== 0
        property bool outputAces: view.colorPipeline
        property real exposure: view.adapter.lighting.exposure
        passes: Pass {
            shaders: Shader {
                stage: Shader.Fragment
                shader: "qrc:/assets/shaders/material-output.frag"
            }
        }
    }
    PostprocessEffect {
        id: bokeh
        objectName: "bokehEffect"
        bokehEnabled: true
        smaaEnabled: false
        bufferPrefix: "bokeh"
        cameraNear: view.renderNear
        cameraFar: view.renderFar
        focus: view.entityOrigin.plus(Qt.vector3d(0,2,0)).minus(view.adapter.cameraPosition).dotProduct(view.adapter.cameraAim.minus(view.adapter.cameraPosition).normalized())
        aperture: (.0003+.0005*view.clarity)*view.adapter.quality.values.depthOfField/100
        aspect: view.width/view.height
        depthTexture: Texture { sourceItem: bokehDepthLoader.item as Item; minFilter: Texture.Nearest; magFilter: Texture.Nearest }
    }
    // Immutable command layouts: preset changes select a prepared Effect rather
    // than retargeting a live Pass/BufferInput while GPU commands are in flight.
    PostprocessEffect {
        id: smaa
        bokehEnabled: false; smaaEnabled: true; bufferPrefix: "smaa"
        depthTexture: bokeh.depthTexture
    }
    PostprocessEffect {
        id: combined
        bokehEnabled: true; smaaEnabled: true; bufferPrefix: "combined"
        cameraNear: bokeh.cameraNear; cameraFar: bokeh.cameraFar
        focus: bokeh.focus; aperture: bokeh.aperture; aspect: bokeh.aspect
        depthTexture: bokeh.depthTexture
    }
    Loader {
        id: bokehDepthLoader
        active: view.postprocessEnabled
        visible: false
        sourceComponent: OcclusionPrepass {
            adapter: view.adapter
            origin: view.entityOrigin
            neighbors: view.reviewNeighbors
            cameraNear: view.renderNear
            cameraFar: view.renderFar
            targetSize: view.adapter.quality.targetSize
            packedDepth: true
        }
    }
    Loader {
        id: normalLoader
        active: view.occlusionEnabled && (view.adapter.quality.aoSamples > 0 || view.occlusionChannel === 3 || view.occlusionChannel === 4)
        visible: false
        sourceComponent: OcclusionPrepass {
            adapter: view.adapter
            origin: view.entityOrigin
            neighbors: view.reviewNeighbors
            cameraNear: view.renderNear
            cameraFar: view.renderFar
        }
    }
    SsaoEffect {
        id: ssao
        quality: view.adapter.quality
        channel: view.occlusionChannel
        cameraNear: view.renderNear
        cameraFar: view.renderFar
        kernelRadius: view.aoRadius
        projectionPlane: Qt.vector3d(Math.tan(view.adapter.fieldOfView*Math.PI/360)*view.width/view.height,Math.tan(view.adapter.fieldOfView*Math.PI/360),0)
        geometryTexture: Texture { sourceItem: normalLoader.item as Item; minFilter: Texture.Nearest; magFilter: Texture.Nearest }
        depthTexture: Texture { sourceItem: depthLoader.item as Item; minFilter: Texture.Nearest; magFilter: Texture.Nearest }
    }
    Loader {
        id: depthLoader
        active: normalLoader.active
        visible: false
        sourceComponent: OcclusionPrepass {
            adapter: view.adapter
            origin: view.entityOrigin
            neighbors: view.reviewNeighbors
            cameraNear: view.renderNear
            cameraFar: view.renderFar
            packedDepth: true
        }
    }
    DirectionalLight {
        objectName: "contactShadowLight"
        // Qt light emits along +Z. Rotate +Z towards the source key's target.
        rotation: Quaternion.lookAt(Qt.vector3d(-6,14,-5),Qt.vector3d(0,0,0),Qt.vector3d(0,0,1),Qt.vector3d(0,1,0))
        brightness: 1
        castsShadow: view.shadowsEnabled && view.adapter.quality.shadows > 0
        shadowFactor: 32
        shadowBias: .035
        shadowMapFar: 45
        shadowMapQuality: view.adapter.quality.shadows <= 1024 ? Light.ShadowMapQualityHigh
            : view.adapter.quality.shadows <= 2048 ? Light.ShadowMapQualityVeryHigh : Light.ShadowMapQualityUltra
        softShadowQuality: Light.PCF16
        pcfFactor: 4
        use32BitShadowmap: true
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
        clipNear: view.renderNear
        clipFar: view.renderFar
    }
    PrincipledMaterial {
        id: geometryMaterial
        lighting: PrincipledMaterial.NoLighting
        baseColor: "#668674"
        cullMode: Material.NoCulling
    }
    Model {
        visible: view.shadowProbe
        source: "#Rectangle"
        position: Qt.vector3d(0,-.05,0)
        eulerRotation.x: -90
        scale: Qt.vector3d(.12,.12,1)
        castsShadows: false
        materials: CustomMaterial {
            shadingMode: CustomMaterial.Shaded
            fragmentShader: "qrc:/assets/shaders/shadow-probe.frag"
        }
    }
    CaseMaterials {
        id: caseMaterials
        lighting: view.adapter.lighting
        lightingEnabled: view.lightingEnabled
        warmth: view.warmth
        clarity: view.clarity
        compositorOutput: view.colorPipeline
        coverSource: view.coverSource
        paperIrradiance: view.paperIrradiance
    }
    Node {
        objectName: "inspectionCase"
        position: view.materialReview ? view.reviewCasePosition : view.adapter.casePosition
        Model {
            objectName: "caseLidProbe"
            source: "qrc:/assets/models/meshes/meshes_0__mesh.mesh"
            castsShadows: false
            visible: !view.materialReview || view.reviewLayer === "all" || view.reviewLayer === "Frosted_Polymer"
            materials: view.geometryOnly ? geometryMaterial : caseMaterials.lid
        }
        Model {
            objectName: "caseSpineProbe"
            source: "qrc:/assets/models/meshes/meshes_1__mesh.mesh"
            castsShadows: false
            visible: !view.materialReview || view.reviewLayer === "all" || view.reviewLayer === "Ivory_Edges"
            materials: view.geometryOnly ? geometryMaterial : caseMaterials.spine
        }
        Model {
            objectName: "caseTrayProbe"
            source: "qrc:/assets/models/meshes/meshes_2__mesh.mesh"
            castsShadows: true
            visible: !view.materialReview || view.reviewLayer === "all" || view.reviewLayer === "Optical_Diffuser"
            materials: view.geometryOnly ? geometryMaterial : caseMaterials.tray
        }
    }
    Model {
        objectName: "albumPaper"
        castsShadows: false
        visible: !view.geometryOnly && (!view.materialReview || view.reviewLayer === "all" || view.reviewLayer === "paper")
        source: "#Rectangle"
        position: view.materialReview ? view.reviewCasePosition.plus(Qt.vector3d(0.17,1.85,0.027)) : view.adapter.casePosition.plus(Qt.vector3d(0.17,1.85,0.027))
        scale: Qt.vector3d(0.0306,0.0306,1)
        materials: caseMaterials.paper
    }
    Repeater3D {
        model: view.reviewNeighbors
        delegate: CaseGeometry {
            required property var modelData
            position: Qt.vector3d(modelData[0],modelData[1],modelData[2])
            layer: view.materialReview ? view.reviewLayer : "all"
            showPaper: !view.materialReview || view.reviewLayer === "all" || view.reviewLayer === "paper"
            lid: caseMaterials.lid; spine: caseMaterials.spine; tray: caseMaterials.tray; paper: caseMaterials.paper
        }
    }
    function updateQuality(): void {
        adapter.quality.resize(Math.max(1,width),Math.max(1,height),materialReview ? 1 : Math.min(width/1920,height/1080),reviewDpr>0 ? reviewDpr : currentDpr,4096)
    }
    onWidthChanged: updateQuality()
    onHeightChanged: updateQuality()
    Component.onCompleted: updateQuality()
    function qtProjection(point: vector3d): vector3d { return mapFrom3DScene(point) }
}
