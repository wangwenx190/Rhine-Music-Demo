#include "DisplayOutput.h"
#include <memory>
#include "DisplayState.h"
#include <QPointer>
#include <QTimer>
#include <atomic>
#include <utility>
#include <QQuickWindow>
#include "Logging.h"
#include <QColorSpace>
#include <QGuiApplication>
#include <rhi/qrhi.h>
#include <algorithm>
#include <cmath>

namespace {
// Polling supplements native display events; unsupported explicit output remains a process failure.
constexpr auto DisplayPollMilliseconds = 1000;
constexpr auto DisplayConfigurationExitCode = 23;
}

using namespace Qt::StringLiterals;

namespace yalahoo {
struct DisplayOutput::Private final
{
    Q_DISABLE_COPY_MOVE(Private)
    explicit Private(DisplayOutput *owner, std::unique_ptr<DisplayMonitor> monitor)
        : q(owner), m_monitor(std::move(monitor))
    {
    }
    DisplayOutput *const q;

    void queueSceneGraphError(const QString &message);
    void reportSwapChain(QQuickWindow &window);
    void reportSceneGraphError(const quint64 &epoch, const QString &message);

    void refresh();
    void configure(bool hdr);
    void recoverOutput();
    QPointer<QQuickWindow> m_window;
    std::unique_ptr<DisplayMonitor> m_monitor;
    QTimer m_poll;
    DisplayState m_state;
    bool m_requested = false;
    bool m_requestedHdr10 = false;
    bool m_active = false;
    bool m_ready = true;
    bool m_reconfiguring = false;
    bool m_rejected = false;
    bool m_useLinearFallback = false;
    std::atomic<quint64> m_epoch = 0;
    quint64 m_reportedEpoch = quint64(-1); // Render-thread state only.
};
DisplayOutput::DisplayOutput(std::unique_ptr<DisplayMonitor> monitor, QObject *parent)
    : QObject(parent), d(std::make_unique<Private>(this, std::move(monitor)))
{
    d->m_monitor->changed = [this] {
        d->refresh();
    };
    // Native events cover topology/mode changes; current headroom/white level may
    // have no notification. One coarse query per second also covers that case.
    d->m_poll.setInterval(DisplayPollMilliseconds);
    d->m_poll.setTimerType(Qt::CoarseTimer);
    connect(&d->m_poll, &QTimer::timeout, this, [this] {
        d->refresh();
    });
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this] {
        d->refresh();
    });
}
DisplayOutput::DisplayOutput(QObject *parent) : DisplayOutput(makeDisplayMonitor(), parent)
{
}
DisplayOutput::~DisplayOutput()
{
    d->m_monitor->changed = { };
    if (d->m_window)
        disconnect(d->m_window, nullptr, this, nullptr);
}
void DisplayOutput::Private::reportSceneGraphError(const quint64 &epoch, const QString &message)
{
    if (!m_window || epoch != m_epoch.load())
        return;
    if (m_requested) {
        recoverOutput();
    } else {
        qCWarning(yalahoo::logRender, "Scene graph initialization failed: %s", qPrintable(message));
        QCoreApplication::exit(DisplayConfigurationExitCode);
    }
}
void DisplayOutput::Private::reportSwapChain(QQuickWindow &window)
{
    // Render thread: only immutable local facts cross back to the GUI.
    auto *swap = window.swapChain();
    const auto format = swap ? swap->format() : QRhiSwapChain::SDR;
    const auto epoch = m_epoch.load();
    if (m_reportedEpoch == epoch)
        return;
    m_reportedEpoch = epoch;
    QMetaObject::invokeMethod(
            q,
            [this, format, epoch] {
                if (!m_window || epoch != m_epoch.load())
                    return;
                const auto expected = m_requestedHdr10 ? QRhiSwapChain::HDR10
                                                       : QRhiSwapChain::HDRExtendedSrgbLinear;
                if (m_requested && format != expected) {
                    recoverOutput();
                    return;
                }
                m_active = m_requested && format == expected;
                m_ready = true;
                Q_EMIT q->changed();
            },
            Qt::QueuedConnection);
}
void DisplayOutput::Private::queueSceneGraphError(const QString &message)
{
    const auto epoch = m_epoch.load();
    QMetaObject::invokeMethod(
            q,
            [this, epoch, message] {
                return reportSceneGraphError(epoch, message);
            },
            Qt::QueuedConnection);
}
void DisplayOutput::attach(QQuickWindow &window)
{
    Q_ASSERT(!d->m_window);
    d->m_window = &window;
    connect(&window, &QWindow::screenChanged, this, [this] {
        d->m_rejected = false;
        d->m_useLinearFallback = false;
        d->refresh();
    });
    connect(&window, &QWindow::visibleChanged, this, [this](bool visible) {
        if (visible) {
            d->m_poll.start();
            d->refresh();
        } else {
            d->m_poll.stop();
        }
    });
    connect(&window, &QWindow::visibilityChanged, this, [this](QWindow::Visibility visibility) {
        if (visibility == QWindow::Hidden || visibility == QWindow::Minimized) {
            d->m_poll.stop();
        } else {
            d->m_poll.start();
            d->refresh();
        }
    });
    connect(
            &window, &QQuickWindow::beforeSynchronizing, this,
            [this, &window] {
                return d->reportSwapChain(window);
            },
            Qt::DirectConnection);
    connect(
            &window, &QQuickWindow::sceneGraphError, this,
            [this](QQuickWindow::SceneGraphError, const QString &message) {
                d->queueSceneGraphError(message);
            },
            Qt::DirectConnection);
    d->refresh();
}
void DisplayOutput::Private::refresh()
{
    if (!m_window || m_reconfiguring)
        return;
    auto state = m_monitor->query(*m_window);
    if (!std::isfinite(state.whiteScale) || state.whiteScale <= 0 || !std::isfinite(state.headroom)
        || state.headroom < 1)
        state = { };
    const bool changedState = state != m_state;
    if (state.enabled != m_state.enabled || state.hdr10 != m_state.hdr10
        || state.allowLinearFallback != m_state.allowLinearFallback) {
        m_rejected = false;
        m_useLinearFallback = false;
    }
    m_state = state;
    const auto api = QQuickWindow::graphicsApi();
    const bool available = api == QSGRendererInterface::Direct3D11
            || api == QSGRendererInterface::Direct3D12 || api == QSGRendererInterface::Metal
            || api == QSGRendererInterface::Vulkan;
    const bool request = available && state.enabled && !m_rejected;
    const bool hdr10 = state.hdr10 && !m_useLinearFallback;
    if (request != m_requested || (request && hdr10 != m_requestedHdr10))
        configure(request);
    else if (changedState)
        Q_EMIT q->changed();
}
void DisplayOutput::Private::recoverOutput()
{
    if (m_requestedHdr10 && m_state.allowLinearFallback) {
        m_useLinearFallback = true;
        configure(true);
    } else {
        m_rejected = true;
        configure(false);
    }
}
void DisplayOutput::Private::configure(bool hdr)
{
    if (!m_window)
        return;
    m_reconfiguring = true;
    const auto visibility = m_window->visibility();
    const bool hadSurface = m_window->handle();
    const bool persistentGraphics = m_window->isPersistentGraphics();
    const bool persistentSceneGraph = m_window->isPersistentSceneGraph();
    m_window->setPersistentGraphics(false);
    m_window->setPersistentSceneGraph(false);
    m_window->hide();
    m_window->releaseResources();
    // A fresh native surface resets platform color metadata as well as the RHI
    // render-pass/PSO compatibility. The QML tree and player state stay alive.
    if (hadSurface)
        m_window->destroy();
    ++m_epoch;
    m_requested = hdr;
    m_requestedHdr10 = hdr && m_state.hdr10 && !m_useLinearFallback;
    m_active = false;
    m_ready = !hdr;
    m_window->setProperty("_qt_sg_hdr_format",
                          hdr ? (m_requestedHdr10 ? "hdr10"_ba : "scrgb"_ba) : QByteArray());
    auto format = m_window->requestedFormat();
#if defined(Q_OS_LINUX)
    // Qt's Wayland integration owns the color-management surface. The Vulkan
    // backend uses pass-through when available to avoid a second owner.
    format.setColorSpace(
            hdr ? QColorSpace(m_requestedHdr10 ? QColorSpace::Bt2100Pq : QColorSpace::SRgbLinear)
                : QColorSpace());
#endif
    m_window->setFormat(format);
    m_window->setPersistentGraphics(persistentGraphics);
    m_window->setPersistentSceneGraph(persistentSceneGraph);
    Q_EMIT q->changed();
    if (visibility != QWindow::Hidden)
        m_window->setVisibility(visibility);
    m_reconfiguring = false;
}
bool DisplayOutput::hdrActive() const
{
    return d->m_active;
}
bool DisplayOutput::ready() const
{
    return d->m_ready;
}
float DisplayOutput::whiteScale() const
{
    return d->m_active && !d->m_useLinearFallback ? d->m_state.whiteScale : 1;
}
float DisplayOutput::headroom() const
{
    return d->m_active ? d->m_state.headroom : 1;
}
bool DisplayOutput::hdr10() const
{
    return d->m_active && d->m_requestedHdr10;
}

}
