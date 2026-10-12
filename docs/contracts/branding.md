# yalahOO!!! · 原创品牌契约

本文件固定 R03B 的矢量资产与后续接入边界。原创几何由本项目绘制，无旧品牌轮廓、位图描摹、字体字形或外部资源。作者及历史素材署名仍在 `legacy-web/LICENSE`、`legacy-web/NOTICE.md` 保留。来源和 SHA-256 在 `resources/manifests/branding.json`。

## 选定稿与比较

三个细节方案在同一 [SVG 版面](../media/branding/concepts.svg) 和 [Qt 渲染](../media/branding/concepts.png) 上比较，从左到右为 A/B/C。均采用同一 −8° 薄玻璃专辑盒、暖白纸面、双唱片圆环与自绘 `yalahOO!!!` 字标。A 为闭口、较细外环和较松字距；B 为开口刻纹、较稳外环和标准字距；C 增加刻纹密度、收紧字距。

评分为本轮视觉审查判断，满分 5；不是用户投票或算法测量。专用小稿共用，细节稿的 48/64px 密度决定识别分的差异。

| 项目 | 权重 | A | B（选定） | C |
|---|---:|---:|---:|---:|
| 小尺寸识别 | 35% | 4.0 | 4.5 | 3.5 |
| 音乐收藏主题 | 25% | 4.5 | 4.5 | 4.5 |
| 细节与光学平衡 | 25% | 4.0 | 4.5 | 3.5 |
| Qt 与主题适配 | 15% | 4.5 | 4.5 | 4.0 |
| 加权总分 | | 4.20 | **4.50** | 3.83 |

B 的三条开口刻纹在大尺寸保持唱片质感，48–128px 不形成密集灰团。此后生产只使用 B。比较板是设计档案，不进入产品资源或可切换设置。

## 几何、颜色与安全区

- 图标 `viewBox="0 0 1024 1024"`，透明背景。盒体以 (512,512) 为轴旋转 −8°，主稿外框 (208,160,608,704)，纸面 (270,218,482,588)，两环中心 (406,520)/(634,520)，外环中心线半径 90、线宽 20；刻纹半径 74/60/46，开口 32°。琥珀唱针孔和极短纸面目录线只在主稿保留。
- 四个视觉层级为 `silhouette`、`paper`、`rings`、`highlights`；外层 `album-case` 统一变换。没有阴影滤镜、光晕、图像、脚本、SVG text 或远程引用。实际 alpha 边界全部落在中心 832×832 区域内，四边至少 96 单位；准确边界记录在导出报告。
- 暖昼来源为冻结 `music.css`：纸面 #efede7、背景 #e8e5e1、墨色 #232722、刻纹 #74786f、琥珀 #b56834。深夜使用同源 #08121f 背景、#e9eef2 墨色、#d9ad7d 琥珀。纸面高光 #fffaf0、玻璃 #c4c9c1→#8a978e→#42534e、纸面尾色 #dcd7c9 是同色相材质层次，不构成第三套界面主题。
- `icon.svg` 在两主题共用玻璃/纸面。`logo-light.svg` 和 `lockup-light.svg` 用于浅背景；dark 稿用于深背景。mono 稿显式使用 #232722；附加 mono-light 稿反白为 #e9eef2。单色图标的纸面和圆环孔是透明负空间，不用白色矩形假装挖空。
- 独立图标不放字标。完整字标 `viewBox="0 0 816 208"`；小写 yalah、大写 OO、三个感叹号均为原始路径。小写线宽 16，大写线宽 18；O 由连续三次贝塞尔闭环构成，细开口内纹呼应唱片。三个感叹号上端 34/30/26，点基线 130；同一倾向、逐步延长。y 下伸到 188（含圆端）；完整 alpha 边界不截断。
- 字标推荐资本高度 31px，紧凑 24px，最小 20px。以 104 单位资本高度换算完整画布尺寸：宽 `816*capHeight/104`、高 `208*capHeight/104`；不能把含下伸空间的 208 单位误当资本高度。相邻控件须继续按冻结源码的容器布局定位。

## 16–32px 光学稿

`icon-small.svg` 独立重绘。删除刻纹、孔点、目录线和窄高光，用一层玻璃描边、暖白内面及两道圆环。边框和圆环线宽 64，内孔直径 64；双环中心距约 263.88，实心外缘间距约 71.88；纸面内缘到环的水平间隙不小于 68 单位。16px/DPR1 的主要线条、孔径及间隙至少 1px。

为避免 1px 内孔落在四个像素的交点，小稿将 −8° 变换后的环心对齐 16px 栅格的 (6.5,8.5)/(10.5,7.5)。外框有光学加宽、收高、圆角及微移补偿，仍为同向专辑盒，完整处于 96 单位安全区。导出器对两孔与相邻环线的灰度差、双环间纸面像素做硬断言；[小稿检查](../media/branding/small-optics.png) 同时显示最近邻放大和真实尺寸，放大锯齿仅展示栅格，不是生产效果目标。

PNG、ICO 的 16/24/32 使用小稿；48 及以上使用主稿。iconset 按**实际像素尺寸**选择稿，例如 16@2x 使用 32px 小稿，32@2x 使用 64px 主稿。应用最小图标尺寸为 16px。

## 开场路径交接

`logo-master.svg`、各 logo 生产稿保留稳定 `word`、`rings`、`accents` 三组；路径 ID 对应 y/a/l/a/h、两个 O、三根感叹号和三个点，完整拼写由此固定。lockup 内采用 `logo-` 前缀避免与图标 rings 重名。

`logo-reveal.svg` 提供与生产字标相同的 17 条中心线路径（内圈装饰不参与中心线揭示）。`reveal-map.json` 记录每段 d、组、strokeWidth、Qt QPainterPath 测得的长度、offset 和总长度归一化 start/end。输入整体 `contourProgress` 后，段进度为 `clamp((progress-start)/(end-start),0,1)`，沿弧长绘制；不能直接把贝塞尔参数 t 当成弧长。零长点以 0.01 单位短线加圆端绘制，长度进度有限且确定。

R20 继续使用冻结 `musicOpeningFrame` 的 `settle(unit(t,0.24,1.64))`，accents 另受原 symbols 包络控制；揭示完成时显示同节点的完整生产稿，补齐细内纹。减少动态直接显示完整生产 Logo。保留 2.72–6.32s 标语、6.40s 三维交接与后续解锁时序，品牌内容不增加时长。

冻结开场来自 `music-boot.css`：mark 容器 left 50%、top 48%，宽 `clamp(155px,17vw,235px)`；原 SVG 坐标为 310×145。后续保留此容器和子槽比例，让新字标 contain 居中适配，subtitle 保留原 17px 间距；新字标不能以自己的长宽比收缩整个容器、改变标题/标语锚点。1280px 时宽为 217.6px，235px 上限及155px下限另检查。[放置预览](../media/branding/placement-1280x720.png) 为品牌子槽设计验收，不宣称已实现播放器页眉/开场；完整交互与容器差异验收由 R14/R20 执行。

## 导出、平台文件与验证

Windows 默认无可见窗口：`./tools/assets/export-branding.ps1 -Preset qt-release`；全套证据：`./tools/build/verify-r03b.ps1`。只有验证原生 expose/paint 时显式加 `-WindowPreview`，窗口仍置底且不接受焦点，遵守 [测试窗口契约](test-window-policy.md)。POSIX：`sh tools/assets/export-branding.sh qt-release`。公共构建每次发现本机最新原生 Kit；工具使用 C++23 / QtGui / QtSvg，不使用其他图形库。原生可执行命令为 `build/<configuration>/tools/export-branding <repository> [--window-preview]`，校验模式加 `--verify-only --report <output.json>`。

历史 `windows-window.png` 保留原验收版本；新窗口 proof 从自己的真实 expose/paint 缓冲采集，报告明确写入 captureMethod/windowPolicy，不读取被其他应用遮挡的桌面区域，不宣称是桌面截屏。未请求 WindowPreview 的重新导出只执行资源、hash、SVG/图像/容器检查，不宣称执行了原生窗口验证。

- SVG 母版/生产稿/中心线/映射：`resources/branding/`；Linux 同源 SVG、九档透明 PNG：`resources/icons/yalahoo/`。
- Windows ICO：16/24/32/48/64/128/256，32-bit PNG 块；目录中的 256 宽高编码为 0，planes=1，位深 32。
- macOS iconset：16/32/128/256/512 五个逻辑尺寸各含 @2x；ICNS 四个 PNG 块 ic07/08/09/10 为 128/256/512/1024。文件头及块长度均 big-endian，块长含 8 字节头，总长含文件头；每块 Qt 解码及与 SVG 像素比对通过。Windows 生成 ICNS，不宣称 Finder 或 iconutil 实机检查通过。
- Qt 预览：9 档尺寸 × DPR1/1.5/2 × 浅/深/透明背景 × 自动主/小稿、小稿、单色 = 243 个 PNG；反白单色用于深背景。另有 54 个字标/开场样本，覆盖 20/24/31px 字高与 155/217.6/235px 开场槽、DPR1/1.5/2；`logos/` 中保存真实物理尺寸。`qt-proof.png` 为字标、组合、实尺寸和揭示进度版面，`windows-window.png` 为实际 Windows Qt 窗口采集。所有 SVG 逐一 Qt 加载、透明/安全区检查；容器和清单内容逐项验证。渐变未见明显色带，细纹/字标无断裂，双主题字标可读，16px 两环与盒体保持分离。
- 证据在 `verification/windows/branding/`；本轮 macOS/Linux 仅审核共用源码、shell入口和衍生容器，结论为“静态审计通过，实机待用户验证”。

主界面接入归 R14/R20，系统资源及平台元数据接入归 R30–R32。这里的图标资产不改变现有 Qt 探针 UI，也不提前生成后续播放器功能。
