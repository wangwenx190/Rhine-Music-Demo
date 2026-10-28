target_sources(YalahooScene PRIVATE "${PROJECT_SOURCE_DIR}/tests/render/StartupInventory.cpp")
target_include_directories(YalahooRhi PRIVATE "${PROJECT_SOURCE_DIR}/tests/support")
target_sources(YalahooRhi PRIVATE
    "${PROJECT_SOURCE_DIR}/tests/render/ReadbackQueue.cpp"
    "${PROJECT_SOURCE_DIR}/tests/render/ReadbackImage.cpp"
    "${PROJECT_SOURCE_DIR}/tests/render/SceneTelemetry.cpp"
    "${PROJECT_SOURCE_DIR}/tests/render/CoverInventory.cpp"
    "${PROJECT_SOURCE_DIR}/tests/render/RendererCapture.cpp"
    "${PROJECT_SOURCE_DIR}/tests/render/UniformCapture.cpp"
    "${PROJECT_SOURCE_DIR}/tests/render/AssetInventory.cpp")
target_include_directories(YalahooRhi PRIVATE "${PROJECT_SOURCE_DIR}/audit/rhi")
include("${PROJECT_SOURCE_DIR}/audit/rhi/NativeAudit.cmake")

if(YALAHOO_REVIEW)
    # Build-only wrapper executes production GLSL functions with independent math inputs.
    set(optics_source "${PROJECT_SOURCE_DIR}/resources/shaders/rhi/shell-body.glsl")
    set(optics_tail "${PROJECT_SOURCE_DIR}/tests/render/OpticsProbe.frag")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${optics_source}" "${optics_tail}")
    file(READ "${optics_source}" optics_body)
    string(PREPEND optics_body "#version 440\n")
    string(REPLACE "void main()" "void productionShellMain()" optics_body "${optics_body}")
    file(READ "${optics_tail}" optics_probe_tail)
    string(APPEND optics_body "\n${optics_probe_tail}")
    foreach(header shell-lighting frame shadow-filter fog color-output)
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
        BASE "${PROJECT_SOURCE_DIR}/tests/render" GLSL "330,300 es" HLSL 50 MSL 12
        FILES "${PROJECT_SOURCE_DIR}/tests/render/VisibilityProbe.frag"
              "${PROJECT_SOURCE_DIR}/tests/render/CoverSamplingProbe.frag")
endif()
