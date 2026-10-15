qt_add_executable(IdentityTest IdentityTest.cpp)
qt_add_executable(TitleTest TitleTest.cpp "${PROJECT_SOURCE_DIR}/src/app/AssetCatalog.cpp")
target_include_directories(TitleTest PRIVATE "${PROJECT_SOURCE_DIR}/src/app")
target_link_libraries(TitleTest PRIVATE YalahooText Qt6::Test)
include("${PROJECT_SOURCE_DIR}/cmake/Assets.cmake")
yalahoo_add_assets(TitleTest)
yalahoo_platform(TitleTest)
add_test(NAME title-reels COMMAND TitleTest)
set_tests_properties(title-reels PROPERTIES TIMEOUT 60)
if(WIN32)
    set_tests_properties(title-reels PROPERTIES ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin")
endif()
qt_add_executable(NavigationTest NavigationTest.cpp)
target_include_directories(NavigationTest PRIVATE "${PROJECT_SOURCE_DIR}/src/app")
target_link_libraries(NavigationTest PRIVATE YalahooNavigation Qt6::Test)
yalahoo_platform(NavigationTest)
add_test(NAME navigation COMMAND NavigationTest)
set_tests_properties(navigation PROPERTIES TIMEOUT 60)
if(WIN32)
    set_tests_properties(navigation PROPERTIES ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin")
endif()
qt_add_executable(CoverCacheTest CoverCacheTest.cpp)
target_link_libraries(CoverCacheTest PRIVATE YalahooRhi Qt6::Test)
target_compile_definitions(CoverCacheTest PRIVATE COVER_REPORT="${CMAKE_BINARY_DIR}/verification/cover-cache.json")
yalahoo_platform(CoverCacheTest)
add_test(NAME cover-cache COMMAND CoverCacheTest)
set_tests_properties(cover-cache PROPERTIES TIMEOUT 90)
if(WIN32)
    set_tests_properties(cover-cache PROPERTIES ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin")
endif()
qt_add_executable(AtlasUploadTest AtlasUploadTest.cpp)
target_link_libraries(AtlasUploadTest PRIVATE YalahooRhi Qt6::Test)
yalahoo_platform(AtlasUploadTest)
add_test(NAME rhi-atlas-queue COMMAND AtlasUploadTest)
if(WIN32)
    set_tests_properties(rhi-atlas-queue PROPERTIES ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin")
endif()
qt_add_executable(VerificationWindowTest VerificationWindowTest.cpp)
target_include_directories(VerificationWindowTest PRIVATE "${PROJECT_SOURCE_DIR}/src/app")
target_link_libraries(VerificationWindowTest PRIVATE Qt6::Quick Qt6::Test)
target_compile_definitions(VerificationWindowTest PRIVATE
    VISIBLE_WINDOW_REPORT="${CMAKE_BINARY_DIR}/verification/native-window-visible.json")
yalahoo_platform(VerificationWindowTest)
add_test(NAME verification-window-policy COMMAND VerificationWindowTest hiddenNativeWindow)
set_tests_properties(verification-window-policy PROPERTIES ENVIRONMENT "YALAHOO_VERIFY_FOREGROUND=0")
if(NOT APPLE OR YALAHOO_MACOS_VISIBLE_TESTS)
    add_test(NAME verification-window-visible COMMAND VerificationWindowTest visibleNativeWindow)
    set_tests_properties(verification-window-visible PROPERTIES TIMEOUT 60
        ENVIRONMENT "YALAHOO_VERIFY_VISIBLE=1;QSG_RENDER_LOOP=threaded;QSG_NO_VSYNC=")
endif()
if(WIN32)
    target_link_libraries(VerificationWindowTest PRIVATE user32)
    set_tests_properties(
        verification-window-policy
        PROPERTIES
        ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin"
    )
endif()
qt_add_executable(RhiMaterialTest RhiMaterialTest.cpp)
target_link_libraries(RhiMaterialTest PRIVATE YalahooRhi Qt6::Test)
target_compile_definitions(RhiMaterialTest PRIVATE FIXTURE_ROOT="${PROJECT_SOURCE_DIR}")
yalahoo_platform(RhiMaterialTest)
add_test(NAME rhi-material-parameters COMMAND RhiMaterialTest)
if(WIN32)
    set_tests_properties(
        rhi-material-parameters
        PROPERTIES
        ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin"
    )
endif()
if(YALAHOO_REVIEW)
    add_test(NAME real-columns-gpu COMMAND archive-pool-audit
        "${CMAKE_BINARY_DIR}/verification/real-columns" --real-columns)
    set_tests_properties(real-columns-gpu PROPERTIES TIMEOUT 240
        ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QT_SCALE_FACTOR=1;QT_ENABLE_HIGHDPI_SCALING=0;QSG_RENDER_LOOP=threaded"
        FAIL_REGULAR_EXPRESSION "Failed to compile;QSpirvCompiler;Nonfinite RHI pixel")
    if(WIN32)
        set_tests_properties(real-columns-gpu PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin")
    endif()
    add_test(NAME archive-pool-gpu COMMAND archive-pool-audit
        "${CMAKE_BINARY_DIR}/verification/archive-pool")
    set_tests_properties(archive-pool-gpu PROPERTIES TIMEOUT 240
        ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QT_SCALE_FACTOR=1;QT_ENABLE_HIGHDPI_SCALING=0;QSG_RENDER_LOOP=threaded"
        FAIL_REGULAR_EXPRESSION "Failed to compile;QSpirvCompiler;Nonfinite RHI pixel")
    if(WIN32)
        set_tests_properties(archive-pool-gpu PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin")
    endif()
    add_test(NAME cover-gpu COMMAND cover-audit "${CMAKE_BINARY_DIR}/verification/covers")
    set_tests_properties(cover-gpu PROPERTIES TIMEOUT 180
        ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QT_SCALE_FACTOR=1;QT_ENABLE_HIGHDPI_SCALING=0;QSG_RENDER_LOOP=threaded")
    if(WIN32)
        set_tests_properties(cover-gpu PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin")
    endif()
    set(rhi_performance_arguments)
    if(YALAHOO_PERFORMANCE_RECORD_ONLY)
        list(APPEND rhi_performance_arguments --record-performance)
    endif()
    add_test(NAME rhi-postprocess-gpu COMMAND rhi-postprocess-audit
        "${CMAKE_BINARY_DIR}/verification/rhi-postprocess" ${rhi_performance_arguments})
    set_tests_properties(rhi-postprocess-gpu PROPERTIES TIMEOUT 420
        ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QT_SCALE_FACTOR=1;QT_ENABLE_HIGHDPI_SCALING=0;QSG_RENDER_LOOP=threaded;QSG_NO_VSYNC=1"
        FAIL_REGULAR_EXPRESSION "Failed to compile;QSpirvCompiler;Nonfinite RHI pixel")
    if(WIN32)
        set_tests_properties(rhi-postprocess-gpu PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin")
    endif()
    add_test(NAME rhi-shell-gpu COMMAND rhi-shell-audit
        "${PROJECT_SOURCE_DIR}/tests/fixtures/rhi-shell/visual.json"
        "${CMAKE_BINARY_DIR}/verification/rhi-shell")
    set_tests_properties(rhi-shell-gpu PROPERTIES TIMEOUT 240
        ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QT_SCALE_FACTOR=1;QSG_RENDER_LOOP=threaded"
        FAIL_REGULAR_EXPRESSION "Failed to compile;QSpirvCompiler;Nonfinite RHI pixel")
    if(WIN32)
        set_tests_properties(rhi-shell-gpu PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin")
    endif()
    add_test(NAME rhi-opaque-gpu COMMAND rhi-audit "${CMAKE_BINARY_DIR}/verification/rhi")
    set_tests_properties(
        rhi-opaque-gpu
        PROPERTIES
        TIMEOUT
        180
        ENVIRONMENT
        "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QT_SCALE_FACTOR=1;QT_ENABLE_HIGHDPI_SCALING=0;QSG_RENDER_LOOP=threaded"
    )
    if(WIN32)
        set_tests_properties(
            rhi-opaque-gpu
            PROPERTIES
            ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin"
        )
    endif()
endif()
qt_add_executable(ImageComparisonTest ImageComparisonTest.cpp)
target_link_libraries(ImageComparisonTest PRIVATE YalahooImageComparison Qt6::Test)
yalahoo_platform(ImageComparisonTest)
add_test(NAME image-comparison COMMAND ImageComparisonTest)
if(WIN32)
    set_tests_properties(
        image-comparison
        PROPERTIES
        ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin"
    )
endif()
qt_add_executable(RenderQualityTest RenderQualityTest.cpp)
target_link_libraries(RenderQualityTest PRIVATE YalahooScene Qt6::Test)
target_compile_definitions(RenderQualityTest PRIVATE FIXTURE_ROOT="${PROJECT_SOURCE_DIR}")
yalahoo_platform(RenderQualityTest)
add_test(
    NAME
    render-quality
    COMMAND
    RenderQualityTest
    -o
    "${CMAKE_BINARY_DIR}/verification/render-quality-test.xml,junitxml"
    -o
    -,txt
)
if(WIN32)
    set_tests_properties(
        render-quality
        PROPERTIES
        ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin"
    )
endif()
qt_add_executable(SceneTest SceneTest.cpp)
target_link_libraries(SceneTest PRIVATE YalahooScene Qt6::Test)
target_compile_definitions(
    SceneTest
    PRIVATE
    FIXTURE_ROOT="${PROJECT_SOURCE_DIR}"
    SCENE_REPORT="${CMAKE_BINARY_DIR}/verification/scene-errors.json"
)
yalahoo_platform(SceneTest)
add_test(
    NAME
    scene
    COMMAND
    SceneTest
    -o
    "${CMAKE_BINARY_DIR}/verification/scene-test.xml,junitxml"
    -o
    -,txt
)
set_tests_properties(scene PROPERTIES TIMEOUT 60)
if(WIN32)
    set_tests_properties(
        scene
        PROPERTIES
        ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin"
    )
endif()
if(YALAHOO_REVIEW)
    add_test(
        NAME
        runtime-render-backend
        COMMAND
        Yalahoo
        --verify-materials
        "${PROJECT_SOURCE_DIR}/tests/fixtures/render-backend/visual.json"
        --material-report
        "${CMAKE_BINARY_DIR}/verification/render-backend/runtime.json"
    )
    set_tests_properties(
        runtime-render-backend
        PROPERTIES
        TIMEOUT
        240
        FAIL_REGULAR_EXPRESSION
        "Failed to compile;QSpirvCompiler;TypeError;out of range"
        ENVIRONMENT
        "QT_ENABLE_HIGHDPI_SCALING=0;QT_SCALE_FACTOR=1;QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins"
    )
    add_test(
        NAME
        render-backend-gpu-audit
        COMMAND
        render-backend-audit
        "${PROJECT_SOURCE_DIR}/tests/fixtures/render-backend/visual.json"
        "${CMAKE_BINARY_DIR}/verification/render-backend"
        "${PROJECT_SOURCE_DIR}/docs/media/render-backend/web"
        "${CMAKE_BINARY_DIR}/verification/render-backend/gpu-checks.json"
    )
    set_tests_properties(
        render-backend-gpu-audit
        PROPERTIES
        DEPENDS
        runtime-render-backend
        TIMEOUT
        60
    )
    if(WIN32)
        set_tests_properties(
            runtime-render-backend
            render-backend-gpu-audit
            PROPERTIES
            ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin"
        )
    endif()
    foreach(dpr IN ITEMS 1 1.5 2)
        add_test(
            NAME
            runtime-postprocess-dpr-${dpr}
            COMMAND
            Yalahoo
            --verify-materials
            "${PROJECT_SOURCE_DIR}/tests/fixtures/postprocess/visual-dpr-${dpr}.json"
            --material-report
            "${CMAKE_BINARY_DIR}/verification/postprocess/dpr-${dpr}/runtime.json"
        )
        set_tests_properties(
            runtime-postprocess-dpr-${dpr}
            PROPERTIES
            TIMEOUT
            420
            FAIL_REGULAR_EXPRESSION
            "Failed to compile;QSpirvCompiler;TypeError;out of range"
            ENVIRONMENT
            "QT_ENABLE_HIGHDPI_SCALING=0;QT_SCALE_FACTOR=${dpr};QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins"
        )
        if(WIN32)
            set_tests_properties(
                runtime-postprocess-dpr-${dpr}
                PROPERTIES
                ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin"
            )
        endif()
    endforeach()
    add_test(
        NAME
        postprocess-gpu-audit
        COMMAND
        postprocess-audit
        "${PROJECT_SOURCE_DIR}/tests/fixtures/postprocess/visual.json"
        "${CMAKE_BINARY_DIR}/verification/postprocess"
        "${PROJECT_SOURCE_DIR}/docs/media/postprocess/web"
        "${CMAKE_BINARY_DIR}/verification/postprocess/gpu-checks.json"
    )
    set_tests_properties(
        postprocess-gpu-audit
        PROPERTIES
        DEPENDS
        "runtime-postprocess-dpr-1;runtime-postprocess-dpr-1.5;runtime-postprocess-dpr-2"
        TIMEOUT
        60
    )
    if(WIN32)
        set_tests_properties(
            postprocess-gpu-audit
            PROPERTIES
            ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin"
        )
    endif()
    add_test(
        NAME
        runtime-occlusion
        COMMAND
        Yalahoo
        --verify-materials
        "${PROJECT_SOURCE_DIR}/tests/fixtures/occlusion/visual.json"
        --material-report
        "${CMAKE_BINARY_DIR}/verification/occlusion/runtime.json"
    )
    set_tests_properties(
        runtime-occlusion
        PROPERTIES
        TIMEOUT
        180
        FAIL_REGULAR_EXPRESSION
        "Failed to compile;QSpirvCompiler;TypeError;out of range"
        ENVIRONMENT
        "QT_ENABLE_HIGHDPI_SCALING=0;QT_SCALE_FACTOR=1;QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins"
    )
    if(WIN32)
        set_tests_properties(
            runtime-occlusion
            PROPERTIES
            ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin"
        )
    endif()
    add_test(
        NAME
        occlusion-gpu-audit
        COMMAND
        occlusion-audit
        "${CMAKE_BINARY_DIR}/verification/occlusion/runtime.json"
        "${CMAKE_BINARY_DIR}/verification/occlusion"
        "${PROJECT_SOURCE_DIR}/docs/media/occlusion/web"
        "${CMAKE_BINARY_DIR}/verification/occlusion/gpu-checks.json"
    )
    set_tests_properties(occlusion-gpu-audit PROPERTIES DEPENDS runtime-occlusion TIMEOUT 60)
    if(WIN32)
        set_tests_properties(
            occlusion-gpu-audit
            PROPERTIES
            ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin"
        )
    endif()
    add_test(
        NAME
        runtime-materials
        COMMAND
        Yalahoo
        --verify-materials
        "${PROJECT_SOURCE_DIR}/tests/fixtures/materials/single-case.json"
        --material-report
        "${CMAKE_BINARY_DIR}/verification/materials/runtime.json"
    )
    set_tests_properties(
        runtime-materials
        PROPERTIES
        TIMEOUT
        150
        ENVIRONMENT
        "QT_ENABLE_HIGHDPI_SCALING=0;QT_SCALE_FACTOR=1;QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins"
    )
    if(WIN32)
        set_tests_properties(
            runtime-materials
            PROPERTIES
            ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin"
        )
    endif()
    add_test(
        NAME
        runtime-lighting
        COMMAND
        Yalahoo
        --verify-materials
        "${PROJECT_SOURCE_DIR}/tests/fixtures/lighting/visual.json"
        --material-report
        "${CMAKE_BINARY_DIR}/verification/lighting/runtime.json"
    )
    set_tests_properties(
        runtime-lighting
        PROPERTIES
        TIMEOUT
        150
        ENVIRONMENT
        "QT_ENABLE_HIGHDPI_SCALING=0;QT_SCALE_FACTOR=1;QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins"
    )
    if(WIN32)
        set_tests_properties(
            runtime-lighting
            PROPERTIES
            ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin"
        )
    endif()
    foreach(dpr IN ITEMS 1 1.5 2)
        add_test(
            NAME
            runtime-scene-dpr-${dpr}
            COMMAND
            Yalahoo
            --verify-scene
            "${PROJECT_SOURCE_DIR}/tests/fixtures/scene/camera-golden.json"
            --scene-report
            "${CMAKE_BINARY_DIR}/verification/scene-dpr-${dpr}/runtime.json"
        )
        set_tests_properties(
            runtime-scene-dpr-${dpr}
            PROPERTIES
            TIMEOUT
            90
            ENVIRONMENT
            "QT_ENABLE_HIGHDPI_SCALING=0;QT_SCALE_FACTOR=${dpr};QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QSG_INFO=1"
        )
        if(WIN32)
            set_tests_properties(
                runtime-scene-dpr-${dpr}
                PROPERTIES
                ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin"
            )
        endif()
    endforeach()
endif()
target_link_libraries(IdentityTest PRIVATE YalahooCore Qt6::Gui Qt6::Test)
yalahoo_platform(IdentityTest)
add_test(NAME identity COMMAND IdentityTest)
qt_add_executable(ModuleProbe ModuleProbe.cpp)
target_link_libraries(
    ModuleProbe
    PRIVATE
    Qt6::Gui
    Qt6::Test
    Qt6::Quick3D
    Qt6::Multimedia
    Qt6::ShaderTools
)
yalahoo_platform(ModuleProbe)
add_test(NAME module-link-and-plugins COMMAND ModuleProbe)
add_test(
    NAME
    shader-tools
    COMMAND
    "${YALAHOO_qsb}"
    --dump
    "${CMAKE_BINARY_DIR}/src/app/.qsb/probe.frag.qsb"
)
set_tests_properties(shader-tools PROPERTIES PASS_REGULAR_EXPRESSION "MSL.*Standard")
add_test(
    NAME
    kit-selection
    COMMAND
    "${CMAKE_COMMAND}"
    -P
    "${CMAKE_CURRENT_SOURCE_DIR}/KitSelectionTest.cmake"
)
add_test(
    NAME
    runtime
    COMMAND
    Yalahoo
    --verify-runtime
    "${CMAKE_BINARY_DIR}/verification/runtime.json"
)
set_tests_properties(runtime PROPERTIES TIMEOUT 30)
if(WIN32)
    set_tests_properties(
        identity
        module-link-and-plugins
        shader-tools
        runtime
        PROPERTIES
        ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin"
    )
endif()
qt_add_executable(MotionTest MotionTest.cpp)
target_link_libraries(MotionTest PRIVATE YalahooCore Qt6::Test)
target_compile_definitions(
    MotionTest
    PRIVATE
    FIXTURE_ROOT="${PROJECT_SOURCE_DIR}"
    MOTION_REPORT="${CMAKE_BINARY_DIR}/verification/motion-errors.json"
)
yalahoo_platform(MotionTest)
add_test(
    NAME
    motion
    COMMAND
    MotionTest
    -o
    "${CMAKE_BINARY_DIR}/verification/motion-test.xml,junitxml"
    -o
    -,txt
)
set_tests_properties(motion PROPERTIES TIMEOUT 60)
add_test(
    NAME
    runtime-motion
    COMMAND
    Yalahoo
    --verify-runtime
    "${CMAKE_BINARY_DIR}/verification/runtime-motion.json"
    --verify-motion
)
set_tests_properties(
    runtime-motion
    PROPERTIES
    TIMEOUT
    30
    ENVIRONMENT
    "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QSG_INFO=1"
)
if(WIN32)
    set_tests_properties(
        motion
        PROPERTIES
        ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin"
    )
    set_tests_properties(
        runtime-motion
        PROPERTIES
        ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin"
    )
endif()
set_tests_properties(
    identity
    module-link-and-plugins
    shader-tools
    runtime
    PROPERTIES
    ENVIRONMENT
    "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QSG_INFO=1"
)
qt_add_executable(LibraryDataTest LibraryDataTest.cpp)
target_link_libraries(LibraryDataTest PRIVATE YalahooCore Qt6::Gui Qt6::Test)
target_compile_definitions(LibraryDataTest PRIVATE FIXTURE_ROOT="${PROJECT_SOURCE_DIR}")
yalahoo_platform(LibraryDataTest)
add_test(
    NAME
    library-data
    COMMAND
    LibraryDataTest
    -o
    "${CMAKE_BINARY_DIR}/verification/library-data-test.xml,junitxml"
    -o
    -,txt
)
set_tests_properties(
    library-data
    PROPERTIES
    TIMEOUT
    60
    ENVIRONMENT
    "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins"
)
if(WIN32)
    set_tests_properties(
        library-data
        PROPERTIES
        ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin"
    )
endif()
add_test(
    NAME
    runtime-library
    COMMAND
    Yalahoo
    --verify-runtime
    "${CMAKE_BINARY_DIR}/verification/runtime-library.json"
    --verify-library
    "${PROJECT_SOURCE_DIR}/tests/fixtures/web-baseline/anonymous.json"
)
set_tests_properties(
    runtime-library
    PROPERTIES
    TIMEOUT
    30
    ENVIRONMENT
    "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QSG_INFO=1"
)
if(WIN32)
    set_tests_properties(
        runtime-library
        PROPERTIES
        ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin"
    )
endif()

qt_add_executable(LightingTest LightingTest.cpp)
target_link_libraries(LightingTest PRIVATE YalahooScene Qt6::Test)
target_compile_definitions(
    LightingTest
    PRIVATE
    FIXTURE_ROOT="${PROJECT_SOURCE_DIR}"
    LIGHTING_REPORT="${CMAKE_BINARY_DIR}/verification/lighting-errors.json"
)
yalahoo_platform(LightingTest)
add_test(
    NAME
    lighting
    COMMAND
    LightingTest
    -o
    "${CMAKE_BINARY_DIR}/verification/lighting-test.xml,junitxml"
    -o
    -,txt
)
set_tests_properties(lighting PROPERTIES TIMEOUT 60)
if(WIN32)
    set_tests_properties(
        lighting
        PROPERTIES
        ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin"
    )
endif()

if(YALAHOO_REVIEW)
    add_test(NAME runtime-rhi-integration COMMAND Yalahoo --verify-integration
        "${CMAKE_BINARY_DIR}/verification/rhi-integration/ctest")
    set_tests_properties(runtime-rhi-integration PROPERTIES TIMEOUT 90
        ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QT_SCALE_FACTOR=1;QT_ENABLE_HIGHDPI_SCALING=0;QSG_RENDER_LOOP=threaded;QSG_NO_VSYNC="
        FAIL_REGULAR_EXPRESSION "Failed to compile;QSpirvCompiler;Nonfinite RHI pixel")
    if(WIN32)
        set_tests_properties(runtime-rhi-integration PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin")
    endif()
endif()
qt_add_executable(ArchivePoolTest ArchivePoolTest.cpp)
qt_add_executable(RealColumnsTest RealColumnsTest.cpp)
target_link_libraries(RealColumnsTest PRIVATE YalahooScene Qt6::Test)
target_compile_definitions(RealColumnsTest PRIVATE FIXTURE_ROOT="${PROJECT_SOURCE_DIR}")
yalahoo_platform(RealColumnsTest)
add_test(NAME real-columns COMMAND RealColumnsTest)
set_tests_properties(real-columns PROPERTIES TIMEOUT 60)
if(WIN32)
    set_tests_properties(real-columns PROPERTIES ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin")
endif()
target_link_libraries(ArchivePoolTest PRIVATE YalahooScene Qt6::Test)
target_compile_definitions(ArchivePoolTest PRIVATE FIXTURE_ROOT="${PROJECT_SOURCE_DIR}")
yalahoo_platform(ArchivePoolTest)
add_test(NAME archive-pool COMMAND ArchivePoolTest)
set_tests_properties(archive-pool PROPERTIES TIMEOUT 60)
if(WIN32)
    set_tests_properties(archive-pool PROPERTIES ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin")
endif()
add_test(NAME runtime-filled COMMAND Yalahoo --verify-runtime
    "${CMAKE_BINARY_DIR}/verification/runtime-filled/runtime.json" --verify-shading --verify-filled)
set_tests_properties(runtime-filled PROPERTIES TIMEOUT 60
    ENVIRONMENT "QT_PLUGIN_PATH=${YALAHOO_SELECTED_QT}/plugins;QT_SCALE_FACTOR=1;QT_ENABLE_HIGHDPI_SCALING=0")
if(WIN32)
    set_tests_properties(runtime-filled PROPERTIES ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin")
endif()

qt_add_executable(PresentationTest PresentationTest.cpp)
target_link_libraries(PresentationTest PRIVATE YalahooCore Qt6::Test)
yalahoo_platform(PresentationTest)
add_test(NAME presentation COMMAND PresentationTest)
set_tests_properties(presentation PROPERTIES TIMEOUT 60)
if(WIN32)
    set_tests_properties(presentation PROPERTIES ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin")
endif()

qt_add_executable(DetailMotionTest DetailMotionTest.cpp)
target_link_libraries(DetailMotionTest PRIVATE YalahooScene Qt6::Test)
target_compile_definitions(DetailMotionTest PRIVATE FIXTURE_ROOT="${PROJECT_SOURCE_DIR}")
yalahoo_platform(DetailMotionTest)
add_test(NAME detail-motion COMMAND DetailMotionTest)
set_tests_properties(detail-motion PROPERTIES TIMEOUT 60)
if(WIN32)
    set_tests_properties(detail-motion PROPERTIES ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin")
endif()

qt_add_executable(OverviewTest OverviewTest.cpp)
target_link_libraries(OverviewTest PRIVATE YalahooScene Qt6::Test)
target_compile_definitions(OverviewTest PRIVATE FIXTURE_ROOT="${PROJECT_SOURCE_DIR}")
yalahoo_platform(OverviewTest)
add_test(NAME overview COMMAND OverviewTest)
set_tests_properties(overview PROPERTIES TIMEOUT 60)
if(WIN32)
    set_tests_properties(overview PROPERTIES ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${YALAHOO_SELECTED_QT}/bin")
endif()
