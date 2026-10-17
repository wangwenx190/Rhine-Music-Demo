#pragma once
// All limited-compatibility APIs are isolated here and in this directory.
#include <rhi/qrhi.h>
#include <rhi/qshader.h>
#include <QFile>
#include <stdexcept>
#include <memory>
namespace yalahoo::rhi {
inline void require(bool ok, const char *reason)
{
    if (!ok)
        throw std::runtime_error(reason);
}
struct ResourceUpdateDeleter
{
    void operator()(QRhiResourceUpdateBatch *batch) const noexcept
    {
        batch->release();
    }
};
using ResourceUpdates = std::unique_ptr<QRhiResourceUpdateBatch, ResourceUpdateDeleter>;
QShader shader(const QString &path);
inline QMatrix4x4 lightView()
{
    QMatrix4x4 m;
    m.lookAt({ -6, 14, -5 }, { 0, 0, 0 }, { 0, 1, 0 });
    return m;
}
inline QMatrix4x4 lightProjection()
{
    QMatrix4x4 m;
    m.ortho(-16, 16, -15, 15, .1f, 45);
    return m;
}
}
