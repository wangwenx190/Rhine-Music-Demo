qt_add_executable(prepare-assets PrepareAssets.cpp AssetFiles.cpp
    "${PROJECT_SOURCE_DIR}/audit/assets/GeometryAudit.cpp")
target_include_directories(prepare-assets PRIVATE "${PROJECT_SOURCE_DIR}/audit/assets")
target_link_libraries(prepare-assets PRIVATE YalahooCore Qt6::Gui)
yalahoo_tool(prepare-assets)

qt_add_executable(export-branding ExportBranding.cpp AssetFiles.cpp)
target_link_libraries(export-branding PRIVATE YalahooCore Qt6::Gui Qt6::Svg)
yalahoo_tool(export-branding)
