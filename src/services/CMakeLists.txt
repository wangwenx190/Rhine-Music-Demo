target_sources(
    YalahooCore
    PRIVATE
    DemoLibrary.h
    DemoLibrary.cpp
    IMetadataProvider.h
    IMetadataProvider.cpp
    OnlineMetadataProvider.h
    metadata/MetadataWorker.cpp metadata/MetadataWorker.h
    metadata/MetadataCachePolicy.cpp metadata/MetadataCachePolicy.h
    OnlineMetadataProvider.cpp
    metadata/MetadataHttp.h
    metadata/MetadataHttp.cpp
    metadata/MetadataMerge.h
    metadata/MetadataMerge.cpp
    metadata/MusicBrainzSource.h
    metadata/MusicBrainzSource.cpp
    metadata/WikipediaSource.h
    metadata/WikipediaSource.cpp
    OfflineMetadataProvider.h
    OfflineMetadataProvider.cpp
    LibrarySession.h
    LibrarySession.cpp
    MetadataPresentation.h
    MetadataPresentation.cpp
    CoverCache.h
    CoverCache.cpp
    SingleInstance.cpp
    SingleInstance.h
    PreferencesStore.cpp
    PreferencesStore.h
)
target_link_libraries(YalahooCore PUBLIC Qt6::Gui Qt6::Concurrent Qt6::Network)
target_include_directories(YalahooCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
