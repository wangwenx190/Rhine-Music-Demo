cmake_minimum_required(VERSION 3.25)

foreach(shader IN ITEMS src/app/.qsb/probe.frag.qsb src/app/.qsb/reel-slot.frag.qsb
        src/app/.qsb/display-output.frag.qsb src/render/rhi/.qsb/output.frag.qsb)
    execute_process(
        COMMAND "${QSB}" --dump "${BUILD_DIRECTORY}/${shader}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE dump
        ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Cannot inspect compiled shader ${shader}: ${error}")
    endif()
    foreach(language IN ITEMS SPIR-V GLSL HLSL MSL)
        if(NOT dump MATCHES "Shader [0-9]+: ${language} [^\n]+\\[Standard\\]")
            message(FATAL_ERROR "Missing ${language} standard variant: ${shader}")
        endif()
    endforeach()
endforeach()
