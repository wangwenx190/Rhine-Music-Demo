target_sources(YalahooCore PRIVATE
    DirectoryScanner.h
    DirectoryScanner.cpp
    LibraryStore.h
    LibraryStore.cpp
)
target_include_directories(YalahooCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
