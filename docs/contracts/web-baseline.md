# 冻结Web来源与输入

本文负责冻结来源映射、匿名输入与组件导出；外观优先级和禁止像素追分由迁移计划规定，测试与审计边界见[test-gates.md](test-gates.md)。

冻结来源为V0.4.1、`57f4f845c20cce646a15dbf4008c2a162aaaf4e2`，音乐入口为`src/main.ts → src/music-app.ts`。`resources/manifests/assets.json::baselineCommit`提供统一身份；`tools/migration/frozen-git.mjs`通过`history-rewrite.json`解析清理后的等价Git对象。`legacy-web/`仅作来源与锁定开发依赖，不编辑或运行其音乐服务。

## 来源核对

`node tools/migration/check-web-baseline.mjs`独立核对冻结源码、资产/字体/品牌及数值fixture的来源hash，不启动浏览器、不生成运行通过报告。`source-map.json`列出冻结模块hash、导入、音乐可达性与当前原生职责；实际逐项判定见[source-fidelity-review.md](source-fidelity-review.md)。代码移动时直接更新对应职责/目标路径。

`tests/fixtures/web-baseline/anonymous.json`提供viewport、DPR、主题、质量、稳定专辑ID、选择、时间和事件。空/单/奇偶/混合/480专辑库使用qrc横竖方封面、长中文/下伸英文/括号/emoji/组合字；roots为空，曲目无音频URL。`library-order.json`记录冻结`setMusicAlbums/orderMusicAlbums`的稳定ID与列顺序，跨平台输入不能改用本机排序结果。

## 组件导出

各组件契约列出`tools/migration/`下的专用导出入口；数学回归直接消费`tests/fixtures/`中的独立期望值。来源导出只在输入缺失或需要核对具体来源时使用，不作为普通构建/CTest前置步骤，也不能用当前生产实现生成自己的期望结果。

`frozen-source.mjs`统一TypeScript配置、显式依赖与来源hash。`browser-source.mjs`为确需DOM/字体框的组件重建同一生产markup/CSS与字体，校验锁定rolling-number，使用已有无界面Chrome/Edge/Chromium；浏览器路径可由`REVIEW_BROWSER`提供，不下载浏览器。会话只访问回环服务，隔离storage/profile、拦截外部请求并禁音；正常和失败路径均关闭连接、进程、服务与临时profile。独立结构、数值轨迹和自身原生输出质量分别验收。

需要锁定Web依赖时，在`legacy-web/`执行`npm ci --ignore-scripts --no-audit --no-fund --allow-remote=all`；该参数适用于npm 12的现有lockfile镜像地址，不更改依赖或锁文件。Node、TypeScript和浏览器仅为显式来源审计依赖，原生产品与长期测试不依赖它们。
