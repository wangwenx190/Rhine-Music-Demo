#include <QQuickWindow>
#include "Logging.h"
#include "DisplayOutput.h"
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
DisplayOutput::DisplayOutput(QObject *parent) : DisplayOutput(makeDisplayMonitor(), parent)
{
}
DisplayOutput::DisplayOutput(std::unique_ptr<DisplayMonitor> monitor, QObject *parent)
    : QObject(parent), m_monitor(std::move(monitor))
{
    m_monitor->changed = [this] {
        refresh();
    };
    // Native events cover topology/mode changes; current headroom/white level may
    // have no notification. One coarse query per second also covers that case.
    m_poll.setInterval(DisplayPollMilliseconds);
    m_poll.setTimerType(Qt::CoarseTimer);
    connect(&m_poll, &QTimer::timeout, this, &DisplayOutput::refresh);
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this] {
        refresh();
    });
}
DisplayOutput::~DisplayOutput()
{
    m_monitor->changed = { };
    if (m_window)
        disconnect(m_window, nullptr, this, nullptr);
}
void DisplayOutput::attach(QQuickWindow &window)
{
    Q_ASSERT(!m_window);
    m_window = &window;
    connect(&window, &QWindow::screenChanged, this, [this] {
        m_rejected = false;
        m_useLinearFallback = false;
        refresh();
    });
    connect(&window, &QWindow::visibleChanged, this, [this](bool visible) {
        if (visible) {
            m_poll.start();
            refresh();
        } else {
            m_poll.stop();
        }
    });
    connect(&window, &QWindow::visibilityChanged, this, [this](QWindow::Visibility visibility) {
        if (visibility == QWindow::Hidden || visibility == QWindow::Minimized) {
            m_poll.stop();
        } else {
            m_poll.start();
            refresh();
        }
    });
    connect(
            &window, &QQuickWindow::beforeSynchronizing, this,
            [this, &window] {
                // Render thread: only immutable local facts cross back to the GUI.
                auto *swap = window.swapChain();
                const auto format = swap ? swap->format() : QRhiSwapChain::SDR;
                const auto epoch = m_epoch.load();
                if (m_reportedEpoch == epoch)
                    return;
                m_reportedEpoch = epoch;
                QMetaObject::invokeMethod(
                        this,
                        [this, format, epoch] {
                            if (!m_window || epoch != m_epoch.load())
                                return;
                            const auto expected = m_requestedHdr10
                                    ? QRhiSwapChain::HDR10
                                    : QRhiSwapChain::HDRExtendedSrgbLinear;
                            if (m_requested && format != expected) {
                                recoverOutput();
                                return;
                            }
                            m_active = m_requested && format == expected;
                            m_ready = true;
                            Q_EMIT changed();
                        },
                        Qt::QueuedConnection);
            },
            Qt::DirectConnection);
    connect(
            &window, &QQuickWindow::sceneGraphError, this,
            [this](QQuickWindow::SceneGraphError, const QString &message) {
                const auto epoch = m_epoch.load();
                QMetaObject::invokeMethod(
                        this,
                        [this, epoch, message] {
                            if (!m_window || epoch != m_epoch.load())
                                return;
                            if (m_requested) {
                                recoverOutput();
                            } else {
                                qCWarning(yalahoo::logRender,
                                          "Scene graph initialization failed: %s",
                                          qPrintable(message));
                                QCoreApplication::exit(DisplayConfigurationExitCode);
                            }
                        },
                        Qt::QueuedConnection);
            },
            Qt::DirectConnection);
    refresh();
}
void DisplayOutput::refresh()
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
        Q_EMIT changed();
}
void DisplayOutput::recoverOutput()
{
    if (m_requestedHdr10 && m_state.allowLinearFallback) {
        m_useLinearFallback = true;
        configure(true);
    } else {
        m_rejected = true;
        configure(false);
    }
}
void DisplayOutput::configure(bool hdr)
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
    Q_EMIT changed();
    if (visibility != QWindow::Hidden)
        m_window->setVisibility(visibility);
    m_reconfiguring = false;
}
}
