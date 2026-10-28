# 原生材质与后处理算法来源

原生C++/QRhi/qsb及Qt Quick 3D Review移植既有锁定Three.js的cubeUV/PMREM、透射bicubic、DFG/GGX与多重散射、LTC求值及64×64查表、ACES/sRGB、PCF/Vogel阴影，以及BokehShader（Martins Upitis采样）、SMAAShader/SMAAPass（SMAA v2.8 WebGL 1x Medium及内嵌area/search PNG）。Three.js不在原生运行时链接或加载；MIT许可证保留于[three-LICENSE](three-LICENSE)。

源码版本和算法/资产hash由`resources/manifests/{materials,lighting,postprocess,rhi-shell}.json`及相应契约JSON维护。音乐世界光场与选择传输来自冻结`music-lighting.ts`/`music-row-lighting.ts`，原项目作者与素材署名见本目录NOTICE.md及project-LICENSE。本说明只维护算法来源和许可；当前实现规范见`docs/contracts/`，历史参数说明不覆盖现行源码。
