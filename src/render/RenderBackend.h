#pragma once
#include <QString>
using Qt::StringLiterals::operator""_s;
#include <QVariantMap>
namespace yalahoo {
enum class RenderBackend {
    Rhi
};
// The product has one renderer; Review-only Quick3D is a source comparison oracle.
struct RenderBackendContract
{
    static constexpr RenderBackend selected = RenderBackend::Rhi;
    static QVariantMap fields()
    {
        return { { u"selected"_s, u"rhi"_s },
                 { u"implementationReady"_s, true },
                 { u"activeImplementation"_s, u"rhi"_s },
                 { u"contractRevision"_s, 4 },
                 { u"contract"_s, u"docs/contracts/render-backend.json"_s } };
    }
};
}
