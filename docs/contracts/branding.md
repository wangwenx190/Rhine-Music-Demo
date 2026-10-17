# yalahOO!!! · 原创品牌契约

本文件固定 R03B 的矢量资产与后续接入边界。原创几何由本项目绘制，无旧品牌轮廓、位图描摹、字体字形或外部资源。作者及历史素材署名仍在 `legacy-web/LICENSE`、`legacy-web/NOTICE.md` 保留。来源和 SHA-256 在 `resources/manifests/branding.json`。

生产采用开口刻纹、标准字距的B稿；未选设计只作历史素材，不进入产品或设置。本文只管矢量/衍生资产和揭示路径；平台操作分别归docs/platforms中的对应文档，尚未执行的目标检查归迁移计划。

## 几何、颜色与安全区

- 图标 `viewBox="0 0 1024 1024"`，透明背景。盒体以 (512,512) 为轴旋转 −8°，主稿外框 (208,160,608,704)，纸面 (270,218,482,588)，两环中心 (406,520)/(634,520)，外环中心线半径 90、线宽 20；刻纹半径 74/60/46，开口 32°。琥珀唱针孔和极短纸面目录线只在主稿保留。
- 四个视觉层级为 `silhouette`、`paper`、`rings`、`highlights`；外层 `album-case` 统一变换。没有阴影滤镜、光晕、图像、脚本、SVG text 或远程引用。实际 alpha 边界全部落在中心 832×832 区域内，四边至少 96 单位；准确边界记录在导出报告。
- 暖昼来源为冻结 `music.css`：纸面 #efede7、背景 #e8e5e1、墨色 #232722、刻纹 #74786f、琥珀 #b56834。深夜使用同源 #08121f 背景、#e9eef2 墨色、#d9ad7d 琥珀。纸面高光 #fffaf0、玻璃 #c4c9c1→#8a978e→#42534e、纸面尾色 #dcd7c9 是同色相材质层次，不构成第三套界面主题。
- `icon.svg` 在两主题共用玻璃/纸面。`logo-light.svg` 和 `lockup-light.svg` 用于浅背景；dark 稿用于深背景。mono 稿显式使用 #232722；附加 mono-light 稿反白为 #e9eef2。单色图标的纸面和圆环孔是透明负空间，不用白色矩形假装挖空。
- 独立图标不放字标。完整字标 `viewBox="0 0 816 208"`；小写 yalah、大写 OO、三个感叹号均为原始路径。小写线宽 16，大写线宽 18；O 由连续三次贝塞尔闭环构成，细开口内纹呼应唱片。三个感叹号上端 34/30/26，点基线 130；同一倾向、逐步延长。y 下伸到 188（含圆端）；完整 alpha 边界不截断。
- 字标推荐资本高度 31px，紧凑 24px，最小 20px。以 104 单位资本高度换算完整画布尺寸：宽 `816*capHeight/104`、高 `208*capHeight/104`；不能把含下伸空间的 208 单位误当资本高度。相邻控件须继续按冻结源码的容器布局定位。

## 16–32px 光学稿

`icon-small.svg` 独立重绘。删除刻纹、孔点、目录线和窄高光，用一层玻璃描边、暖白内面及两道圆环。边框和圆环线宽 64，内孔直径 64；双环中心距约 263.88，实心外缘间距约 71.88；纸面内缘到环的水平间隙不小于 68 单位。16px/DPR1 的主要线条、孔径及间隙至少 1px。

为避免 1px 内孔落在四个像素的交点，小稿将 −8° 变换后的环心对齐 16px 栅格的 (6.5,8.5)/(10.5,7.5)。外框有光学加宽、收高、圆角及微移补偿，仍为同向专辑盒，完整处于 96 单位安全区。导出器对两孔与相邻环线的灰度差、双环间纸面像素做硬断言。

PNG、ICO 的 16/24/32 使用小稿；48 及以上使用主稿。iconset 按**实际像素尺寸**选择稿，例如 16@2x 使用 32px 小稿，32@2x 使用 64px 主稿。应用最小图标尺寸为 16px。

## 开场路径交接

`logo-master.svg`、各 logo 生产稿保留稳定 `word`、`rings`、`accents` 三组；路径 ID 对应 y/a/l/a/h、两个 O、三根感叹号和三个点，完整拼写由此固定。lockup 内采用 `logo-` 前缀避免与图标 rings 重名。

`logo-reveal.svg` 提供与生产字标相同的 17 条中心线路径（内圈装饰不参与中心线揭示）。`reveal-map.json` 记录每段 d、组、strokeWidth、Qt QPainterPath 测得的长度、offset 和总长度归一化 start/end。输入整体 `contourProgress` 后，段进度为 `clamp((progress-start)/(end-start),0,1)`，沿弧长绘制；不能直接把贝塞尔参数 t 当成弧长。零长点以 0.01 单位短线加圆端绘制，长度进度有限且确定。

OpeningInk按reveal-map的分组与弧长绘制；揭示完成时在同节点补齐完整生产稿的细内纹。时间包络、减少动态及序章交接由[opening.md](opening.md)维护，品牌内容不增加片长。

冻结开场来自 `music-boot.css`：mark 容器 left 50%、top 48%，宽 `clamp(155px,17vw,235px)`；原 SVG 坐标为 310×145。Opening.qml保留此容器和子槽比例，让新字标 contain 居中适配，subtitle 保留原 17px 间距；新字标不能以自己的长宽比收缩整个容器、改变标题/标语锚点。1280px 时宽为 217.6px，235px 上限及155px下限另检查；完整开场交互与容器验证入口见opening.md。

## 导出、平台文件与验证

Windows 默认无可见窗口：`./tools/assets/export-branding.ps1 -Preset qt-release`；组件验证：`ctest --preset qt-release -R "^branding$"`。只有验证原生 expose/paint 时显式加 `-WindowPreview`，窗口仍置底且不接受焦点，遵守 [测试窗口契约](test-window-policy.md)。POSIX：`sh tools/assets/export-branding.sh qt-release`。公共构建每次发现本机最新原生 Kit；工具使用 C++23 / QtGui / QtSvg，不使用其他图形库。原生可执行命令为 `build/<configuration>/tools/export-branding <repository> [--window-preview]`，校验模式加 `--verify-only --report <output.json>`。

- SVG 母版/生产稿/中心线/映射：`resources/branding/`；Linux 同源 SVG、九档透明 PNG：`resources/icons/yalahoo/`。
- Windows ICO：16/24/32/48/64/128/256，32-bit PNG 块；目录中的 256 宽高编码为 0，planes=1，位深 32。
- macOS iconset：16/32/128/256/512 五个逻辑尺寸各含 @2x；ICNS 四个 PNG 块 ic07/08/09/10 为 128/256/512/1024。文件头及块长度均 big-endian，块长含 8 字节头，总长含文件头；每块须能由Qt解码并与SVG核对；Finder/iconutil归macOS部署验证。
- Qt 预览：9 档尺寸 × DPR1/1.5/2 × 浅/深/透明背景 × 自动主/小稿、小稿、单色 = 243 个 PNG；反白单色用于深背景。另有 54 个字标/开场样本，覆盖 20/24/31px 字高与 155/217.6/235px 开场槽、DPR1/1.5/2；`logos/` 中保存真实物理尺寸。`qt-proof.png` 为字标、组合、实尺寸和揭示进度版面，`windows-window.png` 为实际 Windows Qt 窗口采集。所有 SVG 逐一 Qt 加载、透明/安全区检查；容器和清单内容逐项验证。要求渐变无明显色带，细纹/字标无断裂，双主题字标可读，16px两环与盒体分离。


跨平台资产校验保留清单的全部精确 bytes/SHA-256、SVG 母版/生产稿逐像素一致及小稿光学断言。衍生 PNG/ICO/ICNS/iconset 对当前 SVG 的比较只允许每个 RGB 通道一个 8-bit 量化单位，alpha 和尺寸仍须完全一致，任一通道超过 1 即失败；资源 hash 仍须完全一致。此规则只用于品牌衍生资产，不改变标题或三维图像门槛，不重新导出/覆盖入库资产。

Windows可执行资源使用原创ICO，Qt应用窗口图标从同源PNG尺寸选择；设置的开发与资源区使用48逻辑px SVG，按实际屏幕DPR解码。该新增品牌槽为项目显式映射，不能当作冻结CSS尺寸。
