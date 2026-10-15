#pragma once
#include "OffscreenSurface.h"
#include "RhiItem.h"
#include "VerificationWindow.h"
#include <QEventLoop>
#include <QPointer>
#include <QJsonDocument>
#include <QTimer>
#include <atomic>
#include <functional>
#include <optional>
#include <stdexcept>

namespace yalahoo::review {
// Review-only serial driver. A normal event loop waits for actual frame/readback events.
class RhiCaptureHarness
{
public:
    RhiCaptureHarness(const std::function<rhi::RhiItem *(QQuickItem *)> &create = { })
        : window(OffscreenSurface::Enabled ? &control : nullptr), surface(window, control)
    {
        verification::prepareWindow(window, true);
        window.setPersistentGraphics(false);
        window.setPersistentSceneGraph(false);
        QObject::connect(
                &window, &QQuickWindow::sceneGraphInvalidated, &window,
                [this] {
                    ++invalidations;
                },
                Qt::DirectConnection);
        item = create ? create(window.contentItem()) : new rhi::RhiItem(window.contentItem());
        QObject::connect(item, &rhi::RhiItem::renderFailed, &window,
                         [state = state](const QString &message) {
                             state->error = message;
                             if (state->loop)
                                 state->loop->quit();
                         });
        const QPointer<QQuickWindow> guard(&window);
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
        surface.show();
    }
    ~RhiCaptureHarness()
    {
        item->captured = { };
        surface.hide();
        window.releaseResources();
    }
    rhi::Capture capture(rhi::Input input, QSize size)
    {
        require(!state->loop, "Concurrent Review capture");
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        input.request = ++state->request;
        input.captureAtlas = false;
        state->capture.reset();
        state->error.clear();
        state->loop = &loop;
        setInput(std::move(input), size);
        timeout.start(30000);
        loop.exec();
        state->loop = nullptr;
        require(state->error.isEmpty(), "RHI render failed");
        require(state->capture.has_value(), "RHI capture timed out");
        if (!verification::windowPolicySatisfied(window)) {
            qWarning("Review policy failure: %s",
                     QJsonDocument(verification::windowPolicy(window))
                             .toJson(QJsonDocument::Compact)
                             .constData());
            throw std::runtime_error("Review window policy failed");
        }
        auto result = std::move(*state->capture);
        state->capture.reset();
        require(result.color.size() == size && result.normal.size() == size
                        && result.depth.size() == size,
                "Stale RHI target dimensions");
        require(result.metadata.contains("guiThreadAccessInRender")
                        && !result.metadata["guiThreadAccessInRender"].toBool(),
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
        surface.connectFrames(&loop, [&] {
            if (++frames == count)
                loop.quit();
            else {
                if (prepare)
                    prepare(frames);
                surface.requestFrame();
            }
        });
        if (prepare)
            prepare(0);
        surface.requestFrame();
        timeout.start(30000);
        loop.exec();
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
        const auto previous = invalidations.load();
        surface.hide();
        window.releaseResources();
        surface.show();
        // The next capture checks actual invalidation after native render-thread retirement.
        previousInvalidations = previous;
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
    struct State
    {
        quint64 request = 0;
        QPointer<QEventLoop> loop;
        std::optional<rhi::Capture> capture;
        QString error;
    };
    void setInput(rhi::Input input, QSize size)
    {
        if (manageGeometry) {
            window.resize(size);
            item->setSize(size);
        }
        item->submit(std::move(input), size);
        surface.requestFrame();
    }
    std::shared_ptr<State> state = std::make_shared<State>();
    std::atomic<int> invalidations = 0;
    int previousInvalidations = 0;
    QQuickRenderControl control;

public:
    bool manageGeometry = true;
    QQuickWindow window;
    OffscreenSurface surface;
    rhi::RhiItem *item = nullptr;
};
}
