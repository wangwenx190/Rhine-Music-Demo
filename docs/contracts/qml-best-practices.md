# Qt Quick和QML最佳实践

适用于产品、Review、组件和探针；在线官方资料核对日期2026-10-17。Qt的[QML和Qt Quick最佳实践](https://doc.qt.io/qt-6/qtquick-bestpractices.html)建议分离UI与业务，大型动态数据适合C++模型；[QML JavaScript表达式](https://doc.qt.io/qt-6/qtqml-javascript-expressions.html)仍允许JavaScript。下面的职责边界是用户要求在本项目执行的更严格规则，不能描述为Qt普遍禁止JavaScript。

编号是项目检查项。Qt文档中的预制Qt Quick Controls示例不能直接照搬：本项目自绘控件要求以[controls.md](controls.md)为唯一权威。具体性能数值、动画时钟和窗口策略仍由相应专项维护。

## QML与C++职责

- **QML-01 视觉表达留在QML。** 声明式视觉树、布局、简单属性绑定、适合QML的图形、动画、转场、焦点和视图操作由QML负责。不能为了消灭所有函数而把视觉对象遍历和几何操作搬到C++，再靠objectName反向查找。
- **QML-02 复杂工作归C++。** 集合搜索/分组/归一化、元数据解析、业务状态机、持久化、I/O、并发和性能敏感算法由C++负责。写在属性绑定中不降低其复杂度；先计算全库再交ListView同样违反边界。[C++集成选择](https://doc.qt.io/qt-6/qtqml-cppintegration-overview.html)
- **QML-03 按工作性质划线。** 不以行数、是否含if/for或“可用JS实现”判断。少量固定控件的焦点/几何遍历可留QML；随库规模增长的循环应检查模型/控制器职责。自绘是否需要C++按渲染API和实际成本判断。
- **QML-04 业务只有一个状态所有者。** QML绑定C++结果并转发用户意图，不复制播放/查询状态机、不合并持久配置、不解析原始Provider payload。界面展开、hover和临时编辑状态可以由视觉组件持有。
- **QML-05 接口保持领域含义。** C++提供模型、typed属性与命名命令；QML实现具体视觉对象。View的forceLayout/itemAtIndex、delegate真实尺寸、polish和焦点恢复属于视图协议，但目标ID、请求代次及计算轨迹由控制器维护。

## 模块、类型和组件接口

- **QML-06 模块可被工具理解。** 使用qt_add_qml_module与QML_ELEMENT/QML_FOREIGN等注册；QML文件进入QML_FILES，资源别名/隐式导入一致。不要只在运行时注入匿名全局名，或绕开模块导入缓存。[QML模块CMake](https://doc.qt.io/qt-6/qt-add-qml-module.html)
- **QML-07 必需依赖显式注入。** 用typed required property及初始属性注入控制器/模型；避免context property。动态创建同样补齐required属性，依赖应在绑定首次求值前可用。[context property限制](https://doc.qt.io/qt-6/qtqml-cppintegration-contextproperties.html)
- **QML-08 类型选择有含义。** 静态类型优先；动态异构JSON、模型记录和ShaderEffect sampler可合理用var，不为类型比例制造无用包装。组件信号/函数参数和返回值在可行时带类型。[QML类型系统](https://doc.qt.io/qt-6/qtqml-typesystem-topic.html)
- **QML-09 作用域可追踪。** 用id限定跨对象属性；适用组件使用`pragma ComponentBehavior: Bound`。delegate用required属性接收role，避免偶然context和未限定名称；不得让相同名称的局部属性遮蔽真实数据。[作用域](https://doc.qt.io/qt-6/qtqml-documents-scope.html)
- **QML-10 接口只暴露需要的可写性。** 派生值readonly，别名指向真实对象/属性；不要用可写复制值冒充原控件。default property、alias和对象列表的所有权要明确，组件外部不依赖内部children顺序。[对象属性语法](https://doc.qt.io/qt-6/qtqml-syntax-objectattributes.html)

## 绑定、事件和异步创建

- **QML-11 持续依赖用绑定。** 不用onCompleted赋值替代静态绑定；命令赋值会移除既有绑定，需要持续更新时保留或显式恢复绑定。尤其核对主题、字体、窗口尺寸和model替换。[属性绑定](https://doc.qt.io/qt-6/qtqml-syntax-propertybinding.html)
- **QML-12 绑定不承担副作用。** getter/绑定不改业务状态，不发起I/O，不依赖多次求值顺序；一个依赖变化不应重建大对象图。复杂计算移交C++，常量提前声明，不通过删除对象属性改变JS形状。[性能建议](https://doc.qt.io/qt-6/qtquick-performance.html)
- **QML-13 区分交互与属性变化。** 控件值也会因绑定、钳制或模型更新变化；业务命令绑定真实用户交互信号，避免双向反馈和意外保存。简单onClicked转发是适当微逻辑。[Qt Quick最佳实践](https://doc.qt.io/qt-6/qtquick-bestpractices.html)
- **QML-14 不依赖完成顺序。** 不假定兄弟/父子Component.onCompleted顺序；动态Loader使用status/onLoaded和显式就绪状态，检查Error。异步创建、callLater和timer回调核对对象、可见状态及revision。[Loader](https://doc.qt.io/qt-6/qml-qtquick-loader.html)
- **QML-15 var修改有通知语义。** 普通JS对象字段原地改变不等于整个QML属性通知；Qt引用序列逐元素写可能反复读取/复制/写回。业务集合在C++批量更新并通知，不靠重复自赋值强迫刷新。[序列性能](https://doc.qt.io/qt-6/qtquick-performance.html)
- **QML-16 动态对象有销毁协议。** 优先声明式Component/Loader；动态创建需要稳定creation context与owner，销毁后不留回调引用。Loader释放只销毁视觉状态，不应停止持续播放或丢失业务状态。[动态创建](https://doc.qt.io/qt-6/qtqml-javascript-dynamicobjectcreation.html)

## 列表、布局和图形成本

- **QML-17 大集合虚拟化。** 无界动态集合使用C++模型与ListView等视图，轻量delegate；Repeater只用于已证明有界的小集合。不要用嵌套Repeater一次实例化全库，或通过cacheBuffer无界保活。[ListView](https://doc.qt.io/qt-6/qml-qtquick-listview.html)
- **QML-18 delegate不是持久状态容器。** delegate可销毁/复用；选择、播放和业务状态按稳定ID存模型。复用时重置hover、局部动画和瞬态输入；异步动作不得只记旧index。[delegate状态](https://doc.qt.io/qt-6/qtquick-bestpractices.html)
- **QML-19 几何只有清晰约束。** Layout管理的子项不要同时用冲突anchors/width/height约束；implicit尺寸不依赖最终尺寸形成循环。检查空模型、窄窗、长文本、字体变化与DPR，不只默认尺寸。[Qt Quick Layouts](https://doc.qt.io/qt-6/qtquicklayouts-overview.html)
- **QML-20 布局操作不变成轮询。** 仅在必要视图交接点forceLayout/polish；不要逐帧JS重排children或反复强制布局直到“看起来稳定”。真实delegate几何与计算模型差异要有明确反馈协议。
- **QML-21 图像按展示需求加载。** sourceSize限制解码尺寸，适当asynchronous，核对status和错误占位。宽高/裁切与源比例清晰；图片缓存、解码和封面资源策略见[covers.md](covers.md)。[Image](https://doc.qt.io/qt-6/qml-qtquick-image.html)
- **QML-22 文本格式显式。** 普通标题/元数据优先PlainText，确需富文本才启用相应格式；外部内容不靠自动HTML探测。核对wrap/elide、字体回退、grapheme、无障碍全名与实际裁切。[Text](https://doc.qt.io/qt-6/qml-qtquick-text.html)
- **QML-23 图层有真实用途。** layer/ShaderEffectSource意味着离屏纹理、额外pass和内存；不用它们盲目“优化”。检查采样源范围、递归依赖、live状态、隐藏和resize生命周期，具体资源图见[rhi-integration.md](rhi-integration.md)。[Item图层](https://doc.qt.io/qt-6/qml-qtquick-item.html)
- **QML-24 动画服从生命周期。** 声明式视觉动画适合QML；性能敏感运动、状态轨迹和唯一时钟按[motion.md](motion.md)。隐藏/最小化/关闭停止无效视觉工作，减少动态与改速不破坏终态；opacity为零不自动等同禁用输入或销毁对象。

## 输入、文案和验证

- **QML-25 输入交给合适Qt基础。** 指针抓取、滚轮、键盘、焦点scope、文本编辑和输入法按控件协议处理；不为外观重复实现完整编辑器。事件accept/propagation有意图，覆盖弹层关闭及焦点恢复。基于Templates的选项控件优先直接绑定currentValue，由Qt处理valueRole查找，不自行扫描选项；不可同时声明式绑定currentIndex和currentValue。[选项值协议](https://doc.qt.io/qt-6/qml-qtquick-controls-combobox.html#currentValue-prop)、[焦点管理](https://doc.qt.io/qt-6/qtquick-input-focus.html)
- **QML-26 无障碍与实际行为一致。** 按控件角色提供名称、状态和动作；不可见项不参与输入，图标操作有可理解名称。截断文本保留完整Accessible名称，工具提示观察真实自绘对象。控件细则见[controls.md](controls.md)。[Qt Quick无障碍](https://doc.qt.io/qt-6/qtquick-accessibility.html)
- **QML-27 可见字符串可翻译。** 使用ASCII英文`qsTr`源句与占位符，其他语言在TS资源；emoji写Unicode转义。列表label翻译而value/ID不翻译，歌曲元数据保留数据原文。数字/时间展示按语义选择locale，协议序列化不随界面语言变化。构建与加载唯一见[Qt翻译规则](qt-best-practices.md#翻译和构建闸门)。[QML国际化](https://doc.qt.io/qt-6/qtquick-internationalization.html)
- **QML-28 文本输入不逐键执行重工作。** 编辑状态保留Qt输入法语义；搜索等业务按控制器背压/取消执行，不能在onTextChanged同步扫描/排序/解析全库。防抖不等于无限排队工作已被取消。
- **QML-29 官方工具与运行诊断并用。** 类型/导入变化执行CMake官方qmllint target；运行时检查undefined、binding loop、required属性、创建/销毁和警告。qmllint通过不能证明复杂逻辑适合QML，或目标GPU效果正确。[qmllint](https://doc.qt.io/qt-6/qtqml-tooling-qmllint.html)
- **QML-30 有具体成本问题时定向profile。** 按performance.md先静态审查；仅未决项需要时检查绑定求值、delegate创建、JavaScript、动画与SG成本；复现场景、数据规模和窗口状态保持明确。不能凭“C++更快”迁移本应QML实现的视觉，也不能用平均帧率掩盖卡顿。[QML Profiler](https://doc.qt.io/qtcreator/creator-qml-performance-monitor.html)

## 本项目职责判定示例

| 组件 | QML职责 | C++职责 |
| --- | --- | --- |
| SearchPanel / MetadataSection / SettingsPanel | 字体宽度绑定、delegate、字段展示、命令转发 | GenreRowsModel分行/测量缓存、MetadataPresentation解析、SettingsController配置合并 |
| TrackList / Detail | ListView真实几何、polish/定位和焦点恢复 | 稳定目标、定位状态、几何修正、滚动/高亮轨迹与展示状态机 |
| 自绘ComboBox / 编辑菜单 / Ruler | 自然宽度绑定、焦点遍历和视觉组合 | OptionMetrics选项测量、Qt Templates输入基础、滑尺/字轮状态及原生绘制 |
| TitleReel / Opening / GeometryScene | 图层尺寸、声明式效果组合、视觉树 | 字体布局、字形、生产绘制、资源和运动计算 |
| MusicStage / PlaybackControls | 视觉启停、命令转发、窗口/焦点和简单时间格式 | 媒体、输入归属、业务模型与生命周期 |

大型组合按可独立说明的视觉职责拆分，不以减少行数新增隐式上下文：DetailHeader组合DetailAlbumInfo、DetailIntroduction，DetailTabs只处理吸附页签；TrackList组合虚拟视图和滚动恢复，TrackListReveal独占已实现项的polish、实际几何读取及带revision的定位交接；TrackRow/CreditCard分别绘制曲目/制作人员；曲目指针按下保存专辑和曲目双身份，释放时复用到其他身份则丢弃操作。SearchFilters与SearchResult分别承担查询输入和单条结果；SettingsPanel组合外观、阵列/画质、声音和联网配置；PlaybackPanel复用同一SoundSettings提供三路声音与设备设置。LibraryPanels只绘制模态外壳、背景和阴影，PanelContents负责内容组合、准备与焦点交接。Opening组合输入/跳过，OpeningFilm只绘制片头图层；LightingModes只显示固定模式与说明。MusicStage组合场景，StageFrameDriver、StageFocus、StagePanels分别承担帧交付、焦点恢复和弹层宿主；HeaderTransport、ColumnNavigation和OverviewLabel使用显式输入绘制各自区域。

制作人员的角色/来源/未分配署名和原始流派展示字符串由DetailModel按专辑准备一次，不在可复用委托中反复连接集合，也不同时发布无消费者的平铺制作人员副本。面板退出后的稳定专辑/曲目意图归PanelController，QML只转发结果，具体协议见[library-data.md](library-data.md#音乐库面板搜索与歌曲定位)。LibraryPanels的背景复用BackdropBlur/BackdropBlurPass，不复制捕获与两pass接线。Ruler和编辑菜单的固定小集合焦点遍历、TrackListReveal对已实现视觉项的polish以及TitleReel验证用实际图层尺寸读取仍是QML视图协议；不反向遍历全库或移交C++查找视觉对象。

这些是边界示例，不是以文件名替代逐函数审查的豁免表。格式入口见[code-style.md](code-style.md)，预算见[performance.md](performance.md)，检查选择见[test-gates.md](test-gates.md)，窗口见[test-window-policy.md](test-window-policy.md)；源码职责、数学/状态、真实视觉质量与实机事实分别证明。
