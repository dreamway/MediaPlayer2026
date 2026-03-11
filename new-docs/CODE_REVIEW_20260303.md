# WZMediaPlayer 全量 Code Review 报告

**日期**: 2026-03-03  
**范围**: 11 个核心文件逐行审查  
**发现**: 6 CRITICAL / 15 HIGH / 14 MEDIUM / 8 LOW

---

## 本次修复的问题

### CRITICAL（已修复）

| ID | 问题 | 文件 | 修复 |
|----|------|------|------|
| C1 | `enterSeeking()/exitSeeking()` 未被 PlayController 使用，Paused→Seek→Playing 状态恢复错误 | PlayController.cpp | `seek()` 改用 `enterSeeking()`，所有 seek 完成/失败路径改用 `exitSeeking()` |
| C2 | `getClockNoLock()` 无锁读取 `currentPts_`/`source_`/`sampleRate_` — data race | OpenALAudio.cpp | `getClock()` 改为持锁调用 `getClockNoLock()` |
| C3 | `handleSeekingState` 中 `dec_` 为 null 时直接调用 `decodeVideo()` — 空指针崩溃 | VideoThread.cpp | 添加 null check |

### HIGH（已修复）

| ID | 问题 | 文件 | 修复 |
|----|------|------|------|
| H1 | `writeAudio` 在 `state != AL_PLAYING` 时设 `playbackStarted_=true` — Seek 后音频静默 | OpenALAudio.cpp | 只在 `AL_PLAYING` 时设置，否则 warn 并让下次重试 |
| H2 | `open()` alGenBuffers 成功但 alGenSources 失败时 buffer 未释放 | OpenALAudio.cpp | 失败路径 `alDeleteBuffers` |
| H3 | `handleSeek()` 重复 Reset 队列（PlayController 已 Reset） | DemuxerThread.cpp | 移除重复 Reset |
| H4 | `run()` 循环只检查 `isStopped()` 不检查 `isStopping()` | DemuxerThread.cpp | 条件加 `!isStopping()` |
| H5 | `seek_target == 0` 被拒绝 — 无法 Seek 到视频开头 | DemuxerThread.cpp | 改为只拒绝 `< 0` |
| H7 | `clear()` 不重置 `br_` — Seek 后 writeAudio 提前返回 | OpenALAudio.cpp | 添加 `br_=false` |

### MEDIUM（已修复）

| ID | 问题 | 修复 |
|----|------|------|
| M1 | `nanoseconds::min()` 在 Frame.cpp, FrameBuffer.cpp, VideoThread.h, AudioThread.h 中仍未替换 | 全部替换为 `kInvalidTimestamp` |

---

## 基础设施评估

### PlaybackStateMachine — ✅ 现在正确使用
- `enterSeeking()`/`exitSeeking()` 现在由 PlayController 统一调用
- `preSeekState_` 正确保存/恢复 Seek 前状态

### ErrorRecoveryManager — ⚠️ 部分集成
- VideoThread、AudioThread、DemuxerThread 都有实例
- Recovery Action 未完全实现（FlushAndRetry/RestartThread 只是跳过）
- 建议：后续实现 FlushAndRetry 的真正 flush 逻辑

### SystemMonitor — ❌ 未集成
- 代码存在但从未被实例化
- 建议：作为可选调试功能，在 PlayController 中条件创建

### ThreadSyncManager — ⚠️ 使用不一致
- 只用于 `videoClockMutex_` 和 `seekMutex_`
- 大部分互斥锁使用 `std::lock_guard`
- 建议：统一使用方式，或简化为纯 RAII 模式

### crash_handler — ✅ 正确
- Windows 端完整，Linux 端 `#ifdef` 保护
- 建议：后续可添加 Linux signal handler

### AVClock — ✅ 正确
- `hasAudioPts_` 标志消除了 `audioPts_==0` 歧义
- `reset()` 正确清除所有状态

---

## 此前报告的 Bug 验证状态（通过代码审查）

| Bug | 之前状态 | Code Review 结论 |
|-----|---------|-----------------|
| Seeking 崩溃 | 已标记修复 | ✅ PacketQueue resetting_ 逻辑正确 |
| Seek 后无声音 | 已标记修复 | ⚠️ `clear()` 中 `alSourceRewind` 正确，但 `playbackStarted_` 在非 PLAYING 时也被设 true **→ 本次修复** |
| 进度条不同步 | 已标记修复 | ⚠️ `enterSeeking/exitSeeking` 未使用导致 Paused 状态丢失 **→ 本次修复** |
| 视频切换崩溃 | 已标记修复 | ✅ setFinished() 唤醒 + 线程等待循环正确 |
| Seeking 多次跳转 | 已标记修复 | ⚠️ seek_target==0 被拒绝导致 seek 到开头失败 **→ 本次修复** |
| 音视频不同步 | 已标记修复 | ⚠️ `getClockNoLock()` data race 可能导致时钟跳跃 **→ 本次修复** |
| 硬件解码黑屏 | 未修复 | ❌ 需要 FFmpeg 版本升级或驱动适配，当前禁用作为 workaround |

---

## 待后续改进的问题

1. **静态局部变量**：VideoThread/DemuxerThread 中仍有 `static` 计数器
2. **ErrorRecoveryManager Action 执行**：FlushAndRetry 需要真正 flush 解码器
3. **SystemMonitor 集成**：添加到 PlayController 的调试模式
4. **线程健康时间戳**：`getLastVideoFrameTime()` 返回引用无锁保护 — 需改为 atomic
