qt_add_executable(
    Yalahoo
    main.cpp
    AssetCatalog.cpp
    AssetCatalog.h
    BrowseReview.cpp
    BrowseReview.h
    OverviewReview.cpp
    OverviewReview.h
    DetailReview.cpp
    DetailReview.h
    NavigationReview.cpp
    NavigationReview.h
)
if(YALAHOO_REVIEW)
    target_sources(
        Yalahoo
        PRIVATE
        SceneReview.cpp
        SceneReview.h
        MaterialReview.cpp
        MaterialReview.h
        RenderProbe.cpp
        RenderProbe.h
        IntegrationReview.cpp
        IntegrationReview.h
        FixedTextureData.cpp
        FixedTextureData.h
    )
    qt_add_resources(
        Yalahoo
        render_probe
        PREFIX
        /review
        BASE
        "${PROJECT_SOURCE_DIR}/tools/review"
        FILES
        "${PROJECT_SOURCE_DIR}/tools/review/InstanceProbe.qml"
        "${PROJECT_SOURCE_DIR}/tools/review/probe-atlas.vert"
        "${PROJECT_SOURCE_DIR}/tools/review/probe-atlas.frag"
        "${PROJECT_SOURCE_DIR}/tools/review/instanced-glass.vert"
        "${PROJECT_SOURCE_DIR}/tools/review/instanced-paper.vert"
    )
    target_include_directories(Yalahoo PRIVATE "${PROJECT_SOURCE_DIR}/tools/review")
    target_link_libraries(Yalahoo PRIVATE Qt6::GuiPrivate)
    target_sources(Yalahoo PRIVATE "${PROJECT_SOURCE_DIR}/tools/review/OffscreenSurface.h")
    qt_add_resources(Yalahoo material_probe PREFIX /review
        BASE "${PROJECT_SOURCE_DIR}/tests/fixtures/materials"
        FILES "${PROJECT_SOURCE_DIR}/tests/fixtures/materials/paper-probe.png")
    target_compile_definitions(Yalahoo PRIVATE YALAHOO_REVIEW)
endif()
include("${PROJECT_SOURCE_DIR}/cmake/Assets.cmake")
yalahoo_add_assets(Yalahoo)
set_target_properties(
    Yalahoo
    PROPERTIES
    OUTPUT_NAME
    yalahoo
    RUNTIME_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/bin"
)
if(WIN32)
    set_target_properties(Yalahoo PROPERTIES WIN32_EXECUTABLE TRUE)
elseif(APPLE)
    set_target_properties(
        Yalahoo
        PROPERTIES
        MACOSX_BUNDLE
        TRUE
        MACOSX_BUNDLE_BUNDLE_NAME
        "yalahOO!!!"
        MACOSX_BUNDLE_GUI_IDENTIFIER
        "yalahoo.music"
        MACOSX_BUNDLE_INFO_PLIST
        "${PROJECT_SOURCE_DIR}/cmake/Info.plist.in"
    )
elseif(UNIX)
    set_target_properties(Yalahoo PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")
endif()
set(app_qml "${PROJECT_SOURCE_DIR}/qml/Yalahoo/Music/App.qml")
set_source_files_properties("${app_qml}" PROPERTIES QT_RESOURCE_ALIAS App.qml)
include("${PROJECT_SOURCE_DIR}/cmake/BrowseQml.cmake")
yalahoo_browse_qml(browse_qml_files)
set(bootstrap_qml "${PROJECT_SOURCE_DIR}/qml/Yalahoo/Music/BootstrapProbe.qml")
set_source_files_properties("${bootstrap_qml}" PROPERTIES QT_RESOURCE_ALIAS BootstrapProbe.qml)
qt_add_resources(Yalahoo branding PREFIX /assets BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES "${PROJECT_SOURCE_DIR}/resources/branding/logo-light.svg"
    "${PROJECT_SOURCE_DIR}/resources/branding/logo-dark.svg")
set(review_qml_files)
set(review_sources)
set(review_dependencies)
if(YALAHOO_REVIEW)
    set(scene_qml "${PROJECT_SOURCE_DIR}/qml/Yalahoo/Music/scene/GeometryScene.qml")
    set_source_files_properties("${scene_qml}" PROPERTIES QT_RESOURCE_ALIAS scene/GeometryScene.qml)
    set(material_qml_files)
    foreach(
        name
        CaseMaterials
        GlassMaterial
        LightingMaterial
        CaseGeometry
        OcclusionPrepass
        SsaoEffect
        PostprocessEffect
    )
        set(file "${PROJECT_SOURCE_DIR}/qml/Yalahoo/Music/scene/${name}.qml")
        set_source_files_properties("${file}" PROPERTIES QT_RESOURCE_ALIAS "scene/${name}.qml")
        list(APPEND material_qml_files "${file}")
    endforeach()
    qt_add_resources(
        Yalahoo
        material_assets
        PREFIX
        /assets
        BASE
        "${PROJECT_SOURCE_DIR}/resources"
        FILES
        "${PROJECT_SOURCE_DIR}/resources/shaders/case-glass.vert"
        "${PROJECT_SOURCE_DIR}/resources/shaders/case-glass.frag"
        "${PROJECT_SOURCE_DIR}/resources/shaders/album-paper.frag"
        "${PROJECT_SOURCE_DIR}/resources/shaders/album-paper.vert"
        "${PROJECT_SOURCE_DIR}/resources/shaders/material-output.frag"
        "${PROJECT_SOURCE_DIR}/resources/shaders/material-background.frag"
        "${PROJECT_SOURCE_DIR}/resources/environment/room-pmrem.rgba32f"
        "${PROJECT_SOURCE_DIR}/resources/environment/dfg.rgba32f"
        "${PROJECT_SOURCE_DIR}/resources/environment/ltc1.rgba32f"
        "${PROJECT_SOURCE_DIR}/resources/environment/ltc2.rgba32f"
        "${PROJECT_SOURCE_DIR}/resources/licenses/three-LICENSE"
        "${PROJECT_SOURCE_DIR}/resources/licenses/materials-NOTICE.md"
        "${PROJECT_SOURCE_DIR}/resources/manifests/materials.json"
    )
    set(occlusion_assets)
    list(APPEND occlusion_assets "${PROJECT_SOURCE_DIR}/resources/manifests/occlusion.json")
    foreach(name ao-prepass.frag ssao.frag ssao-blur.frag ssao-composite.frag shadow-probe.frag)
        list(APPEND occlusion_assets "${PROJECT_SOURCE_DIR}/resources/shaders/${name}")
    endforeach()
    foreach(count 16 32 64)
        foreach(kind kernel noise)
            list(
                APPEND
                occlusion_assets
                "${PROJECT_SOURCE_DIR}/resources/environment/ssao-${kind}-${count}.rgba32f"
            )
        endforeach()
    endforeach()
    qt_add_resources(
        Yalahoo
        occlusion_assets
        PREFIX
        /assets
        BASE
        "${PROJECT_SOURCE_DIR}/resources"
        FILES
        ${occlusion_assets}
    )
    qt_add_resources(
        Yalahoo
        postprocess_assets
        PREFIX
        /assets
        BASE
        "${PROJECT_SOURCE_DIR}/resources"
        FILES
        "${PROJECT_SOURCE_DIR}/resources/shaders/bokeh.frag"
        "${PROJECT_SOURCE_DIR}/resources/shaders/smaa-edges.frag"
        "${PROJECT_SOURCE_DIR}/resources/shaders/smaa-weights.frag"
        "${PROJECT_SOURCE_DIR}/resources/shaders/smaa-blend.frag"
        "${PROJECT_SOURCE_DIR}/resources/postprocess/smaa-area.png"
        "${PROJECT_SOURCE_DIR}/resources/postprocess/smaa-search.png"
        "${PROJECT_SOURCE_DIR}/resources/manifests/postprocess.json"
    )
    set(research_qml "${PROJECT_SOURCE_DIR}/qml/Yalahoo/Music/ResearchApp.qml")
    set_source_files_properties("${research_qml}" PROPERTIES QT_RESOURCE_ALIAS ResearchApp.qml)
    list(APPEND review_qml_files "${research_qml}" "${scene_qml}" ${material_qml_files})
    list(APPEND review_sources FixedTextureData.h)
    list(APPEND review_dependencies QtQuick3D)
endif()
qt_add_qml_module(Yalahoo URI Yalahoo.Music VERSION 1.0
    DEPENDENCIES QtQuick ${review_dependencies}
    RESOURCE_PREFIX /qt/qml
    QML_FILES "${app_qml}" "${bootstrap_qml}" ${browse_qml_files} ${review_qml_files}
    SOURCES QmlTypes.h ApplicationIdentity.h ${review_sources})
qt_add_shaders(
    Yalahoo
    r02_shader_probe
    PREFIX
    /assets/shaders
    BASE
    "${PROJECT_SOURCE_DIR}/resources/shaders"
    FILES
    "${PROJECT_SOURCE_DIR}/resources/shaders/probe.frag"
    "${PROJECT_SOURCE_DIR}/resources/shaders/opening-paper.frag"
    "${PROJECT_SOURCE_DIR}/resources/shaders/opening-blur.frag"
    "${PROJECT_SOURCE_DIR}/resources/shaders/edge-fade.frag"
    "${PROJECT_SOURCE_DIR}/resources/shaders/reel-slot.frag"
    "${PROJECT_SOURCE_DIR}/resources/shaders/overview-blur.frag"
    "${PROJECT_SOURCE_DIR}/resources/shaders/detail-tabs-blur.frag"
)
target_link_libraries(
    Yalahoo
    PRIVATE
    YalahooCore
    YalahooScene
    YalahooRhi
    YalahooNavigation
    YalahooText
    YalahooOpening
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickDialogs2
    Qt6::Multimedia
    Qt6::Concurrent
    Qt6::Svg
    Qt6::ShaderTools
    Qt6::CanvasPainter
)
yalahoo_platform(Yalahoo)
install(TARGETS Yalahoo BUNDLE DESTINATION . RUNTIME DESTINATION bin)

if(YALAHOO_REVIEW)
    target_link_libraries(Yalahoo PRIVATE Qt6::Quick3D Qt6::Quick3DPrivate Qt6::Quick3DRuntimeRenderPrivate)
endif()
