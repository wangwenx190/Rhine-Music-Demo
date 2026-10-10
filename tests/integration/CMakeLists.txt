qt_add_executable(AssetTest AssetTest.cpp
    "${PROJECT_SOURCE_DIR}/tools/assets/GeometryAudit.cpp"
    "${PROJECT_SOURCE_DIR}/src/app/AssetCatalog.cpp")
target_include_directories(AssetTest PRIVATE "${PROJECT_SOURCE_DIR}/tools/assets" "${PROJECT_SOURCE_DIR}/src/app")
target_link_libraries(AssetTest PRIVATE Qt6::Gui Qt6::Test Qt6::Quick3D Qt6::Qml Qt6::Svg Qt6::Multimedia)
include("${PROJECT_SOURCE_DIR}/cmake/Assets.cmake")
yalahoo_add_assets(AssetTest)
yalahoo_platform(AssetTest)
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/verification")
add_test(NAME assets COMMAND AssetTest -o "${CMAKE_BINARY_DIR}/verification/assets-test.xml,junitxml" -o -,txt)
set_tests_properties(assets PROPERTIES TIMEOUT 90
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/verification"
    ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins")
if(WIN32)
    set_tests_properties(assets PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin")
endif()
add_test(NAME branding COMMAND export-branding "${PROJECT_SOURCE_DIR}" --verify-only
    --report "${CMAKE_BINARY_DIR}/verification/branding-report.json")
set_tests_properties(branding PROPERTIES TIMEOUT 90
    ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins")
if(WIN32)
    set_tests_properties(branding PROPERTIES ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin")
endif()
