# The executable and the deployment scanner consume the same production QML list.
# Research QML remains outside this closure even when it lives under the source root.
set(deployment_qml "${app_qml}" ${browse_qml_files})
set(deployment_architecture "${CMAKE_SYSTEM_PROCESSOR}")
if(WIN32 AND CMAKE_CXX_COMPILER_ARCHITECTURE_ID)
    set(deployment_architecture "${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}")
endif()
set(deployment_qml_json)
foreach(file IN LISTS deployment_qml)
    file(RELATIVE_PATH relative "${PROJECT_SOURCE_DIR}/qml" "${file}")
    list(APPEND deployment_qml_json "\"${relative}\"")
endforeach()
list(JOIN deployment_qml_json ",\n    " deployment_qml_json)
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/deployment.json" CONTENT
"{
  \"configuration\": \"$<CONFIG>\",
  \"review\": $<IF:$<BOOL:${YALAHOO_REVIEW}>,true,false>,
  \"system\": \"${CMAKE_SYSTEM_NAME}\",
  \"architecture\": \"${deployment_architecture}\",
  \"compiler\": \"${CMAKE_CXX_COMPILER_ID}\",
  \"qtVersion\": \"${Qt6_VERSION}\",
  \"qtKit\": \"${YALAHOO_SELECTED_QT}\",
  \"executable\": \"$<TARGET_FILE:Yalahoo>\",
  \"qmlFiles\": [
    ${deployment_qml_json}
  ]
}
")

# Qt's native deployment API follows the compiled production imports, including
# Templates/Dialogs/Multimedia. Never install a Review executable as a product.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT YALAHOO_REVIEW)
    qt_generate_deploy_qml_app_script(TARGET Yalahoo OUTPUT_SCRIPT linux_deploy_script
        EXCLUDE_PLUGIN_TYPES sqldrivers)
    install(SCRIPT "${linux_deploy_script}")
    install(FILES "${PROJECT_SOURCE_DIR}/resources/branding/yalahoo.desktop"
        DESTINATION share/applications)
    install(FILES "${PROJECT_SOURCE_DIR}/resources/icons/yalahoo/yalahoo.svg"
        DESTINATION share/icons/hicolor/scalable/apps)
endif()
