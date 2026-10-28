add_custom_target(source-policy ALL
    COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${PROJECT_SOURCE_DIR}/audit/source/CheckSourcePolicy.cmake"
    COMMENT "Check ASCII sources and explicit Qt string policy"
    VERBATIM)
