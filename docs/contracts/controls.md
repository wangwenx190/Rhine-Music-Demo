# 项目自绘交互控件

本契约维护产品、探针和Review共用控件的视觉、输入与验证边界。冻结来源经历史映射解析，逐项判定见[source-fidelity-review.md](source-fidelity-review.md)；字体/主题常量由DesignTokens与assets/materials契约维护，不在这里复制色值。

## 长期强制规则

禁止直接使用QtQuick.Controls（QQC）提供的预制控件及其样式、主题、默认视觉委托。所有QML控件必须完全自绘；QtQuick.Templates仅可提供行为基础，必须显式提供完整视觉树。以冻结原项目生产源码的理论渲染值为准，显式设置颜色、尺寸、字体、内外边距、边框、圆角及各交互状态；禁止依赖Qt自动设置、主题/调色板注入或默认隐式尺寸决定外观。没有统一来源数值的原生内部装饰须明确记录项目映射，不能冒称来源值。本规则适用于全部产品、研究、探针、Review及后续新增QML，后续session必须遵守。

## 绘制与行为

`ArchiveButton/Slider/CheckBox/ComboBox/ScrollBar/ScrollView/TextField/TextArea/ProgressBar/ToolTip/EditMenu`使用`QtQuick.Templates`的公开行为类型。背景、内容、指示器、滑块、委托、弹出层及滚动条全部由项目QML绘制；独立页面按钮/窗口/模态层同样显式提供视觉树。颜色直接绑定DesignTokens，字体显式绑定MiSans的family/styleName/weight与逻辑字号，尺寸、内边距、边框、圆角和状态透明度由项目设置。系统主题不能决定控件绘制。

Templates负责Qt事件、键盘、焦点、无障碍、文本编辑/输入法和滚动行为。弹出层固定为Item，显式关闭默认过渡，关闭/Escape与外部点击策略由组件设置；选择项仅打开时实例化并由ListView虚拟化。文本编辑菜单使用项目按钮提供剪切/复制/粘贴/全选，操作能力遵循选区、剪贴板与readOnly。目录选择使用`QtQuick.Dialogs.FolderDialog`的操作系统对话框，其系统外观不属于产品控件绘制。

ArchiveButton的键盘焦点沿冻结`music.css::.music-app button:focus-visible`：2px accent外轮廓、4px间距，对应独立矩形在控件外扩6px；绑定Templates的visualFocus，不能用覆盖文字边缘的内边框或鼠标焦点代替。轮廓不参与布局与命中尺寸。

## 来源尺寸

以下均为逻辑px，取冻结生产最终CSS及匿名DOM计算框；不依赖截图或特定Qt皮肤。

| 来源 | 当前映射 |
| --- | --- |
| `music.css::.settings-row input[type=checkbox]` | 指示器16×16；浏览器上下3px外边距映射为22高的行内命中框 |
| `.settings-row input[type=range]`与原生range框 | 常规150×16，16×16手柄；行内上下2px间距由SettingRange容器提供 |
| `music-lighting-lab.css::.lighting-lab-sliders` | 标签上距5，字号10/行高15；滑杆满宽×20、外边距0，每组总高40，disabled透明度.3 |
| `music.css::.settings-row select` | 高28、最小宽90、字号11、垂直内距5/水平10、边框1；长选项自然宽度由OptionMetrics在数据/字体变化时测量并计入箭头槽 |
| `.music-panel textarea/input[type=search/text/url]` | 字号12、行高20.4、垂直内距12/水平13、边框1、圆角0；单行总高46.4，三行textarea最小87.2 |
| `music-lighting-lab.css`模式与操作按钮 | 内距6/10、边框1、圆角2；模式字号11/行高16.5/总高30.5，操作字号10/行高15/总高29；按住对照背景accent×.08，选中模式背景accent×.11 |
| `music.css`专辑介绍/制作信息progress | 专辑介绍3高，制作信息16高；进度直接映射到项目accent填充 |

textarea用`TextAreaFormat`在公开QTextDocument上设置固定20.4段落行高，插入时只处理受影响段落；格式变更合并到原编辑事务，保留撤销/重做。尺寸不能以Qt字体默认行距代替CSS行高。

浏览器原生内部装饰没有跨平台统一矢量：项目显式采用2px轨道、圆形16px手柄、1px框及主题accent，保留来源控制框与状态语义。弹出列表每行至少30px/最多264px、编辑菜单宽140px、tooltip最大320px/边距8px、滚动条8px槽/4px笔画/最小20px手柄，是原生交互的明确项目布局值；均不作为冻结CSS声明。面板尺寸/响应式边界仍由各面板所属契约维护。

## 验证入口

Release `controls-qml`（`ui-tests --controls 全新目录`）分开核对双主题自身输出、来源控制框/段落行高、checkbox空格、range方向键、combo打开/提交、选区/Unicode输入法、菜单方向键/Enter/Escape、只读操作能力与textarea撤销/重做/新增段落行高。`settings-qml/experiments-qml/library-panels-qml/playback-qml`核对实际宿主滚动、输入隔离、关闭及资源；浏览/标题/序章对应入口按test-gates.md选择。正式官方qmllint、格式、模块加载与默认隐藏策略分别执行。

Windows/Linux目标编译、物理输入/无障碍/原生输入法与目录对话框、跨屏/原生窗口仍按计划UI-CONTROLS-P分工；自身RenderControl图像不能证明这些事实。
