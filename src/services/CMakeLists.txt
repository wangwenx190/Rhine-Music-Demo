target_sources(
    YalahooCore
    PRIVATE
    IMetadataProvider.h
    OfflineMetadataProvider.h
    OfflineMetadataProvider.cpp
    LibrarySession.h
    LibrarySession.cpp
    CoverCache.h
    CoverCache.cpp
    PreferencesStore.cpp
    PreferencesStore.h
)
target_link_libraries(YalahooCore PUBLIC Qt6::Gui Qt6::Concurrent)
target_include_directories(YalahooCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
