#pragma once
#include <QObject>
#include <QString>

namespace yalahoo {
// viewport-layout.ts, in actual window content logical pixels (never DPR pixels).
class LayoutMetrics : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(LayoutMetrics)
    Q_PROPERTY(double width READ width NOTIFY changed FINAL)
    Q_PROPERTY(double height READ height NOTIFY changed FINAL)
    Q_PROPERTY(bool portrait READ portrait NOTIFY changed FINAL)
    Q_PROPERTY(bool compact READ compact NOTIFY changed FINAL)
    Q_PROPERTY(QString kind READ kind NOTIFY changed FINAL)
    Q_PROPERTY(double layoutScale READ layoutScale NOTIFY changed FINAL)
    Q_PROPERTY(double referenceWidth READ referenceWidth NOTIFY changed FINAL)
    Q_PROPERTY(double referenceHeight READ referenceHeight NOTIFY changed FINAL)
    Q_PROPERTY(double informationX READ informationX NOTIFY changed FINAL)
    Q_PROPERTY(double informationRight READ informationRight NOTIFY changed FINAL)
public:
    explicit LayoutMetrics(QObject *parent = nullptr) : QObject(parent)
    {
    }
    double width() const
    {
        return m_width;
    }
    double height() const
    {
        return m_height;
    }
    bool portrait() const
    {
        return m_width / m_height < 1.05;
    }
    bool compact() const
    {
        return portrait() || m_width < 1100 || (m_coarse && m_height < 600);
    }
    QString kind() const;
    double layoutScale() const;
    double referenceWidth() const;
    double referenceHeight() const;
    double informationX() const;
    double informationRight() const;
    Q_INVOKABLE void setViewport(double width, double height, bool coarse = false,
                                 bool cinematic = false);
Q_SIGNALS:
    void changed();

private:
    double m_width = 960, m_height = 640;
    bool m_coarse = false, m_cinematic = false;
};
}
