# Linux开发与部署操作

本文只管Linux工具、部署和平台操作；Kit规则见[native-bootstrap.md](../contracts/native-bootstrap.md)，后端选择见[render-backend.md](../contracts/render-backend.md)。目标欠验及门槛集中在迁移计划R32-P，剩余各后端检查归R36；其他系统的构建或qsb翻译不能替代Linux执行。

## 环境与构建

准备本机C++23编译器、CMake、Ninja、clang-format和最新完整Qt Desktop Kit。部署工具另需Python 3.9以上、binutils的readelf/strip、ldd以及patchelf；播放器运行不依赖这些工具。Qt必须包含匹配的私有开发头、CanvasPainter、Multimedia及xcb/Wayland插件；默认Vulkan编译路径还需Vulkan开发头；Wayland颜色管理构建还需pkg-config、wayland-client开发文件和wayland-scanner。标准安装根自动发现，自定义完整前缀通过YALAHOO_QT_ROOTS提供。最新Kit缺模块必须补齐，不降级。

```sh
sh tools/build/linux.sh
```

构建脚本不隐式执行测试；按[test-gates.md](../contracts/test-gates.md)选择实际受影响项。Kit升级后移除过期覆盖，并传`sh tools/build/linux.sh --fresh`。普通启动`./yalahoo.sh`，匿名演示`./tools/launchers/demo.sh`，实验入口`./tools/launchers/lighting-lab.sh`；均保留原样参数，优先部署包，开发回退核对Release产物与cache中选中的Kit。

## 独立目录部署

```sh
sh tools/deploy/linux.sh
python3 tools/deploy/verify-linux.py --output /tmp/yalahoo-linux-check
```

部署使用独立`build/linux-release`、生产Release/Review OFF及同Kit Qt CMake部署API，产物在`out/linux/`。Qt从实际编译的QML导入闭包部署Templates、Dialogs、Multimedia等模块，工具补齐运行期动态加载的会话集成，并递归核对ELF依赖。C++ CanvasPainter的共享库和内嵌shader随包保留；研究QML与测试工具不进产品包。官方依据为[Qt Linux部署](https://doc.qt.io/qt-6/linux-deployment.html)和[QML部署API](https://doc.qt.io/qt-6/qt-generate-deploy-qml-app-script.html)。

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

## 目标验证操作

验收对象、尺寸/DPR选择、复用与停止条件唯一见[test-gates.md](../contracts/test-gates.md)，具体未验目标/设备和入口由迁移计划R36维护。不要将本文的会话示例展开成全组件或全尺寸矩阵。

在上述真实xcb/Wayland会话中分别启动普通产品，使用`--data-dir /绝对/隔离目录 --music-root /匿名音乐目录`。系统特有操作包括大小写/权限/XDG错误、触控板pixelDelta、系统输入法/目录对话框、同窗口最小化恢复和实际跨屏；功能/声音预期按对应契约。物理DPR记录真实值，不要求精确三档屏幕。

仅目标组件确需Review时配置`sh tools/build/linux.sh -DYALAHOO_REVIEW=ON`。CTest按准确名称定向选择，例如`ctest --preset qt-release -R '^directory-scanner$'`；真实媒体用qt-release-media，native窗口用qt-release-visible并遵守窗口契约。已有组件证据未覆盖产品联动时才选择`python3 tests/runners/verify-gui.py --backend opengl --output verification/全新目录`，或将后端参数改为目标实际支持的vulkan。

工具错误路径检查为`python3 tests/tools/LinuxDeploymentTest.py`，strip/重定位检查为`python3 tests/tools/UnixDeploymentTest.py`；运行成功路径仍须目标ELF与真实系统。性能是否测量及入口范围唯一见performance.md。
