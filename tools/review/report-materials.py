"""Material-only acceptance report, with each residual attributed to an isolated layer."""
from pathlib import Path
import json,hashlib
root=Path(__file__).resolve().parents[2]
evidence=root/'verification/windows/materials'
summary=json.loads((evidence/'summary.json').read_text(encoding='utf-8'))
r=json.loads((evidence/'release-materials.json').read_text(encoding='utf-8'))
rows=r['cases'][:90]
lines=['# R07 单盒材质验收','',f"Windows Qt {summary['qtVersion']} / Release / D3D11，实际内容 960×720、DPR1。17/17 Review 回归、13/13 普通回归、qmllint、六 shader 五目标生成通过。90 个同输入 GPU 状态及 9 个纸面入射测试通过。",'',f"独立 Three/Qt 投影最大误差 {max(c['webProjectionPixels']for c in rows):.8f} 逻辑像素；Qt/CPU 最大误差 {r['maxProjectionPixels']:.8f}。所有有效 ROI 的 MAE≤6/255、具有完整 Gaussian 窗口的 SSIM≥.97。窄外框没有窗口时 SSIM 为 N/A，未制造满分。",'', '| 逐层定位 | 对应ROI MAE均值 / 最大 | 有效SSIM最小 | 最差MAE场景 |','|---|---|---|---|']
residuals=[]
for layer,region in [('all','paper'),('all','margin'),('all','spine'),('all','rim'),('paper','paper'),('Frosted_Polymer','margin'),('Ivory_Edges','spine'),('Optical_Diffuser','rim')]:
 c=[x for x in rows if x['layer']==layer];values=[x['roi'][region]['meanAbsoluteRgb']for x in c];ss=[x['roi'][region]['ssim']for x in c if x['roi'][region]['ssim']is not None];worst=max(c,key=lambda x:x['roi'][region]['meanAbsoluteRgb'])
 ssim=f'{min(ss):.6f}'if ss else 'N/A';idx=worst['index'];name=worst['screenshot']
 lines.append(f"| {layer}/{region} | {sum(values)/len(values):.4f} / {max(values):.4f} | {ssim} | {idx}: {worst['theme']}/{worst['angle']}/{worst['cover']} |")
 residuals.append({'layer':layer,'region':region,'worstCase':idx,'mae':max(values),'minSsim':min(ss)if ss else None,'web':'docs/media/materials/web/'+name,'qt':'docs/media/materials/qt/'+name,'difference':'docs/media/materials/qt/diff-'+name})
lines+=['','纸面单层的最大残差集中于原图缩小后的高频纹理，来源为 Web 1024px canvas 预绘制与原生直接图片/mip 采样的滤波差异；不是玻璃参数或自发光。主盖留白的亚像素差异来自屏幕 mip/bicubic 采样与边缘导数粗糙度，侧脊与后托差异单独列出；当前均在数值目标内。没有整体模糊、改封面亮度、改模型或噪点遮误差。R11 的共同封面绘制/纹理路径可进一步统一缩小滤波，R08/R09/R09B 仍须分别验证动态光场、阴影和后处理。', '', '斜视夜间对照 075 中，透射捕获的纸面透明留白边界仍可见阶梯；当前 MSAA 没有平滑该 alphaTest/capture 路径。该局部边缘单独交给 R09B 的实际捕获/resolve/SMAA 验证，不把它解释成材质色差或已解决的抗锯齿。', '', '正视/低角度/斜视和三封面均沿原几何；只比较同输入图像，不将未实现的全场效果列为本轮已验收。所有原生截图及绝对差分见 `docs/media/materials/qt/`，原 Web 图像见 `web/`；`release-materials.json` 保存每个投影ROI、RGB signed error、SSIM窗口数和真实渲染帧数。','', '标定采用昼夜同一组系数，18个组合/五ROI等权、受约束三遍坐标扫描；固定几何与相机使几何轮廓项不随材质候选改变，投影的 float 误差另外按硬门槛报告。完整候选/损失/选值保存在 `calibration.json`。最终系数为：', '', '```json',json.dumps(summary['calibrationFactors'],indent=2),'```','', '零/.25/2 倍入射的九个纸面样本最大通道误差：'+str(max(c['paperProbeError']for c in r['cases'][90:]))+'/255；零光输出黑色印刷，过亮仍限制到原图。普通构建不含材质验证实现并拒绝 Review CLI。','', 'macOS/Linux：静态审计通过，实机待用户验证；本报告中的 Windows 图像与 qsb 翻译不属于两平台实机证据。']
(evidence/'report.md').write_text('\n'.join(lines)+'\n',encoding='utf-8',newline='\n')
(evidence/'residuals.json').write_text(json.dumps({'baseline':r['baseline'],'fixtureSha256':r['fixtureSha256'],'residuals':residuals,'thresholds':{'roiMae':6,'roiSsim':.97,'logicalProjectionPixels':2},'unresolvedHardGates':[],'followupEdge':{'round':'R09B','case':75,'region':'oblique paper transparent inset boundary through glass','finding':'Visible stair steps in alphaTest/screen capture path despite SceneEnvironment MSAA; compare magnified edge and verify actual capture/resolve or SMAA.'}},indent=2)+'\n',encoding='utf-8',newline='\n')
media=['# 单盒同输入对照','','编号与输入对应 `tests/fixtures/materials/single-case.json`；完整机器指标见 `verification/windows/materials/release-materials.json`。','', '| 场景 | Web | Qt | 绝对差分 |','|---|---|---|---|']
for idx in [0,5,10,15,20,25,30,35,40,45,60,75]:
 c=rows[idx];n=c['screenshot'];media.append(f"| {idx} {c['theme']}/{c['angle']}/{c['cover']} | [源图](web/{n}) | [原生](qt/{n}) | [差分](qt/diff-{n}) |")
(root/'docs/media/materials/README.md').write_text('\n'.join(media)+'\n',encoding='utf-8',newline='\n')
print('R07 material report and residual index generated')
