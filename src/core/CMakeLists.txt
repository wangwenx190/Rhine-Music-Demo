qt_add_library(
    YalahooCore
    STATIC
    Logging.cpp
    Logging.h
    Require.h
    MonotonicTime.h
    JsonFile.h
    NumericValue.h
    Units.h
    ColorTransfer.h
    CoverPixels.h
    ProductDefaults.h
    MetadataConfiguration.h
    SceneGeometry.h
    AppIdentity.cpp
    AppIdentity.h
    MusicTypes.cpp
    MusicTypes.h
    LibraryModels.cpp
    LibraryModels.h
    Localization.cpp
    Localization.h
)
target_link_libraries(YalahooCore PUBLIC Qt6::Core PRIVATE Qt6::Gui)
target_compile_features(YalahooCore PUBLIC cxx_std_23)
target_compile_definitions(YalahooCore PUBLIC QT_MESSAGELOGCONTEXT)
target_include_directories(YalahooCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
yalahoo_platform(YalahooCore)
