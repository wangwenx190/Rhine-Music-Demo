target_sources(YalahooCore PRIVATE MediaMetadataProbe.cpp MediaMetadataProbe.h
    PlaybackController.cpp PlaybackController.h)
target_link_libraries(YalahooCore PUBLIC Qt6::Multimedia)
target_include_directories(YalahooCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
