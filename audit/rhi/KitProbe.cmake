if(WIN32 AND YALAHOO_REVIEW)
    set(CMAKE_REQUIRED_LIBRARIES Qt6::Quick Qt6::GuiPrivate)
    set(CMAKE_REQUIRED_INCLUDES "${PROJECT_SOURCE_DIR}/audit/rhi"
        "${PROJECT_SOURCE_DIR}/src/render/rhi" "${PROJECT_SOURCE_DIR}/src/core")
    set(CMAKE_REQUIRED_DEFINITIONS -DYALAHOO_RHI_AUDIT)
    unset(YALAHOO_RHI_TRANSFER_AUDIT_API CACHE)
    check_cxx_source_compiles(
        "#include \"TransferAudit.h\"\nint main() { auto function = &yalahoo::rhi::nativeTransfers; (void)function; return 0; }"
        YALAHOO_RHI_TRANSFER_AUDIT_API
    )
    unset(CMAKE_REQUIRED_LIBRARIES)
    unset(CMAKE_REQUIRED_INCLUDES)
    unset(CMAKE_REQUIRED_DEFINITIONS)
    if(NOT YALAHOO_RHI_TRANSFER_AUDIT_API)
        message(FATAL_ERROR "Latest Qt D3D11 command audit API changed. Adapt the isolated Review probe; do not substitute CPU byte counters.")
    endif()
endif()
if(YALAHOO_REVIEW)
    set(CMAKE_REQUIRED_LIBRARIES Qt6::Quick3DPrivate Qt6::Quick3DRuntimeRenderPrivate Qt6::GuiPrivate)
    set(CMAKE_REQUIRED_INCLUDES "${PROJECT_SOURCE_DIR}/audit/rhi"
        "${PROJECT_SOURCE_DIR}/src/render/rhi" "${PROJECT_SOURCE_DIR}/src/core")
    unset(YALAHOO_REVIEW_MSAA_API CACHE)
    check_cxx_source_compiles(
        "#include \"ReviewGlCapabilities.h\"\n#include <QtQuick3D/private/qquick3dscenemanager_p.h>\n#include <QtQuick3DRuntimeRender/ssg/qssgrendercontextcore.h>\n#include <QtQuick3DRuntimeRender/ssg/qssgrhicontext.h>\nint main() { auto function = &yalahoo::review::useReviewGlMultisampleBuffers; auto attachment = &QQuick3DSceneManager::getOrSetWindowAttachment; auto samples = &QSSGRhiContext::mainPassSampleCount; (void)function; (void)attachment; (void)samples; return 0; }"
        YALAHOO_REVIEW_MSAA_API
    )
    unset(CMAKE_REQUIRED_LIBRARIES)
    unset(CMAKE_REQUIRED_INCLUDES)
    if(NOT YALAHOO_REVIEW_MSAA_API)
        message(FATAL_ERROR "Latest Qt Review MSAA capability/descriptor API changed. Adapt audit/rhi/ReviewGlCapabilities.h; do not reduce sample count or color precision.")
    endif()
endif()
