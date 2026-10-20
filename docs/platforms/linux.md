# Linux开发与部署操作

本文只管Linux工具、部署和平台操作；Kit规则见[native-bootstrap.md](../contracts/native-bootstrap.md)，后端选择见[render-backend.md](../contracts/render-backend.md)。目标欠验及门槛集中在迁移计划R32-P，剩余各后端检查归R36；其他系统的构建或qsb翻译不能替代Linux执行。

## 环境与构建

准备本机C++23编译器、CMake、Ninja、clang-format和最新完整Qt Desktop Kit。部署工具另需Python 3.9以上、binutils的readelf/strip、ldd以及patchelf；播放器运行不依赖这些工具。Qt必须包含匹配的私有开发头、CanvasPainter、Multimedia及xcb/Wayland插件；默认Vulkan编译路径还需Vulkan开发头；Wayland颜色管理构建还需pkg-config、wayland-client开发文件和wayland-scanner。标准安装根自动发现，自定义完整前缀通过YALAHOO_QT_ROOTS提供。最新Kit缺模块必须补齐，不降级。

```sh
sh tools/build/linux.sh
cmake --build --preset qt-release --target format
cmake --build --preset qt-release --target check-format
ctest --preset qt-release -R '^(kit-selection|identity|module-link-and-plugins|source-format|directory-scanner|library-data|media-metadata|settings|playback|audio-ambience|verification-window-policy)$' --output-on-failure
python3 tests/tools/LinuxDeploymentTest.py
```

构建脚本不隐式执行测试；按[test-gates.md](../contracts/test-gates.md)选择实际受影响项。Kit升级后移除过期覆盖，并传`sh tools/build/linux.sh --fresh`。普通启动`./yalahoo.sh`，匿名演示`./tools/launchers/demo.sh`，实验入口`./tools/launchers/lighting-lab.sh`；均保留原样参数，优先部署包，开发回退核对Release产物与cache中选中的Kit。

## 独立目录部署

```sh
sh tools/deploy/linux.sh
python3 tools/audit/audit-linux-deployment.py --build-directory build/linux-release
python3 tools/deploy/verify-linux.py --output /tmp/yalahoo-linux-check
```

部署使用独立`build/linux-release`、生产Release/Review OFF及同Kit Qt CMake部署API，产物在`out/linux/`。Qt从实际编译的QML导入闭包部署Templates、Dialogs、Multimedia等模块，工具补齐运行期动态加载的会话集成，并递归核对ELF依赖。产品不使用QtCanvas2D，C++ CanvasPainter的共享库和内嵌shader随包保留；研究QML与测试工具不进产品包。官方依据为[Qt Linux部署](https://doc.qt.io/qt-6/linux-deployment.html)和[QML部署API](https://doc.qt.io/qt-6/qt-generate-deploy-qml-app-script.html)。

目录包含`bin/yalahoo`、相对`bin/qt.conf`、`lib/`、`plugins/`、`qml/`、`licenses/`、品牌desktop/SVG及`yalahoo.sh`。补齐全部运行依赖后，对暂存包中的每个ELF用`patchelf --set-rpath`更新相对自身`$ORIGIN`的RPATH，再用`strip --strip-unneeded`去除非运行必需符号，保留动态符号表；这些操作在清单hash和隐藏加载检查之前完成，不修改构建产物或开发Kit。启动脚本仅为子进程设置包内库路径并清除外来Qt/QML/预加载覆盖；保留显示会话、XDG、音频路由及显式后端选择。无需复制开发Kit、使用sudo或改动系统配置。desktop文件供用户安装到选定前缀时使用，当前目录包直接执行`./yalahoo.sh`，不自动注册桌面项。

静态依赖、文件hash/权限/符号链接和包外隐藏产品加载全部成功后才替换旧包；失败保留原包。`package.json`记录内容/权限清单，`system-dependencies.json`记录构建系统与实际外部库。验证器从包外新工作目录和隔离XDG目录启动当前进程，核对退出码、实际加载ELF、Qt搜索路径、QML、qrc和无输出媒体加载；使用offscreen QPA，窗口始终隐藏，不创建GPU上下文、不播放声音。报告不能证明图形、设备听感或正常呈现。

保留整目录复制至相同CPU架构、兼容系统ABI且未安装开发Qt的另一台Linux；另复制到包含中文和空格的目录，在包外执行其`verify-linux.py --output 全新绝对目录`并从`yalahoo.sh --demo`启动。验证器需Python/readelf/ldd，普通播放器无需这些工具。包不可仅复制二进制；跨发行版兼容性须实际证明。

原生strip/重定位最小检查：`python3 tests/tools/UnixDeploymentTest.py`；动态导出、去除开发路径和包外执行须在对应原生平台通过。

## 系统依赖与会话

系统保留glibc/动态加载器、libstdc++/libgcc、显卡驱动及GL/EGL/Vulkan loader/ICD、X11或Wayland会话、Fontconfig/FreeType、音频与D-Bus服务。精确库名由当前包`system-dependencies.json`和全部ELF的ldd结果确定，不把某发行版包名当作通用依赖。

- xcb：X11/XCB、xkbcommon/X11、xcb-cursor及插件实际依赖的扩展库；需要真实X11或XWayland服务，包内含`platforms/libqxcb.so`和可用的xcb GL集成。
- Wayland：真实compositor、wayland-client/cursor/egl、xkbcommon及实际shell/graphics integration依赖；包内至少一个`platforms/libqwayland*.so`及同Kit集成插件。不能只设置环境变量冒充会话存在。
- 媒体：包内Qt Multimedia与Qt随附FFmpeg插件/共享库；系统PulseAudio兼容服务（可由PipeWire提供）/ALSA及当前Qt动态加载的音频依赖须可用。无外部ffmpeg进程，不改设备/驱动配置。可播放格式以Qt真实结果为准。
- 字体：产品嵌入MiSans并核对实际物理表；Linux Fontconfig提供系统fallback/emoji，不能把macOS字形栅格或基线适配外推Linux。

缺包时由用户按发行版包管理器安装，工具只报告缺失库，不自动安装或改环境。打包器要求Qt媒体库来自选定Kit；若该Kit依赖其他位置的媒体库，须先补齐Kit闭包，不能借开发机的另一套Qt/媒体库放行。

用户在各自真实会话中主动运行以下入口；开发产物亦由根启动器配置同Kit路径：

```sh
QT_QPA_PLATFORM=xcb ./yalahoo.sh --demo
QT_QPA_PLATFORM=wayland ./yalahoo.sh --demo
QT_QPA_PLATFORM=xcb QSG_RHI_BACKEND=vulkan ./yalahoo.sh --demo
QT_QPA_PLATFORM=wayland QSG_RHI_BACKEND=vulkan ./yalahoo.sh --demo
```

默认Vulkan；未显式选择时初始化能力不足会提示回退OpenGL。显式Vulkan请求遇到Qt特性、开发头、运行时/设备或格式不足时非零退出并输出原因，后续验证必须记录实际后端，回退不等于Vulkan通过。实际surface/swapchain及全部效果兼容性仍需目标验证，不能由初始化探测代替。

## 目标操作清单

下列可见检查由用户在对应会话主动运行。代理自动验证遵守[test-window-policy.md](../contracts/test-window-policy.md)，默认隐藏，不能为采集激活窗口。具体数值门槛与退出条件只在迁移计划及所属组件契约维护。

| 检查 | 操作与预期 |
| --- | --- |
| 包/路径/权限/XDG | 包外启动、中文/空格音乐目录和数据根；显式`--data-dir /绝对/隔离目录 --music-root /匿名音乐目录`。区分大小写的目录/文件身份不合并；只读数据根/不可读音乐目录给出明确错误，歌曲与标签不被修改。未指定数据根时遵循XDG并保持yalahoo身份。 |
| 离线/导入 | 空库和匿名库的扫描、搜索/取消、重启缓存、只读旧JSON导入；无网络请求、无伪造资料，浏览定位不改变播放。 |
| 浏览/详情/总览 | 九尺寸、双主题与DPR1/1.5/2分别记录真实值；鼠标、键盘、触控板pixelDelta、首尾/反向/切列、抽取/连选/归位、详情拖动、总览折叠定位和焦点均正常；输入法和系统目录对话框另验。 |
| Canvas/文字/片头 | 生产C++滑尺、二维序章、glyph/Quick Text标题及彩色emoji的轮廓/透明合成/裁切/淡隐正常；片头输入锁/跳过/重播/三维交接、改速/减少动态沿唯一帧源。不能只用模块导入成功代替绘制质量。 |
| 声音 | 默认/显式设备、播放/暂停/停止/seek/上下曲/自然结束、未知格式/权限错误恢复；BGM/音效独立。最小化歌曲继续；设备切换/消失/恢复及实际听感分别确认，关闭停止三路。 |
| 生命周期/资源 | 同一窗口hide/minimize/restore、跨屏DPR及resize/画质/主题变化后输出有效；资源/寿命先静态审计，重复SG/视觉重建仅按test-gates.md补具体未决行为。 |
| 呈现/性能 | xcb/Wayland各自核对实际上下文/swapchain、附件/格式/MSAA/resolve与正常呈现，共用无会话依赖结果。性能先按performance.md静态审计，具体未决问题才最小实测，不默认追加吞吐/计时；未知显示/GPU完成/driver显存保留null。 |

需要目标组件检查时才构建Review：`sh tools/build/linux.sh -DYALAHOO_REVIEW=ON`，按test-gates.md选择尚缺的断言。OpenGL与实际启用Vulkan的剩余图形/窗口入口归R36，分别显式选择后端并核对报告实际API。入口见各组件契约；只有已有组件证据未覆盖产品联动时才使用`python3 tools/build/verify-gui.py --backend opengl或vulkan --output verification/全新目录`，不重复已有有效矩阵。具体性能问题按performance.md决定是否需要verify-startup及所需场景/时长，工具选项不成为本平台的额外任务清单。
