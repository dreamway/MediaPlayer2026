# WZMediaPlayer Bug 分析与修复计划

**日期**: 2026-03-03  
**作者**: Cursor Cloud Agent  
**基于**: docs/ 全部文档 + 源码分析

---

## 一、项目概况

WZMediaPlayer 是一个基于 C++/Qt6/FFmpeg/OpenGL/OpenAL 的 3D 立体视频播放器。经历了多轮重构和 Bug 修复，目前架构已较完善（VideoRenderer、AVClock、PlaybackStateMachine、ThreadSyncManager 等），但仍有一些关键 Bug 需要修复。

---

## 二、Bug 修复历史总结

### 已修复（约 12 个）
| 编号 | 描述 | 核心修复方案 |
|------|------|-------------|
| BUG-001 | 播放/切换崩溃 | PacketQueue resetting_ 标志 + setFinished() 唤醒 |
| BUG-002 | 音视频同步 | getMasterClock() 改用 AL_SAMPLE_OFFSET 实际播放位置 |
| BUG-003 | Seek 后无声 | Audio::clear() 添加 alSourceRewind() |
| BUG-004 | 进度条不同步 | 随 BUG-002 修复 + 动态 max 更新 |
| BUG-005 | 黑屏闪烁 | lastFrame_ 缓存机制 |
| BUG-013 | 首次打开等待 | isOpened() 判断 |
| BUG-014 | 列表自动下一首 | handlePlaybackFinished() |
| BUG-015 | 退出崩溃(logger) | LOG_MODE 判断 |
| BUG-016 | 播放列表切换崩溃 | stop() 等待增强 |
| BUG-017 | 退出崩溃(AdvWidget) | 指针初始化 + 析构顺序 |
| SEEK-001 | Seeking 花屏 | 关键帧跳过 + 直接 Seek |
| 线程同步系列 | Mutex/线程生命周期 | ThreadSyncManager、锁顺序统一 |

### 未修复（约 7 个）
| 编号 | 描述 | 优先级 |
|------|------|--------|
| BUG-006 | 硬件解码黑屏 | 中（已禁用作为 workaround） |
| BUG-007 | 视频色彩错误 | 中 |
| BUG-008 | FPS 过低 | 中（依赖 BUG-002 验证） |
| BUG-009 | 摄像头功能 | 低（后端 DLL 缺失） |
| BUG-010 | 3D 切换无效 | 中 |
| BUG-011 | MainLogo | 低 |
| BUG-012 | VS 项目文件 | 低 |

---

## 三、源码级 Bug 分析（本次发现的可修复问题）

### P0 - 严重（内存安全 / 崩溃风险）

#### 1. AudioThread::cleanupDecoder() 内存泄漏
**文件**: `AudioThread.cpp` ~L531  
**问题**: `decodedFrame_.release()`, `swrctx_.release()`, `codecctx_.release()` 调用了 `release()` 而不是 `reset()`。`release()` 放弃所有权但**不释放内存**，返回的裸指针被丢弃，导致内存泄漏。  
**修复**: 改用 `reset()` 释放资源。

#### 2. updateVideoClock(nanoseconds::min()) 溢出风险
**文件**: `VideoThread.cpp` ~L187, ~L254  
**问题**: Seeking 时调用 `controller_->updateVideoClock(nanoseconds::min())` 作为"重置"标记，但 `getVideoClock()` 中 `result += delta` 对 `nanoseconds::min()` 做算术运算会导致整数溢出（未定义行为）。  
**修复**: 使用 `nanoseconds(0)` 替代 `nanoseconds::min()` 作为重置值，或添加无效值判断。

#### 3. OpenAL 资源泄漏
**文件**: `OpenALAudio.cpp` ~L53, `AudioThread.cpp` ~L667  
**问题**: `Audio::open()` 不检查已有资源，重复打开视频时旧的 OpenAL source/buffer 未释放。  
**修复**: 在 `open()` 开头或 `initializeDecoder()` 中先调用 `close()`。

### P1 - 高优先级（功能异常）

#### 4. Seek 后音频不恢复
**文件**: `AudioThread.cpp` handleSeekingState(), `OpenALAudio.cpp` clear()  
**问题**: `audio_->play()` 在 Seeking 结束时可能队列为空而返回 false，之后 `writeAudio()` 填充缓冲区后不会自动重启播放（`playbackStarted_` 在 `clear()` 中被设为 false，但 `writeAudio` 中的自动启动条件 `queued >= 2` 可能竞争失败）。  
**修复**: 在 `writeAudio()` 中，当 `!playbackStarted_` 且有可用 buffer 时强制调用 `alSourcePlay()`。

#### 5. 静态变量 wasSeekingBefore 共享问题
**文件**: `VideoThread.cpp` ~L648  
**问题**: `static bool wasSeekingBefore` 在多次打开/关闭视频时状态残留。  
**修复**: 改为成员变量。

### P2 - 中优先级（同步/精度问题）

#### 6. AVClock::value() 中 audioPts_==0 歧义
**文件**: `AVClock.cpp` ~L86  
**问题**: `audioPts_ == 0.0` 既可能是"尚未收到 PTS"也可能是合法的 PTS=0，导致时钟行为不确定。  
**修复**: 添加 `hasPts_` 标志区分。

#### 7. videoBasePtsSet 在 Seek 后未重置
**文件**: `VideoThread.cpp` ~L447  
**问题**: `videoBasePtsSet` 只在 `run()` 开始时初始化为 false，Seek 后仍为 true。  
**修复**: 在 Seeking 处理中重置。

---

## 四、本次修复计划

优先修复 P0（内存安全）和 P1（功能异常）：

| 序号 | Bug | 文件 | 修复方案 |
|------|-----|------|---------|
| 1 | cleanupDecoder 内存泄漏 | AudioThread.cpp | `release()` → `reset()` |
| 2 | nanoseconds::min() 溢出 | VideoThread.cpp | 替换为 `nanoseconds(0)` + 添加有效性检查 |
| 3 | OpenAL 资源泄漏 | OpenALAudio.cpp | `open()` 前检查并 `close()` |
| 4 | Seek 后音频不恢复 | OpenALAudio.cpp | `writeAudio()` 增强自动播放逻辑 |
| 5 | 静态 wasSeekingBefore | VideoThread.cpp | 改为成员变量 |
| 6 | AVClock audioPts_==0 歧义 | AVClock.h/cpp | 添加 hasPts_ 标志 |
| 7 | videoBasePtsSet 未重置 | VideoThread.cpp | Seeking 中重置 |

---

## 五、反复出现的 Bug 模式

1. **线程同步**：锁顺序、跨线程 Mutex、条件变量无超时 — 已通过 ThreadSyncManager 大幅改善
2. **资源生命周期**：`release()` vs `reset()`、OpenAL 资源未关闭 — 需要 RAII 更彻底
3. **Seeking 状态管理**：多个标志位（wasSeekingRecently_、flushVideo_、videoBasePtsSet、wasSeekingBefore）分散管理，容易遗漏重置
4. **时钟连续性**：basePts_、initialValue_、nanoseconds::min() 哨兵值 — 需要统一的时钟重置语义
5. **音频恢复**：OpenAL 状态机（Stopped→Playing）的过渡条件不够健壮

---

## 六、长期架构建议

1. **统一 Seek 状态管理**：将所有 Seek 相关标志合并到 PlaybackStateMachine，提供 `enterSeeking()` / `exitSeeking()` 方法统一重置
2. **RAII 化 OpenAL**：将 Audio 的 source/buffer 用 RAII wrapper 管理，避免手动 close
3. **时钟子系统重构**：统一使用 `nanoseconds(0)` 和 `hasPts` 标志，消除哨兵值
4. **错误恢复增强**：ErrorRecoveryManager 已创建但未完全集成到所有线程
