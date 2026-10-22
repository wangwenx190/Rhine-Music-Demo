target_sources(YalahooCore PRIVATE MediaMetadataProbe.cpp MediaMetadataProbe.h
    PlaybackQueue.cpp PlaybackQueue.h
    PlaybackController.cpp PlaybackController.h
    BackgroundMusicController.cpp BackgroundMusicController.h
    InterfaceSoundController.cpp InterfaceSoundController.h)
target_link_libraries(YalahooCore PUBLIC Qt6::Multimedia)
target_include_directories(YalahooCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
