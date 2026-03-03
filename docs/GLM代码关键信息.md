# GLM代码关键信息

本文档记录WZMediaPlayer项目的核心代码架构、设计模式、关键类和数据流，供后续开发和维护参考。

## 1. 架构概览

### 1.1 整体架构
```
PlayController（主控制器）
    ├── DemuxerThread（解复用线程）
    │   └── Demuxer（解复用器）
    ├── VideoThread（视频解码线程）
    │   ├── Decoder（解码器接口）
    │   │   ├── FFDecSW（软件解码器）
    │   │   └── FFDecHW（硬件解码器）
    │   └── VideoWriter（渲染器接口）
    │       ├── OpenGLWriter（2D渲染器）
    │       └── StereoWriter（3D渲染器）
    ├── AudioThread（音频解码线程）
    │   ├── Audio（音频输出设备）
    │   └── SwrContext（重采样器）
    ├── PlaybackStateMachine（播放状态机）
    └── ThreadSyncManager（线程同步管理器）
```

### 1.2 数据流
```
媒体文件
    ↓ Demuxer
AVPacket队列（PacketQueue）
    ↓ VideoThread / AudioThread
解码（Decoder）
    ↓ 重采样（AudioThread） / 转换（VideoThread）
渲染（VideoWriter） / 播放（Audio）
```

### 1.3 线程模型
- **DemuxerThread**: 单独线程，负责读取和解复用
- **VideoThread**: 单独线程，负责视频解码和渲染
- **AudioThread**: 单独线程，负责音频解码、重采样和播放
- **主线程**: Qt UI线程，处理用户交互

---

## 2. 核心类详解

### 2.1 PlayController（主控制器）
**文件**: `WZMediaPlay/PlayController.h`, `PlayController.cpp`

**职责**:
- 协调所有线程（DemuxerThread、VideoThread、AudioThread）
- 管理播放状态（通过PlaybackStateMachine）
- 处理用户命令（play/pause/stop/seek）
- 管理共享数据结构（Demuxer、PacketQueue）

**关键成员**:
```cpp
// 状态管理
PlaybackStateMachine stateMachine_;

// 线程管理
DemuxerThread* demuxThread_;
VideoThread* videoThread_;
AudioThread* audioThread_;

// 数据流
std::unique_ptr<Demuxer> demuxer_;
PacketQueue<20*1024*1024> vPackets_;  // 视频队列（20MB）
PacketQueue<1024*1024> aPackets_;     // 音频队列（1MB）

// 解码器和渲染器
std::unique_ptr<Decoder> videoDecoder_;
std::shared_ptr<VideoWriter> videoWriter_;
Audio* audio_;

// 音视频时钟
microseconds clockbase_;
SyncMaster sync_;
nanoseconds videoClock_;  // 视频时钟（基于最后渲染帧的PTS）
mutable std::mutex videoClockMutex_;

// Seeking机制
bool flushVideo_;
bool flushAudio_;
double videoSeekPos_;
double audioSeekPos_;
QWaitCondition emptyBufferCond_;

// 线程健康检查时间戳
std::chrono::steady_clock::time_point lastVideoFrameTime_;
std::chrono::steady_clock::time_point lastAudioFrameTime_;
std::chrono::steady_clock::time_point lastDemuxTime_;
```

**关键方法**:
- `open()`: 打开文件，初始化线程
- `play()`: 开始播放
- `pause()`: 暂停播放
- `stop()`: 停止播放（按顺序停止：Demuxer → Video → Audio）
- `seek()`: 跳转到指定位置
- `getMasterClock()`: 获取主时钟（音频或视频）
- `updateVideoClock()`: 更新视频时钟
- `getVideoClock()`: 获取视频时钟

**状态检查**:
- `isPlaying()`, `isPaused()`, `isStopped()`, `isSeeking()`, `isStopping()`

---

### 2.2 Demuxer（解复用器）
**文件**: `WZMediaPlay/videoDecoder/Demuxer.h`, `Demuxer.cpp`

**职责**:
- 打开媒体文件
- 读取流信息
- 读取数据包（AVPacket）
- Seek操作

**关键成员**:
```cpp
AVFormatContextPtr fmtctx_;

// 流信息
int videoStreamIndex_;
int audioStreamIndex_;
int subtitleStreamIndex_;

AVStream* videoStream_;
AVStream* audioStream_;
AVStream* subtitleStream_;

// 媒体信息
int64_t durationMs_;
int videoWidth_;
int videoHeight_;
double videoFrameRate_;
int64_t totalFrameNum_;

// 状态
std::atomic<bool> eof_;
std::mutex mutex_;
```

**关键方法**:
- `open()`: 打开文件
- `close()`: 关闭文件
- `readPacket()`: 读取数据包（线程安全）
- `seek()`: 跳转到指定位置（支持AVSEEK_FLAG_FRAME）
- `isEof()`: 检查是否到达文件末尾

---

### 2.3 DemuxerThread（解复用线程）
**文件**: `WZMediaPlay/videoDecoder/DemuxerThread.h`, `DemuxerThread.cpp`

**职责**:
- 从Demuxer读取数据包
- 分发数据包到PacketQueue
- 处理Seek请求
- 检测EOF

**关键成员**:
```cpp
PlayController* controller_;
Demuxer* demuxer_;
PacketQueue<20*1024*1024>* vPackets_;
PacketQueue<1024*1024>* aPackets_;

int videoStreamIndex_;
int audioStreamIndex_;
int subtitleStreamIndex_;

// Seeking相关
int64_t seekPositionUs_;
bool seekBackward_;
std::atomic<bool> seekInProgress_;
std::mutex seekMutex_;

// 健康检查
std::chrono::steady_clock::time_point lastDemuxTime_;
```

**关键方法**:
- `run()`: 主循环，读取和分发数据包
- `requestSeek()`: 请求Seek操作（先清空队列）
- `handleSeek()`: 处理Seek请求
- `readAndDistributePacket()`: 读取并分发数据包
- `distributeVideoPacket()`: 分发视频数据包
- `distributeAudioPacket()`: 分发音频数据包

**关键逻辑**:
```cpp
// Seeking时立即清空队列，避免阻塞
void requestSeek(int64_t positionUs, bool backward) {
    {
        std::lock_guard<std::mutex> guard(seekMutex_);
        seekPositionUs_ = positionUs;
        seekBackward_ = backward;
        seekInProgress_ = false;
    }

    // 立即清空队列
    if (vPackets_ && vPackets_->Size() > 0) {
        vPackets_->Reset("VideoQueue");
    }
    if (aPackets_ && aPackets_->Size() > 0) {
        aPackets_->Reset("AudioQueue");
    }
}
```

---

### 2.4 VideoThread（视频解码线程）
**文件**: `WZMediaPlay/videoDecoder/VideoThread.h`, `VideoThread.cpp`

**职责**:
- 从PacketQueue获取视频数据包
- 调用Decoder解码视频帧
- 处理音视频同步
- 调用VideoWriter渲染帧

**关键成员**:
```cpp
PlayController* controller_;
PacketQueue<20*1024*1024>* vPackets_;
Decoder* dec_;  // 由AVThread管理
VideoWriter* writer_;

// Seeking状态管理
bool wasSeekingRecently_;  // 跟踪是否刚刚完成seeking
int eofAfterSeekCount_;

// FPS计算
std::chrono::steady_clock::time_point lastFpsTime_;
int fpsFrameCount_;
float currentFPS_;
bool showFPS_;

// 跳帧优化
Frame lastRenderedFrame_;  // 上一帧（用于跳帧时避免黑屏）
bool hasLastRenderedFrame_;

// 错误恢复
ErrorRecoveryManager errorRecoveryManager_;
```

**关键方法**:
- `run()`: 主循环
- `handleFlushVideo()`: 处理flushVideo标志
- `handlePausedState()`: 处理暂停状态
- `handleSeekingState()`: 处理seeking状态
- `decodeFrame()`: 解码视频帧
- `processDecodeResult()`: 处理解码结果
- `renderFrame()`: 渲染帧

**关键逻辑**:
```cpp
// 视频解码和渲染主循环（2026-01-18更新）
void run() {
    int consecutiveSkipCount_ = 0;  // 连续跳过帧计数
    
    while (!br_) {
        // 1. 解码视频帧
        Frame videoFrame;
        int bytesConsumed = 0;
        bool isKeyFrame = false;
        
        if (!decodeFrame(videoFrame, bytesConsumed, newPixFmt, isKeyFrame)) {
            continue;  // 没有数据包或跳过
        }
        
        // 2. 处理解码结果（包括 popPacket）
        bool packetPopped = false;
        if (!processDecodeResult(bytesConsumed, videoFrame, isKeyFrame, packetPopped)) {
            continue;  // 继续循环
        }
        
        // 3. 渲染帧（音视频同步）
        if (!renderFrame(videoFrame, frames)) {
            continue;  // 渲染失败
        }
    }
}

// 音视频同步：延迟/跳过视频帧（2026-01-18优化）
bool renderFrame(Frame &videoFrame, int &frames) {
    nanoseconds framePts = videoFrame.ts();
    nanoseconds masterClock = controller_->getMasterClock();
    nanoseconds diff = framePts - masterClock;
    
    // 根据延迟和动态阈值决定渲染策略
    if (diff > controller_->getMaxSyncThreshold()) {
        // 延迟 > 阈值，考虑跳过帧
        milliseconds delayMs = std::chrono::duration_cast<milliseconds>(diff);
        
        // 如果连续跳过太多帧（>5帧）或延迟不是特别大（<300ms），强制渲染
        if (consecutiveSkipCount_ >= 5 || delayMs.count() < 300) {
            writer_->writeVideo(videoFrame);
            consecutiveSkipCount_ = 0;
            return true;
        }
        
        // 跳过帧
        consecutiveSkipCount_++;
        return false;
    } else if (diff > controller_->getSyncThreshold()) {
        // 延迟 > 阈值，延迟渲染
        nanoseconds delay = std::min(diff, nanoseconds(200));
        std::this_thread::sleep_for(delay);
        writer_->writeVideo(videoFrame);
        consecutiveSkipCount_ = 0;
        return true;
    } else if (diff < -controller_->getMaxSyncThreshold()) {
        // 延迟 < -阈值，重复上一帧（超前太多）
        if (hasLastRenderedFrame_) {
            writer_->writeVideo(lastRenderedFrame_);
        } else {
            writer_->writeVideo(videoFrame);
        }
        consecutiveSkipCount_ = 0;
        return true;
    } else {
        // 延迟在阈值范围内，立即渲染
        writer_->writeVideo(videoFrame);
        consecutiveSkipCount_ = 0;
        return true;
    }
}
```

---

### 2.5 AudioThread（音频解码线程）
**文件**: `WZMediaPlay/videoDecoder/AudioThread.h`, `AudioThread.cpp`

**职责**:
- 从PacketQueue获取音频数据包
- 解码音频数据
- 重采样音频数据
- 调用Audio::writeAudio()输出到音频设备
- 处理Seeking状态
- 更新音频时钟供视频同步使用

**关键成员**:
```cpp
PlayController* controller_;
Audio* audio_;
PacketQueue<1024*1024>* aPackets_;

// FFmpeg解码资源
AVStream* stream_;
AVCodecContextPtr codecctx_;
AVFramePtr decodedFrame_;
SwrContextPtr swrctx_;

// 重采样参数
uint64_t dstChanLayout_;
AVSampleFormat dstSampleFmt_;
ALuint frameSize_;
uint8_t* samples_;
int samplesLen_;
int samplesPos_;
int samplesMax_;

// 音频时钟
nanoseconds currentPts_;
nanoseconds deviceStartTime_;

// Seeking状态
bool wasSeeking_;

// 错误恢复
ErrorRecoveryManager errorRecoveryManager_;

// 音频结束标志
std::atomic<bool> audioFinished_;
```

**关键方法**:
- `run()`: 主循环
- `decodeFrame()`: 解码一帧音频
- `readAudio()`: 读取解码后的音频数据
- `getClock()`: 获取音频时钟
- `setCodecContext()`: 设置解码器上下文

**关键逻辑**:
```cpp
// 音频时钟计算
nanoseconds AudioThread::getClock() const {
    // 使用Audio::getClock()
    if (audio_) {
        return audio_->getClock();
    }
    return currentPts_;
}
```

---

### 2.6 Audio（音频输出设备）
**文件**: `WZMediaPlay/videoDecoder/OpenALAudio.h`, `OpenALAudio.cpp`

**职责**:
- OpenAL资源管理
- 写入已解码的音频数据到OpenAL缓冲区
- 音量控制
- 音频时钟获取

**关键成员**:
```cpp
PlayController& controller_;

// 音频时钟
nanoseconds currentPts_;
std::chrono::steady_clock::time_point deviceStartTime_;

// OpenAL相关
ALenum format_;
ALuint frameSize_;
int sampleRate_;
ALuint source_;
std::array<ALuint, AudioBufferCount> buffers_;  // 所有缓冲区 ID（用于创建/销毁）
std::queue<ALuint> availableBuffers_;          // 可用缓冲区队列（关键：正确的缓冲区生命周期管理）

std::mutex srcMutex_;
std::condition_variable srcCond_;
bool br_{false};  // 停止标志
bool isMute_;
float storeVolumeWhenMute_;
bool playbackStarted_{false};  // 音频播放启动标志
```

**关键方法**:
- `open()`: 初始化音频设备（创建OpenAL资源，初始化可用缓冲区队列）
- `close()`: 关闭音频设备
- `writeAudio()`: 写入音频数据（使用 availableBuffers_ 管理缓冲区）
- `recycleProcessedBuffers()`: 回收已处理的缓冲区到可用队列
- `updateClock()`: 更新音频时钟
- `play()`, `pause()`, `stop()`: 播放控制
- `clear()`: 清空缓冲区（重置可用缓冲区队列）
- `setVolume()`, `getVolume()`, `ToggleMute()`: 音量控制
- `getClock()`: 获取音频时钟（多层fallback策略）

**关键逻辑**:
```cpp
// 缓冲区管理：使用 availableBuffers_ 队列精确跟踪可用缓冲区
void Audio::recycleProcessedBuffers() {
    ALint processed = 0;
    alGetSourcei(source_, AL_BUFFERS_PROCESSED, &processed);
    if (processed > 0) {
        std::vector<ALuint> bufids(processed);
        alSourceUnqueueBuffers(source_, processed, bufids.data());
        // 将回收的缓冲区放入可用队列
        for (int i = 0; i < processed; ++i) {
            availableBuffers_.push(bufids[i]);
        }
    }
}

// 音频时钟计算（多层fallback策略）
nanoseconds Audio::getClock() {
    // 策略1：如果从来没有播放过，返回基准PTS
    if (deviceStartTime_ == std::chrono::steady_clock::time_point{}) {
        return std::max(currentPts_, nanoseconds::zero());
    }

    // 策略2：基于系统时间的估算时钟
    auto now = std::chrono::steady_clock::now();
    nanoseconds deviceElapsed = now - deviceStartTime_;
    nanoseconds estimatedClock = currentPts_ + deviceElapsed;

    // 策略3：优先使用OpenAL SAMPLE_OFFSET进行精确计算
    if (source_ && sampleRate_ > 0 && status == AL_PLAYING) {
        ALint offset = 0;
        alGetSourcei(source_, AL_SAMPLE_OFFSET, &offset);
        nanoseconds sampleOffset = nanoseconds{static_cast<long long>(
            static_cast<double>(offset) * 1000000000.0 / sampleRate_)};
        nanoseconds openalClock = currentPts_ + sampleOffset;

        // 只有当偏差在合理范围内（200ms以内）才使用OpenAL时钟
        nanoseconds diff = openalClock - estimatedClock;
        if (std::abs(diff.count()) < 200000000) {
            return std::max(openalClock, nanoseconds::zero());
        }
        // 差异过大时拒绝使用，fallback到估算时钟
        // 日志: "OpenAL clock diff too large: -8731ms, rejecting"
    }

    // 策略4：使用系统时间估算（最可靠的fallback）
    return std::max(estimatedClock, nanoseconds::zero());
}
```

**当前问题**（2026-01-18）:
- 日志显示 "OpenAL clock diff too large: -8731ms, rejecting"
- OpenAL 时钟和估算时钟差异过大（约8.7秒）
- 说明 `currentPts_` 或 `deviceStartTime_` 可能有问题
- 需要检查 `currentPts_` 的更新逻辑和 `deviceStartTime_` 的设置时机
- **相关代码**: `WZMediaPlay/videoDecoder/OpenALAudio.cpp:500-586`

---

### 2.7 Decoder（解码器接口）
**文件**: `WZMediaPlay/videoDecoder/Decoder.h`

**职责**:
- 统一管理视频/音频解码逻辑
- 分离硬件解码和软件解码
- 提供统一的解码接口

**关键接口**:
```cpp
// 基本信息
virtual std::string name() const = 0;
virtual bool hasHWDecContext() const;
virtual std::shared_ptr<HWDecContext> getHWDecContext() const;
virtual std::shared_ptr<VideoFilter> hwAccelFilter() const;

// 解码接口
virtual int decodeVideo(
    const AVPacket *encodedPacket,
    Frame &decoded,
    AVPixelFormat &newPixFmt,
    bool flush,
    unsigned hurry_up
) = 0;

virtual int decodeAudio(
    const AVPacket *encodedPacket,
    std::vector<uint8_t> &decoded,
    double &ts,
    uint8_t &channels,
    uint32_t &sampleRate,
    bool flush = false
) = 0;

// 状态管理
virtual int pendingFrames() const;
virtual bool hasCriticalError() const;
virtual void clearFrames();
virtual void flushBuffers();

// 初始化
virtual bool init(AVCodecContext *codec_ctx, AVRational stream_time_base) = 0;
virtual bool open(AVCodecContext *codec_ctx) = 0;
```

---

### 2.7.1 FFDecSW（软件解码器）
**文件**: `WZMediaPlay/videoDecoder/FFDecSW.h`, `FFDecSW.cpp`

**职责**:
- 实现软件解码逻辑
- 处理像素格式转换
- 提供统一的解码接口

**关键方法**:
- `decodeVideo()`: 解码视频帧（返回值约定见下方）

**decodeVideo 返回值约定**（2026-01-18 重新设计）:
```cpp
// 返回值语义
// 0: 成功，有输出帧，packet 已消费
// 1: packet 已消费，但没有输出帧（B帧等情况，需要更多数据）
// 2: 有输出帧，但 packet 未消费（解码器缓冲区满，需要先取出帧）
// 负数: 错误码（AVERROR(EAGAIN), AVErrorEOF, 其他错误）

int FFDecSW::decodeVideo(const AVPacket *encodedPacket, Frame &decoded, ...) {
    bool packetSent = false;
    bool packetNotConsumed = false;
    
    if (encodedPacket) {
        int ret = avcodec_send_packet(codec_ctx_, encodedPacket);
        if (ret == 0) {
            packetSent = true;
        } else if (ret == AVERROR(EAGAIN)) {
            // 解码器缓冲区满，先尝试取出帧
            packetNotConsumed = true;
            ret = avcodec_receive_frame(codec_ctx_, temp_frame_.get());
            if (ret == 0) {
                // 成功取出一帧，packet 未消费
                // 创建 Frame 对象...
                return 2;  // 有输出帧，但 packet 未消费
            }
            return AVERROR(EAGAIN);
        }
    }
    
    // 接收解码后的帧
    int ret = avcodec_receive_frame(codec_ctx_, temp_frame_.get());
    if (ret == AVERROR(EAGAIN)) {
        if (packetSent) {
            return 1;  // packet 已消费，但没有输出帧
        }
        return AVERROR(EAGAIN);
    }
    
    // 成功接收到帧
    // 创建 Frame 对象...
    return 0;  // 成功，有输出帧，packet 已消费
}
```

**VideoThread 处理逻辑**:
```cpp
// processDecodeResult 根据返回值决定是否 popPacket
bool processDecodeResult(int bytesConsumed, Frame &videoFrame, ...) {
    if (bytesConsumed == 0) {
        // 解码成功，有输出帧，packet 已消费
        vPackets_->popPacket("VideoQueue");
        return true;  // 可以渲染
    } else if (bytesConsumed == 1) {
        // packet 已消费，但没有输出帧（B帧等情况）
        vPackets_->popPacket("VideoQueue");
        return false;  // 继续循环，处理下一个 packet
    } else if (bytesConsumed == 2) {
        // 有输出帧，但 packet 未消费（解码器缓冲区满）
        // 不 popPacket，但可以渲染这一帧
        return true;  // 可以渲染，下次循环会重试发送同一个 packet
    } else if (bytesConsumed == AVERROR(EAGAIN)) {
        // 解码器异常状态，flush 并重试
        dec_->flushBuffers();
        return false;
    }
    // ... 其他错误处理
}
```

---

### 2.8 FFDecHW（硬件解码器）
**文件**: `WZMediaPlay/videoDecoder/FFDecHW.h`, `FFDecHW.cpp`

**职责**:
- 实现硬件解码逻辑
- 管理硬件解码上下文
- 处理硬件帧转换
- 自动回退到软件解码

**关键成员**:
```cpp
std::unique_ptr<HardwareDecoder> hw_decoder_;
std::unique_ptr<FFDecSW> sw_fallback_;  // 软件解码器回退
AVCodecContext *codec_ctx_;
AVRational stream_time_base_;
bool use_hw_decoder_;
```

**关键逻辑**:
```cpp
// 初始化：尝试硬件解码，失败则回退
bool init(AVCodecContext *codec_ctx, AVRational stream_time_base) {
    const AVCodec *hw_codec = hw_decoder_->tryInitHardwareDecoder(
        codec_ctx->codec_id, codec_ctx);

    if (hw_codec && hw_decoder_->isInitialized()) {
        use_hw_decoder_ = true;
        logger->info("Hardware decoder initialized: {}",
            hw_decoder_->getDeviceTypeName());
    } else {
        use_hw_decoder_ = false;
        // 初始化软件解码器作为回退
        if (!sw_fallback_->init(codec_ctx, stream_time_base)) {
            return false;
        }
    }
    return true;
}
```

---

### 2.9 PacketQueue（线程安全的数据包队列）
**文件**: `WZMediaPlay/videoDecoder/packet_queue.h`

**职责**:
- 线程安全的数据包队列
- 支持大小限制
- 支持条件变量等待/通知

**关键成员**:
```cpp
std::mutex mtx_;
std::condition_variable cond_;
std::deque<AVPacket> packets_;
size_t totalsize_{0};
bool finished_{false};
```

**关键方法**:
- `put()`: 放入数据包（线程安全，检查大小限制）
- `peekPacket()`: 查看下一个数据包（不取出）
- `popPacket()`: 移除并返回下一个数据包
- `sendTo()`: 发送数据包到解码器（直接使用avcodec_send_packet）
- `Reset()`: 重置队列（清空所有数据包，重置finished标志）
- `setFinished()`: 设置结束标志（清空队列并设置finished）
- `IsEmpty()`: 检查是否为空
- `IsFinished()`: 检查是否结束
- `Size()`: 获取队列大小
- `logQueueState()`: 打印队列状态（调试用）

**关键逻辑**:
```cpp
// 放入数据包（检查大小限制）
bool put(const AVPacket *pkt, const char* queueName = nullptr) {
    std::lock_guard<std::mutex> lck(mtx_);
    if (totalsize_ >= SizeLimit) {
        return false;  // 队列满
    }

    packets_.push_back(AVPacket{});
    if (av_packet_ref(&packets_.back(), pkt) != 0) {
        packets_.pop_back();
        return false;
    }

    totalsize_ += packets_.back().size;
    cond_.notify_one();
    return true;
}

// 重置队列（Seeking时使用）
bool Reset(const char* queueName = nullptr) {
    std::lock_guard<std::mutex> guard(mtx_);
    for (AVPacket &pkt : packets_) {
        av_packet_unref(&pkt);
    }
    packets_.clear();
    totalsize_ = 0;
    finished_ = false;
    cond_.notify_one();
    return true;
}
```

---

### 2.10 PlaybackStateMachine（播放状态机）
**文件**: `WZMediaPlay/PlaybackStateMachine.h`

**职责**:
- 统一管理播放状态
- 确保状态转换的正确性和原子性
- 提供状态查询和验证

**状态枚举**:
```cpp
enum class PlaybackState {
    Idle,           // 空闲状态（初始状态）
    Opening,        // 正在打开文件
    Ready,          // 文件已打开，准备播放
    Playing,        // 正在播放
    Paused,         // 已暂停
    Seeking,        // 正在Seeking
    Stopping,       // 正在停止
    Stopped,        // 已停止
    Error           // 错误状态
};
```

**关键方法**:
- `transitionTo()`: 转换到新状态（带验证）
- `canTransitionTo()`: 检查是否可以转换到新状态
- `getState()`: 获取当前状态
- `isPlaying()`, `isPaused()`, `isStopped()`, `isSeeking()`, `isStopping()`: 状态查询
- `stateName()`: 获取状态名称（用于日志）
- `setStateChangeCallback()`: 设置状态变化回调

**合法的状态转换**:
```
Idle → Opening → Ready → Playing ↔ Paused
Playing → Seeking → Playing
Playing → Stopping → Stopped → Idle
Paused → Stopping → Stopped → Idle
任何状态 → Error
Error → Stopped → Idle
```

---

### 2.11 ThreadSyncManager（线程同步管理器）
**文件**: `WZMediaPlay/ThreadSyncManager.h`

**职责**:
- 统一管理线程同步
- 避免死锁和资源竞争
- 提供超时和死锁检测功能

**关键方法**:
- `tryLock()`: 获取锁（带超时）
- `tryLockMultiple()`: 获取多个锁（按顺序，避免死锁）
- `unlock()`: 释放锁
- `detectDeadlock()`: 死锁检测
- `getLockCount()`, `getDeadlockCount()`: 获取统计信息

**关键逻辑**:
```cpp
// 获取多个锁（按顺序，避免死锁）
bool tryLockMultiple(std::vector<std::mutex*> mutexes,
                    std::chrono::milliseconds timeout) {
    // 按地址排序，确保锁定顺序一致
    std::sort(mutexes.begin(), mutexes.end());

    for (auto* mutex : mutexes) {
        if (!tryLock(*mutex, timeout)) {
            // 失败，释放已获取的锁
            unlockMultiple(mutexes);
            return false;
        }
    }
    return true;
}
```

---

### 2.12 ErrorRecoveryManager（错误恢复管理器）
**文件**: `WZMediaPlay/ErrorRecoveryManager.h`

**职责**:
- 统一管理错误处理和自动恢复机制
- 提供错误统计和重试机制

**错误类型**:
```cpp
enum class ErrorType {
    DecodeError,    // 解码错误
    NetworkError,   // 网络错误
    ResourceError,  // 资源错误
    ThreadError,    // 线程错误
    UnknownError    // 未知错误
};
```

**恢复动作**:
```cpp
enum class RecoveryAction {
    None,           // 不处理
    Retry,          // 重试
    FlushAndRetry,  // 刷新并重试
    RestartThread,  // 重启线程
    StopPlayback    // 停止播放
};
```

**关键方法**:
- `handleError()`: 处理错误，返回恢复动作
- `resetErrorCount()`: 重置错误计数
- `getErrorCount()`, `getTotalErrorCount()`: 获取错误统计
- `setMaxRetries()`: 设置最大重试次数
- `setErrorCallback()`: 设置错误回调

---

## 3. 数据流详解

### 3.1 播放启动流程
```
1. PlayController::open()
   ├─ 停止旧播放（如果有）
   ├─ 创建/重置Demuxer
   ├─ Demuxer::open()
   ├─ 创建Audio
   ├─ 创建AudioThread（在initializeCodecs之前）
   ├─ PlayController::initializeCodecs()
   │  ├─ streamComponentOpen() - 打开音频编解码器
   │  │  └─ audioThread_->setCodecContext()
   │  ├─ streamComponentOpen() - 打开视频编解码器
   │  ├─ 创建视频解码器（FFDecSW/FFDecHW）
   │  └─ 创建VideoWriter（OpenGLWriter）
   ├─ 创建VideoThread（在initializeCodecs之后）
   ├─ 创建DemuxerThread
   ├─ 启动所有线程（demuxThread_, videoThread_, audioThread_）
   └─ 转换到Playing状态
```

### 3.2 解复用流程
```
DemuxerThread::run()
   └─ 循环：
      ├─ 检查seek请求
      │  └─ handleSeek()
      │     ├─ Demuxer::seek()
      │     ├─ Reset队列
      │     └─ emit seekFinished()
      │
      ├─ readAndDistributePacket()
      │  ├─ Demuxer::readPacket()
      │  ├─ 检查stream_index
      │  │  ├─ 视频流 → distributeVideoPacket()
      │  │  │  └─ vPackets_->put()
      │  │  └─ 音频流 → distributeAudioPacket()
      │  │     └─ aPackets_->put()
      │  └─ av_packet_unref()
      │
      └─ 检查EOF
         └─ emit eofReached()
```

### 3.3 视频解码流程
```
VideoThread::run()
   └─ 循环：
      ├─ 检查flushVideo标志
      │  └─ dec_->flushBuffers()
      │
      ├─ 检查暂停状态
      │  └─ sleep(10ms)
      │
      ├─ 检查seeking状态
      │  ├─ 刷新解码器
      │  ├─ 消费队列中的旧数据包
      │  └─ 等待seeking结束
      │
      ├─ decodeFrame()
      │  ├─ vPackets_->peekPacket()  // 查看下一个packet（不取出）
      │  ├─ dec_->decodeVideo(packet, videoFrame, ...)
      │  │  └─ 返回值约定：
      │  │     - 0: 成功，有输出帧，packet 已消费
      │  │     - 1: packet 已消费，但没有输出帧（B帧等情况）
      │  │     - 2: 有输出帧，但 packet 未消费（解码器缓冲区满）
      │  │     - 负数: 错误码（EAGAIN, EOF等）
      │  └─ 返回 true（解码成功或需要处理）
      │
      ├─ processDecodeResult(bytesConsumed, videoFrame, ...)
      │  ├─ bytesConsumed == 0: popPacket(), 返回 true（可渲染）
      │  ├─ bytesConsumed == 1: popPacket(), 返回 false（继续循环）
      │  ├─ bytesConsumed == 2: 不 popPacket(), 返回 true（可渲染，下次重试发送packet）
      │  ├─ bytesConsumed == EAGAIN: flush解码器，返回 false
      │  ├─ bytesConsumed == EOF: 处理EOF，返回 false
      │  └─ 其他错误: popPacket(), 返回 false
      │
      ├─ renderFrame(videoFrame, frames)
      │  ├─ 获取主时钟（音频时钟）
      │  ├─ 计算帧延迟 diff = framePts - masterClock
      │  ├─ 根据延迟决定渲染策略：
      │  │  ├─ diff > maxThreshold: 跳过帧（但连续跳过>5帧或延迟<300ms时强制渲染）
      │  │  ├─ diff > threshold: 延迟渲染
      │  │  ├─ diff < -maxThreshold: 重复上一帧
      │  │  └─ 其他: 立即渲染
      │  ├─ writer_->writeVideo()
      │  └─ controller_->updateVideoClock(framePts)
      │
      └─ 检查播放完成
         └─ 清理并退出
```

### 3.4 音频解码流程
```
AudioThread::run()
   └─ 循环：
      ├─ 检查flushAudio标志
      │  ├─ avcodec_flush_buffers()
      │  ├─ audio_->clear()
      │  └─ 重置解码状态
      │
      ├─ 检查暂停状态
      │  └─ sleep(10ms)
      │
      ├─ 检查seeking状态
      │  ├─ 刷新解码器
      │  ├─ 清空音频缓冲区
      │  └─ 消费队列中的旧数据包
      │
      ├─ decodeFrame()
      │  ├─ aPackets_->sendTo(codecctx_)
      │  ├─ avcodec_receive_frame()
      │  ├─ swr_convert() - 重采样
      │  └─ 更新currentPts_
      │
      ├─ readAudio()
      │  └─ audio_->writeAudio(samples, length)
      │     ├─ 填充OpenAL缓冲区
      │     ├─ alSourcePlay()
      │     └─ 更新deviceStartTime_
      │
      └─ 检查播放完成
         └─ 清理并退出
```

---

## 4. 同步机制详解

### 4.1 音视频同步机制
**主时钟选择**:
- 有音频时：使用音频时钟（Audio::getClock()）
- 无音频时：使用视频时钟（PlayController::getVideoClock()）

**音频时钟计算**（多层fallback策略，2026-01-18更新）:
```cpp
nanoseconds Audio::getClock() {
    std::lock_guard<std::mutex> lock(srcMutex_);

    // 策略1：如果从来没有播放过，返回基准PTS
    if (deviceStartTime_ == std::chrono::steady_clock::time_point{}) {
        return std::max(currentPts_, nanoseconds::zero());
    }

    // 策略2：基于系统时间的估算时钟
    auto now = std::chrono::steady_clock::now();
    nanoseconds deviceElapsed = now - deviceStartTime_;
    nanoseconds estimatedClock = currentPts_ + deviceElapsed;

    // 策略3：优先使用OpenAL SAMPLE_OFFSET进行精确计算
    if (source_ && sampleRate_ > 0) {
        ALint offset = 0;
        alGetSourcei(source_, AL_SAMPLE_OFFSET, &offset);
        ALint status = AL_STOPPED;
        alGetSourcei(source_, AL_SOURCE_STATE, &status);
        
        if (status == AL_PLAYING && offset >= 0) {
            nanoseconds sampleOffset = nanoseconds{static_cast<long long>(
                static_cast<double>(offset) * 1000000000.0 / sampleRate_)};
            nanoseconds openalClock = currentPts_ + sampleOffset;

            // 只有当偏差在合理范围内（200ms以内）才使用OpenAL时钟
            nanoseconds diff = openalClock - estimatedClock;
            if (std::abs(diff.count()) < 200000000) {
                return std::max(openalClock, nanoseconds::zero());
            }
            // 差异过大时拒绝使用，fallback到估算时钟
            // 日志: "OpenAL clock diff too large: -8731ms, rejecting"
        }
    }

    // 策略4：使用系统时间估算（最可靠的fallback）
    return std::max(estimatedClock, nanoseconds::zero());
}
```

**当前问题**（2026-01-18）:
- 日志显示 "OpenAL clock diff too large: -8731ms, rejecting"
- OpenAL 时钟和估算时钟差异过大（约8.7秒）
- 说明 `currentPts_` 或 `deviceStartTime_` 可能有问题
- 需要检查 `currentPts_` 的更新逻辑和 `deviceStartTime_` 的设置时机

**视频时钟计算**:
```cpp
// 视频时钟基于最后渲染的帧的PTS
void PlayController::updateVideoClock(nanoseconds pts) {
    std::lock_guard<std::mutex> lock(videoClockMutex_);
    videoClock_ = pts;
}

nanoseconds PlayController::getVideoClock() const {
    std::lock_guard<std::mutex> lock(videoClockMutex_);
    return videoClock_;
}
```

**主时钟获取**:
```cpp
nanoseconds PlayController::getMasterClock() const {
    if (sync_ == SyncMaster::Audio && audio_) {
        return audio_->getClock();
    } else {
        return getVideoClock();
    }
}
```

**视频帧同步策略**:
```cpp
// 计算帧延迟
nanoseconds diff = framePts - masterClock;

if (diff > controller_->getMaxSyncThreshold()) {
    // 延迟 > 阈值，考虑跳过帧（落后太多）
    // 但如果连续跳过 >5 帧或延迟 <300ms，强制渲染一帧
    if (consecutiveSkipCount_ >= 5 || delayMs.count() < 300) {
        // 强制渲染，但加快速度（不延迟）
        writer_->writeVideo(videoFrame);
        consecutiveSkipCount_ = 0;
    } else {
        // 跳过帧
        consecutiveSkipCount_++;
        return false;
    }
} else if (diff > controller_->getSyncThreshold()) {
    // 延迟 > 阈值，延迟渲染
    nanoseconds delay = std::min(diff, nanoseconds(200)); // 最大延迟到200ms
    std::this_thread::sleep_for(delay);
    writer_->writeVideo(videoFrame);
    consecutiveSkipCount_ = 0;
} else if (diff < -controller_->getMaxSyncThreshold()) {
    // 延迟 < -阈值，重复上一帧（超前太多）
    if (hasLastRenderedFrame_) {
        writer_->writeVideo(lastRenderedFrame_);
    } else {
        writer_->writeVideo(videoFrame);
    }
    consecutiveSkipCount_ = 0;
} else {
    // 延迟在阈值范围内，立即渲染
    writer_->writeVideo(videoFrame);
    consecutiveSkipCount_ = 0;
}
```

**FFDecSW::decodeVideo 返回值约定**:
```cpp
// 返回值语义（2026-01-18 重新设计）
// 0: 成功，有输出帧，packet 已消费
// 1: packet 已消费，但没有输出帧（B帧等情况，需要更多数据）
// 2: 有输出帧，但 packet 未消费（解码器缓冲区满，需要先取出帧）
// 负数: 错误码（AVERROR(EAGAIN), AVErrorEOF, 其他错误）

int FFDecSW::decodeVideo(const AVPacket *encodedPacket, Frame &decoded, ...) {
    // 发送数据包到解码器
    int ret = avcodec_send_packet(codec_ctx_, encodedPacket);
    if (ret == AVERROR(EAGAIN)) {
        // 解码器缓冲区满，先尝试取出帧
        ret = avcodec_receive_frame(codec_ctx_, temp_frame_.get());
        if (ret == 0) {
            // 成功取出一帧，packet 未消费
            // 创建 Frame 对象...
            return 2;  // 有输出帧，但 packet 未消费
        }
        return AVERROR(EAGAIN);
    }
    
    // 接收解码后的帧
    ret = avcodec_receive_frame(codec_ctx_, temp_frame_.get());
    if (ret == AVERROR(EAGAIN)) {
        if (packetSent) {
            return 1;  // packet 已消费，但没有输出帧
        }
        return AVERROR(EAGAIN);
    }
    
    // 成功接收到帧
    // 创建 Frame 对象...
    return 0;  // 成功，有输出帧，packet 已消费
}
```

---

### 4.2 Seeking同步机制
**Seek操作流程**:
```
1. PlayController::seek()
   ├─ 转换到Seeking状态
   ├─ 锁定VideoThread和AudioThread（按顺序）
   ├─ 设置flush标志和seek位置
   ├─ Reset队列（vPackets_, aPackets_）
   ├─ 重置时钟（clockbase_, videoClock_）
   ├─ 清空OpenGLWriter图像
   ├─ 解锁线程
   ├─ 唤醒线程
   └─ demuxThread_->requestSeek()
```

**DemuxerThread处理Seek**:
```
2. DemuxerThread::requestSeek()
   ├─ 设置seek参数
   └─ 立即Reset队列（避免阻塞）
```

```
3. DemuxerThread::handleSeek()
   ├─ Demuxer::seek() - 使用AVSEEK_FLAG_FRAME
   ├─ 再次Reset队列（防御性）
   ├─ emit seekFinished()
   └─ 重置seekInProgress标志
```

**VideoThread处理Seek**:
```
4. VideoThread::handleSeekingState()
   ├─ 检测seeking状态变化
   ├─ 刷新解码器
   ├─ 消费队列中的旧数据包
   └─ 设置wasSeekingRecently标志
```

**AudioThread处理Seek**:
```
5. AudioThread处理seeking状态
   ├─ 检测seeking状态变化
   ├─ 刷新解码器
   ├─ 清空音频缓冲区
   └─ 消费队列中的旧数据包
```

**PlayController处理Seek完成**:
```
6. PlayController::onDemuxerThreadSeekFinished()
   ├─ 转换到Playing状态
   └─ 清除flush标志
```

**关键优化**:
- **AVSEEK_FLAG_FRAME**: 直接跳到目标点附近的关键帧，避免"先跳回原点再跳到目标点"
- **立即清空队列**: 在requestSeek时立即Reset队列，避免队列满导致阻塞
- **消费旧数据包**: Seeking期间，VideoThread和AudioThread消费队列中的旧数据包，防止阻塞
- **限制跳过速度**: 每次循环最多跳过10个数据包，给其他线程时间处理

---

### 4.3 线程同步机制
**锁定顺序**（避免死锁）:
1. 先VideoThread，后AudioThread
2. 使用tryLock带超时，避免永久阻塞

**PlayController::seek()示例**:
```cpp
// 先锁定VideoThread
bool vLocked = false;
if (videoThread_) {
    vLocked = videoThread_->lock();
    if (!vLocked) {
        // 锁定失败，回退状态
        stateMachine_.transitionTo(PlaybackState::Playing, "Seek aborted");
        return false;
    }
}

// 再锁定AudioThread
bool aLocked = false;
if (audioThread_) {
    aLocked = audioThread_->lock();
    if (!aLocked) {
        // 锁定失败，解锁VideoThread
        if (vLocked && videoThread_) {
            videoThread_->unlock();
        }
        stateMachine_.transitionTo(PlaybackState::Playing, "Seek aborted");
        return false;
    }
}

// 执行seek操作...

// 解锁顺序与锁定顺序相反：先AudioThread，后VideoThread
if (aLocked && audioThread_) {
    audioThread_->unlock();
}
if (vLocked && videoThread_) {
    videoThread_->unlock();
}
```

---

## 5. 硬件解码详解

### 5.1 硬件解码器选择
**优先级**:
1. D3D11VA（Windows 10+，性能最优）
2. 软件解码（最后选择）

**初始化流程**:
```cpp
bool FFDecHW::init(AVCodecContext *codec_ctx, AVRational stream_time_base) {
    const AVCodec *hw_codec = hw_decoder_->tryInitHardwareDecoder(
        codec_ctx->codec_id, codec_ctx);

    if (hw_codec && hw_decoder_->isInitialized()) {
        use_hw_decoder_ = true;
        logger->info("Hardware decoder initialized: {}",
            hw_decoder_->getDeviceTypeName());
    } else {
        use_hw_decoder_ = false;
        // 初始化软件解码器作为回退
        if (!sw_fallback_->init(codec_ctx, stream_time_base)) {
            return false;
        }
    }
    return true;
}
```

### 5.2 硬件帧处理
**解码流程**:
```cpp
int FFDecHW::decodeVideo(...) {
    if (use_hw_decoder_ && hw_decoder_->isInitialized()) {
        // 发送数据包到解码器
        avcodec_send_packet(codec_ctx_, encodedPacket);

        // 接收解码后的帧（可能是硬件帧）
        AVFrame* hw_frame = av_frame_alloc();
        avcodec_receive_frame(codec_ctx_, hw_frame);

        if (hw_frame->format == hw_pix_fmt) {
            // 硬件帧，需要转换
            // TODO: 使用硬件帧零拷贝渲染
            // 目前自动转换为软件帧
            av_hwframe_transfer_data(sw_frame, hw_frame, 0);
        }

        // 复制到输出Frame
        decoded.copyFromAVFrame(sw_frame);
        av_frame_free(&hw_frame);
    } else {
        // 使用软件解码器
        return sw_fallback_->decodeVideo(...);
    }
}
```

### 5.3 硬件帧零拷贝渲染（待实现）
**目标**: 使用WGL_NV_DX_interop扩展，直接渲染硬件帧，无需CPU拷贝

**步骤**:
1. 创建D3D11纹理
2. 创建WGL NV_DX互对象
3. 注册D3D11纹理到OpenGL
4. 将硬件帧数据直接渲染到D3D11纹理
5. OpenGL从D3D11纹理读取数据

**当前状态**: 框架已搭建，但未完全实现

---

## 6. 3D渲染详解

### 6.1 StereoWriter架构
**继承关系**:
```
VideoWriter（接口）
    └── OpenGLWriter
         └── StereoWriter（支持3D）
```

**关键类**:
- `StereoWriter`: 3D渲染器，继承OpenGLWriter
- `StereoOpenGLCommon`: 3D OpenGL实现，继承OpenGLCommon
- `StereoVideoWidget`: UI组件，管理3D参数
- `ShaderManager`: Shader管理

**3D渲染模式**:
- 左右格式（Left-Right）
- 上下格式（Top-Bottom）
- 棋盘格格式（Checkerboard）

**3D参数**:
- 视差调节
- 局部3D区域（RegionEnabled）
- 3D深度调节

---

## 7. 错误处理和恢复

### 7.1 错误恢复管理器
**错误类型**:
- DecodeError: 解码错误
- NetworkError: 网络错误
- ResourceError: 资源错误
- ThreadError: 线程错误
- UnknownError: 未知错误

**恢复动作**:
- None: 不处理
- Retry: 重试
- FlushAndRetry: 刷新并重试
- RestartThread: 重启线程
- StopPlayback: 停止播放

**使用示例**:
```cpp
try {
    bytesConsumed = dec_->decodeVideo(...);
} catch (const std::exception& e) {
    ErrorInfo error(ErrorType::DecodeError, e.what(), AVERROR(EINVAL));
    RecoveryResult recovery = errorRecoveryManager_.handleError(error);

    if (recovery.action == RecoveryAction::StopPlayback) {
        emit errorOccurred(-1);
        return false;
    }

    // 根据恢复动作处理
    if (recovery.action == RecoveryAction::FlushAndRetry) {
        dec_->flushBuffers();
    }
}
```

---

## 8. 已知问题分析

### 8.1 BUG-001: 视频播放完成后切换下一个视频时崩溃
**可能原因**:
1. 线程清理时序问题：`VideoThread`或`AudioThread`在播放完成后退出，但`PlayController`仍在尝试访问这些线程
2. 状态机状态转换问题：播放完成后的状态转换可能导致不一致
3. 队列清理问题：`PacketQueue`的`finished`状态设置后，线程可能仍在访问队列

**相关代码位置**:
- `PlayController::stop()` - 线程停止逻辑
- `PlayController::open()` - 线程创建逻辑
- `VideoThread::run()` - 播放完成检测
- `AudioThread::run()` - 播放完成检测

### 8.2 BUG-003: Seeking操作时的闪烁和卡顿
**已完成的修复**:
- 添加`lastRenderedFrame_`机制，减少非关键帧跳过时的闪烁
- 优化队列清空时机，在`requestSeek()`时立即清空队列
- 实现KeyFrame直接跳转，使用`AVSEEK_FLAG_FRAME`避免"先跳回原点"的问题
- 优化缓冲管理，在seeking期间直接丢弃满队列的数据包
- 改进Seeking同步机制，在成功解码关键帧后立即清除`wasSeekingRecently_`标志

### 8.3 音频播放崩溃
**可能原因**:
1. OpenAL资源管理问题
2. 线程安全问题
3. Seeking期间的音频状态混乱

---

## 9. 性能优化建议

### 9.1 解码性能优化
- 使用多线程解码（`avCodecContext->thread_count = 8`）
- 使用硬件解码（DXVA2/D3D11VA）
- 实现硬件帧零拷贝渲染（WGL_NV_DX_interop）

### 9.2 渲染性能优化
- 使用VBO/VAO减少CPU-GPU数据传输
- 使用PBO（Pixel Buffer Object）异步传输纹理数据
- 优化Shader性能

### 9.3 缓冲管理优化
- 自适应缓冲大小控制
- 防止缓冲区溢出和饥饿
- 缓冲区状态监控和报警

---

## 10. 调试和日志

### 10.1 日志系统
**使用spdlog**:
```cpp
extern spdlog::logger *logger;

logger->info("Info message");
logger->warn("Warning message");
logger->error("Error message");
logger->debug("Debug message");
```

### 10.2 调试技巧
- 使用`PacketQueue::logQueueState()`打印队列状态
- 使用`PlaybackStateMachine::stateName()`打印状态名称
- 使用FPS显示（配置文件中设置`ShowFPS=true`）

---

## 11. 配置文件

### 11.1 SystemConfig.ini
```ini
[System]
EnableHardwareDecoding=true
ShowFPS=false
```

---

## 12. 总结

### 12.1 架构优点
1. **职责分离**: 每个类都有明确的单一职责
2. **线程安全**: 所有缓冲区操作都是线程安全的
3. **接口抽象**: 通过抽象接口降低模块耦合度
4. **错误处理**: 完善的错误传播和日志记录
5. **可扩展性**: 易于添加新的解码器和渲染器实现

### 12.2 需要改进的地方
1. **稳定性**: 视频切换崩溃、音频播放崩溃需要修复
2. **Seeking**: Seeking时的闪烁和卡顿需要进一步优化
3. **硬件解码**: 硬件帧零拷贝渲染需要完整实现
4. **音视频同步**: 同步机制需要更精确
5. **缓冲管理**: 自适应缓冲大小需要实现

### 12.3 代码规范
- 使用智能指针管理资源（AVCodecContextPtr, AVFramePtr等）
- 使用RAII模式管理资源生命周期
- 使用异常处理保护关键代码段
- 使用spdlog记录详细的日志信息
- 使用状态机管理播放状态

---

**文档版本**: 1.0
**最后更新**: 2026-01-16
**维护者**: GLM（通用语言模型）
