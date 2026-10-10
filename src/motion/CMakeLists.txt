target_sources(YalahooCore PRIVATE
    ScalarSpring.cpp ScalarSpring.h QuinticTrack.cpp QuinticTrack.h
    MotionFunctions.cpp MotionFunctions.h FrameClock.cpp FrameClock.h
    MotionController.cpp MotionController.h)
target_include_directories(YalahooCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
