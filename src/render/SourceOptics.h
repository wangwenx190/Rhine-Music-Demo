// Exposes cached source optical values to the fixed three-material Quick3D Review scene.
// Only parameter changes recalculate values; tint endpoints depend solely on surface identity.

#pragma once
#include "OpticalParameters.h"
#include <QObject>
#include <QColor>
#include <cmath>

namespace yalahoo {
class SourceOptics : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SourceOptics)
    Q_PROPERTY(int surface READ surface WRITE setSurface NOTIFY surfaceChanged FINAL)
    Q_PROPERTY(double warmth READ warmth WRITE setWarmth NOTIFY changed FINAL)
    Q_PROPERTY(double clarity READ clarity WRITE setClarity NOTIFY changed FINAL)
    Q_PROPERTY(double transmission READ transmission NOTIFY changed FINAL)
    Q_PROPERTY(double roughness READ roughness NOTIFY changed FINAL)
    Q_PROPERTY(double thickness READ thickness NOTIFY changed FINAL)
    Q_PROPERTY(double attenuationDistance READ attenuationDistance NOTIFY changed FINAL)
    Q_PROPERTY(double clearcoat READ clearcoat NOTIFY changed FINAL)
    Q_PROPERTY(double coatRoughness READ coatRoughness NOTIFY changed FINAL)
    Q_PROPERTY(QColor dayTint READ dayTint NOTIFY surfaceChanged FINAL)
    Q_PROPERTY(QColor nightTint READ nightTint NOTIFY surfaceChanged FINAL)
    Q_PROPERTY(QColor dayAttenuation READ dayAttenuation CONSTANT FINAL)
    Q_PROPERTY(QColor nightAttenuation READ nightAttenuation NOTIFY surfaceChanged FINAL)
    Q_PROPERTY(double fallbackEnvironment READ fallbackEnvironment NOTIFY changed FINAL)
    Q_PROPERTY(double fallbackExposure READ fallbackExposure NOTIFY changed FINAL)
public:
    explicit SourceOptics(QObject *parent = nullptr) : QObject(parent)
    {
    }
    int surface() const
    {
        return m_surface;
    }
    double warmth() const
    {
        return m_warmth;
    }
    double clarity() const
    {
        return m_clarity;
    }
    void setSurface(int value)
    {
        if (value == m_surface || value < 0 || value >= opticalSource::SurfaceCount)
            return;
        m_surface = value;
        update();
        Q_EMIT surfaceChanged();
    }
    void setWarmth(double value)
    {
        setAmount(m_warmth, value);
    }
    void setClarity(double value)
    {
        setAmount(m_clarity, value);
    }
    double transmission() const
    {
        return m_values.transmission;
    }
    double roughness() const
    {
        return m_values.roughness;
    }
    double thickness() const
    {
        return m_values.thickness;
    }
    double attenuationDistance() const
    {
        return m_values.attenuationDistance;
    }
    double clearcoat() const
    {
        return m_values.clearcoat;
    }
    double coatRoughness() const
    {
        return m_values.coatRoughness;
    }
    QColor dayTint() const
    {
        return QColor(opticalSource::DayTint[m_surface]);
    }
    QColor nightTint() const
    {
        return QColor(opticalSource::NightTint[m_surface]);
    }
    QColor dayAttenuation() const
    {
        return QColor(opticalSource::DayAttenuation);
    }
    QColor nightAttenuation() const
    {
        return QColor(m_surface == opticalSource::Tray ? opticalSource::DayAttenuation
                                                       : opticalSource::NightAttenuation);
    }
    double fallbackEnvironment() const
    {
        return opticalSource::FallbackEnvironment
                - opticalSource::FallbackEnvironmentWarmthLoss * m_warmth;
    }
    double fallbackExposure() const
    {
        return opticalSource::FallbackExposure
                - opticalSource::FallbackExposureWarmthLoss * m_warmth;
    }
Q_SIGNALS:
    void changed();
    void surfaceChanged();

private:
    void setAmount(double &amount, double value)
    {
        if (!std::isfinite(value))
            return;
        value = std::clamp(value, 0.0, 1.0);
        if (value == amount)
            return;
        amount = value;
        update();
    }
    void update()
    {
        m_values = opticalSource::parameters(m_surface, m_warmth, m_clarity);
        Q_EMIT changed();
    }
    int m_surface = opticalSource::Lid;
    double m_warmth = 1, m_clarity = 0;
    opticalSource::Parameters<double> m_values =
            opticalSource::parameters(m_surface, m_warmth, m_clarity);
};
}
