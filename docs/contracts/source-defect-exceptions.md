# 原项目缺陷与复刻例外

处理原则以[迁移计划](../../QT_QUICK_MIGRATION_PLAN.md#优先级与源码基准)为准。本文仅记录冻结原项目自身的问题；原生迁移遗漏、API适配和迁移实现bug归[source-fidelity-review.md](source-fidelity-review.md)及组件契约，不混为原项目缺陷。

每项须列出冻结来源/实际生产符号、可复现触发条件、为何属于缺陷或不合理行为、可确定的正常预期及依据、实现或跳过决定、受影响边界。无法推断预期时明确记录跳过的特性，不用猜测值或截图补偿代替。

## SDE-01 · 详情返回按钮字号被通用reset覆盖

- 来源：冻结`music.css::.music-app button { font: inherit }`与`.music-back`及其≥1700/≤1100/≤700宽度规则；`music-theme-switch.css`提供最终竖屏left22/top118。
- 触发：全部尺寸的详情返回按钮。通用reset选择器优先级高于单个`.music-back`类，使明确声明的10px、宽屏12px和窄屏9px字号失效，最终继承页面16px。位置规则仍正常应用。
- 判定与预期：源码为该按钮明确设计了三档字号，并随宽度调整left/top/gap；固定继承页面字号会使这些专属声明全部失效，与该响应式定义冲突。采用显式三档字号，保留最终位置与状态规则，不复制reset覆盖缺陷。
- 实现与边界：`Detail::detailBack`显式设置上述字号。独立结构导出保留原computed值`resetFontSize`，从冻结CSSOM实际适用的`.music-back`声明解析预期`fontSize`并标记SDE-01；不改冻结文件、不影响其他控件或文字。比较仍核对位置与预期字号≤1逻辑px；不同文案与原生键盘命中框不作宽高等同断言。

## SDE-02 · 小屏导航最小内容宽度溢出

- 来源：冻结`music.css::.music-navigation`的74px首列、12px间距与两个`1fr`列；`.album-stepper`的固定滑尺/箭头/间距，`.genre-stepper`的最小标签宽度；`music-navigation-ruler.css`与music-ruler.ts提供窄屏滑尺宽度。
- 触发：小屏或过长列名使计数器、滑尺组、列选择组的最小内容宽度之和超过导航容器。CSS Grid的`1fr`自动最小值会将列选择组推出视口。
- 判定与预期：所有专辑/列导航均应可见且独立命中。保留原字号、滑尺尺寸、组内间距与底部锚点；空间不足时用既有grid gap分成两行，计数器/滑尺在首行、列选择居中在次行。超长列名只在可用标签宽度内省略显示，完整名称保留无障碍说明。
- 实现与边界：`NavigationFrame.wrapped`按实际组的自然宽度判断，容器高度随第二行增长，既有BrowseInfo.calloutBottom联动；能够容纳的布局保持原单行算法。整场窗口内几何断言要求两组完整位于viewport且不相交，正常原生输入路由与滑尺数学不变。不以截图相似度决定换行或缩小源控件。
