qt_add_library(YalahooNavigation STATIC
    RulerController.cpp RulerController.h
    RulerInk.cpp RulerInk.h
    InputRouter.cpp InputRouter.h
    OrdinalMotion.cpp OrdinalMotion.h)
target_include_directories(YalahooNavigation PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(YalahooNavigation PUBLIC YalahooScene Qt6::Quick Qt6::CanvasPainter)
target_compile_features(YalahooNavigation PUBLIC cxx_std_23)
yalahoo_platform(YalahooNavigation)
