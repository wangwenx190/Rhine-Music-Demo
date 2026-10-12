qt_add_library(
    YalahooCore
    STATIC
    AppIdentity.cpp
    AppIdentity.h
    MusicTypes.cpp
    MusicTypes.h
    LibraryModels.cpp
    LibraryModels.h
)
target_link_libraries(YalahooCore PUBLIC Qt6::Core PRIVATE Qt6::Gui)
target_compile_features(YalahooCore PUBLIC cxx_std_23)
target_include_directories(YalahooCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
yalahoo_platform(YalahooCore)
