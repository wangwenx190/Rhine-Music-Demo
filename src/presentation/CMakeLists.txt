target_sources(YalahooCore PRIVATE
    SurfaceTransition.h SurfaceTransition.cpp
    PresentationController.h PresentationController.cpp
    GenreRowsModel.h GenreRowsModel.cpp
    OptionMetrics.h OptionMetrics.cpp
    DetailModel.h DetailModel.cpp
    TrackFocusController.h TrackFocusController.cpp PanelController.h PanelController.cpp)
target_include_directories(YalahooCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
