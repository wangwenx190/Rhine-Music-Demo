target_sources(
    YalahooCore
    PRIVATE
    IMetadataProvider.h
    OfflineMetadataProvider.h
    OfflineMetadataProvider.cpp
    LibrarySession.h
    LibrarySession.cpp
)
target_include_directories(YalahooCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
