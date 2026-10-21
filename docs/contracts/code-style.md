# Qt 源码风格契约

本契约适用于所有后续轮次及维护提交。依据 [Qt Coding Style](https://wiki.qt.io/Qt_Coding_Style) 和 [QML Coding Conventions](https://doc.qt.io/qt-6/qml-codingconventions.html)，可执行配置为根 `.clang-format`、`.qmlformat.ini` 和 `.editorconfig`。本项目按用户要求进一步展开短函数、短分支和循环体。

- 缩进统一为四个空格，禁止 Tab；删除行尾空白，文件末尾保留换行。C++ 延续行按配置使用八空格或参数对齐；其他语言遵循四空格层级。JSON 配置使用两空格。
- C++ 函数、类、结构体的左花括号另起一行；控制流、命名空间的左花括号留在行尾。`case` 与 `switch` 对齐，命名空间内部不额外缩进。指针/引用采用 `Type *name`、`const Type &name`。
- C++ 实现文件中，同一命名作用域的私有常量、类型和辅助函数集中在一个连续的匿名命名空间，公开函数和成员定义放在其外；不重复嵌套匿名命名空间，也不按单个辅助函数反复开闭命名空间。
- 每行只写一个独立语句；短函数、lambda、分支和循环体也展开。允许 `for` 头部的必要分号及同一声明的多个声明符。保留现有花括号和控制流，不借格式整理添加或删除分支。
- QML 对象体展开，属性、信号处理器内的独立语句逐行书写。`qmlformat` 使用同一最新原生 Kit；保留现有属性、声明和导入顺序，以免改变绑定或类型解析。
- 换行目标为 100 列；不可安全拆分的字符串、URL、许可证行、宏声明及源码模板可例外。格式化不更改字面量、数值、宏行为、shader 算法、资源内容、执行顺序或接口名称。嵌入字符串的冻结 Web/GLSL 模板保留原文。
- JavaScript 维护工具使用 `.clang-format` 的 JavaScript 分节；PowerShell 使用 `tools/format/PowerShellStyle.psd1`，展开语句分隔符时保留 `for` 头部。Python/CMake/shell 同样采用四空格和逐行语句，遵守各自语法；Python 的短套件及 shell 的 `case` 分支也必须展开。

范围包括 `src/`、`qml/`、`tests/`、`tools/`、`cmake/`、根构建配置及 `resources/shaders/`。`legacy-web/`、历史验收文件、第三方/导入资产（包括 `resources/models/Music_case_v040.qml`）、构建和部署输出保持原貌。源码生成工具运行后，必须重新执行格式化和检查，不得提交重新压缩的源码。

## 格式化与提交门槛

源码改动后按需格式化；提交前确认相关格式结果有效。功能测试及结果复用范围按[test-gates.md](test-gates.md)选择，不能将以下格式入口扩展为默认全套测试。以下是已配置Release中的独立入口，按实际变更与已有有效结果选择；无需为格式检查完整编译应用：

```sh
cmake --build --preset qt-release --target format
cmake --build --preset qt-release --target check-format
ctest --preset qt-release -R '^source-format$' --output-on-failure
git diff --check
```

`format`/`check-format` 覆盖 C++、shader、JavaScript 和 QML；`source-format` 纳入 CTest，任何格式差异或解析失败均使检查失败。`clang-format` 从 PATH/当前 MSVC 工具链发现，也可用本机 CMake cache 变量 `YALAHOO_CLANG_FORMAT` 指定；不在公共配置中写机器安装路径。开发环境需提供支持仓库配置的 clang-format；缺失或配置不兼容时明确失败，不静默跳过。格式化工具只用于开发，应用运行不依赖它们。

Windows 维护 PowerShell 时另执行：

```powershell
./tools/format/FormatTools.ps1
./tools/format/FormatTools.ps1 -Check
```

该入口复用已安装的 PSScriptAnalyzer（包括 VS Code/Cursor PowerShell 扩展所带模块），不自动下载。它先解析源码，保留字符串和非分隔 token，再格式化并复核；检查模式只报告差异。Python/CMake/shell的逻辑或语法结构变化时，人工复核布局并执行对应语法检查；不影响行为的注释/文档类修改按test-gates.md复用结果，不触发额外测试；涉及源码生成器时，还须验证生成结果的语义，不可只检查脚本能否解析。

QML或其实际类型/导入依赖变化时，静态诊断通过 Qt/CMake 自动生成的 `Yalahoo_qmllint`、`ui-audit_qmllint` 等对应 target 执行；不自行组装命令行或直接调用 qmllint 程序。QQmlEngine 运行时 warnings 与日志检查独立保留，不能替代官方 target。

上述适用门槛须有仍有效的通过结果才可宣告轮次完成或提交；不影响行为或格式结果的改动直接复用已有结果，无需专门重跑。禁止关闭检查、临时覆盖风格配置或添加压缩代码豁免。历史证据中的文件 hash 对应其原验收版本；本次格式整理不回写历史结果。
