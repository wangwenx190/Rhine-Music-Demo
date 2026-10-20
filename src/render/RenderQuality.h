#pragma once
#include <QObject>
#include <QSize>
#include <QString>
#include <QVariantMap>

namespace yalahoo {
// Physical output dimensions are independent of artistic effect controls.
class RenderQuality : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap values READ values NOTIFY changed)
    Q_PROPERTY(int shadows READ shadows NOTIFY changed)
    Q_PROPERTY(int aoSamples READ aoSamples CONSTANT)
    Q_PROPERTY(QSize targetSize READ targetSize NOTIFY changed)
    Q_PROPERTY(QSize aoSize READ targetSize NOTIFY changed)
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
        return 64;
    }
    QSize targetSize() const
    {
        return m_size;
    }
Q_SIGNALS:
    void changed();

private:
    QSize m_size{ 960, 640 };
    double m_depthOfField = 100;
};
}
