function(yalahoo_platform target)
    if(WIN32)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
        target_compile_definitions(${target} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    elseif(UNIX)
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# Developer tools keep one console/output policy on every native platform.
function(yalahoo_tool target)
    target_link_libraries(${target} PRIVATE YalahooVerification)
    target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/src/core")
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools"
        WIN32_EXECUTABLE FALSE MACOSX_BUNDLE FALSE)
    yalahoo_platform(${target})
endfunction()
