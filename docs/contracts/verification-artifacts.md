# 验证产物与 Git 边界

Git 只保留源码、产品资产、真正的测试输入、必要的参数/来源契约和简明结论。采集截图、差分图、逐帧 JSON、场景导出、性能原始序列、日志、机器审计矩阵及产物 hash 清单属于本地生成输出，不提交。

`verification/`、`docs/media/`、`docs/contracts/web-scenes/` 及旧 Web 对应输出目录均由 `.gitignore` 排除。现有工具可以继续在这些路径生成和读取文件；保留证据指本地或单独归档留存，不代表放进源代码仓库。失败结果不能伪装成通过，简明结论应记录范围和未决项。

普通 Release 构建与单元测试使用提交的资源和 `tests/fixtures/`。Review 的成对图和完整 Web 状态需要先在本地重建：按 `web-baseline.md` 运行基础导出，再运行对应 `export-*-visual.mjs`、`export-render-backend.mjs` 与 `export-rhi-shell.mjs`。缺少输入时检查应失败，不允许用空目录或默认值伪造通过。全套重建是开发工具流程，原生程序运行不依赖 Node 或浏览器。

历史清理保留原始冻结来源标识。`history-rewrite.json` 将其映射到删除生成产物后的等价源码提交；Node 和 PowerShell 的 Git 读取通过 `frozen-git.mjs` / `ResolveFrozenRevision.ps1` 解析。原测试输入、数值 golden、资源来源 hash 不因清理而重写。旧提交的脚本若仍直接引用旧 SHA，需使用当前解析入口或清理前备份；不能把旧 SHA 当成当前仓库必然存在的对象。

R10D 的结构、功能、视觉、上传和资源验证已执行；其性能按用户要求只记录，曾观测到 20.98 ms 超过 20 ms 参考值，未建立实际性能达标结论。原始产物另行备份，R28 的预算与噪声任务保留。
