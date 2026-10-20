#pragma once
#include "ReadbackImage.h"
#include <QString>
using Qt::StringLiterals::operator""_s;
#include <QQuickRenderControl>
#include <QQuickRenderTarget>
#include <QQuickWindow>
#include <QTimer>
#include <QJsonObject>
#include <QElapsedTimer>
#include <rhi/qrhi.h>
#include <functional>
#include <memory>
#include <cmath>

// Review-only rendering. Cocoa can stop swapchain frames for fully occluded
// bottom windows. RenderControl submits the same QML scene into a real Metal target.
class OffscreenSurface final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(OffscreenSurface)
public:
    bool measurementEnabled = false;
    static inline bool Enabled =
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
    void setDevicePixelRatio(qreal dpr)
    {
        if (!Enabled || !std::isfinite(dpr) || dpr <= 0)
            qFatal("Explicit target DPR requires RenderControl and a positive finite value");
        m_requestedDpr = dpr;
        schedule();
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
            connect(&m_window, &QQuickWindow::frameSwapped, context, callback,
                    Qt::QueuedConnection);
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
        try {
            return yalahoo::rhi::readbackImage(result, rhi()->isYUpInFramebuffer());
        } catch (const std::runtime_error &error) {
            qWarning("Offscreen readback failed: %s", error.what());
            return { };
        }
    }
    QJsonObject diagnostics() const
    {
        return { { u"enabled"_s, Enabled },
                 { u"targetWidth"_s, m_texture ? m_texture->pixelSize().width() : 0 },
                 { u"targetHeight"_s, m_texture ? m_texture->pixelSize().height() : 0 },
                 { u"targetDpr"_s, m_targetDpr },
                 { u"nativeWindowVisible"_s, m_window.isVisible() },
                 { u"colorFormat"_s, u"RGBA8"_s },
                 { u"samples"_s, 1 },
                 { u"mips"_s, false },
                 { u"colorBytesPerPixel"_s, 4 },
                 { u"depthStencilBytesPerPixelUpperBound"_s, 8 },
                 { u"attachmentEstimateBytesPerPixel"_s, 12 },
                 { u"attachmentPeakBytesUpperBound"_s, double(m_peakBytes) },
                 { u"actualDriverAllocationBytes"_s, QJsonValue::Null },
                 { u"includesQtLayersOrFontAtlas"_s, false } };
    }
Q_SIGNALS:
    void frameMeasured(QJsonObject costs);
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
        const qreal dpr = m_requestedDpr > 0 ? m_requestedDpr : m_window.devicePixelRatio();
        const QSize size = m_window.size() * dpr;
        bool targetChanged = false;
        if (!m_texture || m_texture->pixelSize() != size) {
            const int limit = rhi()->resourceLimit(QRhi::TextureSizeMax);
            if (size.isEmpty() || size.width() > limit || size.height() > limit)
                qFatal("Offscreen target exceeds the device texture limit");
            const QSize oldSize = m_texture ? m_texture->pixelSize() : QSize();
            // RGBA8 is 4 bytes; depth/stencil may use an 8-byte format (D32FS8).
            // Include both old/new plans even if the driver uses memoryless storage.
            constexpr qint64 attachmentBytesPerPixel = 4 + 8;
            const qint64 bytes = qint64(size.width()) * size.height() * attachmentBytesPerPixel;
            const qint64 oldBytes =
                    qint64(oldSize.width()) * oldSize.height() * attachmentBytesPerPixel;
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
        if (targetChanged || m_targetDpr != dpr) {
            auto target = QQuickRenderTarget::fromRhiRenderTarget(m_target.get());
            target.setDevicePixelRatio(dpr);
            m_window.setRenderTarget(target);
            const bool dprChanged = m_targetDpr != dpr;
            m_targetDpr = dpr;
            if (dprChanged) {
                // Changing an external RenderControl target has no native screen event.
                // Deliver Qt's window DPR notification before polishing the actual target.
                QEvent change(QEvent::DevicePixelRatioChange);
                QCoreApplication::sendEvent(&m_window, &change);
            }
        }
        QElapsedTimer timer;
        if (measurementEnabled)
            timer.start();
        m_control.polishItems();
        const qint64 polished = measurementEnabled ? timer.nsecsElapsed() : 0;
        m_control.beginFrame();
        const qint64 begun = measurementEnabled ? timer.nsecsElapsed() : 0;
        m_control.sync();
        const qint64 synced = measurementEnabled ? timer.nsecsElapsed() : 0;
        m_control.render();
        const qint64 rendered = measurementEnabled ? timer.nsecsElapsed() : 0;
        m_control.endFrame();
        if (measurementEnabled)
            Q_EMIT frameMeasured({ { u"polishMs"_s, polished / 1e6 },
                                   { u"beginFrameMs"_s, (begun - polished) / 1e6 },
                                   { u"syncMs"_s, (synced - begun) / 1e6 },
                                   { u"quickRenderMs"_s, (rendered - synced) / 1e6 },
                                   { u"endFrameMs"_s, (timer.nsecsElapsed() - rendered) / 1e6 } });
        Q_EMIT frameRendered();
    }
    QQuickWindow &m_window;
    QQuickRenderControl &m_control;
    bool m_pending = false;
    bool m_active = false;
    bool m_initialized = false;
    qint64 m_peakBytes = 0;
    qreal m_targetDpr = 0;
    qreal m_requestedDpr = 0;
    std::unique_ptr<QRhiTexture> m_texture;
    std::unique_ptr<QRhiRenderBuffer> m_depth;
    std::unique_ptr<QRhiRenderPassDescriptor> m_pass;
    std::unique_ptr<QRhiTextureRenderTarget> m_target;
};
