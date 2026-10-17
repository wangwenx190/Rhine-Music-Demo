#pragma once
#include <QQuick3DTextureData>
#include <QtQml/qqmlregistration.h>
class FixedTextureData : public QQuick3DTextureData
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString asset READ asset WRITE setAsset NOTIFY assetChanged)
public:
    explicit FixedTextureData(QQuick3DObject *parent = nullptr) : QQuick3DTextureData(parent)
    {
    }
    QString asset() const
    {
        return m_asset;
    }
    void setAsset(const QString &asset);
Q_SIGNALS:
    void assetChanged();

private:
    QString m_asset;
};
