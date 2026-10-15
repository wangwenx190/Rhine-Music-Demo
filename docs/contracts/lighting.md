# 灯光与连续主题契约

冻结源码及锁定 Three 算法 hash 在 [lighting.json](lighting.json)。这是音乐生产调用的光照扩展；R07 的隔离白色入射测试仍保留。本文只管光场与主题；阴影归rhi-opaque.md，AO/DOF归occlusion.md/postprocess.md。

`SceneAdapter.lighting` 持有 `SelectionLighting`，唯一 FrameClock 在相机更新后传入已经缩放一次的 motionDt；没有灯光自己的计时器或第二次 speed 乘法。`RowLightController` 以 double 保存行/列位置与速度，按源函数限幅同向动量并使用 rate=4.743864518390578/duration。guided 默认参数为 strength=1.3、width=1.35、offset=.3、fill=1、duration=1.1。设置校验保持原范围，改变模式只修改 uniform。

`RowLightingInput` 接收绝对 row/lane、已渲染的盒体世界 origin、independentColumns、可见/减少动态/电影状态与可选 introTime。独立列跨列先把旧 rowBasis 换算到新 basis，再跟随；最终 rowColumn 加上当前实体 origin，不对整列平移再次滤波。rebase 保留绝对逻辑坐标。开场偏移采用原 22.2/24.15/25.75 秒轨迹，离开开场直接落到所选行；隐藏的模型清空跟随初始化，窗口隐藏则由 FrameClock 完全暂停。真实列纵向导航沿用 SceneAdapter 的相机俯仰隔离。

选中 key 的 aim 是盒体本地 (-2.085,2.252,0) 经盒体旋转后的世界位置；anchor 与宽光 column 用 rate=5 临界阻尼，camera-local offset=(-48,-17.6,36)。spot 角度 .32、penumbra .95、逆平方衰减、无额外阴影。area 宽39、高 .18×width，位置为 rowColumn + (0,4.025,offset×.62+.44)，朝向 rowColumn + (0,3.275,offset×.62)。baseline/guided 的面积光强为0，area 为完整强度，hybrid 为 .3 倍，详情再乘 (1-smoothstep(detail))。

音乐入口覆盖后的暖昼/深夜：environment .25/.16，key .45/.30，hemisphere .32/.21，fill .045，spot 180×64/130×64，area 34/24，scatter 1/.72，printAmbient .5/.12，曝光1.05/1.08；灯色和背景色见 LightingPalette::theme。key/fill 方向取 (-6,14,-5)/(7,8,-10)，保留 RoomEnvironment 的方向/颜色和原半球上下色。

主题在650ms内按 cubic-bezier(.4,0,.2,1) 解横坐标并在线性RGB空间插值。反向请求以当前已显示 palette 为起点，相同目标不重启；减少动态直接完成。warmth 驱动既有材质数值与线性 tint 插值；背景和未来阅读层共用同一个 palette/backgroundColor。QML 不另用 Behavior/ColorAnimation 重算主题时间。

灯光通过公有 CustomMaterial uniform 在 GLSL 中计算，保留源 Lambert、GGX/Smith 多次散射、LTC 面光与固定 PMREM；没有使用 Qt 内置灯的不同 brightness 单位，因此无需以不相同的单位抄数或搜索换算因子。灯/面积光计算因子均为1，暖昼与深夜相同，未按主题调参。材质参数不乘图像标定系数；所有材质/灯光参数直接按源公式，不使用图像标定系数。LTC 两张64×64原始表经已有锁定依赖导出为 qrc RGBA32F；原 MIT 许可与来源保留。

`lighting-field.glsl` 是纸面和壳体唯一的光场源码，开发工具展开成两个独立 Qt shader，`expand-lighting-shaders.py --check` 校验一致性。row field 以实体 world origin 的x/z计算，排除纸面局部位置、y高度和相机；源 CoverAtlas 的纸面偏移烘焙在几何中，Qt 的独立 Rectangle 反向消去本地偏移得到同一实体原点。主盖/侧脊接入边缘输运与角度 glint，后托只接入源曝光规则；纸面只乘已有漫反射和 fieldExposure，原图上限限制先于光场乘法，不加白/自发光。纸面环境漫反射按原 Lambert PMREM 参与，Lambert 不接收 area 的合成照明。面积光沿用原 LTC 面积积分，不能再额外除π。

当前光场数值复建入口为 `node tools/review/export-lighting-golden.mjs`，直接执行冻结MusicSelectionLighting/ThemeTransition/music-row-lighting并生成20/30/60/120fps轨迹、主题反向/减少动态/隐藏/rebase和392组光场采样。`ctest --preset qt-release -R '^lighting$' --output-on-failure` 检查其数值，不需要原图。sampleRowLighting的核本身不依赖mode/fill，测试仍显式传入两者；灯光shader的模式合成另由R19C2复查，不能用核测试代替。


`runtime-lighting`保留48源状态/灯光参数/投影/有效输出与root重建检查，共用Review执行机制见materials.md；公共分组见test-gates.md。`python3 tools/audit/audit-material-parameters.py`与`audit-material-review.py`分别检查实际参数链与Review生命周期，目标欠验归计划R34。
