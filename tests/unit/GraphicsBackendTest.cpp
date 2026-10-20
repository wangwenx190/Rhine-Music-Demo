#include <QQuickWindow>
#include "GraphicsBackend.h"
#include <QtTest>

using namespace Qt::StringLiterals;

class GraphicsBackendTest final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(GraphicsBackendTest)
public:
    GraphicsBackendTest() = default;

private Q_SLOTS:
    void selection()
    {
        const auto previous = qgetenv("QSG_RHI_BACKEND");
        const bool hadPrevious = qEnvironmentVariableIsSet("QSG_RHI_BACKEND");
        const auto restore = qScopeGuard([&] {
            if (hadPrevious)
                qputenv("QSG_RHI_BACKEND", previous);
            else
                qunsetenv("QSG_RHI_BACKEND");
        });
        qunsetenv("QSG_RHI_BACKEND");
        QVERIFY(yalahoo::selectGraphicsBackend());
#if defined(Q_OS_WIN)
        QCOMPARE(QQuickWindow::graphicsApi(), QSGRendererInterface::Direct3D11);
        qputenv("QSG_RHI_BACKEND", "d3d12"_ba);
#  if QT_CONFIG(direct3d12)
        QVERIFY(yalahoo::selectGraphicsBackend());
        QCOMPARE(QQuickWindow::graphicsApi(), QSGRendererInterface::Direct3D12);
#  else
        QVERIFY(!yalahoo::selectGraphicsBackend());
#  endif
#elif defined(Q_OS_MACOS)
        QCOMPARE(QQuickWindow::graphicsApi(), QSGRendererInterface::Metal);
#elif defined(Q_OS_LINUX)
        QCOMPARE(QQuickWindow::graphicsApi(), QSGRendererInterface::Vulkan);
#endif
        qputenv("QSG_RHI_BACKEND", "vulkan"_ba);
#if QT_CONFIG(vulkan)
        QVERIFY(yalahoo::selectGraphicsBackend());
        QCOMPARE(QQuickWindow::graphicsApi(), QSGRendererInterface::Vulkan);
#else
        QVERIFY(!yalahoo::selectGraphicsBackend());
#endif
        qputenv("QSG_RHI_BACKEND", "invalid-api"_ba);
        QVERIFY(!yalahoo::selectGraphicsBackend());
    }
};
QTEST_GUILESS_MAIN(GraphicsBackendTest)
#include "GraphicsBackendTest.moc"
