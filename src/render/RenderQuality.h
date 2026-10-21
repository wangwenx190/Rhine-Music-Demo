// Derives physical render-target sizes and quality parameters from logical output. Explicit
// bounds protect integer dimensions and attachment allocation.

#pragma once
#include "SceneFrame.h"
#include <QObject>
#include <QSize>
#include <QString>
#include <QVariantMap>

namespace yalahoo {
namespace renderQuality {
// The default output follows the source quality contract without a DPR-dependent sample reduction.
inline constexpr auto AoSamples = 64;
inline constexpr auto DefaultDepthOfFieldPercent = 100;
}

// Physical output dimensions are independent of artistic effect controls.
class RenderQuality : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(RenderQuality)
    Q_PROPERTY(QVariantMap values READ values NOTIFY changed FINAL)
    Q_PROPERTY(int shadows READ shadows NOTIFY changed FINAL)
    Q_PROPERTY(int aoSamples READ aoSamples CONSTANT FINAL)
    Q_PROPERTY(QSize targetSize READ targetSize NOTIFY changed FINAL)
    Q_PROPERTY(QSize aoSize READ targetSize NOTIFY changed FINAL)
public:
    explicit RenderQuality(QObject *parent = nullptr);
    QVariantMap values() const
    {
        using namespace Qt::StringLiterals;
        return { { u"depthOfField"_s, m_depthOfField } };
    }
    Q_INVOKABLE void configure(const QVariantMap &values);
    Q_INVOKABLE void resize(double width, double height, double dpr);
    int shadows() const;
    int aoSamples() const
    {
        return renderQuality::AoSamples;
    }
    double depthOfField() const
    {
        return m_depthOfField;
    }
    QSize targetSize() const
    {
        return m_size;
    }
Q_SIGNALS:
    void changed();

private:
    QSize m_size{ yalahoo::sceneDefaults::WidthPixels, yalahoo::sceneDefaults::HeightPixels };
    double m_depthOfField = renderQuality::DefaultDepthOfFieldPercent;
};
}
