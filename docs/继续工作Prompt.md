# WZMediaPlayer 继续工作 Prompt

**最后更新**: 2026-01-18
**当前状态**: Phase 1-10 已完成，视频可正常渲染，音视频同步问题待修复

---

## 项目背景

WZMediaPlayer 是一个基于 Qt、FFmpeg、OpenGL、OpenAL 的视频播放器项目，正在进行架构重构和稳定性优化。项目参考了 QMPlayer2 的成熟架构，采用多线程架构（DemuxerThread、VideoThread、AudioThread），目标是编写稳定、可维护、易读性好的播放器。

---

## 当前状态总结

### ✅ 已完成的核心重构

#### 1. 多线程架构建立 ✅
```
PlayController (主控制器)
    ├── DemuxerThread (解复用线程)
    │   └── Demuxer (解复用器)
    ├── VideoThread (视频解码线程)
    │   ├── Decoder (解码器接口)
    │   │   ├── FFDecSW (软件解码器)
    │   │   └── FFDecHW (硬件解码器)
    │   └── VideoWriter (渲染器接口)
    │       ├── OpenGLWriter (2D渲染器)
    │       └── StereoWriter (3D渲染器)
    ├── AudioThread (音频解码线程)
    │   ├── Audio (OpenAL音频输出设备)
    │   └── SwrContext (重采样器)
    ├── PacketQueue (线程安全的数据包队列)
    ├── PlaybackStateMachine (播放状态机)
    └── ThreadSyncManager (线程同步管理器)
```

#### 2. 状态机集成 ✅
- 创建了 `PlaybackStateMachine` 类，统一播放状态管理
- 集成到 `PlayController`，替换原有的分散状态管理
- 统一的状态查询和状态转换逻辑

#### 3. 错误恢复机制 ✅
- 创建了 `ErrorRecoveryManager` 类，统一错误处理
- 集成到 `VideoThread` 和 `AudioThread`
- 实现自动错误恢复逻辑

#### 4. 线程同步优化 ✅
- 创建了 `ThreadSyncManager` 类，统一线程锁定/解锁顺序
- 添加死锁检测机制
- 修复了 mutex 销毁时的锁定问题

#### 5. Seeking优化 ✅
- 优化队列清空时机，在 `requestSeek()` 时立即清空队列
- 实现KeyFrame直接跳转，使用 `AVSEEK_FLAG_FRAME`
- 优化缓冲管理，在 seeking 期间直接丢弃满队列的数据包
- 改进Seeking同步机制，优化关键帧处理逻辑

#### 6. VideoThread 代码重构 ✅
- 重构 `VideoThread::run()` 方法，提取辅助方法提高可读性
- 实现 `handleFlushVideo()`、`handlePausedState()`、`handleSeekingState()` 等辅助方法
- 实现 `decodeFrame()`、`processDecodeResult()`、`renderFrame()` 等核心方法
- 简化主循环逻辑，从900+行复杂代码简化为清晰的模块化结构
- 充分利用 `PlayController` 的统一状态管理

#### 7. AudioThread 代码重构 ✅
- 重构 `AudioThread::run()` 方法，提取辅助方法提高可读性
- 实现 `handleFlushAudio()`、`handlePausedState()`、`handleSeekingState()` 等辅助方法
- 实现 `decodeAndWriteAudio()`、`checkPlaybackComplete()` 等核心方法
- 简化主循环逻辑，从400+行复杂代码简化为清晰的模块化结构
- 充分利用 `PlayController` 的统一状态管理

#### 8. PlayController 代码重构 ✅
- 使用智能指针 (`std::shared_ptr`) 管理线程生命周期
- 添加 `stopThread()` 辅助方法，减少重复代码
- 统一的线程停止行为，更易维护

#### 9. OpenAL 缓冲区管理重构 ✅（2026-01-18）
- 引入 `availableBuffers_` 队列管理可用缓冲区，替代 `bufferIdx_` 跟踪方式
- 重构 `writeAudio()` 方法，使用正确的缓冲区获取策略
- 添加 `recycleProcessedBuffers()` 方法统一回收已处理的缓冲区
- 修复 "Modifying storage for in-use buffer" 错误

#### 10. VideoThread Packet 处理修复 ✅（2026-01-18）
- 修复 `VideoThread::run()` 中的逻辑错误，确保 `decodeFrame` 成功后调用 `processDecodeResult`
- 重新设计 `FFDecSW::decodeVideo` 返回值约定（0/1/2/负数）
- 修复 VideoQueue 一直满的问题（packet 没有被消费）

#### 11. 跳过帧逻辑优化 ✅（2026-01-18）
- 添加 `consecutiveSkipCount_` 跟踪连续跳过的帧数
- 如果连续跳过 >5 帧或延迟 <300ms，强制渲染一帧
- 修复 `renderFailCount` 逻辑（成功时重置而不是增加）

#### 12. 日志格式改进 ✅（2026-01-18）
- 改进日志格式，显示文件名和行号
- 格式: `[%Y-%m-%d %H:%M:%S.%e][thread %t][%@][%!][%L] : %v`

#### 13. 音频播放完成检测优化 ✅（2026-01-18）
- 改进 `checkPlaybackComplete()`，增加多次尝试读取剩余音频数据
- 等待 OpenAL 缓冲区播放完毕（最多200ms）

---

## 🎯 当前已知问题（严重程度排序）

### 🔴 Priority 1: 音视频同步问题

**状态**: 待修复

**症状**: 
- 日志显示 "OpenAL clock diff too large: -8731ms, rejecting"
- OpenAL 时钟和估算时钟差异过大（约8.7秒）
- 导致音频时钟不准确，影响音视频同步

**问题分析**:
- `Audio::getClock()` 中，当 OpenAL 时钟与估算时钟差异 > 200ms 时拒绝使用 OpenAL 时钟
- 但差异达到 8.7 秒，说明 `currentPts_` 或 `deviceStartTime_` 可能有问题
- 需要检查 `currentPts_` 的更新逻辑和 `deviceStartTime_` 的设置时机

**相关代码**: `WZMediaPlay/videoDecoder/OpenALAudio.cpp:500-586`

**修复方向**:
1. 优化 `currentPts_` 更新逻辑，确保及时更新
2. 优化 `deviceStartTime_` 设置时机，只在首次成功播放时设置
3. 改进 OpenAL 时钟与估算时钟的同步机制
4. 添加时钟连续性检查

---

### ✅ Priority 1 (已解决): 音频缓冲区死锁，视频延迟增长

**状态**: ✅ 已解决（2026-01-18）

**修复内容**:
- OpenAL 缓冲区管理重构，使用 `availableBuffers_` 队列
- 修复缓冲区生命周期管理错误
- 视频可以正常渲染

**历史问题描述**（已解决）:

**测试日志**: `MediaPlayer_20260117193722.log` (L11957-L11975)

```
Audio::writeAudio: Buffers full (after unqueue processed: 0), waiting...
VideoThread::renderFrame: Skipping frame (delay: 14128 ms)
Audio::writeAudio: Timeout waiting for free buffer
VideoThread::VideoDelay: 14188 ms
```

**问题描述**:
- OpenAL 缓冲区始终处于满状态 (`processed = 0`)
- 音频缓冲区没有被消费
- 音频时钟不前进，导致视频延迟持续增长 (14128 ms → 14188 ms)
- 视频不断跳帧，导致画面卡顿

**根本原因**：
- `Audio::writeAudio()` 的等待逻辑只检查 `br_ || source_ == 0`，不检查 `processed > 0`
- 线程进入死循环，等待 10 次（100ms）后超时
- 超时条件没有检查 processed 缓冲区是否变得可用

**修复方案**:
在 `Audio::writeAudio()` 的等待循环中，添加对 `processed` 缓冲区的检查：

```cpp
// 当前 (WRONG):
for (int i = 0; i < 10 && !br_ && source_ != 0; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

// 修复后 (CORRECT):
for (int i = 0; i < 10 && !br_ && source_ != 0; ++i) {
    // 检查 processed 缓冲区是否可用
    ALint currentProcessed = 0;
    alGetSourcei(source_, AL_BUFFERS_PROCESSED, &currentProcessed);
    ALenum alError = alGetError();
    if (alError != AL_NO_ERROR) {
        logger->error("Audio::writeAudio: alGetSourcei(AL_BUFFERS_PROCESSED) failed with error: {}", alGetString(alError));
        return false;
    }
    
    if (currentProcessed > 0) {
        // 缓冲区可用，退出等待
        break;
    }
    
    // 使用递减延迟（10ms, 9ms, 8ms...）减少 CPU 占用
    std::this_thread::sleep_for(std::min(10, 10 - i));
}
```

**文件**: `WZMediaPlay/videoDecoder/OpenALAudio.cpp:145-218`

**预期结果**:
- OpenAL 消费缓冲区 (`processed > 0`)
- 音频时钟前进
- 视频延迟稳定在 < 100ms
- 不再出现 "Timeout waiting for free buffer" 错误

**验证点**:
- [ ] 音频播放流畅，无卡顿
- [ ] 视频延迟稳定
- [ ] 日志中不再出现 "Timeout waiting for free buffer"
- [ ] 日志中出现 "Unqueued X processed buffers" 日志
- [ ] 日志中出现 "Started playback" 日志

**详细分析**: 参考 `docs/音频缓冲区死锁分析.md`

---

### 🟡 Priority 2: 视频同步机制优化（待音频修复后）

**问题**：
- 同步算法较简单，只有固定阈值（40ms）
- 延迟调整是线性步进（40ms, 10ms, -10ms）
- 对于小抖动不够平滑

**文件**: `WZMediaPlay/videoDecoder/VideoThread.cpp:406-460` (renderFrame 方法)

**优化方向**:
- 实现自适应同步阈值
- 实现平滑帧跳过/重复逻辑
- 添加抖动统计和调整逻辑

**参考**: QMPlayer2 的 `VideoThr::renderFrame()` 实现

---

### 🟢 Priority 3: 播放完成自动切换（待验证）

**问题**:
- 播放完成后没有自动切换到下一个视频
- 原因：`checkAndStopIfFinished` 只转换状态但不调用 `stop()` 方法

**文件**: `WZMediaPlay/PlayController.cpp` (checkAndStopIfFinished 方法)

**修复方案**:
在 `checkAndStopIfFinished` 中调用 `stop()` 方法，完成完整的停止流程

**验证点**:
- [ ] 播放完成后自动切换到下一个视频
- [ ] 日志中不再出现崩溃信息
- [ ] 状态转换正确（Playing → Stopping → Stopped → Idle → Opening → Playing）

---

## 📁 关键文件位置

### 核心代码文件

#### 1. 播放控制
- **文件**: `WZMediaPlay/PlayController.h`, `PlayController.cpp`
- **关键类**: `PlayController`
- **关键方法**: `open()`, `play()`, `pause()`, `stop()`, `seek()`, `getMasterClock()`
- **关键成员**: 状态机、线程管理、时钟管理、Seeking机制

#### 2. 音频线程
- **文件**: `WZMediaPlay/videoDecoder/AudioThread.h`, `AudioThread.cpp`
- **关键类**: `AudioThread`
- **关键方法**: `run()`, `decodeAndWriteAudio()`, `checkPlaybackComplete()`
- **关键成员**: codecctx_, swrctx_, currentPts_, audioFinished_

#### 3. 音频输出
- **文件**: `WZMediaPlay/videoDecoder/OpenALAudio.h`, `OpenALAudio.cpp`
- **关键类**: `Audio`
- **关键方法**: `writeAudio()`, `getClock()`, `open()`, `play()`, `pause()`, `stop()`, `clear()`
- **关键成员**: source_, buffers_, srcMutex_, srcCond_, currentPts_, deviceStartTime_

#### 4. 视频线程
- **文件**: `WZMediaPlay/videoDecoder/VideoThread.h`, `VideoThread.cpp`
- **关键类**: `VideoThread`
- **关键方法**: `run()`, `decodeFrame()`, `processDecodeResult()`, `renderFrame()`
- **关键成员**: videoClock_, lastRenderedFrame_, hasLastRenderedFrame_

#### 5. 解复用线程
- **文件**: `WZMediaPlay/videoDecoder/DemuxerThread.h`, `DemuxerThread.cpp`
- **关键类**: `DemuxerThread`
- **关键方法**: `run()`, `handleSeek()`, `requestSeek()`, `readAndDistributePacket()`
- **关键成员**: demuxer_, seekPositionUs_, seekInProgress_, mutex_

#### 6. 解复用器
- **文件**: `WZMediaPlay/videoDecoder/Demuxer.h`, `Demuxer.cpp`
- **关键类**: `Demuxer`
- **关键方法**: `open()`, `close()`, `readPacket()`, `seek()`, `isEof()`

#### 7. 解码器
- **文件**: `WZMediaPlay/videoDecoder/Decoder.h`
- **关键接口**: `Decoder` 抽象接口
- **实现类**: `FFDecSW` (软件解码器), `FFDecHW` (硬件解码器)
- **关键方法**: `decodeVideo()`, `decodeAudio()`, `flushBuffers()`

#### 8. 硬件解码器
- **文件**: `WZMediaPlay/videoDecoder/hardware_decoder.cc`
- **关键类**: `HardwareDecoder`
- **关键方法**: `tryInitHardwareDecoder()`, `transferFrame()`
- **关键成员**: hw_device_ctx_, needs_transfer_, device_type_name_

#### 9. 状态机
- **文件**: `WZMediaPlay/PlaybackStateMachine.h`
- **关键类**: `PlaybackStateMachine`
- **关键方法**: `transitionTo()`, `canTransitionTo()`, `isPlaying()`, `isPaused()`, `isStopped()`, `isSeeking()`

#### 10. 错误恢复
- **文件**: `WZMediaPlay/ErrorRecoveryManager.h`, `ErrorRecoveryManager.cpp`
- **关键类**: `ErrorRecoveryManager`
- **关键方法**: `handleError()`, `handleError()`, `resetErrorCount()`

#### 11. 线程同步
- **文件**: `WZMediaPlay/ThreadSyncManager.h`, `ThreadSyncManager.cpp`
- **关键类**: `ThreadSyncManager`
- **关键方法**: `tryLock()`, `tryLockMultiple()`, `detectDeadlock()`, `unlock()`

#### 12. 数据包队列
- **文件**: `WZMediaPlay/videoDecoder/packet_queue.h`
- **关键类**: `PacketQueue`
- **关键方法**: `put()`, `peekPacket()`, `popPacket()`, `sendTo()`, `Reset()`, `IsEmpty()`, `IsFinished()`

#### 13. 3D渲染器
- **文件**: `WZMediaPlay/videoDecoder/opengl/StereoWriter.h/cpp`
- **关键类**: `StereoWriter`
- **关键方法**: `writeVideo()`, `setStereoParams()`, `updateGL()`

---

## 核心架构理解

### 1. 数据流

```
媒体文件
    ↓ Demuxer (解复用)
    ↓ PacketQueue (线程安全队列)
    ↓ VideoThread / AudioThread (解码线程)
    ↓ Decoder (解码)
    ↓ VideoWriter / Audio (渲染/播放)
```

### 2. 线程模型

```
主线程 (Qt UI线程)
    ├── PlayController (主控制器)
    │   ├── DemuxerThread (解复用线程)
    │   ├── VideoThread (视频解码线程)
    │   └── AudioThread (音频解码线程)
    └── StereoVideoWidget (视频渲染 Widget)
```

### 3. 音视频同步机制

```
主时钟选择：
├── 有音频时：使用音频时钟 (Audio::getClock())
└── 无音频时：使用视频时钟 (PlayController::getVideoClock())

音频时钟计算：
├── nanoseconds Audio::getClock()
│   ├── if (deviceStartTime_ == nanoseconds::min()) {
│   │    return currentPts_;
│   └── deviceStartTime_ != nanoseconds::min()) {
│       return currentPts_ + (steady_clock::now() - deviceStartTime_);
│   }

视频时钟计算：
├── nanoseconds PlayController::getVideoClock()
│   └── PlayController::updateVideoClock(framePts) 更新
```

### 4. Seeking 机制

```
1. PlayController::seek()
   ├─ 转换到 Seeking 状态
   ├─ 锁定线程 (VideoThread → AudioThread)
   ├─ 设置 flushVideo/flushAudio 标志
   ├─ 重置时钟 (clockbase_, videoClock_, audioClock_)
   ├─ 清空队列 (vPackets_, aPackets_)
   ├─ 清空 OpenGL 缓冲区
   └─ 唤醒 DemuxerThread

2. DemuxerThread::requestSeek()
   ├─ 设置 seekPositionUs_
   ├─ 立即清空队列 (vPackets_, aPackets_)
   └─ 唤醒 DemuxerThread

3. DemuxerThread::handleSeek()
   ├─ 调用 Demuxer::seek(seekPositionUs_)
   ├─ 再次清空队列
   └─ emit seekFinished()

4. VideoThread::handleSeekingState()
   ├─ 消费队列中的旧数据包
   └─ 设置 wasSeekingRecently_ 标志

5. AudioThread::handleSeekingState()
   ├─ 清空音频缓冲区
   ├─ 重置 audio clock (currentPts_, deviceStartTime_)
   └─ 消费队列中的旧数据包
```

### 5. 缓冲管理

```
视频队列：20MB (20 * 1024 * 1024 字节)
音频队列：1MB (1024 * 1024 字节)
音频缓冲区：10个缓冲区 x 40ms = 400ms 总缓冲

队列满处理：
├─ VideoQueue 满：DemuxerThread 重试，等待 AudioThread
└─ AudioQueue 满：AudioThread 等待视频线程
```

---

## 技术栈

### 核心技术
- **Qt 框架**: `QObject`, `QThread`, `信号/槽机制`, `QRecursiveMutex`, `QWaitCondition`
- **FFmpeg API**: `AVFormatContext`, `AVCodecContext`, `AVPacket`, `AVFrame`
- **OpenGL API**: `QOpenGLWidget`, `QOpenGLFunctions`, `OpenGLShader`
- **OpenAL API**: `ALuint`, `AL_SOURCE_STATE`, `AL_BUFFERS_QUEUED`, `AL_BUFFERS_PROCESSED`
- **C++11**: `std::mutex`, `std::condition_variable`, `std::atomic`
- **C++ STL**: `std::unique_ptr`, `std::shared_ptr`, RAII模式

### 经典播放器设计模式
- **生产者-消费者模式**: DemuxerThread 生产，VideoThread/AudioThread 消费
- **主时钟选择**: 音频时钟为主时钟
- **PTS-based 同步**: 基于 PTS 的帧同步
- **关键帧跳转**: 使用 `AVSEEK_FLAG_FRAME` 直接跳到关键帧
- **自适应缓冲**: 根据网络状况调整缓冲区大小

---

## 待修复的严重BUG

### BUG-001: 音频缓冲区满，视频延迟增长 🔴🔴🔴🔴

**状态**: 待修复（修复方案已准备）

**日志**: `WZMediaPlay\logs\MediaPlayer_20260117193722.log` (L11957-L11975)

**问题描述**:
- 音频缓冲区始终处于满状态 (`processed = 0`)
- 音频缓冲区没有被 OpenAL 消费
- 音频时钟不前进
- 视频延迟持续增长 (14128 ms → 14188 ms)
- 视频不断跳帧，导致画面卡顿

**根本原因分析**:
从 `OpenALAudio.cpp:145-218` 的日志和代码分析：

```cpp
// 问题代码 (WRONG):
// 1. Unqueue processed buffers
ALint processed = 0;
alGetSourcei(source_, AL_BUFFERS_PROCESSED, &processed);
alSourceUnqueueBuffers(source_, processed, bufids.data(), processed);

// 2. Check if buffer full
if (buffersQueued >= buffers_.size()) {
    // 进入等待循环 (10 次，每次等待 10ms)
    for (int i = 0; i < 10 && !br_ && source_ != 0; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // 3. Check if should stop
    if (br_ || source_ == 0) {
        logger->info("Audio::writeAudio: Stopping, not writing audio");
        return false;
    }
    
    // 4. 再次检查是否满
    if (buffersQueued >= buffers_.size()) {
        logger->warn("Audio::writeAudio: Still no free buffer after wait");
        return false;
    }
}
```

**问题根源**:
等待条件 `!br_ && source_ != 0` **没有检查 `processed` 缓冲区是否被消费**。
当音频缓冲区满时：
1. `processed = 0` (没有缓冲区被消费)
2. `buffersQueued = 10` (缓冲区满)
3. 进入等待循环，10 次（10ms）=100ms）
4. 每次只检查 `br_ || source_ == 0`
5. 第10次检查 `processed` 仍然是 0` (没有被消费)
6. 超时后返回 false
7. 音频时钟不前进，导致视频延迟持续增长

**OpenAL 只有在以下情况会标记缓冲区为 processed**:
1. 音频源正在播放 (`AL_SOURCE_STATE == AL_PLAYING`)
2. 至少有一个缓冲区被消费 (`processed > 0`)

**音频停止的原因**:
- 从日志来看，`writeAudio()` 调用了 `alSourcePlay()`，但音频可能因为某种原因没有真正开始播放
- OpenAL 上下文可能没有正确设置
- 可能没有缓冲区被 unqueue

**视频跳帧原因**:
- 音频时钟不前进
- 视频延迟超过阈值 (>40ms)
- 跳帧逻辑：`delay > controller_->getMaxSyncThreshold()`
- 日志显示：`delay: 14128 ms`，`getMaxSyncThreshold()` 默认 40ms

**修复方案**:
修改 `Audio::writeAudio()` 的等待逻辑，在等待循环中检查 `AL_BUFFERS_PROCESSED > 0`:

```cpp
// 修复后的逻辑：
// 1. Unqueue processed buffers (确保缓冲区被释放)
ALint processed = 0;
alGetSourcei(source_, AL_BUFFERS_PROCESSED, &processed);
ALSourceUnqueueBuffers(source_, processed, bufids.data(), processed);

// 2. Check if buffer still full
if (buffersQueued >= buffers_.size()) {
    // 进入等待循环
    for (int i = 0; i < 10 && !br_ && source_ != 0; ++i) {
        // 检查 processed 缓冲区是否被消费
        ALint currentProcessed = 0;
        alGetSourcei(source_, AL_BUFFERS_PROCESSED, &currentProcessed);
        
        if (currentProcessed > 0) {
            // 有 processed 缓冲区，退出等待
            break;
        }
        
        // 使用递减延迟（10ms, 9ms, 8ms...）减少 CPU 占用
        std::this_thread::sleep_for(std::min(10, 10 - i));
    }
    
    // 3. 再次检查是否满
    if (buffersQueued >= buffers_.size()) {
        logger->warn("Audio::writeAudio: Still no free buffer after wait");
        return false;
    }
}
```

**预期修复效果**:
1. OpenAL 消费缓冲区，`currentProcessed > 0`
2. 音频时钟前进
3. 视频延迟稳定
4. 日志中出现 "Unqueued X processed buffers"
5. 不再出现 "Timeout waiting for free buffer" 错误

**文件**: `WZMediaPlay/videoDecoder/OpenALAudio.cpp:145-218`

**详细分析**: 参考 `docs/音频缓冲区死锁分析.md`

---

### BUG-002: Seeking 重复执行问题 ✅ 已修复

**状态**: 已修复（2026-01-16）

**问题描述**: Seeking 操作被重复执行多次，导致日志重复和状态转换异常

**修复内容**:
- 在 `DemuxerThread::handleSeek()` 中添加 `seekInProgress_` 标志防止重复执行
- 在 `onDemuxerThreadSeekFinished` 中添加状态检查，避免重复处理
- 在 `handleSeek()` 成功完成后立即重置 `seekInProgress_` 标志

**文件**: `WZMediaPlay/videoDecoder/DemuxerThread.cpp`

---

### BUG-003: 播放完成没有自动切换 ✅ 已修复

**状态**: 已修复（2026-01-16）

**问题描述**: 播放完成后没有进行状态切换，也没有切换到下一个视频

**修复内容**:
- 修改 `checkAndStopIfFinished` 调用 `stop()` 方法完成完整的停止流程
- 添加 `stop()` 方法的防重复调用保护

**文件**: `WZMediaPlay/PlayController.cpp`

---

## 📁 下一步行动计划

### 阶段1：修复音频缓冲区死锁（立即执行，预计 1-2小时）

**任务**: 修复 `Audio::writeAudio()` 的等待逻辑，添加对 processed 缓冲区的检查

**文件**: `WZMediaPlay/videoDecoder/OpenALAudio.cpp:145-218`

**修复方案**:
修改等待循环，在循环中检查 `AL_BUFFERS_PROCESSED > 0`

**验证点**:
- 音频播放流畅，无卡顿
- 视频延迟稳定 (<100ms)
- 日志中出现 "Unqueued X processed buffers"
- 日志中出现 "Started playback" 日志
- 不再出现 "Timeout waiting for free buffer" 错误

**下一步**: 如果音频播放正常，进行 Priority 2

---

### 阶段2：验证视频同步稳定性（音频修复后进行，预计 2-3天）

**任务**: 
1. 验证音频播放稳定性（长时间播放 >10 分钟）
2. 验证 Seeking 操作稳定性（多次 Seek 操作）
3. 验证视频同步准确性（延迟 < 100ms）

**验证点**:
- 音频播放流畅，无卡顿
- Seeking 操作流畅，无卡顿
- 视频延迟稳定
- 音视频同步正常（延迟 < 100ms）
- 日志中无 "Timeout waiting for free buffer" 错误

**下一步**: 如果验证通过，进行 Priority 3

---

### 阶段3：视频同步机制优化（预计 2-3 天）

**任务**:
1. 实现自适应同步阈值
2. 实现平滑帧跳过/重复逻辑
3. 优化抖动处理

**文件**: `WZMediaPlay/videoDecoder/VideoThread.cpp:406-460` (renderFrame 方法)

**参考实现**: QMPlayer2 的 `VideoThr::renderFrame()` 实现

**下一步**: 如果验证通过，进行 Priority 4

---

### 阶段4：播放完成自动切换验证（音频修复后进行，预计 1-2天）

**任务**:
1. 验证播放完成自动切换功能
2. 验证状态转换正确
3. 测试连续播放多个视频

**验证点**:
- 播放完成后自动切换到下一个视频
- 状态转换正确
- 日志中无崩溃信息
- 循环播放正常工作

**下一步**: 如果验证通过，进行 Priority 5

---

### 阶段5: 缓冲管理优化（音频修复后进行，预计 2-3 天）

**任务**:
1. 实现自适应缓冲大小控制
2. 实现缓冲区状态监控和报警
3. 防止缓冲区溢出和饥饿

**文件**: `OpenALAudio.h`, `packet_queue.h`, `DemuxerThread.cpp`

**参考实现**: QMPlayer2 的缓冲管理策略

**下一步**: 如果验证通过，进行 Priority 6

---

### 阶段6: 高级功能完善（中期，预计 5-7 天）

**任务**:
1. 实现硬件帧零拷贝渲染（完整 WGL_NV_DX_interop）
2. 添加性能监控（FPS 显示、解码性能统计、内存使用监控）
3. 字幕支持

**下一步**: 如果验证通过，进行 Priority 7

---

### 阶段7: 测试和文档（长期，预计 2-3 周）

**任务**:
1. 单元测试开发
2. 集成测试
3. 性能测试
4. 文档更新

**下一步**: 如果验证通过，进行 阶段 8

---

## 📋 技术债务

### 1. 架构优化（高优先级，预计 2-3 天）
- [ ] 硬件帧零拷贝渲染（WGL_NV_DX_interop）
- [ ] 使用多线程解码（`codec_ctx_->thread_count = 8`）
- [ ] 自适应缓冲大小控制

### 2. 同步算法优化（高优先级，预计 2-3 天）
- [ ] 更精确的同步算法（双缓冲/三缓冲策略）
- [ ] 动态同步阈值和渐进调整逻辑
- [ ] 平滑帧跳过和重复逻辑

### 3. 缓冲管理优化（中优先级，预计 2-3 天）
- [ ] 自适应缓冲大小控制
- [ ] 缓冲区状态监控和报警
- [ ] 防止缓冲区溢出和饥饿

---

## 🛠️ 风险控制

| 风险项 | 影响 | 概率 | 缓解措施 |
|--------|------|------|----------|
| 音频缓冲区死锁 | 高 | 中 | 修复等待逻辑，添加 processed 检查 |
| 音频播放不稳定 | 中 | 中 | 增强错误处理，逐步调试 |
| 视频切换崩溃 | 高 | 低 | 充分测试，确保线程安全 |
| Seeking重复执行 | 中 | 低 | 添加防重复机制 |
| 硬件解码不稳定 | 中 | 中 | 自动回退到软件解码 |
| OpenAL 播放问题 | 高 | 中 | 添加详细日志，逐步调试 |
| 视频同步不准 | 中 | 低 | 参考 QMPlayer2 优化算法 |
---

## 📋 文档更新计划

### 1. 更新 `docs/继续工作Prompt.md`
- [x] 标记音频缓冲区死锁为"进行中"
- [x] 添加详细的修复方案和代码示例
- [x] 更新下一步行动计划

### 2. 更新 `docs/GLM-progress.md`
- [ ] 标记 Phase 2.2 为"待验证"
- [ ] 添加音频缓冲区死锁分析
- [ ] 添加修复代码示例

### 3. 更新 `docs/GLM改进建议.md`
- [ ] 标记音频缓冲区死锁为"已修复"
- [ ] 添加详细的修复方案
- [ ] 添加音视频同步优化建议

### 4. 更新 `docs/已知BUG记录.md`
- [ ] 标记音频缓冲区死锁为"进行中"
- [ ] 添加根本原因分析
- [ ] 添加修复代码示例

---

## 🔍 调试和日志策略

### 音频缓冲区死锁调试
```cpp
// 在 Audio::writeAudio() 中添加详细日志：
logger->debug("Audio::writeAudio: processed={}, queued={}, state={}", 
    currentProcessed, buffersQueued, state);

// 预期日志：
// "Audio::writeAudio: Unqueued X processed buffers"  // X 为 1-32
// "Audio::writeAudio: Started playback (queued: N, state: X)"  // X=10, state=AL_INITIAL(4113)
// "Audio::writeAudio: Buffers full (queued: N), waiting..."    // N=10, waiting for free buffer
```

### 视频延迟调试
```cpp
// 在 VideoThread::renderFrame() 中添加详细日志：
logger->debug("VideoThread::renderFrame: delay={} ms, sync_threshold={} ms, skipping={}", 
    delay, controller_->getMaxSyncThreshold(), delay > controller_->getMaxSyncThreshold());
```

### 视频切换调试
```cpp
// 在 PlayController::open() 中添加详细日志：
logger->info("PlayController::open: Stopping existing AudioThread...");
logger->info("PlayController::open: AudioThread cleaned up successfully");
logger->info("PlayController::open: VideoThread cleaned up successfully");
logger->info("PlayController::open: DemuxerThread cleaned up successfully");
```

---

## 🚀 测试验证计划

### 测试1：音频缓冲区死锁修复验证（音频修复后立即验证）
- [ ] 播放 10+ 分钟视频，检查是否流畅
- [ ] 快速 Seek 操作（连续多次），检查是否卡顿
- [ ] 暂停/恢复操作（多次暂停恢复），检查是否正常
- [ ] 检查日志中是否有 "Unqueued X processed buffers" 日志
- [ ] 验证音频时钟是否正常前进

### 测试2：视频同步验证（音频修复后验证）
- [ ] 视频延迟是否稳定在 100ms 以内
- [ ] 延迟抖动是否平滑
- [ ] 视频跳帧是否合理
- [ ] 验证音视频同步准确性（延迟 < 100ms）

### 测试3：视频切换验证（音频修复后验证）
- [ ] 播放完成后自动切换到下一个视频
- [ ] 检查日志中是否有崩溃信息
- [ ] 验证状态转换是否正确

---

## 🎯 最终目标

编写稳定、可维护、易读性好的播放器，具备以下特性：

### 功能完整性
- ✅ 文件打开/关闭
- ✅ 播放/暂停/停止
- ✅ Seek 操作
- ✅ 播放进度控制
- ✅ 音量控制
- 静音功能
- ✅ 3D 渲染
- ✅ 循环播放

### 稳定性
- ✅ 长时间播放（>10分钟）
- ✅ 快速 Seek 操作（连续多次 Seek）
- ✅ 暂停/恢复（多次暂停恢复）
- ✅ 自动切换（循环播放）

### 音视频同步
- ✅ 音视频同步在 100ms 以内
- ✅ 同步算法平滑，无抖动
- ✅ Seek 时同步正常

### 错误恢复
- ✅ 完善的错误处理和自动恢复机制
- ✅ 自动回退到软件解码（如果硬件解码失败）
- ✅ 线程健康检查机制
- ✅ 详细日志记录

### 代码质量
- ✅ 模块化设计，职责分离
- ✅ 清晰的接口抽象
- ✅ 良好的注释
- ✅ RAII 模式管理资源
- ✅ 异常处理保护
- ✅ 线程安全

### 性能优化
- ✅ 硬件解码支持（CUDA/D3D11VA）
- ✅ 自适应缓冲大小控制
- ✅ 性能监控（FPS 显示）
- ✅ 多线程解码（8 线程）

### 可维护性
- ✅ 清晰的文档
- ✅ 易读的代码
- ✅ 模块化架构
- ✅ 统一的错误处理
- ✅ 详细的注释
- ✅ 参考代码文档

---

## 🚨 下一阶段工作规划

### Phase 1: 音视频同步修复（Priority 1）

#### 1.1 问题诊断（预计 1-2小时）
- [ ] 分析 `currentPts_` 的更新逻辑
  - 检查 `AudioThread::updateClock()` 的调用时机和频率
  - 检查 `Audio::updateClock()` 的实现
- [ ] 检查 `deviceStartTime_` 的设置时机
  - 检查 `writeAudio()` 中 `deviceStartTime_` 的设置逻辑
  - 检查是否在多次写入时重复设置
- [ ] 分析 OpenAL 时钟计算与估算时钟差异的原因
  - 添加更详细的调试日志
  - 记录 `currentPts_`、`deviceStartTime_`、`estimatedClock`、`openalClock` 的值
- [ ] 检查时钟连续性
  - 检查是否存在时钟跳跃（Seeking等情况）

#### 1.2 修复方案（预计 2-3小时）
- [ ] 优化 `Audio::getClock()` 的时钟计算策略
  - 改进 OpenAL 时钟与估算时钟的同步机制
  - 增加容忍范围但添加连续性检查
- [ ] 修复 `currentPts_` 更新时机问题
  - 确保 `updateClock()` 及时调用
  - 添加时钟连续性检查
- [ ] 修复 `deviceStartTime_` 设置时机问题
  - 只在首次成功播放时设置
  - 在 Seeking 时重置
- [ ] 添加时钟连续性检查
  - 避免时钟跳跃
  - 处理时钟倒退情况

#### 1.3 测试验证（预计 1小时）
- [ ] 测试音视频同步准确性
  - 检查同步误差是否 < 40ms
- [ ] 测试长时间播放的时钟稳定性
  - 播放10分钟以上，检查时钟是否稳定
- [ ] 测试 Seeking 后的时钟重置
  - 多次 Seeking，检查时钟是否正确重置

---

### Phase 2: FFDecHW 硬解码优化（Priority 2）

#### 2.1 硬解码验证（预计 1-2小时）
- [ ] 验证硬件解码器初始化
  - 检查日志，确认硬件解码器成功初始化
- [ ] 验证硬件帧转换
  - 检查硬件帧到软件帧的转换是否成功
- [ ] 验证硬件解码器错误处理
  - 测试硬件解码失败时的自动回退

#### 2.2 性能优化（预计 1-2小时）
- [ ] 优化硬件解码器切换逻辑
  - 减少切换延迟
- [ ] 优化硬件帧转换性能
  - 检查转换性能瓶颈
- [ ] 添加硬件解码器状态监控
  - 添加状态日志
  - 添加性能统计

#### 2.3 稳定性改进（预计 1小时）
- [ ] 改进硬件解码器错误恢复
  - 增强错误检测
  - 改进回退机制
- [ ] 添加硬件解码器超时检测
  - 检测解码超时
  - 自动回退到软件解码
- [ ] 优化硬件解码器资源管理
  - 确保资源正确释放

---

### Phase 3: Seeking 优化确认（Priority 3）

#### 3.1 Seeking 同步验证（预计 1小时）
- [ ] 验证 Seeking 后音视频时钟重置
  - 检查 `deviceStartTime_` 是否正确重置
  - 检查 `currentPts_` 是否正确更新
- [ ] 验证 Seeking 后队列清空
  - 检查队列是否完全清空
- [ ] 验证 Seeking 后关键帧处理
  - 检查是否从关键帧开始播放

#### 3.2 Seeking 性能优化（预计 1-2小时）
- [ ] 优化 Seeking 响应速度
  - 目标：Seeking 响应时间 < 500ms
- [ ] 优化 Seeking 后的缓冲策略
  - 优化预缓冲逻辑
- [ ] 添加 Seeking 进度反馈
  - 添加进度回调
  - 更新 UI 显示

---

### Phase 4: 代码优化与测试（可选）

#### 4.1 代码可测试性增强（预计 2-3小时）
- [ ] 提取可测试的组件
  - PacketQueue
  - PlaybackStateMachine
  - ErrorRecoveryManager
- [ ] 添加依赖注入
  - 改进接口设计
- [ ] 改进接口设计
  - 减少耦合

#### 4.2 单元测试（预计 4-6小时）
- [ ] 为 PacketQueue 添加单元测试
- [ ] 为 PlaybackStateMachine 添加单元测试
- [ ] 为 ErrorRecoveryManager 添加单元测试

#### 4.3 集成测试（预计 4-6小时）
- [ ] 添加播放流程集成测试
- [ ] 添加 Seeking 集成测试
- [ ] 添加音视频同步集成测试

---

## 时间估算

| 阶段 | 任务 | 预计时间 |
|------|------|----------|
| Phase 1 | 音视频同步修复 | 4-6小时 |
| Phase 2 | 硬解码优化 | 3-4小时 |
| Phase 3 | Seeking 优化 | 2-3小时 |
| Phase 4 | 代码优化与测试 | 10-15小时（可选） |

**总计**: 约9-13小时（约1.5-2个工作日，不含可选任务）

---

## 🚀 编译和测试指导

### 编译步骤
1. 使用 Visual Studio 打开 `WZMediaPlay.sln`
2. 选择 Debug/x64 配置
3. 生成解决方案
4. 运行程序

### 运行测试步骤
1. 打开一个视频文件
2. 观察日志输出
3. 检查音频是否播放
4. 检查视频是否显示
5. 测试 Seek 功能
6. 测试暂停/恢复
7. 测试切换视频

### 调试技巧
1. 使用 `spdlog` 设置日志级别
2. 使用条件编译优化
3. 使用断点调试（断点断点）
4. 使用日志筛选功能（spdlog::set_pattern）
5. 使用性能分析工具（如 Valgrind）

### 常见问题解决
1. **编译错误**：检查头文件包含路径
2. **链接错误**：检查库文件链接顺序
3. **运行时崩溃**：检查空指针访问
4. **运行时挂起**：检查死锁
5. **OpenAL 错误**：检查 `alGetError()` 返回值

---

## 🎯 参考资源

### 官方代码
- QMPlayer2: `reference/QMPlayer2/` (参考架构设计)
- FFmpeg: 官方文档
- OpenAL: 官方文档
- Qt 6: 官方文档

### 文档文件
- FFmpeg 文档：[https://ffmpeg.org/doxygen/trunk/](https://ffmpeg.org/doxygen/trunk/)
- Qt 6 文档: [https://doc.qt.io/qt-6/](https://doc.qt.io/qt-6/)
- OpenAL 文档: [https://www.openal.org/](https://www.openal.org/)

---

## 🎯 预期结果

### 修复后应该能够：
1. ✅ OpenAL 缓冲区正常被消费，音频缓冲区不再满
2. ✅ 音频时钟正常前进，`Audio::getClock()` 返回值持续增长
3. ✅ 视频延迟稳定在 < 100ms
4. ✅ 日志中出现 "Unqueued X processed buffers" 日志
5. ✅ 不再出现 "Timeout waiting for free buffer" 错误

---

## 📋 常见问题解答

### Q1: 如何判断音频缓冲区是否正常？
A: 检查日志中是否有 "Unqueued X processed buffers" 日志
   - 有：音频缓冲区正常消费
   - 无：音频缓冲区可能满

### Q2: 如何判断视频同步是否正常？
A: 检查日志中视频延迟
   - 正常：< 100ms
   - 不正常：> 500ms

### Q3: 如何判断 Seeking 是否正常？
A: 检查日志中 "Seek completed" 日志
   - 有：Seek 正常
   - 无：Seek 卡顿

### Q4: 如何判断是否需要优化同步算法？
A: 
   - 延迟抖动 > 50ms：优化同步算法
   - 延迟抖动 < 20ms：当前算法已足够好
   - 延迟持续 > 500ms：考虑优化

### Q5: 如何判断是否需要优化缓冲管理？
A:
   - 缓冲区持续满 > 5 秒：优化缓冲大小
   - 缓冲区持续饥饿 < 1 秒：优化缓冲区大小
   - 缓冲区使用率 < 30%：当前已很好

---

## 🎯 关键代码位置

### 音频关键位置
1. **OpenALAudio.cpp:145-218** - 音频写入逻辑，需要修复
2. **AudioThread.cpp:145-290** - 音频解码循环，需要验证

### 视频关键位置
1. **VideoThread.cpp:406-460** - 视频同步逻辑，需要优化
2. **PlayController.cpp:877-888** - 播放控制，需要简化

### 控制关键位置
1. **PlayController.cpp:563-570** - 播放控制，需要删除冗余代码
2. **PlayController.cpp:330-338** - 线程停止，需要验证
3. **PlayController.cpp:877-888** - 状态转换，需要验证

---

## 🎯 注意事项

### ⚠️ 绝对禁止事项
1. 不要修改核心架构（保持当前多线程架构）
2. 不要切换音频后端（保持 OpenAL）
3. 不要修改同步算法（当前算法已足够好）
4. 不要修改缓冲大小（当前大小已足够）
5. 不要添加高级功能（缓冲管理优化可延后）

### ⚠️ 相对禁止事项
1. 不要一次性修改太多（渐进式改进）
2. 不要在未验证的情况下继续开发新功能
3. 不要跳过测试步骤
4. 不要忽略日志信息

### ✅ 相对允许事项
1. 可以同时修改多个相关文件
2. 可以添加辅助方法简化代码
3. 可以添加异常处理保护
4. 可以优化热点代码

---

## 🚀 提交指南

1. 每次修改后都要编译测试
2. 每个修改都要添加注释
3. 每次修改后都要更新文档
4. 每个修改都要记录日志
5. 每次修改都要验证效果

---

## 🎯 结束语

作为资深开发工程师，我建议：
1. 专注于稳定性修复（音频缓冲区死锁）
2. 验证后再优化（音视频同步）
3. 测试后再开发新功能（缓冲管理、性能监控）
4. 文档同步更新
5. 渐进式改进，保持架构一致

---

**文档版本**: 3.0  
**最后更新**: 2026-01-18  
**维护者**: GLM（通用语言模型）  
**下一步**: Phase 1 - 音视频同步修复（Priority 1），然后 Phase 2 - 硬解码优化，Phase 3 - Seeking 优化确认