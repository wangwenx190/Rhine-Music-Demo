# Qt 原生实现参考

来源：[MikeQu223/Rhine-Music-Database](https://github.com/MikeQu223/Rhine-Music-Database)。仅适配 `AudioPlayer::followDefaultOutput` / `restorePlaybackPosition` 的公开 Qt API 局部状态隔离思路及 PlatformRegression 的事件序列；本项目自行实现离线队列、source revision 与显式设备策略。没有复制私有设备补丁、在线规则或整套播放器架构。

实际读取文件 SHA256：

- `Qt/src/AudioPlayer.cpp`：`3d2ef544887394a7d0d90635e3d27a3755ae7f1e163f2c0d03bc201684b5710f`
- `Qt/tests/PlatformRegression.cpp`：`a5500f7979c4bbda50912d5382453699e1cb151941d699a00431f238bf4e44d3`
- `LICENSE`：`b42f104f4b3687773c76efbf990c1ab7b306a8c59035958c0b886dfbbcf2cd91`

MIT License

Copyright (c) 2026 LBEILC

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

异步结果隔离另参考同项目 `Qt/src/Host.cpp::acceptCookies` 与 `cover` 完成回调中的 epoch/订阅者检查思路；本项目沿既有 requestId/generation 自行实现测试与批量调度，没有复制 Host、账号、网络或封面队列代码。实际读取 `Qt/src/Host.cpp` SHA256：`b9bb74b1595e640531722d741f84f5730c5deaf5bbfcff0e458e4a890599a71f`。作者与 MIT 许可同上。

同项目`Qt/tests/UiChecks.cpp`的缓存命中/同封面换曲/仅更新采样绑定断言及静止、绘制后失效、隐藏恢复、缓存回选、重建保留歌曲等场景，仅用于本项目独立测试的意图参考；没有复制实现、测试代码或素材，也未采用其资源预算/架构。实际读取文件SHA256：`b7a3878617aab8125dd43b0978d9c3bc0b29ad2501c445b5b21092a3538e56af`。同项目`ItemRenderer::render`的遮挡早退思路用于序章提交门控，未复制其代码、时间阈值或调度器。作者与MIT许可同上。
