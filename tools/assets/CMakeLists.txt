qt_add_executable(prepare-assets PrepareAssets.cpp GeometryAudit.cpp)
target_include_directories(prepare-assets PRIVATE "${PROJECT_SOURCE_DIR}/src/app")
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
target_include_directories(export-branding PRIVATE "${PROJECT_SOURCE_DIR}/src/app")
target_link_libraries(export-branding PRIVATE Qt6::Gui Qt6::Svg)
yalahoo_platform(export-branding)
set_target_properties(
    export-branding
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/tools"
)
set_target_properties(export-branding PROPERTIES WIN32_EXECUTABLE FALSE MACOSX_BUNDLE FALSE)
