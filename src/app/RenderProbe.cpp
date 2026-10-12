#include "RenderProbe.h"
void RenderProbeInstances::setPositions(const QVariantList &positions) {
    m_buffer.clear();m_count=int(positions.size());
    for(int i=0;i<m_count;++i){const auto p=positions[i].toList();
        const auto entry=calculateTableEntry({p[0].toFloat(),p[1].toFloat(),p[2].toFloat()},
            {1,1,1},{},Qt::white,{float(i),0,0,0});
        m_buffer.append(reinterpret_cast<const char *>(&entry),sizeof(entry));
    }markDirty();
}
RenderProbeAtlas::RenderProbeAtlas(QQuick3DObject *parent):QQuick3DTextureData(parent),m_pixels(pageBytes,'\0') {
    setSize({2048,2048});setFormat(RGBA8);setHasTransparency(false);
    // 64 deterministic tiles, no user music/artwork. Byte RGBA has no endian dependency.
    for(int y=0;y<2048;++y)for(int x=0;x<2048;++x){const int i=(y*2048+x)*4,tile=(y/256)*8+x/256;
        m_pixels[i]=char(40+tile*3);m_pixels[i+1]=char(180-tile*2);m_pixels[i+2]=char(70+tile*2);m_pixels[i+3]=char(-1);
    }setPhase(0);
}
void RenderProbeAtlas::setPhase(int phase) {
    if(phase==m_phase)return;
    m_phase=phase;
    for(int y=0;y<256;++y)for(int x=0;x<256;++x){const int i=(y*2048+x)*4;
        m_pixels[i]=char(phase?230:40);m_pixels[i+1]=char(phase?30:180);m_pixels[i+2]=char(phase?90:70);
    }
    setTextureData(m_pixels);update();++m_submissions;
}
