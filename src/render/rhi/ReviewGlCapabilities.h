// Adapts Quick 3D review capability flags to the actual OpenGL context. Changes are restricted
// to the review renderer and retain requested sample counts.

#pragma once
#include <QLoggingCategory>
#include <QString>
using Qt::StringLiterals::operator""_s;
// Review-only private Qt adaptation; never linked or called by the product renderer.
#include <QJsonObject>
#include <QOpenGLContext>
#include <QQuickWindow>
#include <QtQuick3D/private/qquick3dscenemanager_p.h>
#include <QtQuick3DRuntimeRender/ssg/qssgrendercontextcore.h>
#include <QtQuick3DRuntimeRender/ssg/qssgrhicontext.h>
#include <rhi/qrhi.h>
#include <memory>
#if defined(Q_OS_MACOS)
#  include <QtGui/private/qrhigles2_p.h>
#endif

namespace yalahoo::review {
Q_STATIC_LOGGING_CATEGORY(logReviewGl, "yalahoo.render", QtInfoMsg)
#if defined(Q_OS_MACOS)
class ReviewRhiResourceAccess : public QRhiResource
{
public:
    static QRhiImplementation *implementation(const QRhiResource &resource)
    {
        // A protected member pointer accesses the actual base subobject, without
        // assuming QRhi layout, casting a base object to a derived one, or patching code.
        return resource.*&ReviewRhiResourceAccess::m_rhi;
    }
};
#endif
inline bool useReviewGlMultisampleBuffers(QRhi *api)
{
    if (!api)
        qCFatal(logReviewGl, "Review capability check requires the initialized RHI");
#if defined(Q_OS_MACOS)
    if (api->backend() != QRhi::OpenGLES2)
        return false;
    {
        auto *context = QOpenGLContext::currentContext();
        if (!context)
            qCFatal(logReviewGl, "Review OpenGL capability check requires the current RHI context");
        if (!context->getProcAddress("glTexStorage2DMultisample")) {
            // Quick3D already has a same-format, same-sample renderbuffer+resolve
            // path. Qt's GL backend advertises texture MSAA even when its storage
            // entry point is absent on Cocoa GL 4.1; correct only that capability.
            std::unique_ptr<QRhiTexture> descriptor(api->newTexture(QRhiTexture::RGBA8, { 1, 1 }));
            auto *backend =
                    static_cast<QRhiGles2 *>(ReviewRhiResourceAccess::implementation(*descriptor));
            if (backend->q != api || !api->isFeatureSupported(QRhi::MultisampleRenderBuffer))
                qCFatal(logReviewGl, "Review OpenGL cannot preserve multisample rendering");
            backend->caps.multisampledTexture = false;
            return true;
        }
    }
#else
    Q_UNUSED(api);
#endif
    return false;
}
// Called only on the scene graph thread, after the actual View3D has rendered.
inline QJsonObject quick3DTarget(QQuickWindow &window)
{
    QJsonObject result;
    auto *attachment = QQuick3DSceneManager::getOrSetWindowAttachment(window);
    if (!attachment || !attachment->rci())
        return result;
    const auto &context = attachment->rci()->rhiContext();
    result.insert(u"samples"_s, context->mainPassSampleCount());
    auto *target = context->renderTarget();
    if (!target || target->resourceType() != QRhiResource::TextureRenderTarget)
        return result;
    const auto description = static_cast<QRhiTextureRenderTarget *>(target)->description();
    if (description.colorAttachmentCount() == 0)
        return result;
    const auto *color = description.colorAttachmentAt(0);
    result.insert(u"renderbuffer"_s, color->renderBuffer() != nullptr);
    result.insert(u"resolved"_s, color->resolveTexture() != nullptr);
    if (color->renderBuffer() && color->resolveTexture())
        result.insert(u"formatPreserved"_s,
                      color->renderBuffer()->backingFormat() == color->resolveTexture()->format());
    return result;
}

}
