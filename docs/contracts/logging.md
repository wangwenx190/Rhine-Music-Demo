# 应用日志

`src/core/Logging.{h,cpp}`维护进程级日志设施。身份设置后、QGuiApplication之前创建LoggingSession，保留到所有Qt/媒体/渲染对象销毁之后；普通启动、演示和验证模式均输出日志。日志属于本地运行输出，不进入Git。

## 分类与事件

业务使用QLoggingCategory及qCInfo/qCWarning/qCCritical/qCDebug：`yalahoo.app`负责启动、退出、参数及QML创建；`yalahoo.render`负责后端请求、能力、实际设备及渲染失败；`yalahoo.library`负责异步库任务、代次、取消和存储；`yalahoo.media`负责歌曲加载、意图、媒体状态、输出设备及三路音频错误；`yalahoo.settings`负责偏好加载、保存和失败。类别默认Info及以上，详细调试通过Qt标准`QT_LOGGING_RULES`开启。

重要事件附带关联ID/代次、来源、设备、状态或错误原因；不在逐帧、进度或音量渐变回调中输出常规日志，不输出完整库、设置或歌曲字节。文件可包含本机路径、曲目身份和设备描述，仅保留本地。存储关闭时的最后提交失败也必须输出。

## 格式与输出

通过qSetMessagePattern设置时间/时区、级别、类别、PID/TID、消息及源码位置；Release显式保留消息上下文。消息回调只调用Qt的`qFormatLogMessage`得到最终文本，不自行构造日志格式。Qt标准`QT_MESSAGE_PATTERN`可覆盖默认格式。处理器串行保护控制台和文件的完整记录；每条记录刷出，Fatal记录在Qt终止前写入。

同一份UTF-8文本同时写stderr及文件。终端Warning使用VT黄色，Critical/Fatal使用VT红色，其余默认颜色；每条彩色记录恢复终端颜色。POSIX用isatty判定stderr；Windows核对真实输出handle、文件类型及GetConsoleMode，开启ENABLE_PROCESSED_OUTPUT/ENABLE_VIRTUAL_TERMINAL_PROCESSING，并显式设置输入/输出控制台编码页CP_UTF8。GUI进程仅在缺输出handle时尝试连接父控制台；保留继承的管道/文件。重定向不写颜色控制序列；无控制台时仍写文件。

验证诊断采集链到统一处理器，不替换双路输出；其最近消息缓存最多1024条。日志内部I/O错误直接通过qFormatLogMessage输出stderr，不递归调用Qt日志，不静默宣告文件写入成功。

## 文件与上界

目录固定为QStandardPaths::AppLocalDataLocation下的`logs/`，独立于歌曲库/偏好数据根。每次运行使用UTC时间、PID及UUID组成独立文件前缀，文件仅当前用户读写，以NewOnly创建；不跟随日志文件或日志目录自身的符号链接。

每段最多8 MiB，当前`.0.log`加`.1.log`至`.4.log`四份归档，每个运行最多40 MiB；轮转删除最旧段并依次重命名。超大单条记录按UTF-8字符边界跨段，仍遵守文件大小和归档上界。进程互不覆盖；锁文件保护仍在运行的会话。

保留最近14天：启动及后续写入时每小时检查一次，仅清理由本设施命名、最后修改时间已过期且所属进程未持锁的日志。其他文件和活跃会话不删。文件创建、轮转、写入或刷出失败时明确报告并停止本次文件输出，继续控制台输出；过期删除失败明确报告。

最小验证入口为Release `logging`、`logging-terminal`，覆盖多线程、Qt格式/分类、UTF-8、管道/文件无颜色、POSIX真实PTY颜色、轮转/超大记录、过期清理、活跃锁及文件失败。Windows控制台模式/代码页和三平台实际AppLocalData路径须在对应系统检查；主机测试不替代目标事实。
