from pathlib import Path
import json
root=Path(__file__).resolve().parents[2]
evidence=root/'verification/windows/lighting'
s=json.loads((evidence/'summary.json').read_text(encoding='utf-8'))
rows=['| 区域 | 最大逐场景 RGB MAE /255 | 最低有效 SSIM |','|---|---:|---:|']
for name,metric in s['roi'].items():
    rows.append(f"| {name} | {metric['maxMeanAbsoluteRgb']:.4f} | {metric['minSsim']:.6f} |")
report=f"""# R08 Windows 灯光验收

Qt {s['qtVersion']}，Release，真实 Windows D3D11 / DPR {s['actualDpr']}；OS/GPU/驱动见 summary.json。Review {s['ctest']}、普通构建 {s['production']['ctest']}，qmllint 通过，普通产物拒绝 Review 参数。六个实际材质片段的五类 qsb 目标生成通过，目标系统实机结果不在此结论内。

冻结提交 `{s['baseline']}`；fixture/golden/source/算法/纹理 hash 保存在 summary.json、tests/fixtures/lighting、docs/contracts/lighting.json 与资源清单。匿名合成封面，未读取真实音乐库，浏览器捕获阻止外部 HTTP。

924 个运动/主题样本覆盖 20/30/60/120fps，轨迹最大误差 {s['errors']['trajectoryError']:.3g}，主题参数/线性颜色最大误差 {s['errors']['paletteError']:.3g}；392 个世界光场样本最大误差 {s['errors']['fieldError']:.3g}。float GPU uniform 位置量化误差 {s['errors']['gpuUniformPositionError']:.3g} 世界单位。重复输入、反向、独立长短列与 basis 换算、rebase、改速、电影/开场偏移、隐藏/恢复、减少动态以及昼夜反向均按冻结源码校验；FrameClock 的倍率只进入一次。

48 个同输入 GPU 状态：双主题 × baseline/area/guided/hybrid × 正视/低角度 × 三个行位置（0/1/4）。光场为同一个世界原点函数，印刷不自发光；抽取/归位的纹理与阵列本体仍由后续对应轮次交付。五ROI图像门槛全部通过，最大投影误差 {s['maxProjectionPixels']:.6g} 逻辑像素。窄外框不够11×11时不编造SSIM，仍检查非空ROI、MAE与结构。

"""+'\n'.join(rows)+"""

成对图为 docs/media/lighting/web/000–047.png 与 qt/000–047.png，差分为 qt/diff-000–047.png；完整逐场景ROI、Web投影、源参数一致性与真实前景/帧数在 release-runtime.json。正视/低角度、暖昼/深夜样本已人工查看，没有封面加白、UV翻转、错误原点或额外发光。局部差异主要是沿用R07的材质标定、近边纹理过滤/抗锯齿及极小PBR积分误差；不改变源码颜色、几何、IOR或封面以贴合采集缺陷。

修复依据来自实际源码：纸面必须包含 Lambert 的 PMREM 漫反射；LTC面积积分不能额外除π；纸面偏移沿生产CoverAtlas烘焙几何规则映射到实体原点；spot/GGX多次散射与主题覆盖采用实际音乐调用顺序。未使用Qt内置灯brightness单位，自定义GLSL直接保留源单位，所有灯种公用因子1且双主题一致。

macOS/Linux本轮严格静态审计结论为“静态审计通过，实机待用户验证”，未声称已编译或已运行Metal/OpenGL/GUI/音频。后续阴影/AO/景深/阵列/完整应用验收不在本轮结论中。
"""
(evidence/'report.md').write_text(report,encoding='utf-8',newline='\n')
print('R08 lighting report written')
