// Reads the exact submitted uniform bytes without deriving expectations from Input defaults.
#include "RhiObservation.h"
#include <QJsonArray>
#include <cstring>
using namespace Qt::StringLiterals;
namespace yalahoo::rhi::renderDetail {
void appendFogUniform(QJsonArray &fogUniforms, const Uniform &submitted)
{
    fogUniforms.append(QJsonObject{
            { u"kind"_s, submitted.draw[0] },
            { u"shadow"_s,
              QJsonArray{ submitted.draw[1], submitted.draw[2], submitted.draw[3],
                          submitted.optics[1], submitted.optics[2], submitted.optics[3] } },
            { u"range"_s, QJsonArray{ submitted.fog[0], submitted.fog[1], submitted.fog[2] } },
            { u"color"_s,
              QJsonArray{ submitted.background[0], submitted.background[1],
                          submitted.background[2] } } });
}
void recordOpaqueUniform(const QByteArray &bytes, quint32 offset, QJsonArray &captureKinds,
                         QJsonArray &fogUniforms)
{
    Uniform submitted;
    std::memcpy(&submitted, bytes.constData() + offset, sizeof(submitted));
    captureKinds.append(submitted.draw[0]);
    appendFogUniform(fogUniforms, submitted);
}
}
