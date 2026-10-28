if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

set(source_files)
foreach(directory src qml tests tools audit)
    file(GLOB_RECURSE candidates
        "${SOURCE_ROOT}/${directory}/*.cpp"
        "${SOURCE_ROOT}/${directory}/*.h"
        "${SOURCE_ROOT}/${directory}/*.mm"
        "${SOURCE_ROOT}/${directory}/*.qml")
    list(APPEND source_files ${candidates})
endforeach()

foreach(source IN LISTS source_files)
    file(READ "${source}" content)
    if(content MATCHES "[^\t\r\n -~]")
        message(FATAL_ERROR "Non-ASCII C++/QML source: ${source}")
    endif()
    if(content MATCHES "QStringLiteral[ \t\r\n]*\\(|QString::fromLatin1[ \t\r\n]*\\(|[.>]toLatin1[ \t\r\n]*\\(")
        message(FATAL_ERROR "Unsupported string conversion or literal: ${source}")
    endif()
    if(content MATCHES "#[ \t]*(undef|define)[ \t]+QT_NO_(CAST_FROM_ASCII|CAST_TO_ASCII|KEYWORDS)")
        message(FATAL_ERROR "Source must not override the project Qt compile gates: ${source}")
    endif()
endforeach()
