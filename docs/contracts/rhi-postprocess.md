# RHI图集、后处理资源图与ABI

本文只管RHI数据布局、读写边、分配/复用及上传；封面服务归covers.md、生产atlas空间归archive-pool.md，SSAO/Bokeh/SMAA数学及数值测试归occlusion.md/postprocess.md，生产同步/遥测归rhi-integration.md。

## 图集与几何数据

- AtlasUpload按页面generation/槽lease/serial验证，重复/过期在复制前淘汰，按槽合并最新值。owned immutable像素按来源cacheKey共享；外部输入隔离一次，转换已有独立存储时不追加无效copy。
- 256² RGBA8分块与独立cover mip按真实字节计费；稳态2MiB、初始化16MiB，其余跨帧。每级坐标钳制在本封面内，缩小与倾斜使用足迹过滤；生成与上传见covers.md。resize保留图集，实际页面重建换generation。
- Frame std140为832bytes：前752保留，coverUv752、paperSize768、originLight0/1为784/800、fog816。C++ static_assert/offset与qsb reflection核对；顶点属性location保持连续前缀，保留未使用输入占位，避免stage linkage错位。
- 几何instance112bytes：model四列、tile UV、两组origin light；固定436实体容量（432池+4接管），draw uniform容量`uniformStride*(436*3+6)`。CPU暂存复用，只写/上传有效aligned前缀，不清零/上传未使用容量。
- 深度键每实体预计算一次，稳定ID排序；112-byte实例直接填值，不生成被丢弃的完整材质块。caster保留必要窗口外实体，阴影未失效跳过专用uniform和重画。
- 可见性、深度键、绘制偏移及壳体顺序使用render独占暂存，容量随有界实体数复用，不交给异步采集。view×model、lightVP×model及有效封面UV按实体计算后供各表面复用，保持矩阵乘法结合顺序；纯实例路径跳过不用的逐实体light矩阵。
- QRhiResourceUpdateBatch::updateDynamicBuffer指针重载返回前复制载荷，CPU暂存可跨帧复用；后端storage轮换由QRhi负责，不能持有/改写其内部存储。

## 通道资源

- 生产颜色/中间颜色RGBA16F，view normal/RGB24 depth为RGBA8；阴影深度与按需诊断附件见[rhi-opaque.md](rhi-opaque.md)。环境PMREM的源float值必须可被half精确表示才转换上传RGBA16F，不能拟合/量化丢精度。
- 不透明捕获resolve/mips后供独立壳体颜色使用；合成复用硬件depth/stencil并preserve/load。两个pass共用生命周期不重叠的MSAA颜色附件（Review含normal/depth），合成前清颜色；各自resolve纹理独立，禁止采样正写入的附件。共享资源由两个目标共同持有，目标先于附件释放，资源计划与清单按身份只计一次。
- AO raw/blur为R16F，normal/depth与颜色同尺寸，full DOF深度在全尺寸兼容时共用。kernel仅创建时写入std140的offset80，固定1104-byte块；每个pass动态80-byte前缀，不保留闲置kernel纹理。
- 颜色ping/pong仅在生命周期不重叠时复用，pass各有uniform/SRB/目标。Bokeh-only一个全尺寸RGBA16F ping/一个uniform，常态80bytes；SMAA-only三个单采样全尺寸RGBA16F目标、两张无mip RGBA8 lookup、三个uniform，常态240bytes，载荷估算`24×width×height+370424`bytes。
- 相机/focus/aperture/主题数值/输入内容变化只更新uniform，不重建兼容目标；尺寸/核/启用状态/实际输入附件身份变化才重建对应图。关闭通道不保留其附件；Review中间目标只读访问在普通构建返回nullptr，不改变跨编译单元类声明。
- qsb由同一最新Kit生成SPIR-V/GLSL330/ES300/HLSL50/MSL12；位置invariant的两个geometry顶点独用MSL2.1。各语言翻译是静态证据，目标运行依计划R36。

## 上传核验

TransferAudit在适用D3D11目标命令列表按原生纹理筛UpdateSubresource，记录destination box/row pitch/destination bytes；实际tile262144bytes，初始化/稳态上限按上文。自身GPU整页读回用于核对区域更新/未改区域/还原，不读取浏览器图片；这不是物理总线流量。其他后端无原生命令观测时仅报告可见API计数和自身读回，不能伪造D3D测量。

## MSAA/resolve与组合门禁

`rhi-postprocess-gpu`提供源数学/实际资源/上传/功能检查。下列状态与重建数描述工具覆盖能力，执行范围和必要入口调整按test-gates.md；具体算法和预算分别归occlusion/postprocess/performance契约。

- `ctest --preset qt-release-render -R '^rhi-postprocess-gpu$'`：macOS真正隐藏RenderControl；其他系统按test-gates.md选择native入口。321×241、241×321、640×480，各1×/4×及AO/Bokeh/SMAA八组合共48状态；读真实RT/PSO/SRB核对颜色RGBA16F、诊断RGBA8、geometry/depth样本一致、同尺寸同格式单样本独立resolve、后处理逐pass单样本/无反馈/正确绑定及最终输出绑定；真实最终合成背景与独立ACES/sRGB公式误差≤1/255且alpha=1。
- 稳态图像/代次复用、每pass80-byte前缀、focus/aperture/exposure更新不重建，关闭释放AO/DOF/后处理；十次真实SG失效后组合资源及输出恢复。实际alpha几何部分覆盖、透明洞/黑边的解析门槛归rhi-shell.md，不用截图边缘分数。
- `rhi-transient-attachments-gpu`或`rhi-postprocess-audit 新目录 --attachments-only`只检查321×241/241×321、1×/4×的普通生产与Review附件。实际GPU资源ID证明MSAA共享且resolve独立，描述符清单与分配前计划一致，生产最终背景核对独立ACES/sRGB公式；不附带图集矩阵、成本采样或重复重建。
- 保留可控64tile初始化、432实例、全可见池和1/7/8/31/32/33/64/432容量、单tile替换/还原/逆序、resize不重传、generation拒绝及第九tile背压；自身atlas整页读回验证区域更新，D3D11额外核对native命令。缺primary租约的合法采集metadata为null；fallback要求完整两组租约。
- `rhi-flicker-gpu`实际SceneItem以original/high两画质覆盖源启用条件和组合图，完整工具序列含双主题/小大场景/静止/波浪/浏览/总览/详情1200帧；按实际缺失断言选状态，不默认全部运行。目标尺度与BLUR修正的独立集成入口见rhi-integration.md；SG失效不冒充同原生窗口恢复或VSYNC。
- `python3 tools/audit/audit-rhi-resolve.py`输出本地历史/三平台COMMON/PERF矩阵；局部CPU提交诊断仅在严格静态分析仍无法确定具体成本问题时选择最少必要通道/样本，不默认六通道或三次；必要入口调整按test-gates.md。不执行图像分数/120秒产品门槛。Qt呈现、GPU完成/driver显存、显示延迟及完整产品指标依performance.md。
