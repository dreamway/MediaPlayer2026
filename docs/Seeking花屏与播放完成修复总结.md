# Seeking花屏与播放完成修复总结

## 问题分析

### 1. Seeking后花屏问题
**现象**：Seeking后，视频出现花屏，日志显示VideoThread在解码非关键帧（keyframe: no）

**原因**：
- Seeking后，VideoThread直接解码了非关键帧，导致花屏
- 非关键帧依赖于前面的关键帧，seeking后解码器状态已清空，无法正确解码非关键帧

**解决方案**：
- 在seeking后，跳过所有非关键帧，直到找到关键帧才开始解码
- 在`VideoThread::run()`中，解码前检查`wasSeekingRecently_`标志和`isKeyFrame`标志
- 如果是seeking后且不是关键帧，跳过该数据包，继续下一个

### 2. 视频播放完成检测问题
**现象**：视频播放完成后，VideoThread一直等待队列数据，没有正确检测到播放完成

**原因**：
- VideoThread在队列为空时只检查队列是否为空，没有检查队列是否finished
- DemuxerThread在EOF时会设置队列为finished，但VideoThread没有检查这个状态

**解决方案**：
- 在`PacketQueue`中添加`IsFinished()`方法，用于检查队列是否finished
- 在`VideoThread::run()`中，当队列为空时，检查队列是否finished
- 如果队列finished且flush后没有更多帧，退出循环，表示播放完成

### 3. 崩溃保护
**现象**：Seeking多次后，程序崩溃，异常没有被捕获

**原因**：
- `decodeVideo()`和`writeVideo()`调用可能抛出异常，但没有异常保护
- OpenGL操作可能失败，导致崩溃

**解决方案**：
- 在`decodeVideo()`调用周围添加try-catch块
- 在`writeVideo()`调用周围添加try-catch块（已有，但确保完整）
- 捕获异常后记录日志，但不退出循环，继续处理下一帧

## 代码修改

### 1. PacketQueue添加IsFinished()方法
**文件**：`WZMediaPlay/videoDecoder/packet_queue.h`

```cpp
bool IsFinished() const { 
    std::lock_guard<std::mutex> lck(const_cast<std::mutex&>(mtx_));
    return finished_; 
}
```

### 2. VideoThread跳过非关键帧
**文件**：`WZMediaPlay/videoDecoder/VideoThread.cpp`

在解码前添加检查：
```cpp
// Seeking后，跳过非关键帧直到找到关键帧（避免花屏）
if (wasSeekingRecently_ && !isKeyFrame) {
    // 跳过非关键帧，继续下一个数据包
    if (logger) {
        logger->info("VideoThread::run: Skipping non-keyframe after seeking (pts: {}, dts: {}), waiting for keyframe", packet->pts, packet->dts);
    }
    vPackets_->popPacket("VideoQueue");
    videoFrame.clear();
    continue;  // 跳过非关键帧，继续下一个数据包
}

// 如果找到关键帧，记录日志
if (wasSeekingRecently_ && isKeyFrame) {
    if (logger) {
        logger->info("VideoThread::run: Found keyframe after seeking (pts: {}, dts: {}), will decode and render", packet->pts, packet->dts);
    }
}
```

### 3. VideoThread检测播放完成
**文件**：`WZMediaPlay/videoDecoder/VideoThread.cpp`

在队列为空时检查finished状态：
```cpp
if (vPackets_->IsEmpty()) {
    // 检查队列是否finished（播放完成）
    if (vPackets_->IsFinished()) {
        if (logger) {
            logger->info("VideoThread::run: Queue is empty and finished, playback completed");
        }
        // 尝试刷新解码器获取剩余帧
        AVPixelFormat dummyPixFmt = AV_PIX_FMT_NONE;
        bytesConsumed = dec_->decodeVideo(nullptr, videoFrame, dummyPixFmt, true, 0);
        if (bytesConsumed < 0 || videoFrame.isEmpty()) {
            // 没有更多帧，播放完成，退出循环
            if (logger) {
                logger->info("VideoThread::run: No more frames after flush, playback finished");
            }
            break;  // 播放完成，退出循环
        }
        // ... 继续处理最后一帧
    } else {
        // 队列为空但未finished，可能是暂时没有数据，尝试刷新解码器
        // ... 原有逻辑
    }
}
```

### 4. 添加异常保护
**文件**：`WZMediaPlay/videoDecoder/VideoThread.cpp`

在`decodeVideo()`调用周围添加异常保护：
```cpp
try {
    bytesConsumed = dec_->decodeVideo(packet, videoFrame, newPixFmt, false, 0);
} catch (const std::exception& e) {
    logger->error("VideoThread::run: Exception in decodeVideo: {}", e.what());
    bytesConsumed = AVERROR(EINVAL);
    vPackets_->popPacket("VideoQueue");
    videoFrame.clear();
    continue;
} catch (...) {
    logger->error("VideoThread::run: Unknown exception in decodeVideo");
    bytesConsumed = AVERROR(EINVAL);
    vPackets_->popPacket("VideoQueue");
    videoFrame.clear();
    continue;
}
```

`writeVideo()`调用已有异常保护，确保完整。

## 测试建议

1. **Seeking测试**：
   - 多次seeking，检查是否还会花屏
   - 检查日志中是否有"Found keyframe after seeking"和"Skipping non-keyframe after seeking"的日志
   - 检查seeking后第一帧是否正确显示

2. **播放完成测试**：
   - 播放视频到结束，检查是否正确切换到下一个视频
   - 检查日志中是否有"Queue is empty and finished, playback completed"和"No more frames after flush, playback finished"的日志
   - 检查VideoThread是否正确退出

3. **崩溃测试**：
   - 多次seeking，检查是否还会崩溃
   - 检查日志中是否有异常相关的错误日志
   - 检查程序是否稳定运行

## 预期效果

1. **Seeking后不再花屏**：跳过非关键帧，只解码关键帧，确保画面正确
2. **播放完成正确检测**：VideoThread正确检测到播放完成，退出循环
3. **更稳定的运行**：异常保护防止崩溃，程序更稳定

## 注意事项

1. **性能影响**：跳过非关键帧可能导致seeking后需要等待更长时间才能显示画面，但这是必要的，以确保画面正确
2. **日志级别**：seeking相关的日志使用`info`级别，便于调试
3. **异常处理**：异常捕获后不退出循环，继续处理下一帧，避免程序卡死

## 后续优化建议

1. **Seeking优化**：可以考虑在seeking时直接seek到关键帧位置，减少跳过的数据包数量
2. **播放完成检测优化**：可以考虑在PlayController中统一检测播放完成，而不是在各个线程中检测
3. **异常处理优化**：可以考虑添加异常计数器，超过一定次数后停止线程，避免无限循环
