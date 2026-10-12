# 渲染后端合同

R10 固定选择 **QQuickRhiItem + QRhi/qsb**。机器合同见 [render-backend.json](render-backend.json)。RHI 不透明基础、快照/资源所有权、阴影有效算法与跨后端 shader 接口见 [rhi-opaque.md](rhi-opaque.md)，真实实现位于 `src/render/rhi/`，Review GPU 入口为 `rhi-audit`。R10B–R10D 扩展并替换当前产品的唯一 Quick3D bootstrap；`SceneAdapter.renderBackendContract` 明确区分已选择的目的后端与正在运行的实现，当前 `implementationReady=false`。

决定依据是实际 Windows D3D11 对照与公开接口的控制边界：阴影投影/法向偏置/源滤波无法由当前 DirectionalLight 参数表达，Unshaded 光学算法不接收生成的 shadow lighting；跨三个壳体模型与独立抽取盒的全局稳定排序不由 `depthSortingEnabled` 保证。近远邻盒背景在 pre/post Bokeh 均未达到 SSIM .97，独立深度/法线及不透明输入对照用于分别定位几何和捕获差异；不能以色彩补偿掩盖它们。所有五 ROI 均计分，不隐藏留白、侧脊、背景等残差。

冻结 `music-model.ts` 指定 `FrontSide`、`transparent=false`。锁定 Three 的 `renderTransmissionPass` 捕获不透明物体、MSAA resolve、mip；只对 DoubleSide 加入额外背面。这些音乐玻璃与 Qt 屏幕采样玻璃都不在不透明捕获中。**不能把差异解释为基线要求递归捕获其他玻璃**。补救应核对 resolve、alpha 边界、深度写入和材质/实例排序，保持源屏幕空间近似。来源仍以冻结源码优先，旧限制表中的“排除其他玻璃”只是公开采样范围，不能单独作为失败原因。

可调参数沿用 R07 已完成的最多三遍确定性坐标搜索及其共享双主题系数；R08 灯光直接使用源单位，无 Qt 内置灯单位需要再次拟合。完整扫描在 `verification/windows/materials/calibration.json`，光学系数在 GlassMaterial。R10 保持这些系数、曝光、主题、几何、IOR、contain 与封面，不用全局调参修饰仅重叠时出现的结构残差。RHI 通道修复后按同一确定顺序重新审阅残差；具体输入、对象、门槛分配给 R10A–D，不能跳过它们。

`tools/review/ImageComparison` 是共用 Qt/C++ 比较器，MaterialReview 和 `compare-images` CLI 共用实现：RGB MAE、有符号差、11×11 Gaussian σ1.5 SSIM、绝对差分，以及对称 Sobel 轮廓距离。SSIM 窗口必须完整在 ROI 内，空/错尺寸输入无效，窄 ROI 的 SSIM 为 null。轮廓阈值 48/255，使用精确欧氏最近距离；等权损失是五 ROI 的 MAE 平均与轮廓距离/255 平均。该图像边缘指标用于诊断；硬投影门槛独立按源顶点验收。

CLI：`build/release/tools/compare-images <actual.png> <source.png> <polygon.json> <metrics.json> <diff.png>`，polygon 为物理像素顶点数组。运行时 Qt 库从同一最新 Kit 解析。`ImageComparisonTest` 覆盖相等/色偏、错尺寸/空区域、窄窗口和移动边缘。

Windows 重建入口：`./tools/build/verify-r10.ps1`（只用 Release）。同 hash 冻结图可加 `-SkipWebCapture`。Web wrapper 从 R09B harness 复用实际冻结模块、锁定依赖、隔离 profile 与阻止外部 HTTP；生成脚本只在忽略的 build 中，不修改 Web 基线或前轮证据。新 fixture 为 70 状态，包括三种封面、双主题、前/低视角、合成抽取中间位置、精确旧输入、1024/2048/4096 阴影、独立法线/深度、不透明输入、9 实例与图集更新。合成中间位置只用于渲染，不冒充 R18 时间轨迹 golden。

图集实验使用公开 TextureData 和共享实例/材质，实际 GPU 抓图验证更新、稳定及还原；没有 mip 跨格污染。一次 CPU 提交为整张 16MiB，改动 tile 为256KiB。**实际 GPU 传输字节未测定**，报告为 null；CPU 提交量与 setter 次数都不作为 GPU 预算证据。后端选择已有独立阴影/排序/resolve 依据，上传是否超预算不作为本轮的选择前提；R10C 必须实测新上传路径，不能继承“预算通过”。

公开接口依据：[CustomMaterial](https://doc.qt.io/qt-6/qml-qtquick3d-custommaterial.html)、[Instancing](https://doc.qt.io/qt-6/qquick3dinstancing.html)、[TextureData](https://doc.qt.io/qt-6/qquick3dtexturedata.html)。实例变体必须使用 INSTANCE_MODEL_MATRIX / INSTANCE_MODELVIEWPROJECTION_MATRIX；本轮仅 Review 变体接入，不改前轮生产 shader。

Windows 证据在 `verification/windows/render-backend/`，成对图/差分/局部放大在 `docs/media/render-backend/`。macOS/Linux 新增调用链矩阵在对应静态目录 `render-backend/`，结论为“静态审计通过，实机待用户验证”。这不代表任何 RHI 实现、平台 GPU 性能或目标系统运行已通过。

诊断材质在 Web 阴影开关后显式失效 program 缓存；Qt 邻盒的 Review layer/showPaper 与选中盒和源隔离输入一致。仅纸面输入仍保留邻盒印刷 ROI 残差，由 R10B 核对光场/采样/capture，不能误记为已经通过。
