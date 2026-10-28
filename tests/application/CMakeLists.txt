target_sources(Yalahoo PRIVATE main.cpp VerificationOptions.cpp VerificationOptions.h
    DeploymentProbe.cpp DeploymentProbe.h OverviewReview.cpp OverviewReview.h
    DetailReview.cpp DetailReview.h NavigationReview.cpp NavigationReview.h)
target_include_directories(Yalahoo PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/src/app" "${PROJECT_SOURCE_DIR}/tests/support")
target_link_libraries(Yalahoo PRIVATE YalahooVerification)
if(YALAHOO_REVIEW)
    target_sources(Yalahoo PRIVATE MaterialReview.cpp MaterialReview.h MaterialReviewInput.h
        FixedTextureData.cpp FixedTextureData.h
        "${PROJECT_SOURCE_DIR}/tests/support/OffscreenSurface.h")
    target_include_directories(Yalahoo PRIVATE "${PROJECT_SOURCE_DIR}/audit/rhi")
    target_link_libraries(Yalahoo PRIVATE Qt6::GuiPrivate Qt6::Quick3D Qt6::Quick3DPrivate
        Qt6::Quick3DRuntimeRenderPrivate)
    target_compile_definitions(Yalahoo PRIVATE YALAHOO_REVIEW)
    qt_add_resources(Yalahoo material_probe PREFIX /review
        BASE "${PROJECT_SOURCE_DIR}/tests/fixtures/materials"
        FILES "${PROJECT_SOURCE_DIR}/tests/fixtures/materials/paper-probe.png")
    set(review_qml_files)
    foreach(name GeometryScene CaseMaterials GlassMaterial LightingMaterial)
        set(file "${CMAKE_CURRENT_SOURCE_DIR}/qml/scene/${name}.qml")
        set_source_files_properties("${file}" DIRECTORY "${PROJECT_SOURCE_DIR}/src/app" PROPERTIES QT_RESOURCE_ALIAS "scene/${name}.qml")
        list(APPEND review_qml_files "${file}")
    endforeach()
    set(review_qml_files ${review_qml_files} PARENT_SCOPE)
    set(review_sources "${CMAKE_CURRENT_SOURCE_DIR}/FixedTextureData.h" PARENT_SCOPE)
    set(review_dependencies QtQuick3D PARENT_SCOPE)
    include("${PROJECT_SOURCE_DIR}/cmake/Assets.cmake")
    yalahoo_environment_assets(environment_assets)
    qt_add_resources(Yalahoo material_assets PREFIX /assets
        BASE "${PROJECT_SOURCE_DIR}/resources" FILES
        "${PROJECT_SOURCE_DIR}/resources/shaders/case-glass.vert"
        "${PROJECT_SOURCE_DIR}/resources/shaders/case-glass.frag"
        "${PROJECT_SOURCE_DIR}/resources/shaders/album-paper.frag"
        "${PROJECT_SOURCE_DIR}/resources/shaders/album-paper.vert"
        "${PROJECT_SOURCE_DIR}/resources/shaders/material-output.frag"
        "${PROJECT_SOURCE_DIR}/resources/shaders/material-background.frag"
        ${environment_assets}
        "${PROJECT_SOURCE_DIR}/resources/licenses/three-LICENSE"
        "${PROJECT_SOURCE_DIR}/resources/licenses/materials-NOTICE.md"
        "${PROJECT_SOURCE_DIR}/resources/manifests/materials.json")
endif()
