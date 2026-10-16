# 浏览静态界面与合成

本文负责Header/BrowseInfo/Footer/ReadingVeil的布局、静态文字与合成；滑尺/输入归[navigation.md](navigation.md)，标题/歌名运动归[title-reels.md](title-reels.md)，绘制API归[canvas-painter.md](canvas-painter.md)。

## 源码与接口

冻结来源为 `57f4f845c20cce646a15dbf4008c2a162aaaf4e2` 的 `src/music-app.ts`、`music.css`、`music-theme.css`、`music-theme-switch.css`、`music-transport-title.css`、`music-title.ts` 及 `viewport-layout.ts`。读取经 `frozen-git.mjs` 映射清理后的 Git 对象；Web 参照由冻结生产 markup/CSS 和实际 `setupMusicTitleLayout` 静态调用生成，不访问用户曲库。本阶段明确保留窄屏两个主题文字，因此参照对横屏 ≤1100px 的 theme span 施加这一项显式差异；其他样式保持冻结定义，品牌按branding契约替换。

- `LayoutMetrics` 是二维和相机共用的逻辑尺寸源，1.05 分界、信息区 x/right 不复制到第二个控制器。
- `LibrarySession::browseInfo` 按稳定导航 ID 查询已有专辑索引和新建的类型名索引，交付标题/艺术家/类型/年份/曲数。快照替换即使 ID 不变也通知；搜索、排序和浏览不发出播放命令。类型索引仅在快照替换构建；全局/列内序号索引在快照或排序列变化时一次构建，读取不扫描集合。
- `ArchiveText` 按同源字体度量提供 CSS 行框与上下留白：normal 行框使用分别取整的 ascent/descent，小数字号以相邻整数的未 hint 矢量文字加局部比例变换保持精确尺寸/字距，不扩大字形缓存，也不增加 layer/FBO。眉题的 1.7 行高、两个 span 的4px边距和空格 advance 分别建模；打开按钮按实际 CSS 级联继承16px，并显式清除 Basic 默认内边距；`NavigationFrame` 保留父级 16px 的 inline strut，并按 ascent/descent 和 inline span 的半行距计算字行盒。这解释浏览信息底边，不能用一张截图反推常量。导航输入/刻线/数字序号由 R15 的 InputRouter/RulerController/OrdinalReel 接管；R16 接管完整文字测量缓存及标题字轮。
- 当前歌名为 `Header.playingTitle`，宽度上限按冻结竖屏优先320px及横屏760/610/340px规则；播放/停止按钮始终锚定最右侧。主题请求和其他按钮通过 `themeRequested` / `actionRequested` 交付，不修改音乐队列。减少动态与速度从已有 `MotionController` / `FrameClock` 注入。
- 七种颜色和 nightMix 的过渡保持 650ms、源 cubic-bezier(.4,0,.2,1)；选择 pill 保持 380ms、(.22,1,.36,1)。夜间侧向遮罩横屏为 32/58/78%，竖屏在 650ms 内消退；阅读层位于页眉之前，不覆盖顶栏。

## 静态层与图标

- 合成次序：SceneItem → 横/竖阅读渐变 → 暖昼/夜间竖向渐变 → 夜间侧向渐变 → 竖屏信息局部渐变 → 页眉/浏览文字/计数/页脚。稳定透明层visible=false；普通渐变直接进入Qt Quick主场景，不创建整屏ShaderEffectSource。
- 主题只改颜色/透明度，不重建三维资源。Flickable矩形裁切，文字/SVG内容不变时复用节点。
- 功能图标保留源SVG几何。settings的横线/圆点以同几何Quick Shape分层；其他图标由源SVG按所属窗口实际DPR栅格化，无额外倍采样/mips，仅内容/主题/DPR变化失效。17px单图RGBA8约1156bytes×DPR²为描述符估算，Qt图集/对齐另计。
- 静态文字用同源字体、CurveRendering和局部矢量比例表达分数字号；不能由截图反推布局常量。静态盒模型判定见source-fidelity-review.md的R19E；完整字体/标题采样仍按R19F复审。

组件入口：`yalahoo --verify-browse 输出目录`；源码导出`node tools/review/export-browse-ui.mjs`。`ui-audit 输出目录`检查九尺寸/双主题、长歌名下按钮固定、最终song-cap及自身Qt键盘动作；用`python3 tools/review/compare-browse-structure.py 原生report.json 冻结report.json 输出.json`检查同尺寸/主题/DPR的CSS锚点≤1逻辑px，不读取图像。字体身份/内容与实际质量单列。
