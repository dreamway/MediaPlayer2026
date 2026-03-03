# AVThread 重构与 Seeking 修复计划

## 一、问题分析

### 1.1 当前 Seeking 问题

从日志 `MediaPlayer_20260114175730.log` 分析：

**问题1：Seeking 后花屏**
- 日志 1300-1301 行：第一帧渲染前清空了 OpenGL 图像
- 但此时可能渲染的是非关键帧，导致花屏
- 根本原因：没有等待关键帧就开始渲染

**问题2：Seeking 后卡顿**
- 日志 1297-1343 行：AudioThread 在 seeking 完成后立即快速处理大量数据包（488->464）
- 日志 1311-1336 行：VideoQueue 满了，VideoThread 解码速度跟不上
- 根本原因：
  1. AudioThread 在 seeking 完成后没有等待视频准备好
  2. 没有使用类似 QMPlayer2 的 `videoSeekPos` 和 `audioSeekPos` 机制
  3. 没有使用 `flushVideo` 和 `flushAudio` 标志来协调线程

**问题3：架构不一致**
- 当前：VideoThread 和 AudioThread 各自管理 seeking 状态
- QMPlayer2：PlayClass 统一管理 seeking 状态（`videoSeekPos`, `audioSeekPos`, `flushVideo`, `flushAudio`）
- 当前：没有 AVThread 基类，代码重复
- QMPlayer2：有 AVThread 基类，统一管理 Decoder 和 Writer

### 1.2 QMPlayer2 的 Seeking 机制

**核心机制**：
1. **DemuxerThr::seek()**：
   - 设置 `playC.flushVideo = true` 和 `playC.flushAudio = true`
   - 设置 `playC.videoSeekPos = seekPos` 和 `playC.audioSeekPos = seekPos`
   - 调用 `vThr->lock()` 和 `aThr->lock()` 确保线程安全
   - 清空缓冲区
   - 调用 `playC.emptyBufferCond.wakeAll()` 唤醒线程

2. **VideoThr::run()**：
   - 检查 `playC.flushVideo`：如果为 true，刷新解码器，然后设置为 false
   - 检查 `playC.videoSeekPos > 0.0`：如果大于 0，等待帧的 PTS >= videoSeekPos 才开始渲染
   - 渲染后设置 `playC.videoSeekPos = -1.0`

3. **AudioThr::run()**：
   - 检查 `playC.flushAudio`：如果为 true，刷新解码器，然后设置为 false
   - 检查 `playC.audioSeekPos > 0.0`：如果大于 0，等待音频 PTS >= audioSeekPos 才开始播放
   - 播放后设置 `playC.audioSeekPos = -1.0`

**关键点**：
- 使用 `lock()` 机制确保线程安全
- 使用 `videoSeekPos` 和 `audioSeekPos` 确保音视频同步开始
- 使用 `flushVideo` 和 `flushAudio` 标志确保解码器正确刷新
- 等待关键帧（通过 PTS 检查）避免花屏

## 二、重构目标

### 2.1 引入 AVThread 基类

**目标**：统一 VideoThread 和 AudioThread 的基类，减少代码重复

**参考**：`reference/QMPlayer2/src/gui/AVThread.hpp/cpp`

**核心功能**：
- 管理 `Decoder* dec` 和 `Writer* writer`
- 提供 `setDec()`, `lock()`, `unlock()` 方法
- 提供 `stop()`, `hasError()`, `hasDecoderError()` 方法
- 管理线程同步（`mutex`, `updateMutex`）

### 2.2 实现 QMPlayer2 风格的 Seeking 机制

**目标**：修复 seeking 后花屏和卡顿问题

**核心机制**：
- PlayController 管理 `flushVideo_`, `flushAudio_`, `videoSeekPos_`, `audioSeekPos_`
- VideoThread 和 AudioThread 检查这些标志
- 等待关键帧（通过 PTS 检查）避免花屏
- 协调音视频开始时间避免卡顿

### 2.3 优化线程同步

**目标**：使用 QMPlayer2 的 `lock()` 机制确保线程安全

**核心机制**：
- AVThread 提供 `lock()` 和 `unlock()` 方法
- PlayController 在 seeking 时调用 `videoThread_->lock()` 和 `audioThread_->lock()`
- 确保在 seeking 期间线程不会继续处理数据

## 三、详细实施步骤

### 阶段1：创建 AVThread 基类

#### 步骤1.1：创建 AVThread.h
**文件**：`WZMediaPlay/videoDecoder/AVThread.h`

**内容**（参考 QMPlayer2）：
```cpp
#pragma once

#include <QThread>
#include <QMutex>
#include <QRecursiveMutex>

class PlayController;  // Forward declaration
class Decoder;
class VideoWriter;  // 对于 VideoThread
class Audio;        // 对于 AudioThread

class AVThread : public QThread
{
    Q_OBJECT

public:
    virtual void setDec(Decoder* dec);
    void destroyDec();

    inline bool updateTryLock()
    {
        return updateMutex_.tryLock();
    }
    inline void updateUnlock()
    {
        updateMutex_.unlock();
    }

    virtual bool lock();
    virtual void unlock();

    inline bool isWaiting() const
    {
        return waiting_;
    }

    virtual void stop(bool terminate = false);

    virtual bool hasError() const;
    virtual bool hasDecoderError() const;

    Decoder* dec_ = nullptr;

protected:
    AVThread(PlayController* controller);
    virtual ~AVThread();

    void maybeStartThread();

    void terminate();

    PlayController* controller_;

    volatile bool br_ = false, br2_ = false;
    bool waiting_ = false;
    QRecursiveMutex mutex_;
    QMutex updateMutex_;
};
```

#### 步骤1.2：创建 AVThread.cpp
**文件**：`WZMediaPlay/videoDecoder/AVThread.cpp`

**内容**（参考 QMPlayer2）：
```cpp
#include "AVThread.h"
#include "PlayController.h"
#include "videoDecoder/Decoder.h"

AVThread::AVThread(PlayController* controller)
    : controller_(controller)
{
    connect(this, SIGNAL(finished()), this, SLOT(deleteLater()));
    mutex_.lock();
}

AVThread::~AVThread()
{
    delete dec_;
    dec_ = nullptr;
}

void AVThread::maybeStartThread()
{
    // 子类实现：检查 writer 是否准备好
}

void AVThread::setDec(Decoder* dec)
{
    dec_ = dec;
}

void AVThread::destroyDec()
{
    delete dec_;
    dec_ = nullptr;
}

bool AVThread::lock()
{
    br2_ = true;
    const int MUTEXWAIT_TIMEOUT = 1000;  // 1秒超时
    if (!mutex_.tryLock(MUTEXWAIT_TIMEOUT))
    {
        // 等待更长时间
        const bool ret = mutex_.tryLock(MUTEXWAIT_TIMEOUT * 2);
        if (!ret)
        {
            br2_ = false;
            return false;
        }
    }
    return true;
}

void AVThread::unlock()
{
    br2_ = false;
    mutex_.unlock();
}

void AVThread::stop(bool terminate)
{
    if (terminate)
        return this->terminate();

    br_ = true;
    mutex_.unlock();
    // 注意：需要 PlayController 提供 emptyBufferCond

    const int TERMINATE_TIMEOUT = 5000;  // 5秒超时
    if (!wait(TERMINATE_TIMEOUT))
        this->terminate();
}

bool AVThread::hasError() const
{
    return false;
}

bool AVThread::hasDecoderError() const
{
    return false;
}

void AVThread::terminate()
{
    disconnect(this, SIGNAL(finished()), this, SLOT(deleteLater()));
    QThread::terminate();
    wait(1000);
}
```

#### 步骤1.3：修改 VideoThread 继承 AVThread
**修改**：`WZMediaPlay/videoDecoder/VideoThread.h`
```cpp
class VideoThread : public AVThread
{
    // 移除 dec_ 和 writer_（从 AVThread 继承 dec_）
    // 添加 writer_（VideoThread 特有）
    VideoWriter* writer_ = nullptr;
    
    // 其他成员保持不变
};
```

#### 步骤1.4：修改 AudioThread 继承 AVThread
**修改**：`WZMediaPlay/videoDecoder/AudioThread.h`
```cpp
class AudioThread : public AVThread
{
    // 移除 codecctx_ 等（这些是 AudioThread 特有的，保留）
    // dec_ 从 AVThread 继承（但 AudioThread 可能不需要 Decoder，需要确认）
    
    // 注意：AudioThread 可能不需要 Decoder，因为音频解码在 AudioThread 内部完成
    // 需要确认 QMPlayer2 的 AudioThr 是否使用 Decoder
};
```

### 阶段2：实现 QMPlayer2 风格的 Seeking 机制

#### 步骤2.1：在 PlayController 中添加 Seeking 相关成员
**修改**：`WZMediaPlay/PlayController.h`
```cpp
class PlayController : public QObject
{
    // 添加 Seeking 相关成员（参考 QMPlayer2 的 PlayClass）
    bool flushVideo_{false};
    bool flushAudio_{false};
    double videoSeekPos_{-1.0};  // -1 表示不需要 seeking
    double audioSeekPos_{-1.0};  // -1 表示不需要 seeking
    
    // 添加条件变量（用于唤醒线程）
    QWaitCondition emptyBufferCond_;
    
    // 其他成员保持不变
};
```

#### 步骤2.2：修改 PlayController::seek() 方法
**修改**：`WZMediaPlay/PlayController.cpp`

**核心逻辑**（参考 QMPlayer2 的 DemuxerThr::seek()）：
```cpp
bool PlayController::seek(int64_t positionMs)
{
    // 1. 检查是否已经在 seeking
    if (seeking_.load()) {
        return false;
    }
    
    // 2. 设置 seeking 标志
    seeking_.store(true);
    
    // 3. 锁定 VideoThread 和 AudioThread
    bool vLocked = false, aLocked = false;
    if (videoThread_) {
        vLocked = videoThread_->lock();
    }
    if (audioThread_) {
        aLocked = audioThread_->lock();
    }
    
    // 4. 清空缓冲区
    vPackets_.Reset("VideoQueue");
    aPackets_.Reset("AudioQueue");
    
    // 5. 设置 flush 标志和 seek 位置
    flushVideo_ = true;
    flushAudio_ = true;
    double seekPos = positionMs / 1000.0;  // 转换为秒
    videoSeekPos_ = seekPos;
    audioSeekPos_ = seekPos;
    
    // 6. 重置时钟
    clockbase_ = microseconds::min();
    videoClock_ = nanoseconds::min();
    
    // 7. 清空 OpenGL 图像（在锁定状态下）
    if (videoWriter_) {
        OpenGLWriter* glWriter = dynamic_cast<OpenGLWriter*>(videoWriter_.get());
        if (glWriter) {
            glWriter->clearImg();
        }
    }
    
    // 8. 请求 DemuxerThread seek
    if (demuxThread_) {
        int64_t seekTargetUs = positionMs * 1000;
        demuxThread_->requestSeek(seekTargetUs, true);  // backward = true 确保关键帧
    }
    
    // 9. 解锁线程（在 seek 请求后）
    if (vLocked) {
        videoThread_->unlock();
    }
    if (aLocked) {
        audioThread_->unlock();
    }
    
    // 10. 唤醒线程
    emptyBufferCond_.wakeAll();
    
    return true;
}
```

#### 步骤2.3：修改 VideoThread::run() 检查 flushVideo 和 videoSeekPos
**修改**：`WZMediaPlay/videoDecoder/VideoThread.cpp`

**核心逻辑**（参考 QMPlayer2 的 VideoThr::run()）：
```cpp
void VideoThread::run()
{
    while (!quit_.load()) {
        // 1. 检查 flushVideo 标志
        if (controller_->getFlushVideo()) {
            // 刷新解码器
            if (dec_) {
                dec_->flushBuffers();
            }
            controller_->setFlushVideo(false);
            
            // 清空当前帧
            videoFrame.clear();
        }
        
        // 2. 检查 videoSeekPos
        double videoSeekPos = controller_->getVideoSeekPos();
        if (videoSeekPos > 0.0) {
            // 等待帧的 PTS >= videoSeekPos
            // 如果当前帧的 PTS < videoSeekPos，跳过渲染
            nanoseconds framePts = videoFrame.ts();
            if (framePts != nanoseconds::min()) {
                double framePtsSeconds = framePts.count() / 1e9;
                if (framePtsSeconds < videoSeekPos) {
                    // 帧的 PTS 还没到 seek 位置，跳过渲染
                    videoFrame.clear();
                    continue;
                } else {
                    // 帧的 PTS >= videoSeekPos，可以开始渲染
                    controller_->setVideoSeekPos(-1.0);  // 清除 seek 位置
                }
            }
        }
        
        // 3. 正常解码和渲染流程
        // ...
    }
}
```

#### 步骤2.4：修改 AudioThread::run() 检查 flushAudio 和 audioSeekPos
**修改**：`WZMediaPlay/videoDecoder/AudioThread.cpp`

**核心逻辑**（参考 QMPlayer2 的 AudioThr::run()）：
```cpp
void AudioThread::run()
{
    while (!quit_.load()) {
        // 1. 检查 flushAudio 标志
        if (controller_->getFlushAudio()) {
            // 刷新解码器
            if (codecctx_) {
                avcodec_flush_buffers(codecctx_.get());
            }
            // 清空 Audio 缓冲区
            audio_->clear();
            controller_->setFlushAudio(false);
        }
        
        // 2. 检查 audioSeekPos
        double audioSeekPos = controller_->getAudioSeekPos();
        if (audioSeekPos > 0.0) {
            // 等待音频 PTS >= audioSeekPos
            // 如果当前音频的 PTS < audioSeekPos，跳过播放
            // 注意：需要从解码后的帧获取 PTS
            // ...
            if (audioPts >= audioSeekPos) {
                controller_->setAudioSeekPos(-1.0);  // 清除 seek 位置
            } else {
                continue;  // 跳过当前音频数据
            }
        }
        
        // 3. 正常解码和播放流程
        // ...
    }
}
```

### 阶段3：修复花屏问题

#### 步骤3.1：等待关键帧
**问题**：Seeking 后第一帧可能是非关键帧，导致花屏

**解决方案**：
- 在 `VideoThread::run()` 中，检查 `videoSeekPos > 0.0` 时，等待关键帧
- 只渲染 PTS >= videoSeekPos 且是关键帧的帧
- 或者：等待第一个 PTS >= videoSeekPos 的帧（不一定是关键帧，但解码器应该能处理）

**实现**：
```cpp
// 在 VideoThread::run() 中
if (videoSeekPos > 0.0) {
    nanoseconds framePts = videoFrame.ts();
    if (framePts != nanoseconds::min()) {
        double framePtsSeconds = framePts.count() / 1e9;
        if (framePtsSeconds < videoSeekPos) {
            // 帧的 PTS 还没到 seek 位置，跳过
            videoFrame.clear();
            continue;
        }
        
        // 检查是否是关键帧（如果是 seeking 后的第一帧）
        // 注意：解码器已经刷新，第一帧应该是有效的
        // 但为了安全，可以检查 packet 的 keyframe 标志
        // ...
        
        // 可以开始渲染
        controller_->setVideoSeekPos(-1.0);
    }
}
```

#### 步骤3.2：优化 OpenGL 图像清空时机
**问题**：在 seeking 时清空 OpenGL 图像，但新帧还没准备好

**解决方案**：
- 不在 seeking 开始时清空，而是在确认第一帧有效后再清空
- 或者：在 PlayController::seek() 中锁定线程后清空（更安全）

**实现**：
```cpp
// 在 PlayController::seek() 中，锁定线程后清空
if (vLocked && videoWriter_) {
    OpenGLWriter* glWriter = dynamic_cast<OpenGLWriter*>(videoWriter_.get());
    if (glWriter) {
        glWriter->clearImg();
    }
}
```

### 阶段4：修复卡顿问题

#### 步骤4.1：协调音视频开始时间
**问题**：AudioThread 在 seeking 完成后立即快速处理数据包，导致音频时钟快速前进

**解决方案**：
- 使用 `audioSeekPos` 机制，确保音频等待到正确位置再开始播放
- 使用 `videoSeekPos` 机制，确保视频等待到正确位置再开始渲染
- 两个线程都等待到 seek 位置后，再开始正常播放

**实现**：
```cpp
// 在 AudioThread::run() 中
if (audioSeekPos > 0.0) {
    // 计算当前音频的 PTS
    double audioPts = getCurrentAudioPts();  // 需要实现此方法
    if (audioPts < audioSeekPos) {
        // 音频还没到 seek 位置，跳过
        continue;
    } else {
        // 音频到了 seek 位置，可以开始播放
        controller_->setAudioSeekPos(-1.0);
    }
}
```

#### 步骤4.2：限制 AudioThread 在 seeking 后的处理速度
**问题**：即使使用了 audioSeekPos，AudioThread 可能还是处理太快

**解决方案**：
- 在 `audioSeekPos > 0.0` 时，限制处理速度
- 或者：等待 videoSeekPos 也清除后再恢复正常速度

**实现**：
```cpp
// 在 AudioThread::run() 中
if (audioSeekPos > 0.0) {
    // 限制处理速度
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}
```

## 四、实施顺序

### 第一步：创建 AVThread 基类（基础重构）
1. 创建 `AVThread.h` 和 `AVThread.cpp`
2. 修改 `VideoThread` 继承 `AVThread`
3. 修改 `AudioThread` 继承 `AVThread`
4. 测试编译和基本功能

### 第二步：实现 Seeking 机制（核心修复）
1. 在 `PlayController` 中添加 `flushVideo_`, `flushAudio_`, `videoSeekPos_`, `audioSeekPos_`
2. 修改 `PlayController::seek()` 使用新机制
3. 修改 `VideoThread::run()` 检查 `flushVideo` 和 `videoSeekPos`
4. 修改 `AudioThread::run()` 检查 `flushAudio` 和 `audioSeekPos`
5. 测试 seeking 功能

### 第三步：修复花屏问题
1. 优化 OpenGL 图像清空时机
2. 确保等待关键帧（通过 PTS 检查）
3. 测试 seeking 后是否还会花屏

### 第四步：修复卡顿问题
1. 协调音视频开始时间
2. 限制 AudioThread 在 seeking 后的处理速度
3. 测试 seeking 后是否还会卡顿

## 五、关键设计决策

### 5.1 AVThread 基类的设计

**问题**：AudioThread 是否需要 Decoder？

**分析**：
- QMPlayer2 的 AudioThr 使用 Decoder（`dec->decodeAudio()`）
- 当前 WZMediaPlayer 的 AudioThread 直接使用 FFmpeg API（`avcodec_send_packet`, `avcodec_receive_frame`）

**决策**：
- **选项1**：AudioThread 也使用 Decoder 接口（与 QMPlayer2 一致）
  - 优点：架构统一，代码更清晰
  - 缺点：需要创建 AudioDecoder 类
- **选项2**：AudioThread 不使用 Decoder，但继承 AVThread（dec_ 可以为 nullptr）
  - 优点：改动小，快速实现
  - 缺点：不完全符合 QMPlayer2 架构

**建议**：先采用选项2，确保功能正常，后续再考虑选项1

### 5.2 Seeking 位置的管理

**问题**：videoSeekPos 和 audioSeekPos 的单位？

**分析**：
- QMPlayer2 使用秒（double）
- 当前 WZMediaPlayer 使用微秒（int64_t）

**决策**：
- 使用秒（double），与 QMPlayer2 一致
- 在 PlayController::seek() 中转换：`double seekPos = positionMs / 1000.0;`

### 5.3 线程锁机制

**问题**：是否需要 QRecursiveMutex？

**分析**：
- QMPlayer2 使用 QRecursiveMutex
- 当前 WZMediaPlayer 使用 std::mutex

**决策**：
- 使用 QRecursiveMutex，与 QMPlayer2 一致
- 但要注意避免死锁（QMPlayer2 的 lock() 有超时机制）

## 六、风险评估

### 6.1 重构风险

**风险1**：引入 AVThread 基类可能影响现有功能
- **缓解**：逐步迁移，先创建基类，再修改子类
- **回退**：如果出现问题，可以回退到当前实现

**风险2**：修改 Seeking 机制可能导致新的问题
- **缓解**：参考 QMPlayer2 的实现，确保逻辑一致
- **测试**：每个步骤都要测试

### 6.2 兼容性风险

**风险**：修改后可能影响其他功能
- **缓解**：保持接口兼容，只修改内部实现
- **测试**：全面测试播放、暂停、停止、seeking 等功能

## 七、测试计划

### 7.1 单元测试（可选）

**目标**：验证 AVThread 基类的基本功能

**测试项**：
- `setDec()` 和 `destroyDec()`
- `lock()` 和 `unlock()`
- `stop()` 和 `terminate()`

### 7.2 集成测试（必须）

**测试项**：
1. **基本播放**：播放、暂停、停止
2. **Seeking 测试**：
   - 向前 seeking
   - 向后 seeking
   - 多次连续 seeking
   - Seeking 到文件开头
   - Seeking 到文件结尾
3. **花屏测试**：
   - Seeking 后是否还会花屏
   - 第一帧是否正确渲染
4. **卡顿测试**：
   - Seeking 后播放是否流畅
   - 音视频是否同步
5. **稳定性测试**：
   - 长时间播放
   - 多次 seeking
   - 播放不同格式的视频

## 八、实施时间估算

- **阶段1（AVThread 基类）**：2-3 小时
- **阶段2（Seeking 机制）**：3-4 小时
- **阶段3（花屏修复）**：1-2 小时
- **阶段4（卡顿修复）**：1-2 小时
- **测试和调试**：2-3 小时

**总计**：约 9-14 小时

## 九、后续优化（可选）

### 9.1 实现 AudioDecoder 类
- 统一音频解码接口
- 与 VideoDecoder 保持一致

### 9.2 实现 VideoFilters
- 处理硬件帧转换
- 支持视频滤镜

### 9.3 性能优化
- 优化缓冲管理
- 优化线程同步

---

## 十、总结

本次重构的核心目标：
1. **引入 AVThread 基类**：统一 VideoThread 和 AudioThread 的基类，减少代码重复
2. **实现 QMPlayer2 风格的 Seeking 机制**：修复 seeking 后花屏和卡顿问题
3. **优化线程同步**：使用 lock() 机制确保线程安全

**关键原则**：
- 参考 QMPlayer2 的实现，确保逻辑一致
- 逐步迁移，每个步骤都要测试
- 保持接口兼容，只修改内部实现
- 如果出现问题，可以回退到当前实现
