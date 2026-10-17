# 显示输出与默认绘制质量

本文维护HDR/SDR选择、平台颜色语义、运行时检测和默认二维绘制质量。后端选择归[render-backend.md](render-backend.md)，三维MSAA/后处理归[postprocess.md](postprocess.md)，数据纹理与封面图集采样归各自专项。

## 输出选择与颜色

`DisplayMonitor → DisplayOutput → App.DisplaySurface/MusicStage → SceneItem`是唯一输出状态链。平台查询在GUI线程，render只消费同步后的值；swapchain实际格式在`beforeSynchronizing`读取并按epoch排回GUI。在实际格式确认前暂不显示内容；Qt拒绝首选HDR10时先尝试协议允许的线性HDR，两者不可用时重建SDR表面，当前输出不反复重试，屏幕或系统模式变化后重新判断。显式OpenGL维持SDR，三平台均保留此后端。

| 平台 | 系统状态与输出 |
| --- | --- |
| Windows / D3D11 | 通过目标HMONITOR的DXGI_OUTPUT_DESC1确认当前为PQ/Rec.2020 HDR，QueryDisplayConfig读取对应活动路径的DISPLAYCONFIG_SDR_WHITE_LEVEL；未知/失败回退。窗口为RGBA16F scRGB，UI线性值乘`SDR白电平nits/80`，高光上限为`max(1,屏幕峰值/SDR白电平)`。 |
| macOS / Metal | NSScreen的potential EDR大于1时使用RGBA16F extended-linear sRGB EDR。macOS自动管理EDR，1.0代表当前SDR白；读取current EDR作为高光上限。current=1时保留EDR能力且限制到SDR范围，避免亮度变化导致反复重建。 |
| Linux / Vulkan | Wayland `color-management-v1`的当前输出描述确认HDR传递函数与高于reference white的target luminance；不使用EDID推断系统开关。优先采用compositor可描述的PQ/Rec.2020 HDR10，否则采用extended-linear sRGB；仍须通过实际swapchain能力。X11、无协议、未知亮度或无可用编码时SDR。协议将输入reference white锚定到输出reference white；线性输入使用协议默认80，PQ输入使用默认203 nits，不再次乘输出reference white。 |

依据：[Microsoft Advanced Color/scRGB与SDR白电平](https://learn.microsoft.com/en-us/windows/win32/direct3darticles/high-dynamic-range)、[Apple EDR支持与当前headroom](https://developer.apple.com/documentation/metal/determining-support-for-edr-values)、[Wayland颜色管理协议](https://gitlab.freedesktop.org/wayland/wayland-protocols/-/blob/main/staging/color-management/color-management-v1.xml)、[Qt swapchain格式](https://doc.qt.io/qt-6/qrhiswapchain.html)。Qt的HDR请求属性`_qt_sg_hdr_format`仅留在RHI适配边界；同Kit编译探针核对API，真实窗口核对实际格式。产品清除外部`QSG_RHI_HDR`强制值，保证环境不能绕过自动判断。

HDR内容先在RGBA16F的DisplaySurface中保留源sRGB编码UI合成，再通过一次sRGB解码、白电平调整和平台编码输出；HDR10另执行D65 Rec.709→Rec.2020与ST2084。三维保留源ACES的SDR基底，仅对曝光后超过1的线性高光追加有界指数肩部，范围不超过当前headroom；32-byte最终输出uniform包含原16-byte参数与HDR参数。三维item输出和背景模糊捕获在HDR时使用RGBA16F，避免中间RGBA8裁切。SDR保持源ACES/sRGB输出。

模式变化只重建原生surface/SG及依赖的GPU资源，QML对象、业务状态和音频对象不替换；白电平/headroom变化只更新值。重建保留窗口visibility，Qt释放旧render资源后再销毁surface。DisplayOutput须比QML引擎/窗口活得更久，旧epoch回调丢弃。`ResourcePlan`计入HDR item输出、整窗浮点合成层及深度保守上界，已接线的面板/跳过按钮模糊捕获计完整mip；窗口swapchain/compositor/driver成本仍为未知。

## 通知与查询

- Windows使用WM_DISPLAYCHANGE/WM_SETTINGCHANGE/WM_DEVICECHANGE，合并为一个排队刷新，避免在Win32 dispatch内销毁窗口；SDR slider无可靠专用Win32通知时由定时查询覆盖。
- macOS观察NSApplicationDidChangeScreenParametersNotification；当前EDR随亮度/电源变化，由查询刷新，不使用Qt可能缓存的hdrInfo代替。
- Wayland观察registry/output image_description_changed，异步取得完整描述后发布状态；不做roundtrip/同步dispatch，不创建第二个color-management surface。Qt拥有surface的颜色声明及事件分发。
- 跨屏、应用状态/恢复触发刷新；可见期间用1000ms coarse timer查询兜底，隐藏停止定时器。无状态变化不通知、不重建、不请求渲染。

## 默认质量

全平台在QML创建前设置QQuickWindow::CurveTextRendering；全局字体及四个嵌入字体面使用PreferAntialias | PreferQuality与PreferNoHinting。自绘文本/编辑器显式关闭hinting，普通文字与字轮保持curve路径；彩色字形继续Qt fallback。QML Rectangle/Shape/Text显式启用可用AA，Shape保持CurveRenderer；CanvasPainter保留几何AA，生产三维保持4×MSAA。二维curve/几何AA已覆盖轮廓，不额外为整窗及模糊层叠加MSAA；它会增加整层存储/resolve带宽，缺少明确额外收益。三维预设的SMAA选择保持原接口。

QML Image与ShaderEffectSource默认smooth/mipmap均为true。生产SVG使用QtQuick.VectorImage的CurveRenderer与PreserveAspectFit；移除源SVG的DPR栅格尺寸绑定，仍保留原viewBox、布局与路径。深度、法线、SMAA lookup等数据纹理及带tile隔离的封面图集继续执行专项采样语义，不把图像显示默认值施加给数据通道或制造跨tile串色。

## 定向检查

Release `display-transfer-gpu`仅用5×1浮点输入核对scRGB、白电平与HDR10转换；`display-output-visible`使用480×160窗口检查实际系统检测/输出格式、参数更新不失效、一次SDR↔HDR重建或不支持时的SDR回退，以及curve文字/VectorImage加载无warning。前者真正离屏，后者按窗口契约显式授权运行。二者不加载音乐库/大型阵列，不构成完整产品质量或性能结论。目标缺环境事项只在迁移计划HDR-OUTPUT-P维护。

`python3 tools/audit/audit-display-output.py`绑定COMMON-01–05/PERF-01–07及平台调用链符号/hash，并检查同Kit的最终输出qsb跨目标变体；仅生成本地源码审计矩阵，不替代原生目标运行。
