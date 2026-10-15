qt_add_library(
    YalahooRhi
    STATIC
    RhiItem.cpp
    RhiItem.h
    SceneItem.cpp
    SceneItem.h
    PreparedAssets.cpp
    PreparedAssets.h
    AtlasUpload.cpp
    AtlasUpload.h
    AtlasAllocator.h
    AtlasAllocator.cpp
    Postprocess.cpp
    Postprocess.h
    TransferAudit.h
    RhiApi.h
    ShellMaterial.cpp
    ShellMaterial.h
    "${PROJECT_SOURCE_DIR}/tools/assets/GeometryAudit.cpp"
)
target_include_directories(
    YalahooRhi
    PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    PRIVATE
    "${PROJECT_SOURCE_DIR}/tools/assets"
)
target_link_libraries(YalahooRhi PUBLIC YalahooScene YalahooNavigation YalahooText Qt6::Quick PRIVATE Qt6::GuiPrivate)
target_compile_features(YalahooRhi PUBLIC cxx_std_23)
if(YALAHOO_REVIEW)
    target_compile_definitions(YalahooRhi PRIVATE YALAHOO_RHI_AUDIT)
endif()
yalahoo_platform(YalahooRhi)
# Position invariance for the shared visibility/Equal color geometry requires MSL 2.1.
qt_add_shaders(YalahooRhi rhi_geometry PREFIX /rhi
    BASE "${PROJECT_SOURCE_DIR}/resources/shaders/rhi"
    GLSL "330,300 es" HLSL 50 MSL 21 FILES
    "${PROJECT_SOURCE_DIR}/resources/shaders/rhi/geometry.vert"
    "${PROJECT_SOURCE_DIR}/resources/shaders/rhi/geometry-instanced.vert")
qt_add_shaders(
    YalahooRhi
    rhi_shaders
    PREFIX
    /rhi
    BASE
    "${PROJECT_SOURCE_DIR}/resources/shaders/rhi"
    GLSL
    "330,300 es"
    HLSL
    50
    MSL
    12
    FILES
    "${PROJECT_SOURCE_DIR}/resources/shaders/rhi/opaque.frag"
    "${PROJECT_SOURCE_DIR}/resources/shaders/rhi/coverage.frag"
    "${PROJECT_SOURCE_DIR}/resources/shaders/rhi/copy.frag"
    "${PROJECT_SOURCE_DIR}/resources/shaders/rhi/prepass.frag"
    "${PROJECT_SOURCE_DIR}/resources/shaders/rhi/shell.frag"
    "${PROJECT_SOURCE_DIR}/resources/shaders/rhi/shell-basic.frag"
    "${PROJECT_SOURCE_DIR}/resources/shaders/rhi/shell-early.frag"
    "${PROJECT_SOURCE_DIR}/resources/shaders/rhi/shell-basic-early.frag"
    "${PROJECT_SOURCE_DIR}/resources/shaders/rhi/shadow.vert"
    "${PROJECT_SOURCE_DIR}/resources/shaders/rhi/shadow-instanced.vert"
    "${PROJECT_SOURCE_DIR}/resources/shaders/rhi/shadow.frag"
    "${PROJECT_SOURCE_DIR}/resources/shaders/rhi/output.vert"
    "${PROJECT_SOURCE_DIR}/resources/shaders/rhi/output.frag"
    "${PROJECT_SOURCE_DIR}/resources/shaders/rhi/ssao.frag"
    "${PROJECT_SOURCE_DIR}/resources/shaders/rhi/ssao16.frag"
    "${PROJECT_SOURCE_DIR}/resources/shaders/rhi/ssao64.frag"
    "${PROJECT_SOURCE_DIR}/resources/shaders/rhi/ssao-blur.frag"
    "${PROJECT_SOURCE_DIR}/resources/shaders/rhi/ssao-composite.frag"
    "${PROJECT_SOURCE_DIR}/resources/shaders/rhi/bokeh.frag"
    "${PROJECT_SOURCE_DIR}/resources/shaders/rhi/smaa-edges.frag"
    "${PROJECT_SOURCE_DIR}/resources/shaders/rhi/smaa-weights.frag"
    "${PROJECT_SOURCE_DIR}/resources/shaders/rhi/smaa-blend.frag"
)
qt_add_resources(
    YalahooRhi
    rhi_assets
    PREFIX
    /rhi
    BASE
    "${PROJECT_SOURCE_DIR}/resources"
    FILES
    "${PROJECT_SOURCE_DIR}/resources/models/music-case-v040.glb"
    "${PROJECT_SOURCE_DIR}/resources/environment/room-pmrem.rgba32f"
    "${PROJECT_SOURCE_DIR}/resources/environment/dfg.rgba32f"
    "${PROJECT_SOURCE_DIR}/resources/environment/ltc1.rgba32f"
    "${PROJECT_SOURCE_DIR}/resources/environment/ltc2.rgba32f"
    "${PROJECT_SOURCE_DIR}/resources/environment/ssao-kernel-16.rgba32f"
    "${PROJECT_SOURCE_DIR}/resources/environment/ssao-kernel-32.rgba32f"
    "${PROJECT_SOURCE_DIR}/resources/environment/ssao-kernel-64.rgba32f"
    "${PROJECT_SOURCE_DIR}/resources/environment/ssao-noise-16.rgba32f"
    "${PROJECT_SOURCE_DIR}/resources/environment/ssao-noise-32.rgba32f"
    "${PROJECT_SOURCE_DIR}/resources/environment/ssao-noise-64.rgba32f"
    "${PROJECT_SOURCE_DIR}/resources/postprocess/smaa-area.png"
    "${PROJECT_SOURCE_DIR}/resources/postprocess/smaa-search.png"
)

if(YALAHOO_REVIEW)
    # Build-only wrapper executes production GLSL functions with independent math inputs.
    set(optics_source "${PROJECT_SOURCE_DIR}/resources/shaders/rhi/shell.frag")
    set(optics_tail "${PROJECT_SOURCE_DIR}/tools/review/OpticsProbe.frag")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${optics_source}" "${optics_tail}")
    file(READ "${optics_source}" optics_body)
    string(REPLACE "void main()" "void productionShellMain()" optics_body "${optics_body}")
    file(READ "${optics_tail}" optics_probe_tail)
    string(APPEND optics_body "\n${optics_probe_tail}")
    foreach(header frame shadow-filter fog color-output)
        set(optics_header_path "${PROJECT_SOURCE_DIR}/resources/shaders/rhi/${header}.glsl")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${optics_header_path}")
        file(READ "${optics_header_path}" optics_header)
        string(REPLACE "#include \"${header}.glsl\"" "${optics_header}"
            optics_body "${optics_body}")
    endforeach()
    set(optics_generated "${CMAKE_CURRENT_BINARY_DIR}/optics-probe.frag")
    file(WRITE "${optics_generated}" "${optics_body}")
    qt_add_shaders(YalahooRhi review_optics PREFIX /rhi BASE "${CMAKE_CURRENT_BINARY_DIR}"
        GLSL "330,300 es" HLSL 50 MSL 12 FILES "${optics_generated}")
    qt_add_shaders(YalahooRhi review_visibility PREFIX /rhi
        BASE "${PROJECT_SOURCE_DIR}/tools/review" GLSL "330,300 es" HLSL 50 MSL 12
        FILES "${PROJECT_SOURCE_DIR}/tools/review/VisibilityProbe.frag")
endif()
