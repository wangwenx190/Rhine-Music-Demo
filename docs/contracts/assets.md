# 导入资产、字体与来源保护

输入冻结提交为 `57f4f845c20cce646a15dbf4008c2a162aaaf4e2`。`resources/manifests/assets.json` 逐项记录原路径、输入 SHA-256、生产文件 SHA-256、转换方式和 qrc 地址；历史CSS来源hash保留在相应设计/来源契约。没有读取私人音乐、配置或缓存，旧品牌图形没有接入。

## 准备与加载

`resources/`为完整原生输入，`resources/manifests/assets.json`的每项path/qrc/bytes/SHA256描述当前资产；历史source/inputSha256仅作来源标识，不读取对应上游文件。修改维护资产时同步其当前bytes/SHA256，保留作者、许可与历史来源。

Windows执行`./tools/assets/prepare-assets.ps1`，macOS/Linux执行`sh tools/assets/prepare-assets.sh qt-release`。同Kit的C++23 `prepare-assets`验证原生清单与四个OTF，并从`resources/models/music-case-v040.glb`经同Kit balsam重导入Qt模型；只更新三mesh、导入QML、geometry及其清单。图标、字体、音频、演示封面与说明原样保留，不从其他工程覆盖。`prepare-assets <repository> <selected-Qt-Kit> --verify-inputs`只验证同Kit qmake及原生清单/字体，不写输出。

`resources/manifests/assets.cmake`为明确文件清单，由`cmake/Assets.cmake::yalahoo_add_assets`同时接入应用和资产测试；新增/删除资产时同步此清单。统一前缀`:/assets/`，QML使用`qrc:/assets/`；qsb仍由正常构建链生成。应用先注册字体再创建QML引擎，不依赖当前工作目录。

## 三批次模型

原 `music-case-v040.glb` 和构造说明原样保留在 `resources/models/`。同 Kit balsam 输出 `Music_case_v040.qml` 和 `meshes/meshes_0__mesh.mesh`、`meshes_1__mesh.mesh`、`meshes_2__mesh.mesh`，分别对应 Frosted_Polymer、Ivory_Edges、Optical_Diffuser。导入关闭 LOD、全局缩放、节点/网格优化、重复顶点合并与缓存索引优化，不做非均匀缩放。导入 QML 的原材质只作为资产参照，实际材质由[materials.md](materials.md)维护。

`audit/assets/GeometryAudit` 使用 Qt 公共 Core/Gui 和独立的有界文件检查器，不链接 Qt 私有渲染接口。生产 GLB 解码由`src/core/GeometryData`提供，工具读写由`tools/assets/AssetFiles`提供；审计对 GLB 和所选 Kit 实际 mesh 格式做探测；目前接受 mesh v7，未来格式变化必须明确报错后适配，不退回旧 Qt。比较完整顶点属性和保留绕序的三角形集合，允许三角形顺序/循环起点变化。Qt 的 V 为 `1 - glTF V`，位置、法线、节点坐标不变。三批次合计 1,000 三角形，逐批包围盒/顶点/三角数和最大属性误差见 `resources/manifests/geometry.json`。

纸面是独立批次，不在原 GLB 三壳体中。保留纸面 3.06×3.06、中心 `(0.17,1.85,0.027)`，后托前面 `-0.032`、前盖背面 `0.052`、正面 `0.070`。检查纸面处于后托和前盖之间。资产几何探针使用无光材质；实际材质规范归materials.md。

## 字体

四个原WOFF2在`resources/fonts/source/`保留来源字节；产品直接使用已提交的`resources/fonts/MiSans-{Light,Regular,Demibold,Bold}.otf`（OTF/CFF）。二者仅容器不同，字形、字距及内部字重保留；准备工具验证已有OTF，不依赖DirectWrite解包或其他工程字体文件。MiSans NOTICE 与许可 PDF 随资源保留。

| 字体 | Web CSS 权重 | 原 OS/2 权重 |
|---|---:|---:|
| Light | 300 | 250 |
| Regular | 400 | 330 |
| Demibold | 600 | 450 |
| Bold | 700 | 630 |

Qt 的数值权重匹配不能直接代替 Web 的 `@font-face` 映射。`src/app/AssetCatalog` 按字体实际 family/styleName 显式选择原字形；C++ 使用 `assetFont(cssWeight)`，QML 使用 `MusicFonts.light/regular/demibold/bold`，随后设置 pixelSize。不要仅设置 `font.bold` 或用系统字体代替。默认应用字体为原 Regular，品牌探针标题使用原 Bold。渲染质量与hinting默认值见[display-output.md](display-output.md)。实际 styleName 和映射见 `resources/manifests/fonts.json`。

启动时核对实际选中字体的 CFF/glyf 表；若 DirectWrite/CoreText/Fontconfig 暴露的字体别名不同，则从本次注册返回的 family/style 中找到同一字形表。无法选中原字形时明确失败并回滚注册，不静默使用系统替代字体。

CoreText 的 `QRawFont::weight()` 可把原 OS/2 数值映射到 Qt 命名字重，例如原 Light 的 250 在本机报告为 300。资产测试分别检查原 OS/2 表/权重、选中与文件直接加载的 native weight 一致，以及完整 OS/2/CFF 表和整行 shaping 来源；不能用平台报告值替换 manifest 中的原始字重，也不能为此改字体资产。

```qml
font.family: MusicFonts.demibold.family
font.styleName: MusicFonts.demibold.styleName
font.weight: MusicFonts.demibold.weight
font.pixelSize: 24
```

测试核对四个独立 CFF 表、原 OS/2 权重、QRawFont 实际选择的字形表、中文/拉丁/数字/括号 glyph 覆盖与 QTextLayout 整行 shaping 的字体来源及宽度。emoji回退/字轮行为由[title-reels.md](title-reels.md)维护，MiSans不包含全部emoji。

原创品牌资产另见 [branding.md](branding.md) 和 `resources/manifests/branding.json`；其 SVG/平台图标及导出验证独立维护，页眉/开场及系统接入分别见browse-ui/opening契约与平台操作文档。

## 功能图标与音频

八个生产功能图标从冻结 `music-app.ts` 的 `svg()` 调用逐路径提取：play、pause、stop、search、settings、folder、overview、overview-return。保留 `viewBox="0 0 24 24"`、1.5 描边、round 端点与连接。每个提供 day/night SVG，由实际 `music.css` 的 ink/surface 解析颜色；不复制旧品牌。相应 qrc 地址如 `qrc:/assets/icons/play-day.svg`。主题过渡由控件处理，图标模板几何不变。

`resources/audio/` 原样复用完整 atmosphere/motif/pulse Ogg 与 observatory-preview MP3、谱面和署名说明。测试从 qrc 经 QMediaPlayer 无声加载四个文件：没有 QAudioOutput、不调用 play、PlayingState 次数为零，并核对 LoadedMedia 和 duration。这是资源/格式验收，实际歌曲/BGM播放由[playback.md](playback.md)维护。

`audio/interface-events.json` 保留原 `Sound` 的 17 个事件名，当前全部 source=null、enabled=false。旧音效使用振荡器/噪声合成或内嵌 typing PCM，没有可直接播放的独立短文件；不合成、裁切或导出 PCM，不把完整 typing-preview 当按键声。InterfaceSoundController消费该映射，对缺失资产静音。

演示square/portrait/landscape封面及来源说明随qrc接入，供匿名演示与fixture使用；来源唯一见resources/demo-covers/README.md。总权利说明唯一见resources/licenses/NOTICE.md；音频作者/源谱归resources/audio/README.md。三份原生说明由prepare-assets原样保留，当前字节/hash由原生清单验证。

## 资产检查

`assets`检查manifest/qrc hash、完整GLB/Qt mesh属性/三批次/纸面间隙、四物理字体表/字重/shaping、功能SVG与无声媒体加载；`branding`检查独立原创资产。准备CLI见本文，通用Kit/构建/窗口规则只引用公共契约；平台剩余验收归计划R36/R19A-P。

## 输入保护与原子发布

准备工具先验证全部原生清单项的路径、重复项、qrc、体积/hash和字体可加载性，再启动模型生成。输入及其祖先不得为符号链接或junction；输入不一致、坏模型、工具失败或几何不等价均非零退出，不改已有资产。

模型重导入使用仓库内临时目录，几何/批次/变换全部通过后才发布派生模型与更新清单。发布前核对整批目的路径；替换失败逆序恢复，恢复失败保留临时目录并明确报告，不能删除唯一旧副本。原生GLB、字体、图标及说明均不属于重导入写集。

先构建Release prepare-assets，再执行`python3 tests/tools/AssetExportTest.py`，在只有resources的隔离目录检查实际重导入、输入错误不写入、符号链接拒绝、清单同步及维护说明保留；不需要Git、旧项目、Node或浏览器。
