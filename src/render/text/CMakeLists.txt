find_package(Qt6 REQUIRED COMPONENTS QuickPrivate)
set(CMAKE_REQUIRED_LIBRARIES Qt6::QuickPrivate)
unset(YALAHOO_REEL_GLYPH_API CACHE)
check_cxx_source_compiles([=[
    #include <QQuickWindow>
    #include <QGlyphRun>
    #include <QtQuick/private/qsginternaltextnode_p.h>
    #include <QtQuick/private/qsgadaptationlayer_p.h>
    int main()
    {
        QQuickWindow window;
        auto *node = static_cast<QSGInternalTextNode *>(window.createTextNode());
        node->addGlyphs({}, QGlyphRun(), Qt::black, nullptr);
        delete node;
        return 0;
    }
]=] YALAHOO_REEL_GLYPH_API)
unset(CMAKE_REQUIRED_LIBRARIES)
if(NOT YALAHOO_REEL_GLYPH_API)
    message(FATAL_ERROR "Latest native Qt Quick glyph API needs adaptation at the RHI text boundary")
endif()
qt_add_library(YalahooText STATIC TextAreaFormat.cpp TextAreaFormat.h TextLayoutAdapter.cpp TextLayoutAdapter.h
    TitleReel.cpp TitleReel.h ReelInk.cpp ReelInk.h TransportMotion.cpp TransportMotion.h
    "${PROJECT_SOURCE_DIR}/src/render/rhi/ReelTextNode.cpp")
target_include_directories(YalahooText PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
    PRIVATE "${PROJECT_SOURCE_DIR}/src/render/rhi")
target_link_libraries(YalahooText PUBLIC YalahooCore Qt6::Quick PRIVATE Qt6::QuickPrivate)
target_compile_features(YalahooText PUBLIC cxx_std_23)
yalahoo_platform(YalahooText)
