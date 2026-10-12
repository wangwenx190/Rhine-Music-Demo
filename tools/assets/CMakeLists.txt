qt_add_executable(prepare-assets PrepareAssets.cpp GeometryAudit.cpp)
target_link_libraries(prepare-assets PRIVATE Qt6::Gui)
if(WIN32)
    target_link_libraries(prepare-assets PRIVATE dwrite)
endif()
yalahoo_platform(prepare-assets)
set_target_properties(
    prepare-assets
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/tools"
)

qt_add_executable(export-branding ExportBranding.cpp)
target_link_libraries(export-branding PRIVATE Qt6::Gui Qt6::Svg)
yalahoo_platform(export-branding)
set_target_properties(
    export-branding
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/tools"
)
set_target_properties(export-branding PROPERTIES WIN32_EXECUTABLE FALSE MACOSX_BUNDLE FALSE)
if(YALAHOO_REVIEW)
    qt_add_executable(postprocess-audit PostprocessAudit.cpp)
    target_link_libraries(postprocess-audit PRIVATE Qt6::Gui Qt6::Quick)
    yalahoo_platform(postprocess-audit)
    set_target_properties(
        postprocess-audit
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY
        "${CMAKE_BINARY_DIR}/tools"
        WIN32_EXECUTABLE
        FALSE
        MACOSX_BUNDLE
        FALSE
    )
    qt_add_executable(occlusion-audit OcclusionAudit.cpp)
    target_link_libraries(occlusion-audit PRIVATE Qt6::Gui Qt6::Quick)
    yalahoo_platform(occlusion-audit)
    set_target_properties(
        occlusion-audit
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY
        "${CMAKE_BINARY_DIR}/tools"
        WIN32_EXECUTABLE
        FALSE
        MACOSX_BUNDLE
        FALSE
    )
endif()
