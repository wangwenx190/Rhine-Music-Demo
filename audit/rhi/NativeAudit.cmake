target_sources(YalahooRhi PRIVATE
    "${PROJECT_SOURCE_DIR}/audit/rhi/RendererAudit.cpp"
    "${PROJECT_SOURCE_DIR}/audit/rhi/PostprocessAudit.cpp")
if(YALAHOO_REVIEW)
    set_source_files_properties(
        "${PROJECT_SOURCE_DIR}/audit/rhi/RendererAudit.cpp"
        "${PROJECT_SOURCE_DIR}/audit/rhi/PostprocessAudit.cpp"
        "${PROJECT_SOURCE_DIR}/tests/render/RendererCapture.cpp"
        PROPERTIES COMPILE_DEFINITIONS YALAHOO_RHI_AUDIT)
endif()
