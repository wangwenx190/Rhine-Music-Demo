target_sources(YalahooCore PRIVATE
    SurfaceTransition.h SurfaceTransition.cpp
    PresentationController.h PresentationController.cpp
    DetailModel.h DetailModel.cpp)
target_include_directories(YalahooCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
