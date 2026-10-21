# RHI不透明几何、阴影与地面

本文负责源几何覆盖、纸面、不透明附件使用、阴影与地面；生产快照/生命周期归rhi-integration.md，统一格式/ABI归rhi-postprocess.md，壳体/resolve边界归rhi-shell.md。

## 不透明通道

- 三壳体读取原GLB的POSITION/NORMAL/TEXCOORD/indices，不重建模型/翻轴；节点/实体变换一次。纸面源几何、contain/inset/alphaTest/像素钳制遵守materials/covers；transparent洞不写颜色/法线/depth。
- paper helper执行源Lambert/sRGB/入射限亮/音乐光场，无白光或自发光补偿；生产统一fog/输出域由postprocess.md约束。
- prepare-rhi-paper.py生成共享数学/声明映射及entityLight0.xy原点场提升；`--check`核对完整token，生成+format复现源码。PowerShell入口转交同一生成器。
- RhiCaptureHarness按正常帧/读回事件串行推进，每状态只提交一次，request对应独立值结果；无手动事件循环/持续计时帧。窗口策略和配置按公共契约。

## 不透明检查

本节及下方阴影/地面入口的固定状态和重建次数描述工具覆盖能力；执行只补test-gates.md选出的实际缺口，资源/寿命先静态审计，不默认重复压力。

36个双主题×前/低视角×三封面×三通道状态保留独立数学检查：已知前平面线性RGB24深度误差<2e-6，view normal按8位编码精度（1/255），源几何四角投影≤2物理px（本入口实际DPR1）。四角只对数学投影与自身合成几何位置，不读取参考图。16个均匀墨色/入射探针直接按sRGBDecode与源Lambert diffuse clamp推导；半球同色并取`intensity=irradiance*PI`隔离已知入射，关闭环境/spot/key/fill并令print exposure=1，覆盖关闭灯光及0/.25/2入射，不据结果拟合参数。

实际附件inventory来自QRhi对象：不透明RGBA16F、法线/线性RGB24深度RGBA8、1×/无mips及真实尺寸；driver分配仍为null。显式采集按实际纹理指针去重，12个语义字段对应4张真实纹理时只读回和转换4次，别名字段共享QImage存储；captured回调仍最多2批，只有显式采集有这项工作，不改产品pass/格式/采样。十次重建由真实sceneGraphInvalidated确认，不靠renderer局部generation递增证明。

保留12个有序尺寸（九种布局尺寸及恢复旧尺寸）、30个真实帧驱动的resize、同capture request的红/蓝封面revision更新、六个双主题/1024/2048/4096阴影状态和十次同item图形重建。阴影只检查已知receiver的源strength=.32产生最低约.68、关闭时为1；完整bias/PCF/深度语义归R19D，不以该结果代替shader全等价。窗口策略逐capture验证；所有输出是自己的诊断目标，线性color PNG不是最终sRGB产品画面。

`rhi-opaque-gpu`、`prepare-rhi-paper.py --check`、`expand-lighting-shaders.py --check`和`audit-rhi-opaque.py`为现行入口。未知driver字节为null，资源统计语义由rhi-integration.md统一维护。

## 阴影

当前产品阴影为单采样 D32F 硬件比较深度，另有 R32F canonical-depth 诊断颜色附件；过滤为Linear/LessOrEqual硬件PCF，保留源五点Vogel+IGN。灯(-6,14,-5)指向原点、正交范围[-16,16,-15,15,.1,45]、Front cull、Optical_Diffuser唯一caster、radius=4、normalBias=.035、bias=-.0003 和音乐入口 strength=.32 均取自冻结源码有效路径。

`geometry.vert` 与实例化变体在每顶点按归一化世界法向偏移，再投影并插值 shadowCoord；片元只做透视除法、canonical→UV、depth bias、frustum 和 PCF。不能在片元重新归一化插值法向后投影。GL/Metal 的 UV、disk offset、IGN 原点翻转继续按实际 QRhi 能力执行；原生 clipSpaceCorrMatrix 只用于 raster。

`rhi-shadow-gpu` 独立执行本次 R32F 深度上的标量源 PCF 数学，覆盖三种 map 尺寸、1/7/8/9 实体批量边界、开关/运动/接收相机与灯色变化、十次真实 SG 重建。`Input.captureShadowDepth` 默认 false，只在显式 Review 捕获时共享已有不可变 QByteArray，不增加 GPU readback 或深复制；QImage 仍按历史诊断格式输出。每次捕获检查真正上传的 paper/shell 参数、比较 sampler、cull 和两个附件的格式/尺寸；生产 `request=0` 不做这些诊断。无变化 caster 复用 map，并跳过非批量caster专用uniform生成/复制/上传，开启时实体变换、开关切换、阴影尺寸或renderer重建失效；关闭后仅在切换或附件新建时清空一次，实体运动不重复clear。阴影目标/PSO/绑定按自身尺寸独立持有，屏幕resize、几何MSAA及诊断布局变化保持兼容阴影；只改阴影尺寸时替换接收材质的采样绑定，保留屏幕附件与后处理图。资源清单按阴影资源revision失效。固定灯相机不依赖接收相机、主题色、焦点或封面。

`rhi-shadow-audit 新目录 --invalidation-only`在1/8实体两种提交路径定向检查resize、MSAA、不透明/壳体布局、相机/光色/焦点、封面、阴影尺寸、caster运动和开关的独立失效；核对实际采样绑定、资源revision、后处理复用、零独立caster uniform及关闭后的零重复clear，不附带完整光学/重建压力矩阵。

诊断receiver独立保留为12×12、y=-.05的白色乘数平面。

## 产品地面

源地面为200×200平面，rotation.x=-PI/2、y=-4.63、向上法向、FrontSide、receiveShadow=true/castShadow=false。源色为暖昼#d8c9b9、深夜#0b1828；dusk的#a6b8c0仅作来源测试输入。生产SceneItem正常路径始终productFloor=true；geometryOnly与独立Review默认关闭。线性 floorColor 属于 SelectionLighting palette，原源色及.65秒主题曲线随现有 FrameClock 更新。地面kind=6复用opaque管线，使用Standard dielectric（metalness=0、f0=.04、roughness=.95，无clearcoat/transmission/emission），不使用纸面限亮和壳体光场调制；它接收既有key阴影、参与capture/resolve/mips、AO和完整Bokeh depth，但不进入caster池。着色后线性fog先于后处理与统一输出。

地面几何一次写入共享buffer（152 bytes）；现有receiver槽位复用，没有新增生产目标/纹理/PSO。显式Review的captureFloorDepth扩展到最多14语义字段/2在途批；按纹理身份去重，完整DOF与AO depth同资源时仅一次读回，生产默认false。历史来源及适配判定见source-fidelity-review.md的R19D3B。验证入口为rhi-shadow-gpu的地面数学/附件/十次重建，以及rhi-flicker-gpu的真实生产连续回放；不使用来源截图配对。
