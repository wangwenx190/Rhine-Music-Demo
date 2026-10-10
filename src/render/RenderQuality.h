#pragma once
#include <QObject>
#include <QVariantMap>
#include <QSize>
namespace yalahoo {
// Authored choices remain separate from effective, device-bounded targets.
class RenderQuality : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap values READ values NOTIFY changed)
    Q_PROPERTY(QString preset READ preset WRITE setPreset NOTIFY changed)
    Q_PROPERTY(int shadows READ shadows NOTIFY changed)
    Q_PROPERTY(int aoSamples READ aoSamples NOTIFY changed)
    Q_PROPERTY(double aoResolution READ aoResolution NOTIFY changed)
    Q_PROPERTY(QSize targetSize READ targetSize NOTIFY changed)
    Q_PROPERTY(QSize aoSize READ aoSize NOTIFY changed)
    Q_PROPERTY(bool limited READ limited NOTIFY changed)
public:
    explicit RenderQuality(QObject *parent=nullptr);
    QVariantMap values() const {return m_values;}
    QString preset() const;
    void setPreset(const QString &name);
    Q_INVOKABLE void configure(const QVariantMap &values, bool legacyHigh=true);
    Q_INVOKABLE void resize(double width,double height,double stageScale,double dpr,int maxTextureSize);
    int shadows() const {return m_values["shadows"].toInt();}
    int aoSamples() const {return m_values["aoSamples"].toInt();}
    double aoResolution() const {return m_values["aoResolution"].toDouble();}
    QSize targetSize() const {return m_size;}
    QSize aoSize() const;
    bool limited() const {return m_limited;}
    static QVariantMap authoredPreset(const QString &name);
signals:
    void changed();
private:
    void measure();
    QVariantMap m_values;
    QSize m_size{960,640};
    double m_width=960,m_height=640,m_stageScale=1,m_dpr=1;
    int m_maxTexture=4096;
    bool m_limited=false;
};
}
