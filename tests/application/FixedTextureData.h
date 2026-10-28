// Exposes immutable bundled pixels to the Quick 3D review materials. Texture ownership follows
// the Qt object tree rather than temporary image buffers.

#pragma once
#include <QQuick3DTextureData>
#include <QtQml/qqmlregistration.h>
class FixedTextureData : public QQuick3DTextureData
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(FixedTextureData)
    QML_ELEMENT
    Q_PROPERTY(QString asset READ asset WRITE setAsset NOTIFY assetChanged FINAL)
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
