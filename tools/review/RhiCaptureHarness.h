// Drives deterministic production RHI inputs and collects owned readbacks. Submission,
// completion and renderer recreation are separate lifecycle facts.

#pragma once
#include <QtClassHelperMacros>
#include <QString>
using Qt::StringLiterals::operator""_s;
#include "OffscreenSurface.h"
#include "RhiItem.h"
#include "VerificationWindow.h"
#include <QEventLoop>
#include <QGuiApplication>
#include <QPointer>
#include <QJsonDocument>
#include <QTimer>
#include <atomic>
#include <functional>
#include <optional>
#include <stdexcept>

namespace yalahoo::review {
// Review-only serial driver. A normal event loop waits for actual frame/readback events.
class RhiCaptureHarness final
{
    Q_DISABLE_COPY_MOVE(RhiCaptureHarness)
public:
    RhiCaptureHarness(const std::function<rhi::RhiItem *(QQuickItem *)> &create = { })
        : m_create(create), m_quitOnClose(QGuiApplication::quitOnLastWindowClosed())
    {
        QGuiApplication::setQuitOnLastWindowClosed(false);
        createWindow();
    }
    ~RhiCaptureHarness()
    {
        destroyWindow();
        QGuiApplication::setQuitOnLastWindowClosed(m_quitOnClose);
    }
    rhi::Capture capture(rhi::Input input, QSize size, bool captureAtlas = false)
    {
        require(!state->loop, "Concurrent Review capture");
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        input.request = ++state->request;
        input.captureAtlas = captureAtlas;
        state->capture.reset();
        state->error.clear();
        state->loop = &loop;
        setInput(std::move(input), size);
        timeout.start(30000);
        loop.exec();
        state->loop = nullptr;
        if (!state->error.isEmpty())
            throw std::runtime_error((u"RHI render failed: "_s + state->error).toStdString());
        require(state->capture.has_value(), "RHI capture timed out");
        if (!verification::windowPolicySatisfied(*window)) {
            qWarning("Review policy failure: %s",
                     QJsonDocument(verification::windowPolicy(*window))
                             .toJson(QJsonDocument::Compact)
                             .constData());
            throw std::runtime_error("Review window policy failed");
        }
        auto result = std::move(*state->capture);
        state->capture.reset();
        require(result.color.size() == size && result.normal.size() == size
                        && result.depth.size() == size,
                "Stale RHI target dimensions");
        require(result.metadata.contains(u"guiThreadAccessInRender"_s)
                        && !result.metadata[u"guiThreadAccessInRender"_s].toBool(),
                "GUI render access");
        return result;
    }
    void submitWithoutCapture(rhi::Input input, QSize size)
    {
        input.request = state->request;
        input.captureAtlas = false;
        setInput(std::move(input), size);
    }
    void replay(int count, const std::function<void(int)> &prepare = { })
    {
        require(count > 0, "Empty Review replay");
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        int frames = 0;
        surface->connectFrames(&loop, [&] {
            // quit() does not stop delivery of the current queued event batch.
            // Admit exactly the requested prefix and never prepare another state after it.
            if (frames >= count)
                return;
            if (++frames == count)
                loop.quit();
            else {
                if (prepare)
                    prepare(frames);
                surface->requestFrame();
            }
        });
        if (prepare)
            prepare(0);
        surface->show();
        surface->requestFrame();
        timeout.start(30000);
        loop.exec();
        if (frames != count || !state->error.isEmpty())
            qWarning() << "Replay state" << frames << count << state->error << window->size()
                       << window->isVisible() << window->isExposed();
        require(frames == count && state->error.isEmpty(), "RHI replay failed or timed out");
    }
    void submitBurst(rhi::Input input, QSize size)
    {
        input.request = ++state->request;
        input.captureAtlas = false;
        setInput(std::move(input), size);
    }
    void rebuild()
    {
        require(!state->loop, "Cannot rebuild during a Review capture");
        previousInvalidations = invalidations.load();
        if (OffscreenSurface::Enabled) {
            surface->hide();
            window->releaseResources();
            surface->show();
        } else {
            // A basic-loop surface can keep its scene graph alive. A real window
            // lifetime boundary guarantees retirement on every native backend.
            const auto size = window->size();
            const auto itemSize = item->size();
            destroyWindow();
            createWindow();
            window->resize(size);
            item->setSize(itemSize);
        }
    }
    bool rebuilt() const
    {
        return invalidations > previousInvalidations;
    }
    static void require(bool condition, const char *message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

private:
    void createWindow()
    {
        window = std::make_unique<QQuickWindow>(OffscreenSurface::Enabled ? &control : nullptr);
        surface = std::make_unique<OffscreenSurface>(*window, control);
        verification::prepareWindow(*window, true);
        window->setPersistentGraphics(false);
        window->setPersistentSceneGraph(false);
        QObject::connect(
                window.get(), &QQuickWindow::sceneGraphInvalidated, window.get(),
                [this] {
                    ++invalidations;
                },
                Qt::DirectConnection);
        item = m_create ? m_create(window->contentItem()) : new rhi::RhiItem(window->contentItem());
        QObject::connect(item, &rhi::RhiItem::renderFailed, window.get(),
                         [state = state](const QString &message) {
                             state->error = message;
                             if (state->loop)
                                 state->loop->quit();
                         });
        const QPointer<QQuickWindow> guard(window.get());
        item->captured = [state = state, guard](rhi::Capture capture) {
            if (guard)
                QMetaObject::invokeMethod(
                        guard,
                        [state, capture = std::move(capture)]() mutable {
                            if (state->loop && capture.request == state->request) {
                                state->capture = std::move(capture);
                                state->loop->quit();
                            }
                        },
                        Qt::QueuedConnection);
        };
    }
    void destroyWindow()
    {
        if (!window)
            return;
        item->captured = { };
        surface->hide();
        window->releaseResources();
        auto oldSurface = std::move(surface);
        window.reset();
        oldSurface.reset();
        item = nullptr;
    }
    struct State final
    {
        quint64 request = 0;
        QPointer<QEventLoop> loop;
        std::optional<rhi::Capture> capture;
        QString error;
    };
    void setInput(rhi::Input input, QSize size)
    {
        if (manageGeometry) {
            window->resize(size);
            item->setSize(size);
        }
        item->submit(std::move(input), size);
        surface->show();
        surface->requestFrame();
    }
    std::shared_ptr<State> state = std::make_shared<State>();
    std::atomic<int> invalidations = 0;
    int previousInvalidations = 0;
    const std::function<rhi::RhiItem *(QQuickItem *)> m_create;
    const bool m_quitOnClose;
    QQuickRenderControl control;

public:
    bool manageGeometry = true;
    std::unique_ptr<QQuickWindow> window;
    std::unique_ptr<OffscreenSurface> surface;
    rhi::RhiItem *item = nullptr;
};
}
