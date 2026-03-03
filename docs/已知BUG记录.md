# 已知BUG记录

## 高优先级BUG

### BUG-001: 视频播放完成后切换下一个视频时崩溃

**状态**: 🔴 待修复（持续关注）

**描述**:
- 当视频播放完成后，自动切换到下一个视频时，应用程序会崩溃
- 崩溃发生在视频切换过程中，可能与线程清理和生命周期管理有关

**复现步骤**:
1. 播放一个视频文件
2. 等待视频播放完成
3. 系统自动切换到播放列表中的下一个视频
4. 应用程序崩溃

**相关日志**:
- `WZMediaPlay/logs/MediaPlayer_20260115214754.log:16391-16990` - 播放过程中的日志
- `WZMediaPlay/logs/MediaPlayer_20260115214754.log:18566-18581` - 播放完成时的日志

**日志关键信息**:
```
[2026-01-15 21:49:46.388][thread 2712][,][info] : VideoThread::run: Queue is empty and finished, playback completed
[2026-01-15 21:49:46.388][thread 2712][,][debug] : VideoThread::run: Flush got final frame, isEmpty: false, width: 1280, height: 720
```

**可能原因**:
1. 线程清理时序问题：`VideoThread` 或 `AudioThread` 在播放完成后退出，但 `PlayController` 仍在尝试访问这些线程
2. 状态机状态转换问题：播放完成后的状态转换可能导致不一致
3. 队列清理问题：`PacketQueue` 的 `finished` 状态设置后，线程可能仍在访问队列

**已尝试的修复**:
- ✅ 修复了线程生命周期管理（断开 `finished()` 信号连接）
- ✅ 添加了异常保护（`isRunning()` 调用）
- ⚠️ 问题仍然存在，需要进一步调查

**下一步计划**:
1. 添加更详细的日志，追踪视频切换时的线程状态
2. 检查 `MainWindow::handlePlaybackFinished()` 和 `playNextVideoInList()` 的实现
3. 验证状态机在播放完成后的状态转换是否正确
4. 检查 `PlayController::open()` 在视频切换时的线程清理逻辑

**测试建议**:
- 在视频切换时添加断点，检查线程状态
- 使用内存检测工具（如 Valgrind）检查内存访问问题
- 添加线程状态监控，记录切换时的线程状态

---

## 中优先级BUG

### BUG-002: 首次打开视频时不必要的等待

**状态**: ✅ 已修复

**描述**:
- 首次打开视频时，代码会等待一个不存在的"旧视频"停止
- 导致首次打开视频需要等待约2秒才能开始播放

**修复方案**:
- 修改 `MainWindow::openPath()`，只在 `playController_->isOpened()` 返回 true 时才等待停止
- 如果 `isOpened()` 返回 false，说明没有视频在播放，直接跳过等待

**修复文件**:
- `WZMediaPlay/MainWindow.cpp`

---

## 低优先级BUG

### BUG-003: Seeking 操作时的闪烁和卡顿

**状态**: ✅ 已优化（待验证）

**描述**:
- Seeking 操作时会出现画面闪烁
- Seeking 后会有短暂的卡顿
- Seeking多次后可能崩溃
- Seeking时"先跳回原点再跳到目标点"的问题

**已完成的修复**:
- ✅ 添加了 `lastRenderedFrame_` 机制，减少非关键帧跳过时的闪烁
- ✅ 优化队列清空时机，在`requestSeek()`时立即清空队列
- ✅ 实现KeyFrame直接跳转，使用`AVSEEK_FLAG_FRAME`避免"先跳回原点"的问题
- ✅ 优化缓冲管理，在seeking期间直接丢弃满队列的数据包
- ✅ 改进Seeking同步机制，在成功解码关键帧后立即清除`wasSeekingRecently_`标志

**待验证**:
- Seeking多次后的稳定性
- 直接跳转是否消除了"先跳回原点"的问题
- 缓冲管理优化是否解决了队列满的问题
- 画面闪烁和卡顿是否已改善

---

## 测试验证清单

在每次修复后，需要验证以下场景：

### 视频切换场景
- [ ] 播放完成后自动切换下一个视频
- [ ] 手动切换视频
- [ ] 快速连续切换多个视频
- [ ] 切换时进行 seeking 操作

### 首次打开场景
- [ ] 首次打开视频（无旧视频）
- [ ] 打开视频后立即关闭
- [ ] 打开视频后立即切换另一个视频

### Seeking 场景
- [ ] 播放过程中 seeking
- [ ] Seeking 后立即切换视频
- [ ] 多次连续 seeking

### 播放完成场景
- [ ] 正常播放完成
- [ ] 播放完成后自动切换
- [ ] 播放完成后手动停止
