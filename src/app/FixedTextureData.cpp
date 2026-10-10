#include "FixedTextureData.h"
#include <QFile>
#include <QtEndian>
#include <QSize>
#include <cstring>
void FixedTextureData::setAsset(const QString &asset) {
    if(asset==m_asset)return;
    QSize size;
    if(asset=="room-pmrem")size={768,1024};
    else if(asset=="dfg")size={16,16};
    else { qWarning("Unknown fixed material texture"); return; }
    QFile file(":/assets/environment/"+asset+".rgba32f");
    if(!file.open(QIODevice::ReadOnly)){qWarning("Missing fixed material texture");return;}
    auto bytes=file.readAll();
    if(bytes.size()!=size.width()*size.height()*16){qWarning("Invalid fixed material texture size");return;}
#if Q_BYTE_ORDER == Q_BIG_ENDIAN
    for(qsizetype i=0;i<bytes.size();i+=4){quint32 word=qFromLittleEndian<quint32>(bytes.constData()+i);std::memcpy(bytes.data()+i,&word,4);}
#endif
    m_asset=asset;setSize(size);setFormat(QQuick3DTextureData::RGBA32F);setHasTransparency(false);setTextureData(bytes);emit assetChanged();
}
