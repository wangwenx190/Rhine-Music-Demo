include("${PROJECT_SOURCE_DIR}/cmake/ProbeQml.cmake")
qt_add_executable(source-ui-probe SourceUiProbe.cpp TitleSourceProbe.cpp RulerSourceProbe.cpp)
yalahoo_tool(source-ui-probe)
yalahoo_probe_qml(source-ui-probe TitleReview.qml RulerReview.qml)

qt_add_executable(metadata-network-probe MetadataNetworkProbe.cpp)
target_link_libraries(metadata-network-probe PRIVATE YalahooCore Qt6::Network)
yalahoo_tool(metadata-network-probe)
