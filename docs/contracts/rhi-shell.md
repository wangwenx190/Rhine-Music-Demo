# RHI透射捕获、壳体与排序

本文只定义捕获/壳体/多pass覆盖及其验证；材质源参数见[materials.md](materials.md)，光场见[lighting.md](lighting.md)，资源格式/ABI见[rhi-postprocess.md](rhi-postprocess.md)。

## 捕获与覆盖

- 不透明捕获含背景/纸面/结构/生产地面，不含透射壳体。采样已resolve的线性颜色mip链，endPass→resolve→generateMips完成后才提交壳体；独立合成颜色目标禁止反馈/递归玻璃捕获。
- 合成先复制已resolve的不透明颜色并保留/共享兼容硬件深度，再绘制壳体；不靠量化颜色猜深度。captureSamples是实际样本数，设备不支持显式失败，不能静默降级。
- 壳体FrontSide（Back cull/CCW）、depthTest/write、LessOrEqual、blend=false、alpha=1。visibility先写front-surface depth，颜色用Equal/read-only depth；两个顶点变体共同执行world=`model*vertex`后`nativeVP*world`并声明invariant，仅二者qsb MSL2.1，其余shader保持既有目标。
- 纸面顶点按contain/1⁄64 inset裁可见矩形并反向映射UV，centroid插值保留几何MSAA部分覆盖；alphaTest=.025。上传预乘RGBA8，滤波后恢复未预乘颜色再sRGB解码，透明黑不形成边；几何normal/DOF覆盖仍为完整原平面。

## 材质适配与排序

- GLB变换一次、法向inverse transpose。ShellMaterial按materials的实际主题/clarity生成uniform，不叠加标定系数；三壳体GGX/DFG/LTC/PMREM/bicubic和源世界光场使用显式binding，署名/来源保留在manifest与materials-NOTICE。
- roughness取view-space normal最大分量导数；LTC零切线/零光半向量用有限奇点处理，不改源物理参数。盖内局部LOD跟随实体，不能替换为屏幕固定模糊。
- 全局壳体以源批次包围球中心的相机深度远→近，平局按稳定entityId再surface；纸面近→远且同样稳定。submit拒绝缺失/错位/重复ID；不能按三个模型分别排序或用数组下标作身份。
- 排序后的壳体着色序列只绑定一次相同管线与顶点/索引缓冲，逐draw仅切动态uniform偏移；不改变原draw顺序、材质或Equal深度语义。
- 阴影只衰减key diffuse/specular/coat，不压暗环境/整幅透射；caster/偏置/PCF由rhi-opaque契约定义。

## 源数学与连续验证

下列固定状态/帧数与重建次数是工具覆盖能力；当前执行只补test-gates.md选出的实际缺口，不默认完整序列或压力。

- `rhi-shell-gpu`保留61源状态（sourceIndex 2、4–57、60–65）、6排序/2隔离、1×/4×alpha和一次SG重建；sourceIndex的direct-material例外独立保持正确颜色域。10数学样本覆盖LTC/GGX/DFG/shadow、view-roll0/45，LTC≤.001，其余max(1e-6,abs(expected)×.001)；投影≤2逻辑px，alpha中心≤1/255/黑边≤原一码值，透明洞不写depth、4×有部分覆盖。
- `rhi-flicker-gpu`使用真实SceneItem/匿名9×64库与封面；1/7/8/9实例跨批量边界、1×/4×及双主题/640×480和1280×720，静止/波浪/浏览/总览/详情连续检查。当前完整序列1200帧，正常光学与覆盖探针均有效，missing/nonfinite精确0、shade>100，运动不重建无关资源。
- 独立光学诊断`rhi-flicker-test 输出目录 --optics-only`只证明其光学断言；其他未覆盖断言另保留，不能将局部报告写为完整通过。旧GL R16F诊断问题的修正归occlusion.md，不能继续把其旧非有限字节当产品缺陷或把optics-only当完整通过。
- `rhi-shell-submission-gpu`或`rhi-flicker-test 新目录 --isolated-only`只运行1/7/8/9实体×1×/4×的Equal覆盖与有限光学检查，不运行生产连续序列或性能采样。
- 入口按test-gates.md选实际隐藏/窗口组；跨平台欠验与完整窗口/呈现要求归计划R36。

壳体四个入口只声明area/early编译期变体并包含`resources/shaders/rhi/shell-body.glsl`；`prepare-rhi-shell.py`由唯一来源生成该适配主体与`shell-lighting.glsl`公共LTC/GGX helper，地面直接包含后者。几何单实例与批量入口共用`geometry-body.glsl`，保留各自模型/实体输入与相同position invariant。
