# Qt最佳实践

适用于Qt Core、Gui、Multimedia、并发、模型、平台与渲染边界；QML职责见[QML实践](qml-best-practices.md)。依据Qt在线官方文档，核对日期2026-10-17。下列编号是项目检查项，链接定位对应API或官方原则；Qt没有一份涵盖所有模块的统一“最佳实践标准”。实际API由[native-bootstrap.md](native-bootstrap.md)发现的最新原生Kit决定。

## 对象、所有权与信号

- **QT-01 QObject只有一个所有者。** 检查堆子对象、栈对象、值成员及父子析构顺序；父树和智能指针不得同时拥有同一对象。QQuickItem的visual parent与QObject parent是不同关系，不能从画面嵌套推断C++所有权。[对象树](https://doc.qt.io/qt-6/objecttrees.html)、[QQuickItem](https://doc.qt.io/qt-6/qquickitem.html)
- **QT-02 观察指针不承诺线程安全。** 可提前销毁QObject的观察者使用QPointer或等价销毁协议；固定依赖可用引用/裸指针。QPointer不会使跨线程解引用安全，检查后也不能假定任意回调后仍非空。[QPointer](https://doc.qt.io/qt-6/qpointer.html)
- **QT-03 连接具有合适context。** 优先成员指针connect；捕获lambda绑定context，单独核对其他捕获的有效期。断开连接不保证已排队完成消失；DirectConnection要明确实际执行线程，不能凭接收对象归属线程推断。[QObject连接](https://doc.qt.io/qt-6/qobject.html)
- **QT-04 信号可能同步重入。** 在发信号前建立一致状态；发出后重新检查可被槽改变的对象、请求和容器。不得保留快照元素裸指针跨越可能换库的通知；同步连接也不能让异常逃出槽调用。[QObject](https://doc.qt.io/qt-6/qobject.html)、[异常安全](https://doc.qt.io/qt-6/exceptionsafety.html)
- **QT-05 属性通知完整且克制。** NOTIFY覆盖所有派生属性依赖，普通标量仅在值改变时发出。CONSTANT确实不变；可写属性的setter校验输入并维护不变量。模型reset和属性变化分别通知，不用无条件全量刷新弥补依赖缺失。[属性系统](https://doc.qt.io/qt-6/properties.html)
- **QT-06 公开接口稳定。** C++暴露窄typed属性、命令和模型；生产逻辑不按objectName反向遍历QML。QML可创建注册类型须允许注册包装派生；测试观察实际树不成为生产依赖。[从C++与QML交互](https://doc.qt.io/qt-6/qtqml-cppintegration-interactqmlfromcpp.html)

## 线程、事件和异步退出

- **QT-07 所属线程执行对象操作。** GUI和模型在GUI线程，计时器/媒体worker在有事件循环的所属线程；QThread对象仍属于创建线程。moveToThread前核对子对象与parent；跨线程交接值，避免直接访问QObject。[线程与QObject](https://doc.qt.io/qt-6/threads-qobject.html)
- **QT-08 可重入不等于线程安全。** 不同实例可并行操作不代表同一实例可并发修改；隐式共享副本也不为共享的原对象加锁。第三方回调和渲染线程引用要有独立同步协议。[可重入与线程安全](https://doc.qt.io/qt-6/threads-reentrancy.html)
- **QT-09 退出协议保证销毁线程。** worker用finished到deleteLater或等价协议，任务依赖销毁前join/结束。核对线程未启动、早退、任务失败与应用退出；不能结束事件循环后才期望普通queued清理执行。[QThread](https://doc.qt.io/qt-6/qthread.html)
- **QT-10 取消能力按真实实现判断。** Future取消未必停止底层计算；worker主动检查取消，结果带身份/版本。QFutureWatcher在启动前接好完成信号；容量覆盖在途、排队和完成邮箱，不只线程池大小。[QFutureWatcher](https://doc.qt.io/qt-6/qfuturewatcher.html)、[Qt Concurrent](https://doc.qt.io/qt-6/qtconcurrent-index.html)
- **QT-11 不阻塞常规GUI事件。** 扫描、解码和媒体探测异步；不在交互路径waitForFinished/sleep/手动processEvents。BlockingQueuedConnection只用于经过无环推演的必要协议，接收事件循环必须活动；不得用于常规热路径。[QEventLoop](https://doc.qt.io/qt-6/qeventloop.html)
- **QT-12 时间与调度分开。** QTimer是唤醒机制，不保证准点；运动/超时依据单调经过时间。隐藏、取消和重建后检查旧timer回调代次；不以补发全部漏掉tick造成突发工作。[QTimer](https://doc.qt.io/qt-6/qtimer.html)、[QElapsedTimer](https://doc.qt.io/qt-6/qelapsedtimer.html)

## 模型、值与I/O

- **QT-13 模型索引属于当前模型。** rowCount处理有效parent，data处理非法/外来索引、列、行和role；角色名保持稳定。外部传入row只表示当前位置，业务操作使用稳定ID。[QAbstractItemModel](https://doc.qt.io/qt-6/qabstractitemmodel.html)
- **QT-14 模型变更遵守事务。** 插入/删除/移动使用匹配begin/end，变值发正确dataChanged角色；reset只用于确需整体替换。begin/end间不执行可重入未知回调，persistent index按协议维护；worker准备值，GUI提交。[模型子类协议](https://doc.qt.io/qt-6/model-view-programming.html)
- **QT-15 只读遍历不触发detach。** 只读容器使用const/cbegin；容器副本共享与元素引用有效期分别判断。非const迭代、扩容和隐式分离后不继续使用旧引用；大快照不逐帧复制/序列化。[隐式共享](https://doc.qt.io/qt-6/implicit-sharing.html)、[容器](https://doc.qt.io/qt-6/containers.html)
- **QT-16 URL与文件路径不混用。** URI用QUrl，原生路径用QFile/QDir/QFileInfo；保留Unicode、空格、#及百分号语义，file URL验证绝对性。不要把URL当路径拼接，或把任意远程URL交给本地扫描。[QUrl](https://doc.qt.io/qt-6/qurl.html)
- **QT-17 数据根和写入可靠。** 使用QStandardPaths，检查目录创建、读取大小、JSON类型、write长度及QSaveFile::commit。符号链接、源音乐只读、失败保留原文件的业务要求见[library-data.md](library-data.md)；不能用直接覆盖降级原子写入。[QStandardPaths](https://doc.qt.io/qt-6/qstandardpaths.html)、[QSaveFile](https://doc.qt.io/qt-6/qsavefile.html)
- **QT-18 编码边界显式。** 字符串规范唯一见[CPP-30](cpp23-best-practices.md)。动态字节按UTF-8解码，Base64/hex等ASCII编码的字节也可经UTF-8无损转换；二进制本体保留QByteArray。处理QString长度时区分UTF-16 code unit、code point与可见grapheme，不以截取code unit实现用户字符编辑。[QString](https://doc.qt.io/qt-6/qstring.html)、[QTextBoundaryFinder](https://doc.qt.io/qt-6/qtextboundaryfinder.html)

## 翻译和构建闸门

- **QT-19 可见文案进入Qt翻译系统。** C++使用`tr("English source")`或明确context的`QCoreApplication::translate`，QML使用`qsTr`；标识、JSON键、URL、原始音乐元数据和日志协议不翻译。Qt提取工具需要这些API的原始char字面量，它们是刻意的API边界，不改为QString后再toUtf8。完整句子用占位符，避免拼接导致译序不可调整。[可翻译源码](https://doc.qt.io/qt-6/i18n-source-translation.html)
- **QT-20 翻译覆盖动态数据来源。** 模型角色、错误、提示、占位符、Accessible名称、原生菜单与命令行说明同样是可见文案。复数使用Qt numerus，不用英文单复数字符串拼接；缓存翻译值在语言改变时更新，或明确仅启动时选择语言。
- **QT-21 翻译资源可提取、编译和加载。** TS保存其他语言，lupdate/lrelease采用同Kit LinguistTools；QM随产品资源部署。QTranslator在创建文案/模型/QML前安装且活到其后；按QLocale选择，缺少目标语言回退英文。不可把未加载翻译的英文界面当作中文资源验证通过。[QTranslator](https://doc.qt.io/qt-6/qtranslator.html)、[Linguist CMake函数](https://doc.qt.io/qt-6/qtlinguist-cmake-qt-add-translations.html)
- **QT-22 三个宏是常规构建要求。** 根CMake统一启用`QT_NO_CAST_FROM_ASCII`、`QT_NO_CAST_TO_ASCII`、`QT_NO_KEYWORDS`，覆盖生产、测试、Review和工具目标；不在单独测试时临时打开，不以局部undef豁免。Qt信号槽关键字用Q_SIGNALS/Q_SLOTS/Q_EMIT；UTF-8及typed字面量见CPP-30。[Qt字符串转换开关](https://doc.qt.io/qt-6/qstring.html)、[第三方信号槽兼容](https://doc.qt.io/qt-6/signalsandslots.html#using-qt-with-3rd-party-signals-and-slots)

## QML集成、场景图和平台

- **QT-23 QML所有权按返回路径判断。** 通常保留C++所有权，但显式C++方法返回QObject时存在QML接管规则；有parent的对象不由引擎删除。核对注册、getter、invokable、初始属性、动态创建和engine销毁；必要时显式CppOwnership。[QML/C++类型转换与所有权](https://doc.qt.io/qt-6/qtqml-cppintegration-data.html)
- **QT-24 渲染线程只处理渲染快照。** GUI经sync交接值；render不访问GUI对象、库或I/O。updatePaintNode在渲染线程操作QSG；sync时GUI阻塞不代表可任意保存GUI指针供以后用。[场景图](https://doc.qt.io/qt-6/qtquick-visualcanvas-scenegraph.html)
- **QT-25 GPU资源适配完整生命周期。** 检查初始化失败、resize/DPR、窗口迁移、SG失效、hide/restore和析构；资源在正确线程/上下文释放；未提交的QRhiResourceUpdateBatch使用release归还池，提交到命令缓冲后由Qt消费，不能delete。异步读回保留结果对象到完成，验证格式/尺寸/长度并在回调内处理失败。releaseResources按Qt协议调度，不用GUI析构直接删除render对象。[QQuickItem](https://doc.qt.io/qt-6/qquickitem.html)、[QQuickRhiItemRenderer](https://doc.qt.io/qt-6/qquickrhiitemrenderer.html)、[QRhi更新批次](https://doc.qt.io/qt-6/qrhiresourceupdatebatch.html)、[读回结果](https://doc.qt.io/qt-6/qrhireadbackresult.html)
- **QT-26 私有API隔离。** 私有Qt API只留在已有渲染/字体适配边界，用同Kit能力探针和目标构建核对；不把私有类型传播到业务或QML接口。不能把Qt公共API兼容承诺套用于私有ABI。[Qt版本与兼容性](https://doc.qt.io/qt-6/qt-releases.html)
- **QT-27 媒体状态按公开API处理。** available、loaded、seekable、error、EndOfMedia和设备变化不是同一事实；请求成功不代表已播放。歌曲/BGM/UI音效、退出与设备恢复规范唯一见[playback.md](playback.md)，不在此引入另一音频引擎。[QMediaPlayer](https://doc.qt.io/qt-6/qmediaplayer.html)
- **QT-28 部署验证真实依赖。** 检查模块/qrc/font/shader/plugin与相对运行路径，不能只在开发Kit中运行就宣称可部署。三平台源码、主机运行和目标实机结果分列；能力缺口不能写成通过。[Qt部署](https://doc.qt.io/qt-6/deployment.html)
- **QT-29 测试与诊断有适用范围。** 模型协议、线程退出、异步陈旧结果与I/O失败用针对性测试；QML warning、资源失败和后端诊断不得无条件过滤。QAbstractItemModelTester可检查模型协议，但不能证明数据含义或业务身份正确。[模型测试器](https://doc.qt.io/qt-6/qabstractitemmodeltester.html)

媒体、库、资源图、数值预算和测试门槛分别由[playback.md](playback.md)、[library-data.md](library-data.md)、[rhi-integration.md](rhi-integration.md)、[performance.md](performance.md)及[test-gates.md](test-gates.md)维护。此处审查通用机制，不重复专项数值或历史验收结果。
