# 雾、Bokeh、SMAA与最终输出

本文维护后处理源算法与组件公式检查；SSAO/画质归[occlusion.md](occlusion.md)，附件/上传图归[rhi-postprocess.md](rhi-postprocess.md)。冻结scene.ts及锁定Three的来源/lookup hash在`resources/manifests/postprocess.json`。

## 顺序与颜色

线性不透明捕获→线性壳体→SSAO→Bokeh→按设置SMAA→一次ACES exposure/精确sRGB。纸面toneMapped=false的入射/限亮语义保留，最终OutputPass仍转换整幅composer（含纸面/背景）。生产GlassMaterial.compositorOutput不提前tone-map；隔离直接材质Review的ACES路径不进入生产。

raw AO/normal/depth/linearColor绕过全部最终转换。浏览器线性诊断须追加identity ShaderPass，防止单RenderPass自动renderToScreen；以真正调用路径判断颜色域。文字/阅读层在三维之后。

## 线性雾

- 使用实际渲染相机距离与detail：near=`distance+lerp(5,-1,detail)`，far=`distance+lerp(25,12,detail)`；仅detail变化也须更新。viewDepth=-viewZ，以`smoothstep(near,far,viewDepth)`混合线性主题fogColor。
- 纸面/不透明地面在各自源着色后、capture/resolve/mips前加雾；壳体在BRDF、透射/Beer及光场完成后加自身表面雾，不能对同一表面重复处理。背景、normal、depth不加雾。
- 雾先于SSAO/Bokeh/SMAA及一次ACES/sRGB；生产始终启用，rawOutput仅供Review。
- `rhi-fog-gpu`覆盖84源helper状态、RGBA16F误差<.001及十次SG重建；生产1200帧核对真正上传的uniform，单独雾/主题更新不得重建无关资源。静态入口`audit-rhi-fog.py`。

## Bokeh

- 初始focus25/aperture.0018，实际帧focus为view空间`entityOrigin+(0,2,0)`正深度，aperture=`(.0003+.0005*detail)*depthOfField/100`，maxblur=.011。
- 固定41次color采样、.9/.7/.4环、aspect修正、正负CoC及±.011限幅、alpha=1；RGB24 linear depth nearest，color线性/clamp-to-edge，独立完整几何深度，不能用仅不透明主通道代替。
- `rhi-bokeh-gpu`直接生产Postprocess/qsb真实QRhi offscreen：36组129×73/257×193/193×257、focus20/25/30、aperture0/.0003/.0008/.0012，加十次reset/recreate与另一组clip.1/1000。独立double双线性/源41点核对所有解析像素，颜色≤3/255、有限/HDR/alpha精确1，覆盖前后景/焦平面/限幅/边界。
- `rhi-flicker-gpu`生产1200帧另核对真正上传的focus/aperture/aspect/clip/maxblur和完整DOF尺寸。`export-bokeh-kernel.py --check`检查源hash/41点数学输入，`audit-rhi-bokeh.py`审计调用与资源边界。

## SMAA

- 锁定Medium：颜色阈值.1、最多八步、160×560 area线性/66×33 search nearest，gamma2.2及alpha混合。lookup原内嵌PNG→RGBA8，flipY=false解码行序，不转sRGB、不建mip；framebuffer Y变换不能作用于lookup。
- 源crossing宏`float(ivec2)`取首分量后广播：(1,0)两轴各+1，(0,1)不增轴并保留调用前y减1，独立公式必须遵守该语义。
- `rhi-smaa-gpu`直接生产三pass真正离屏；34状态含64×64/129×73/73×129、七种解析模式、动态后flat清零与十次重建。edges≤1/255、weights/blend≤3/255，全部RGBA通道有限，HDR/变化alpha保持。`audit-rhi-smaa.py`核对完整源helper/lookup/历史调用。
- Quick3D Review保留仅DOF/仅SMAA/DOF→SMAA三个固定Effect实例；组合在同Effect内用具名RGBA16F中间目标，最终pass必须输出最终目标；不动态重定向Pass.output/BufferInput。

## AA与执行边界

原始antialias=off只关闭后期SMAA，几何MSAA仍开启；高/极高追加SMAA。实际样本数/resolve/部分覆盖由几何与壳体检查证明，SSAA仅Review诊断，不是产品预设或绕过目标上限的方法。

Bokeh/SMAA独立项无swapchain/可见窗口，GL fallback surface须活到QRhi销毁之后。`ctest --preset qt-release-render -R '^rhi-(bokeh|smaa)-gpu$'`运行组件公式；完整资源图/计数在rhi-postprocess.md维护。旧四套图像入口保持停用，未覆盖的非像素检查和MSAA/resolve归计划R19D4B2，尺度/BLUR-01归R19D4C。
