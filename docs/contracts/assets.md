# R03 · 原生资产契约

输入冻结提交为 `57f4f845c20cce646a15dbf4008c2a162aaaf4e2`。`resources/manifests/assets.json` 逐项记录原路径、输入 SHA-256、生产文件 SHA-256、转换方式和 qrc 地址；额外的 CSS 输入也记录 hash。没有读取私人音乐、配置或缓存，旧品牌图形没有接入。

## 准备与加载

Windows 运行 `./tools/assets/prepare-assets.ps1`。脚本通过根 CMake 发现最新 Kit，构建 C++23 `prepare-assets`，核对同 Kit balsam 的 Windows 版本资源；C++ 工具再核对该 Kit 的 qmake 版本与运行 Qt。不使用任意 PATH 中的导入器，不固定 Qt minor/patch。脚本核对 24 个不同输入的 Git blob 与冻结提交。

macOS/Linux 可运行 `sh tools/assets/prepare-assets.sh qt-debug` 重新导入模型、提取图标并校验字体。系统 DirectWrite 解包只在 Windows 资产工具中编译、链接和执行；其他平台复用已入库的 SFNT，并核对 WOFF2 输入与 SFNT 输出的 hash，不链接 Dwrite。不需要在产品启动时转换字体或导入 GLB。平台实机执行尚未验证。

`resources/manifests/assets.cmake` 是生成的明确文件清单，由 `cmake/Assets.cmake::yalahoo_add_assets` 同时接入应用和资产测试。统一前缀 `:/assets/`，QML 使用 `qrc:/assets/`。独立 `qsb` 资源仍由原构建链生成。应用先注册字体，再创建 QML 引擎，不依赖工作目录。

## 三批次模型

原 `music-case-v040.glb` 和构造说明原样保留在 `resources/models/`。同 Kit balsam 输出 `Music_case_v040.qml` 和 `meshes/meshes_0__mesh.mesh`、`meshes_1__mesh.mesh`、`meshes_2__mesh.mesh`，分别对应 Frosted_Polymer、Ivory_Edges、Optical_Diffuser。导入关闭 LOD、全局缩放、节点/网格优化、重复顶点合并与缓存索引优化，不做非均匀缩放。导入 QML 的原材质只作为资产参照，R07 负责实际材质。

`tools/assets/GeometryAudit` 使用 Qt 公共 Core/Gui 和独立的有界文件检查器，不链接 Qt 私有渲染接口。对 GLB 和所选 Kit 实际 mesh 格式做探测；目前接受 mesh v7，未来格式变化必须明确报错后适配，不退回旧 Qt。比较完整顶点属性和保留绕序的三角形集合，允许三角形顺序/循环起点变化。Qt 的 V 为 `1 - glTF V`，位置、法线、节点坐标不变。三批次合计 1,000 三角形，逐批包围盒/顶点/三角数和最大属性误差见 `resources/manifests/geometry.json`。

纸面是后续独立批次，不在原 GLB 三壳体中。保留纸面 3.06×3.06、中心 `(0.17,1.85,0.027)`，后托前面 `-0.032`、前盖背面 `0.052`、正面 `0.070`。检查纸面处于后托和前盖之间。本轮窗口以均匀比例显示原几何，使用临时探针材质，不作为材质复刻验收。

## 字体

四个原 WOFF2 在 `resources/fonts/source/` 保留原字节。先用 Qt 尝试各文件；Windows 所选 Kit 均不支持，按 SFNT 签名通过 `IDWriteFactory5::UnpackFontFile` 解包为 **OTF/CFF**，保存为 `resources/fonts/MiSans-{Light,Regular,Demibold,Bold}.otf`。转换只改变容器，不修改字形、字距或内部字重。MiSans NOTICE 与许可 PDF 随资源保留。

| 字体 | Web CSS 权重 | 原 OS/2 权重 |
|---|---:|---:|
| Light | 300 | 250 |
| Regular | 400 | 330 |
| Demibold | 600 | 450 |
| Bold | 700 | 630 |

Qt 的数值权重匹配不能直接代替 Web 的 `@font-face` 映射。`src/app/AssetCatalog` 按字体实际 family/styleName 显式选择原字形；C++ 使用 `assetFont(cssWeight)`，QML 使用 `MusicFonts.light/regular/demibold/bold`，随后设置 pixelSize。不要仅设置 `font.bold` 或用系统字体代替。默认应用字体为原 Regular，品牌探针标题使用原 Bold。实际 styleName 和映射见 `resources/manifests/fonts.json`。

启动时核对实际选中字体的 CFF/glyf 表；若 DirectWrite/CoreText/Fontconfig 暴露的字体别名不同，则从本次注册返回的 family/style 中找到同一字形表。无法选中原字形时明确失败并回滚注册，不静默使用系统替代字体。

```qml
font.family: MusicFonts.demibold.family
font.styleName: MusicFonts.demibold.styleName
font.weight: MusicFonts.demibold.weight
font.pixelSize: 24
```

测试核对四个独立 CFF 表、原 OS/2 权重、QRawFont 实际选择的字形表、中文/拉丁/数字/括号 glyph 覆盖与 QTextLayout 整行 shaping 的字体来源及宽度。Windows 24px 测量在资产报告中，四字重预览在 `docs/media/assets/`。emoji 的通用回退/字轮行为留给 R16，不声称 MiSans 包含全部 emoji。

## 功能图标与音频

八个生产功能图标从冻结 `music-app.ts` 的 `svg()` 调用逐路径提取：play、pause、stop、search、settings、folder、overview、overview-return。保留 `viewBox="0 0 24 24"`、1.5 描边、round 端点与连接。每个提供 day/night SVG，由实际 `music.css` 的 ink/surface 解析颜色；不复制旧品牌。相应 qrc 地址如 `qrc:/assets/icons/play-day.svg`。后续主题过渡由控件处理，图标模板几何不变。

`resources/audio/` 原样复用完整 atmosphere/motif/pulse Ogg 与 observatory-preview MP3、谱面和署名说明。测试从 qrc 经 QMediaPlayer 无声加载四个文件：没有 QAudioOutput、不调用 play、PlayingState 次数为零，并核对 LoadedMedia 和 duration。这是资源/格式验收，歌曲/BGM 实际播放分别在 R24/R25 实现。

`audio/interface-events.json` 保留原 `Sound` 的 17 个事件名，当前全部 source=null、enabled=false。旧音效使用振荡器/噪声合成或内嵌 typing PCM，没有可直接播放的独立短文件；不合成、裁切或导出 PCM，不把完整 typing-preview 当按键声。后续 InterfaceSoundController 消费该映射并对缺失资产静音。

演示 square/portrait/landscape 封面及原说明随 qrc 接入，供后续匿名 fixture 使用。原项目 LICENSE、NOTICE 和音乐作者署名保留。

## 验证入口与范围

`./tools/build/verify-r03.ps1` 重建同 Kit 可复现资源，运行 Windows Debug/Release 的全部 CTest、qmllint、真实 D3D11 窗口、中文/空格工作目录启动与无声媒体加载。证据在 `verification/windows/assets/`，预览在 `docs/media/assets/`。`./tools/audit/audit-r03.ps1` 检查逐分支资源闭包，报告在 `verification/{macos,linux}-static/assets/`；结论为“静态审计通过，实机待用户验证”。本轮不覆盖后续完整播放器、材质/动画复刻和独立部署。

接口依据：[Qt balsam/glTF 导入](https://doc.qt.io/qt-6/quick3d-asset-intro.html)、[DirectWrite UnpackFontFile](https://learn.microsoft.com/en-us/windows/win32/api/dwrite_3/nf-dwrite_3-idwritefactory5-unpackfontfile)。实际 API 与格式同时按已选 Kit 的头文件和执行结果核对。
