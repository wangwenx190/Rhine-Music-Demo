#pragma once
#include <QQuickRenderControl>
#include <QQuickRenderTarget>
#include <QQuickWindow>
#include <QTimer>
#include <QJsonObject>
#include <rhi/qrhi.h>
#include <functional>
#include <memory>

// Review-only rendering. Cocoa can stop swapchain frames for fully occluded
// bottom windows. RenderControl submits the same QML scene into a real Metal target.
class OffscreenSurface : public QObject
{
    Q_OBJECT
public:
    static constexpr bool Enabled =
#if defined(Q_OS_MACOS)
            true;
#else
            false;
#endif
    OffscreenSurface(QQuickWindow &window, QQuickRenderControl &control)
        : m_window(window), m_control(control)
    {
        if (!Enabled)
            return;
        connect(&control, &QQuickRenderControl::renderRequested, this, &OffscreenSurface::schedule);
        connect(&control, &QQuickRenderControl::sceneChanged, this, &OffscreenSurface::schedule);
        connect(&window, &QWindow::visibleChanged, this, [this](bool visible) {
            if (visible) {
                schedule();
            } else {
                invalidate();
            }
        });
        connect(&window, &QWindow::widthChanged, this, &OffscreenSurface::schedule);
        connect(&window, &QWindow::heightChanged, this, &OffscreenSurface::schedule);
    }
    ~OffscreenSurface() override
    {
        invalidate();
    }
    void schedule()
    {
        if (m_pending || !m_active)
            return;
        m_pending = true;
        QTimer::singleShot(0, this, [this] {
            m_pending = false;
            if (m_active)
                render();
        });
    }
    void show()
    {
        if (!Enabled) {
            m_window.show();
            return;
        }
        m_active = true;
        schedule();
    }
    void hide()
    {
        if (!Enabled) {
            m_window.hide();
            return;
        }
        m_active = false;
        invalidate();
    }
    void requestFrame()
    {
        if (Enabled)
            schedule();
        else
            m_window.update();
    }
    void connectFrames(QObject *context, const std::function<void()> &callback)
    {
        if (Enabled)
            connect(this, &OffscreenSurface::frameRendered, context, callback);
        else
            connect(&m_window, &QQuickWindow::frameSwapped, context, callback);
    }
    QRhi *rhi() const
    {
        return Enabled ? m_control.rhi()
                       : static_cast<QRhi *>(m_window.rendererInterface()->getResource(
                                 &m_window, QSGRendererInterface::RhiResource));
    }
    QRhiCommandBuffer *commandBuffer() const
    {
        if (Enabled)
            return m_control.commandBuffer();
        auto *swapchain = static_cast<QRhiSwapChain *>(m_window.rendererInterface()->getResource(
                &m_window, QSGRendererInterface::RhiSwapchainResource));
        return swapchain ? swapchain->currentFrameCommandBuffer() : nullptr;
    }
    QRhiReadbackDescription readbackDescription() const
    {
        return Enabled ? QRhiReadbackDescription(m_texture.get()) : QRhiReadbackDescription();
    }
    QImage grab()
    {
        if (!Enabled)
            return m_window.grabWindow();
        if (!m_initialized || !m_texture)
            return { };
        QRhiReadbackResult result;
        m_control.beginFrame();
        auto *batch = rhi()->nextResourceUpdateBatch();
        batch->readBackTexture(readbackDescription(), &result);
        commandBuffer()->resourceUpdate(batch);
        m_control.endFrame();
        rhi()->finish();
        QImage image(reinterpret_cast<const uchar *>(result.data.constData()),
                     result.pixelSize.width(), result.pixelSize.height(), QImage::Format_RGBA8888);
        return rhi()->isYUpInFramebuffer() ? image.flipped(Qt::Vertical) : image.copy();
    }
    QJsonObject diagnostics() const
    {
        return { { "enabled", Enabled },
                 { "nativeWindowVisible", m_window.isVisible() },
                 { "colorFormat", "RGBA8" },
                 { "samples", 1 },
                 { "mips", false },
                 { "attachmentPeakBytesUpperBound", double(m_peakBytes) },
                 { "includesQtLayersOrFontAtlas", false } };
    }
signals:
    void frameRendered();

private:
    void invalidate()
    {
        if (!m_initialized)
            return;
        m_window.setRenderTarget({ });
        m_control.invalidate();
        m_target.reset();
        m_pass.reset();
        m_depth.reset();
        m_texture.reset();
        m_initialized = false;
    }
    void render()
    {
        if (!m_initialized) {
            if (!m_control.initialize())
                qFatal("Offscreen Qt Quick initialization failed");
            m_initialized = true;
        }
        const QSize size = m_window.size() * m_window.devicePixelRatio();
        bool targetChanged = false;
        if (!m_texture || m_texture->pixelSize() != size) {
            const int limit = rhi()->resourceLimit(QRhi::TextureSizeMax);
            if (size.isEmpty() || size.width() > limit || size.height() > limit)
                qFatal("Offscreen target exceeds the device texture limit");
            const QSize oldSize = m_texture ? m_texture->pixelSize() : QSize();
            const qint64 bytes = qint64(size.width()) * size.height() * 8;
            const qint64 oldBytes = qint64(oldSize.width()) * oldSize.height() * 8;
            m_peakBytes = std::max(m_peakBytes, bytes + oldBytes);
            m_window.setRenderTarget({ });
            m_target.reset();
            m_pass.reset();
            m_depth.reset();
            m_texture.reset(rhi()->newTexture(QRhiTexture::RGBA8, size, 1,
                                              QRhiTexture::RenderTarget
                                                      | QRhiTexture::UsedAsTransferSource));
            m_depth.reset(rhi()->newRenderBuffer(QRhiRenderBuffer::DepthStencil, size, 1));
            if (!m_texture->create() || !m_depth->create())
                qFatal("Offscreen attachment creation failed");
            m_target.reset(rhi()->newTextureRenderTarget(QRhiTextureRenderTargetDescription(
                    QRhiColorAttachment(m_texture.get()), m_depth.get())));
            m_pass.reset(m_target->newCompatibleRenderPassDescriptor());
            m_target->setRenderPassDescriptor(m_pass.get());
            if (!m_target->create())
                qFatal("Offscreen render target creation failed");
            targetChanged = true;
        }
        if (targetChanged || m_targetDpr != m_window.devicePixelRatio()) {
            auto target = QQuickRenderTarget::fromRhiRenderTarget(m_target.get());
            target.setDevicePixelRatio(m_window.devicePixelRatio());
            m_window.setRenderTarget(target);
            m_targetDpr = m_window.devicePixelRatio();
        }
        m_control.polishItems();
        m_control.beginFrame();
        m_control.sync();
        m_control.render();
        m_control.endFrame();
        emit frameRendered();
    }
    QQuickWindow &m_window;
    QQuickRenderControl &m_control;
    bool m_pending = false;
    bool m_active = false;
    bool m_initialized = false;
    qint64 m_peakBytes = 0;
    qreal m_targetDpr = 0;
    std::unique_ptr<QRhiTexture> m_texture;
    std::unique_ptr<QRhiRenderBuffer> m_depth;
    std::unique_ptr<QRhiRenderPassDescriptor> m_pass;
    std::unique_ptr<QRhiTextureRenderTarget> m_target;
};
