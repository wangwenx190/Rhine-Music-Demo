add_library(YalahooVerification INTERFACE)
target_include_directories(YalahooVerification INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
if(APPLE)
    add_library(YalahooVerificationMac STATIC VerificationWindowMac.mm)
    target_include_directories(YalahooVerificationMac PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(YalahooVerificationMac PRIVATE Qt6::Gui
        "-framework AppKit" "-framework CoreGraphics")
    target_link_libraries(YalahooVerification INTERFACE YalahooVerificationMac)
endif()
