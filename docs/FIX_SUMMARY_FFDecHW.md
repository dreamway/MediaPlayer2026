# FFDecHW 硬解码修复总结

## 修复日期
2026-01-19

## 问题分析

### 日志分析
从日志 `MediaPlayer_20260119150135.log` 的 L138 开始，发现：
- 硬解码器持续报 "EAGAIN - unexpected state" 错误
- VideoThread 持续 flush decoder，形成死循环
- 第一帧解码失败后，后续帧也无法正常解码

### 根本原因
1. **重复代码块**：FFDecHW.cpp:304-348 存在重复的 `avcodec_receive_frame` 调用和错误处理代码
2. **EAGAIN 处理不正确**：没有参考 FFDecSW 的成熟实现，导致 packet 状态跟踪混乱
3. **返回值约定不一致**：返回值处理逻辑与 VideoThread 的期望不匹配

## 修复内容

### 1. 删除重复代码块
**文件**：`FFDecHW.cpp`

**问题**：L304-348 有重复的 `avcodec_receive_frame` 调用和错误处理代码

**修复**：删除重复代码块，保留唯一的接收帧逻辑

### 2. 修复 EAGAIN 处理逻辑
**文件**：`FFDecHW.cpp` (avcodec_send_packet 部分)

**修复前**：
```cpp
ret = avcodec_send_packet(codec_ctx_, encodedPacket);
if (ret == 0) {
    packetSent = true;
} else if (ret < 0 && ret != AVERROR(EAGAIN)) {
    // 处理错误
}
```

**修复后**：
```cpp
ret = avcodec_send_packet(codec_ctx_, encodedPacket);
if (ret == 0) {
    packetSent = true;
} else if (ret == AVERROR(EAGAIN)) {
    // 解码器内部缓冲区满，需要先 receive_frame（参考 FFDecSW）
    packetNotConsumed = true;
    logger->debug("FFDecHW::decodeVideo: avcodec_send_packet EAGAIN, decoder buffer full, packet not consumed");
    // 不要在这里返回，继续执行 receive_frame
} else {
    // 处理其他错误
}
```

### 3. 修复返回值约定
**文件**：`FFDecHW.cpp` (avcodec_receive_frame EAGAIN 处理)

**修复前**：直接返回 EAGAIN，导致 VideoThread 判断为错误并 flush

**修复后**：参考 FFDecSW 的返回值约定
```cpp
if (ret == AVERROR(EAGAIN)) {
    if (packetSent && !packetNotConsumed) {
        // packet已消费，但没有输出帧（正常情况）
        logger->debug("FFDecHW::decodeVideo: EAGAIN with packet consumed, no output frame - returning 1");
        return 1; // 特殊值1：packet已消费，无输出帧
    } else if (packetNotConsumed) {
        // packet未消费，需要先receive_frame（正常情况）
        logger->debug("FFDecHW::decodeVideo: EAGAIN with packet not consumed - returning EAGAIN to retry");
        return AVERROR(EAGAIN); // 返回EAGAIN，需要重试
    } else {
        // 其他情况（如flush）
        logger->debug("FFDecHW::decodeVideo: EAGAIN without packet - returning EAGAIN");
        return AVERROR(EAGAIN); // 返回EAGAIN
    }
}
```

### 4. 优化 flush 逻辑
**文件**：`FFDecHW.cpp` (flushBuffers 方法)

**修复**：在 flush 后重置 EAGAIN 计数
```cpp
void FFDecHW::flushBuffers()
{
    logger->debug("FFDecHW::flushBuffers: Flushing decoder buffers");

    if (use_hw_decoder_ && codec_ctx_ && hw_decoder_->isInitialized()) {
        logger->debug("FFDecHW::flushBuffers: Flushing hardware decoder buffers");
        avcodec_flush_buffers(codec_ctx_);
        logger->debug("FFDecHW::flushBuffers: Hardware decoder buffers flushed");

        // 重置 EAGAIN 计数（flush 后重新开始计数）
        consecutiveEAGAINCount_ = 0;
        logger->debug("FFDecHW::flushBuffers: EAGAIN count reset");
    }

    // ... 其余代码
}
```

## 返回值约定

参考 FFDecSW 的成熟实现，FFDecHW 现在遵循以下返回值约定：

| 返回值 | 含义 | VideoThread 处理 |
|--------|------|------------------|
| `0` | 成功，有输出帧，packet 已消费 | pop packet，渲染帧 |
| `1` | packet 已消费，但没有输出帧（需要更多数据） | pop packet，继续循环 |
| `2` | 有输出帧，但 packet 未消费（解码器缓冲区满） | 不 pop packet，渲染帧 |
| `AVERROR(EAGAIN)` | 需要更多数据，packet 未消费 | 继续循环 |
| `AVERROR_EOF` | 播放结束 | 特殊处理 |
| 其他负数 | 错误 | 记录日志，pop packet |

## 编译验证

**编译命令**：`WZMediaPlay\build.bat`

**编译结果**：✅ 成功

**编译输出**：
```
FFDecHW.cpp
...
WZMediaPlay.vcxproj -> E:\WZMediaPlayer_2025\WZMediaPlay\x64\Debug\WZMediaPlay.exe

========================================
编译成功!
========================================
```

## 关键改进

### 1. 与 FFDecSW 对齐
- 完全参考 FFDecSW 的 EAGAIN 处理逻辑
- 统一返回值约定
- 统一 packet 状态跟踪（packetSent、packetNotConsumed）

### 2. Seeking 优化
- flush 后重置 EAGAIN 计数，避免误判为硬件解码失败
- 确保解码器状态干净，避免第一帧解码失败

### 3. 代码简化
- 删除重复代码块
- 统一错误处理逻辑
- 改进日志输出，便于调试

## 测试建议

1. **第一帧解码测试**：
   - 播放视频，观察第一帧是否正常解码
   - 检查日志中是否有 "EAGAIN - unexpected state" 错误

2. **Seeking 测试**：
   - 拖动进度条进行 seeking
   - 观察解码器状态是否正确恢复
   - 检查 seeking 后视频是否正常播放

3. **硬件解码回退测试**：
   - 观察硬件解码失败时是否正确回退到软件解码
   - 检查回退后播放是否正常

## 后续工作

根据 `docs/TODO.md`，优先级较高的后续任务：
1. **Phase 1.2**：改进时钟连续性检查，扩大容忍范围
2. **Phase 2**：验证硬件解码器初始化和硬件帧转换
3. **Phase 3**：验证 Seeking 后音视频时钟重置

## 文件清单

修改的文件：
- `WZMediaPlay\videoDecoder\FFDecHW.cpp`

相关文件（参考）：
- `WZMediaPlay\videoDecoder\FFDecSW.cpp`
- `WZMediaPlay\videoDecoder\VideoThread.cpp`
