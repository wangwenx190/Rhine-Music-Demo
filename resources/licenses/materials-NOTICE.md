# 单盒材质算法来源

原生材质移植项目既有锁定 Three.js 的 cubeUV、透射 bicubic、DFG、物理环境 BRDF 与 ACES 数学；不链接 Three.js 到原生程序。MIT 许可证保存在 `three-LICENSE`，算法与资产 hash 在 `docs/contracts/materials.json` 和 `resources/manifests/materials.json`。原有项目与素材署名保留在原 NOTICE 及 legacy-web 归档。

R08 also derives the LTC evaluator, 64×64 LTC tables, cubeUV diffuse environment sampling and GGX multiscatter compensation from the same locked Three.js distribution. See resources/manifests/lighting.json and docs/contracts/lighting.md for the source/hash and retained MIT license. Original world-space field and selection transport follow the frozen project's music-lighting.ts/music-row-lighting.ts; original author notices remain in legacy-web/.

R09B ports the locked Three.js BokehShader (Martins Upitis bokeh sampling), SMAAShader/SMAAPass (SMAA v2.8 WebGL 1x Medium), their embedded area/search PNG lookups, and OutputPass ACES/sRGB mathematics. They are retained under the same Three MIT license in three-LICENSE; exact dependency/source and asset hashes are in resources/manifests/postprocess.json. Native execution uses only Qt resources and public Effect APIs. All original project authors and music artwork notices remain in legacy-web/.
