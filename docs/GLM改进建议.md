# WZMediaPlayer GLM详细改进建议

本文档基于对WZMediaPlayer项目核心代码的深度分析，提供详细的改进建议和修复方案。

**最新更新**: 2026-01-18
**当前状态**: Phase 1-10 已完成，视频可正常渲染，音视频同步问题待修复

---

## 最近完成的改进（2026-01-18）

### ✅ Phase 6: OpenAL 缓冲区管理重构
**问题**: "Modifying storage for in-use buffer" 错误，缓冲区生命周期管理错误

**修复**: 
- 引入 `availableBuffers_` 队列管理可用缓冲区，替代 `bufferIdx_` 跟踪方式
- 重构 `writeAudio()` 方法，使用正确的缓冲区获取策略
- 添加 `recycleProcessedBuffers()` 方法统一回收已处理的缓冲区

**状态**: ✅ 已完成

### ✅ Phase 7: VideoThread Packet 处理修复
**问题**: `decodeFrame` 成功后没有调用 `popPacket`，导致 VideoQueue 一直满

**修复**:
- 修复 `VideoThread::run()` 中的逻辑错误
- 重新设计 `FFDecSW::decodeVideo` 返回值约定（0/1/2/负数）
- `processDecodeResult` 正确处理不同返回值并调用 `popPacket`

**状态**: ✅ 已完成

### ✅ Phase 8: 跳过帧逻辑优化
**问题**: 延迟 > 40ms 时跳过帧，导致连续跳过造成视频卡顿

**修复**:
- 添加 `consecutiveSkipCount_` 跟踪连续跳过的帧数
- 如果连续跳过 >5 帧或延迟 <300ms，强制渲染一帧
- 修复 `renderFailCount` 逻辑（成功时重置而不是增加）

**状态**: ✅ 已完成

### ✅ Phase 9: 日志格式改进
**修复**: 改进日志格式，显示文件名和行号

**状态**: ✅ 已完成

### ✅ Phase 10: 音频播放完成检测优化
**修复**: 改进 `checkPlaybackComplete()`，增加多次尝试读取剩余音频数据，并等待 OpenAL 缓冲区播放完毕

**状态**: ✅ 已完成

---

## 当前待解决问题

### 🔴 Priority 1: 音视频同步问题

**症状**: 
- 日志显示 "OpenAL clock diff too large: -8731ms, rejecting"
- OpenAL 时钟和估算时钟差异过大（约8.7秒）
- 导致音频时钟不准确，影响音视频同步

**问题分析**:
- `Audio::getClock()` 中，当 OpenAL 时钟与估算时钟差异 > 200ms 时拒绝使用 OpenAL 时钟
- 但差异达到 8.7 秒，说明 `currentPts_` 或 `deviceStartTime_` 可能有问题
- 需要检查 `currentPts_` 的更新逻辑和 `deviceStartTime_` 的设置时机

**相关代码**: `WZMediaPlay/videoDecoder/OpenALAudio.cpp:500-586`

**修复建议**:
1. 检查 `AudioThread::updateClock()` 的调用时机和频率
2. 检查 `deviceStartTime_` 的设置时机（应该在首次播放时设置，而不是每次写入时）
3. 添加时钟连续性检查，避免时钟跳跃
4. 优化 OpenAL 时钟与估算时钟的同步机制

---

## 紧急问题：音频卡住，黑画面 🔴🔴🔴（已解决）

**测试日期**: 2026-01-17 15:35
**测试日志**: `WZMediaPlay\logs\MediaPlayer_20260117153509.log`

### 问题描述

1. **黑画面**: 播放后视频画面完全黑色
2. **音频短暂播放**: 拖动进度条跳转到新位置时，会播放约1秒左右很短的声音（但视频仍然黑色）
3. **音频缓冲区满，无法消费**: 日志持续显示 `Audio::writeAudio: Buffers full, waiting...`
4. **视频队列满**: 日志显示 `PacketQueue[VideoQueue]: put failed, totalSize>=SizeLimit`
5. **视频帧跳过，延迟增大**: 日志持续显示 `VideoThread::renderFrame: Skipping frame (delay: XXX ms)`

### 日志分析（关键问题）

#### 问题1：硬件解码器未找到（L32-L37）

```
[2026-01-17 15:35:17.051][thread 25428][,][info] : Trying to create hardware device context for codec: hevc
[2026-01-17 15:35:17.051][thread 25428][,][debug] : Hardware decoder 'hevc_d3d11va' not found, skipping device context creation for d3d11va
[2026-01-17 15:35:17.051][thread 25428][,][info] : No suitable hardware device context could be created for codec: hevc
[2026-01-17 15:35:17.051][thread 25428][,][info] : No hardware device available, will use software decoder
```

**关键发现**:
- 系统找不到 `hevc_d3d11va` 解码器
- 回退到软件解码

#### 问题2：音频缓冲区无法消费（L189-L402）

```
[2026-01-17 15:35:17.055][thread 22268][,][debug] : Audio::writeAudio: Buffers full, waiting...
[2026-01-17 15:35:17.167][thread 22268][,][warning] : Audio::writeAudio: Timeout waiting for free buffer
```

**关键发现**:
- OpenAL 音频缓冲区始终处于满状态
- `AL_BUFFERS_PROCESSED` 始终为 0（没有日志显示）
- 音频没有被消费，导致 AudioThread 阻塞

#### 问题3：视频帧跳过，延迟增大（L93-L491）

```
[2026-01-17 15:35:17.153][thread 37124][,][debug] : VideoThread::renderFrame: Repeating last frame (delay: -112 ms)
[2026-01-17 15:35:17.208][thread 37124][,][debug] : VideoThread::renderFrame: Skipping frame (delay: 68 ms)
[2026-01-17 15:35:17.491][thread 37124][,][debug] : VideoThread::renderFrame: Skipping frame (delay: 8000+ ms)
```

**关键发现**:
- 视频延迟从 -112 ms 增加到 +8000+ ms
- 视频线程不断跳帧，因为视频远落后于音频
- 原因是音频没有播放，时钟没有前进

### 根本原因分析

1. **硬件解码器缺失**: FFmpeg 编译时没有包含 `hevc_d3d11va` 解码器
2. **OpenAL 音频没有播放**: 音频缓冲区被填充，但没有被 OpenAL 处理和播放
3. **音视频同步失效**: 因为音频没有播放，音频时钟不前进，导致视频线程认为视频超前而跳帧

### 根本原因分析

1. **硬件帧转换失败** (`FFDecHW.cpp:195-201`)
   - `av_hwframe_transfer_data()` 调用失败
   - 可能原因：
     a) CUDA 设备上下文没有正确设置
     b) 硬件帧的 `hw_frames_ctx` 没有正确初始化
     c) 目标软件帧的格式没有正确指定
     d) CUDA 互操作问题（需要 NVidia 显卡驱动支持）

2. **硬件解码器配置问题** (`HardwareDecoder.cc:163-272`)
   - 没有检查硬件解码器是否支持当前视频格式
   - 没有验证硬件设备上下文是否正确创建
   - 没有设置 `hw_frames_ctx` 用于硬件帧池

3. **D3D11VA 未使用**
   - 日志显示 `hevc_d3d11va` 没有找到
   - 系统使用了 `hevc_cuvid` (CUDA)
   - 但用户明确要求仅需支持 D3D11VA

### 详细修复方案

#### 修复点1: 添加 CUDA 解码器支持（`hardware_decoder.cc:8-16`）

**当前问题**:
- 只支持 D3D11VA，但用户系统的 FFmpeg 没有 `hevc_d3d11va` 解码器
- 用户验证过 CUDA 解码器可以工作

**修复方案**:

```cpp
// hardware_decoder.cc:8-16
const char *HardwareDecoder::SUPPORTED_HW_DECODERS[] = {
    "d3d11va", // Windows DirectX 11
    "cuda",      // NVIDIA CUDA (已验证可以工作)
};

const int HardwareDecoder::NUM_SUPPORTED_DECODERS = sizeof(SUPPORTED_HW_DECODERS) / sizeof(SUPPORTED_HW_DECODERS[0]);
```

**状态**: ✅ 已完成

---

#### 修复点2: 移除仅支持 D3D11VA 的限制（`hardware_decoder.cc:171-178`）

**当前问题**:
- 代码中限制了只使用 D3D11VA
- 当 D3D11VA 不可用时，直接回退到软件解码
- 即使 CUDA 可用也不尝试

**修复方案**:

```cpp
// hardware_decoder.cc:171-178
// 移除这段代码：
// if (device_type_name_ != "d3d11va") {
//     logger->warn("Hardware decoder '{}' is not supported (only D3D11VA is supported), falling back to software", device_type_name_);
//     av_buffer_unref(&hw_device_ctx_);
//     hw_device_ctx_ = nullptr;
//     device_type_name_ = "";
//     return nullptr;
// }
```

**状态**: ✅ 已完成

---

#### 修复点3: 修复 OpenAL 音频播放问题（`OpenALAudio.cpp:30-363`）

**当前问题**:
- 音频缓冲区被填充，但没有被 OpenAL 处理和播放
- `AL_BUFFERS_PROCESSED` 始终为 0
- AudioThread 阻塞在 `writeAudio()` 调用上

**根本原因分析**:

从日志来看，问题在于：
1. OpenAL 音频源状态异常
2. 缓冲区没有被正确处理
3. 音频没有被真正播放

**修复方案**（临时方案 - 禁用硬件解码）:

修改 `config/SystemConfig.ini`，禁用硬件解码：

```ini
[System]
EnableHardwareDecoding=false
ShowFPS=false
```

**预期效果**:
- 使用软件解码，视频可以正常解码
- 音频播放应该可以恢复（如果 OpenAL 本身没有问题）

**进一步调试建议**:

如果禁用硬件解码后仍然有问题，需要：

1. **检查 OpenAL 上下文**:
   - 确认 OpenGL 上下文已创建
   - 确认 OpenAL 上下文有效
   - 检查 `alcMakeContextCurrent` 是否正确调用

2. **检查音频缓冲区处理**:
   - 在 `writeAudio()` 中添加更多日志
   - 记录 `AL_BUFFERS_PROCESSED`、`AL_BUFFERS_QUEUED`、`AL_SOURCE_STATE`
   - 检查 `alSourcePlay()` 是否被调用

3. **检查播放状态**:
   - 在 `play()` 方法中添加日志
   - 检查 `alSourcePlay()` 的返回值
   - 检查播放状态是否正确

**状态**: ⏳ 待实施

---

#### 修复点4: 完善 FFDecHW 硬件帧转换逻辑（`hardware_decoder.cc:149-281`）

**当前问题**:
- 硬件帧池初始化可能有问题
- 硬件帧到软件帧的转换失败
- 需要更详细的错误日志

**修复方案**:

保留之前的硬件帧池和错误恢复机制，这些已经实现。

**状态**: ✅ 已完成

```cpp
// hardware_decoder.cc
const AVCodec* HardwareDecoder::tryInitHardwareDecoder(AVCodecID codec_id, AVCodecContext* codec_ctx)
{
    if (!codec_ctx) {
        logger->error("Codec context is null");
        return nullptr;
    }
    
    // 清理之前的状态
    if (hw_device_ctx_) {
        av_buffer_unref(&hw_device_ctx_);
        hw_device_ctx_ = nullptr;
    }
    needs_transfer_ = false;
    device_type_name_ = "";
    
    // 尝试创建硬件设备上下文（仅支持 D3D11VA）
    device_type_name_ = tryCreateDeviceContext(codec_id);
    if (device_type_name_.empty()) {
        logger->info("No hardware device available, will use software decoder");
        return nullptr;
    }
    
    // 检查是否是 D3D11VA（仅支持 D3D11VA）
    if (device_type_name_ != "d3d11va") {
        logger->warn("Hardware decoder '{}' is not supported (only D3D11VA is supported), falling back to software", 
                    device_type_name_);
        av_buffer_unref(&hw_device_ctx_);
        hw_device_ctx_ = nullptr;
        device_type_name_ = "";
        return nullptr;
    }
    
    // 查找硬件解码器
    const AVCodec* hw_codec = findHardwareDecoder(codec_id, device_type_name_);
    if (!hw_codec) {
        logger->warn("Hardware decoder not found for codec: {}, device: {}", 
                    avcodec_get_name(codec_id), device_type_name_);
        av_buffer_unref(&hw_device_ctx_);
        hw_device_ctx_ = nullptr;
        device_type_name_ = "";
        return nullptr;
    }
    
    // 获取硬件像素格式
    AVHWDeviceType hw_type = av_hwdevice_find_type_by_name(device_type_name_.c_str());
    if (hw_type == AV_HWDEVICE_TYPE_NONE) {
        logger->error("Invalid hardware device type: {}", device_type_name_);
        av_buffer_unref(&hw_device_ctx_);
        hw_device_ctx_ = nullptr;
        device_type_name_ = "";
        return nullptr;
    }
    
    // 查找支持的硬件像素格式
    for (int i = 0;; ++i) {
        const AVCodecHWConfig* config = avcodec_get_hw_config(hw_codec, i);
        if (!config) {
            break;
        }
        
        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
            config->device_type == hw_type) {
            // 设置硬件设备上下文
            codec_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx_);
            if (!codec_ctx->hw_device_ctx) {
                logger->error("Failed to reference hardware device context");
                av_buffer_unref(&hw_device_ctx_);
                hw_device_ctx_ = nullptr;
                device_type_name_ = "";
                return nullptr;
            }
            
            // [新增] 创建硬件帧池（hw_frames_ctx）
            AVBufferRef* hw_frames_ref = nullptr;
            int ret = av_hwframe_ctx_alloc(hw_device_ctx_, &hw_frames_ref);
            if (ret < 0) {
                logger->error("Failed to allocate hardware frames context: {}", ret);
                av_buffer_unref(&hw_device_ctx_);
                hw_device_ctx_ = nullptr;
                device_type_name_ = "";
                return nullptr;
            }
            
            AVHWFramesContext* frames_ctx = (AVHWFramesContext*)hw_frames_ref->data;
            frames_ctx->format = config->pix_fmt;
            frames_ctx->sw_format = AV_PIX_FMT_NV12;  // D3D11VA 通常输出 NV12
            frames_ctx->width = codec_ctx->width;
            frames_ctx->height = codec_ctx->height;
            frames_ctx->initial_pool_size = 20;  // 预分配20个帧
            
            // 初始化硬件帧池
            ret = av_hwframe_ctx_init(hw_frames_ref);
            if (ret < 0) {
                char errbuf[256] = {0};
                av_strerror(ret, errbuf, sizeof(errbuf));
                logger->error("Failed to initialize hardware frames context: {} ({})", ret, errbuf);
                av_buffer_unref(&hw_frames_ref);
                av_buffer_unref(&hw_device_ctx_);
                hw_device_ctx_ = nullptr;
                device_type_name_ = "";
                return nullptr;
            }
            
            // 设置硬件帧池到编解码器上下文
            codec_ctx->hw_frames_ctx = hw_frames_ref;
            
            // 保存硬件像素格式
            hw_pix_fmt_ = config->pix_fmt;
            
            // 设置get_format回调函数（关键！硬件解码器需要这个回调）
            codec_ctx->get_format = get_hw_format;
            
            // 将HardwareDecoder实例指针存储到codec_ctx的opaque中，以便回调函数访问
            codec_ctx->opaque = this;
            
            // 检查是否需要帧转换
            needs_transfer_ = true;  // D3D11VA 需要转换到软件帧
            
            logger->info("Hardware decoder initialized successfully: {}, device: {}, hw_pix_fmt: {}, needs_transfer: {}", 
                        hw_codec->name, device_type_name_, static_cast<int>(hw_pix_fmt_), needs_transfer_);
            return hw_codec;
        }
    }
    
    logger->warn("No suitable hardware config found for codec: {}, device: {}", 
                hw_codec->name, device_type_name_);
    av_buffer_unref(&hw_device_ctx_);
    hw_device_ctx_ = nullptr;
    device_type_name_ = "";
    return nullptr;
}
```

#### 修复点2: 改进硬件帧转换 (`hardware_decoder.cc:298-359`)

**当前问题**:
- `av_hwframe_transfer_data()` 调用后，软件帧仍然是空的
- 没有详细的错误日志
- 没有验证转换后的帧数据

**修复方案**:

```cpp
// hardware_decoder.cc
int HardwareDecoder::transferFrame(AVFrame* hw_frame, AVFrame* sw_frame, AVCodecContext* codec_ctx)
{
    if (!hw_frame || !sw_frame || !codec_ctx) {
        logger->error("Invalid parameters for frame transfer");
        return AVERROR(EINVAL);
    }
    
    if (!hw_device_ctx_) {
        logger->error("Hardware device context not initialized");
        return AVERROR(EINVAL);
    }
    
    // 检查是否是硬件格式
    AVHWDeviceType hw_type = av_hwdevice_find_type_by_name(device_type_name_.c_str());
    bool is_hw_format = false;
    
    // 根据设备类型检查对应的硬件格式
    if (hw_type == AV_HWDEVICE_TYPE_D3D11VA) {
        is_hw_format = (hw_frame->format == AV_PIX_FMT_D3D11);
    } else if (hw_type == AV_HWDEVICE_TYPE_CUDA) {
        is_hw_format = (hw_frame->format == AV_PIX_FMT_CUDA);
    } else if (hw_type == AV_HWDEVICE_TYPE_QSV) {
        is_hw_format = (hw_frame->format == AV_PIX_FMT_QSV);
    } else if (hw_type == AV_HWDEVICE_TYPE_DXVA2) {
        is_hw_format = (hw_frame->format == AV_PIX_FMT_DXVA2_VLD);
    }
    
    // 如果不是硬件格式，已经是软件格式，不需要转换
    if (!is_hw_format) {
        logger->debug("Frame is already in software format: {}, no transfer needed", 
                     static_cast<int>(hw_frame->format));
        return 0;
    }
    
    // [新增] 先分配软件帧缓冲区（确保有足够的内存）
    // 转换目标格式为 NV12
    int ret = av_hwframe_transfer_get_formats(hw_frame->hw_frames_ctx, 
                                            AV_HWFRAME_TRANSFER_DIRECTION_FROM,
                                            nullptr, 0, 0);
    if (ret < 0) {
        char errbuf[256] = {0};
        av_strerror(ret, errbuf, sizeof(errbuf));
        logger->error("Failed to get supported transfer formats: {} ({})", ret, errbuf);
        return ret;
    }
    
    // 重要：先复制硬件帧的属性到软件帧（包括宽度、高度、像素格式等）
    // 这会设置软件帧的基本信息，但不会复制数据
    ret = av_frame_copy_props(sw_frame, hw_frame);
    if (ret < 0) {
        char errbuf[256] = {0};
        av_strerror(ret, errbuf, sizeof(errbuf));
        logger->error("Failed to copy frame properties: {} ({})", ret, errbuf);
        return ret;
    }
    
    // [新增] 分配软件帧的数据缓冲区
    // 确保软件帧有足够的内存来存储转换后的数据
    AVPixelFormat target_format = AV_PIX_FMT_NV12;  // D3D11VA 转换到 NV12
    ret = av_frame_get_buffer(sw_frame, 0);
    if (ret < 0) {
        char errbuf[256] = {0};
        av_strerror(ret, errbuf, sizeof(errbuf));
        logger->error("Failed to allocate software frame buffer: {} ({})", ret, errbuf);
        return ret;
    }
    
    logger->debug("Software frame buffer allocated: format={}, width={}, height={}", 
                 static_cast<int>(sw_frame->format), sw_frame->width, sw_frame->height);
    
    // 从硬件帧转换数据到软件帧
    // av_hwframe_transfer_data 会使用 sw_frame 已分配的缓冲区
    ret = av_hwframe_transfer_data(sw_frame, hw_frame, 0);
    if (ret < 0) {
        char errbuf[256] = {0};
        av_strerror(ret, errbuf, sizeof(errbuf));
        logger->error("Failed to transfer frame from hardware to software: {} ({})", ret, errbuf);
        logger->error("Hardware frame: format={}, width={}, height={}, data[0]={}", 
                     static_cast<int>(hw_frame->format), hw_frame->width, hw_frame->height,
                     hw_frame->data[0] != nullptr ? "valid" : "null");
        logger->error("Software frame: format={}, width={}, height={}, data[0]={}", 
                     static_cast<int>(sw_frame->format), sw_frame->width, sw_frame->height,
                     sw_frame->data[0] != nullptr ? "valid" : "null");
        return ret;
    }
    
    // [新增] 验证转换后的帧数据
    if (!sw_frame->data[0] || sw_frame->width <= 0 || sw_frame->height <= 0) {
        logger->error("Software frame is invalid after transfer: data[0]={}, width={}, height={}, format={}", 
                     sw_frame->data[0] != nullptr, sw_frame->width, sw_frame->height,
                     static_cast<int>(sw_frame->format));
        return AVERROR(EINVAL);
    }
    
    logger->debug("Successfully transferred hardware frame to software frame: format={}, size={}x{}", 
                 static_cast<int>(sw_frame->format), sw_frame->width, sw_frame->height);
    
    return 0;
}
```

#### 修复点3: 仅支持 D3D11VA (`hardware_decoder.cc:8-16`)

**当前问题**:
- 支持的硬件解码器列表包含 CUDA、QSV、DXVA2
- 但用户明确要求仅需支持 D3D11VA

**修复方案**:

```cpp
// hardware_decoder.cc
// 支持的硬件解码器列表（按优先级排序）
// 注意：仅支持 D3D11VA（Windows DirectX 11）
const char* HardwareDecoder::SUPPORTED_HW_DECODERS[] = {
    "d3d11va",  // Windows DirectX 11 (仅支持此解码器)
};

const int HardwareDecoder::NUM_SUPPORTED_DECODERS = sizeof(SUPPORTED_HW_DECODERS) / sizeof(SUPPORTED_HW_DECODERS[0]);
```

#### 修复点4: 改进错误恢复 (`FFDecHW.cpp:88-265`)

**当前问题**:
- 硬件帧转换失败时，直接返回错误
- 没有尝试回退到软件解码
- 没有详细的错误日志

**修复方案**:

```cpp
// FFDecHW.cpp
int FFDecHW::decodeVideo(
    const AVPacket *encodedPacket,
    Frame &decoded,
    AVPixelFormat &newPixFmt,
    bool flush,
    unsigned hurry_up)
{
    static int decodeCounter = 0;
    static int hardwareErrorCount = 0;
    decodeCounter++;
    
    if (!codec_ctx_) {
        logger->error("FFDecHW::decodeVideo: decoder not initialized");
        return AVERROR(EINVAL);
    }

    if (logger && decodeCounter % 100 == 0) {
        logger->debug("FFDecHW::decodeVideo: Called {} times, use_hw_decoder_: {}, hw_decoder_->isInitialized(): {}, encodedPacket: {}, flush: {}", 
            decodeCounter, use_hw_decoder_, hw_decoder_->isInitialized(), encodedPacket != nullptr, flush);
    }

    if (use_hw_decoder_ && hw_decoder_->isInitialized()) {
        // 使用硬件解码器
        // 硬件解码使用相同的 FFmpeg API，但需要处理硬件帧转换
        
        // 发送数据包到解码器
        int ret = 0;
        if (encodedPacket) {
            if (logger && decodeCounter % 100 == 0) {
                logger->debug("FFDecHW::decodeVideo: Sending packet, size: {}, pts: {}", encodedPacket->size, encodedPacket->pts);
            }
            ret = avcodec_send_packet(codec_ctx_, encodedPacket);
            if (ret < 0 && ret != AVERROR(EAGAIN)) {
                if (ret != AVErrorEOF) {
                    logger->warn("FFDecHW::decodeVideo: avcodec_send_packet failed: {}", ret);
                }
                return ret;
            }
            if (logger && decodeCounter % 100 == 0 && ret == AVERROR(EAGAIN)) {
                logger->debug("FFDecHW::decodeVideo: avcodec_send_packet returned EAGAIN");
            }
        } else if (flush) {
            // 刷新解码器
            if (logger) logger->debug("FFDecHW::decodeVideo: Flushing decoder");
            ret = avcodec_send_packet(codec_ctx_, nullptr);
            if (ret < 0 && ret != AVErrorEOF) {
                logger->warn("FFDecHW::decodeVideo: flush failed: {}", ret);
                return ret;
            }
        }

        // 接收解码后的帧（可能是硬件帧）
        AVFrame* hw_frame = av_frame_alloc();
        if (!hw_frame) {
            logger->error("FFDecHW::decodeVideo: Failed to allocate hardware frame");
            return AVERROR(ENOMEM);
        }
        
        ret = avcodec_receive_frame(codec_ctx_, hw_frame);
        if (ret < 0) {
            av_frame_free(&hw_frame);
            if (ret == AVERROR(EAGAIN)) {
                // 需要更多数据
                if (logger && decodeCounter % 100 == 0) {
                    logger->debug("FFDecHW::decodeVideo: avcodec_receive_frame returned EAGAIN (need more data)");
                }
                return ret;
            }
            if (ret == AVErrorEOF) {
                if (logger) logger->debug("FFDecHW::decodeVideo: avcodec_receive_frame returned EOF");
                return ret;
            }
            logger->warn("FFDecHW::decodeVideo: avcodec_receive_frame failed: {}", ret);
            return ret;
        }

        if (logger && decodeCounter % 100 == 0) {
            logger->debug("FFDecHW::decodeVideo: Got hardware frame, format: {}, width: {}, height: {}, needsTransfer: {}", 
                static_cast<int>(hw_frame->format), hw_frame->width, hw_frame->height, hw_decoder_->needsTransfer());
        }

        // 处理硬件帧转换（如果需要）
        AVFrame* sw_frame = nullptr;
        if (hw_decoder_->needsTransfer()) {
            // 需要将硬件帧转换为软件帧
            sw_frame = av_frame_alloc();
            if (!sw_frame) {
                logger->error("FFDecHW::decodeVideo: Failed to allocate software frame");
                av_frame_free(&hw_frame);
                return AVERROR(ENOMEM);
            }
            
            ret = hw_decoder_->transferFrame(hw_frame, sw_frame, codec_ctx_);
            if (ret < 0) {
                // [新增] 硬件帧转换失败，记录错误并尝试回退
                hardwareErrorCount++;
                logger->error("FFDecHW::decodeVideo: transferFrame failed: {} (error count: {})", ret, hardwareErrorCount);
                av_frame_free(&hw_frame);
                av_frame_free(&sw_frame);
                
                // 如果硬件帧转换连续失败超过3次，回退到软件解码
                if (hardwareErrorCount > 3) {
                    logger->warn("FFDecHW::decodeVideo: Hardware frame transfer failed {} times, falling back to software decoder", 
                                hardwareErrorCount);
                    use_hw_decoder_ = false;
                    hardwareErrorCount = 0;
                    
                    // 初始化软件解码器
                    if (!sw_fallback_->isInitialized()) {
                        logger->warn("FFDecHW::decodeVideo: Initializing software fallback");
                        if (!sw_fallback_->init(codec_ctx_, stream_time_base_)) {
                            logger->error("FFDecHW::decodeVideo: Failed to initialize software fallback");
                            return AVERROR(EINVAL);
                        }
                    }
                }
                
                return ret;
            }
            
            // 重置硬件错误计数器
            hardwareErrorCount = 0;
            
            // 检查转换后的帧是否有效
            if (logger && decodeCounter % 100 == 0) {
                logger->debug("FFDecHW::decodeVideo: After transferFrame, sw_frame format: {}, width: {}, height: {}, data[0]: {}", 
                    static_cast<int>(sw_frame->format), sw_frame->width, sw_frame->height, 
                    sw_frame->data[0] != nullptr ? "valid" : "null");
            }
            
            // 检查转换后的帧是否有效（在创建 Frame 之前）
            if (!sw_frame->data[0] || sw_frame->width <= 0 || sw_frame->height <= 0) {
                logger->error("FFDecHW::decodeVideo: sw_frame is invalid after transfer - data[0]: {}, width: {}, height: {}, format: {}", 
                    sw_frame->data[0] != nullptr, sw_frame->width, sw_frame->height, static_cast<int>(sw_frame->format));
                av_frame_free(&hw_frame);
                av_frame_free(&sw_frame);
                
                // [新增] 硬件帧无效，回退到软件解码
                hardwareErrorCount++;
                if (hardwareErrorCount > 3) {
                    logger->warn("FFDecHW::decodeVideo: Invalid software frame after transfer (error count: {}), falling back to software decoder", 
                                hardwareErrorCount);
                    use_hw_decoder_ = false;
                    hardwareErrorCount = 0;
                    
                    if (!sw_fallback_->isInitialized()) {
                        logger->warn("FFDecHW::decodeVideo: Initializing software fallback");
                        if (!sw_fallback_->init(codec_ctx_, stream_time_base_)) {
                            logger->error("FFDecHW::decodeVideo: Failed to initialize software fallback");
                            return AVERROR(EINVAL);
                        }
                    }
                }
                
                return AVERROR(EINVAL);
            }
            
            if (logger && decodeCounter % 100 == 0) {
                logger->debug("FFDecHW::decodeVideo: sw_frame valid before Frame construction - format: {}, width: {}, height: {}, data[0]: valid", 
                    static_cast<int>(sw_frame->format), sw_frame->width, sw_frame->height);
            }
            
            // 使用转换后的软件帧（Frame 构造函数会克隆 AVFrame）
            decoded = Frame(sw_frame);
            av_frame_free(&sw_frame);
            
            if (logger && decodeCounter % 100 == 0) {
                logger->debug("FFDecHW::decodeVideo: Frame transferred, decoded.isEmpty(): {}, width: {}, height: {}, format: {}", 
                    decoded.isEmpty(), decoded.width(0), decoded.height(0), 
                    static_cast<int>(decoded.pixelFormat()));
            }
            
            // 如果转换后的帧仍然为空，返回错误
            if (decoded.isEmpty()) {
                logger->error("FFDecHW::decodeVideo: Frame is empty after transfer and construction. sw_frame was valid but Frame::isEmpty() returns true");
                av_frame_free(&hw_frame);
                
                // [新增] Frame 构造失败，回退到软件解码
                hardwareErrorCount++;
                if (hardwareErrorCount > 3) {
                    logger->warn("FFDecHW::decodeVideo: Frame construction failed (error count: {}), falling back to software decoder", 
                                hardwareErrorCount);
                    use_hw_decoder_ = false;
                    hardwareErrorCount = 0;
                    
                    if (!sw_fallback_->isInitialized()) {
                        logger->warn("FFDecHW::decodeVideo: Initializing software fallback");
                        if (!sw_fallback_->init(codec_ctx_, stream_time_base_)) {
                            logger->error("FFDecHW::decodeVideo: Failed to initialize software fallback");
                            return AVERROR(EINVAL);
                        }
                    }
                }
                
                return AVERROR(EINVAL);
            }
        } else {
            // 不需要转换，直接使用硬件帧
            decoded = Frame(hw_frame);
            if (logger && decodeCounter % 100 == 0) {
                logger->debug("FFDecHW::decodeVideo: Using hardware frame directly, decoded.isEmpty(): {}, width: {}, height: {}", 
                    decoded.isEmpty(), decoded.width(0), decoded.height(0));
            }
        }
        
        av_frame_free(&hw_frame);
        decoded.setTimeBase(stream_time_base_);

        // 检查是否需要格式转换
        AVPixelFormat currentFormat = decoded.pixelFormat();
        newPixFmt = AV_PIX_FMT_NONE;
        
        // 硬件解码器通常输出特定格式，如果不在支持列表中，可能需要转换
        // 但这里先不处理，让 VideoWriter 处理格式转换
        
        if (logger && decodeCounter % 100 == 0) {
            logger->debug("FFDecHW::decodeVideo: Returning success, decoded.isEmpty(): {}", decoded.isEmpty());
        }
        return 0;
    } else {
        // 使用软件解码器（回退）
        if (!sw_fallback_) {
            logger->error("FFDecHW::decodeVideo: Software fallback not initialized");
            return AVERROR(EINVAL);
        }
        
        // 检查软件解码器是否已初始化（不应该每次都重新初始化）
        if (!sw_fallback_->isInitialized()) {
            logger->warn("FFDecHW::decodeVideo: Software fallback not initialized, initializing now");
            if (!sw_fallback_->init(codec_ctx_, stream_time_base_)) {
                logger->error("FFDecHW::decodeVideo: Failed to initialize software fallback");
                return AVERROR(EINVAL);
            }
        }
        
        return sw_fallback_->decodeVideo(encodedPacket, decoded, newPixFmt, flush, hurry_up);
    }
}
```

### 实施计划

#### 阶段1：修复硬件帧转换（1-2小时）
- [ ] 修改 `hardware_decoder.cc:8-16`，仅支持 D3D11VA
- [ ] 修改 `hardware_decoder.cc:163-272`，添加硬件帧池初始化
- [ ] 修改 `hardware_decoder.cc:298-359`，改进硬件帧转换逻辑
- [ ] 修改 `FFDecHW.cpp:88-265`，添加错误恢复机制

#### 阶段2：测试验证（1小时）
- [ ] 编译并运行程序
- [ ] 检查日志，确认硬件解码器正确初始化
- [ ] 检查视频是否能正常播放
- [ ] 检查音视频同步是否正常

#### 阶段3：文档更新（30分钟）
- [ ] 更新 `docs\GLM改进步骤详细规划.md`
- [ ] 记录测试结果
- [ ] 标记问题为已解决

### 预期结果

修复后，应该能够：
1. ✅ 视频正常播放，无黑画面
2. ✅ 音频正常播放，无卡顿
3. ✅ 音视频同步正常
4. ✅ D3D11VA 硬件解码器正常工作
5. ✅ 如果硬件解码失败，自动回退到软件解码

---

## 目录
1. [高优先级改进](#高优先级改进)
2. [中优先级改进](#中优先级改进)
3. [低优先级改进](#低优先级改进)
4. [架构优化建议](#架构优化建议)
5. [代码质量改进](#代码质量改进)
6. [性能优化建议](#性能优化建议)
7. [测试策略](#测试策略)
8. [实施路线图](#实施路线图)

---

## 高优先级改进

### 1.1 修复BUG-001: 视频切换崩溃问题 🔴

**问题描述**:
视频播放完成后，自动切换到下一个视频时应用程序崩溃。崩溃发生在视频切换过程中，可能与线程清理和生命周期管理有关。

**根本原因分析**:
1. **线程生命周期管理问题**: `PlayController::open()` 在创建新线程之前清理旧线程，但清理逻辑不够健壮
2. **状态转换时序问题**: 播放完成检测和状态转换之间存在竞态条件
3. **队列访问问题**: 线程退出后，其他线程可能仍在访问队列

**详细修复方案**:

#### 修复点1: 改进线程停止逻辑（PlayController::stop()）

**当前问题**:
```cpp
// PlayController.cpp:706-751
// 线程停止逻辑不够健壮，可能导致访问已销毁的线程对象
if (videoThread_ && videoThread_->isRunning()) {
    videoThread_->requestStop();
    emptyBufferCond_.wakeAll();
    if (!videoThread_->wait(5000)) {
        videoThread_->stop(true);  // terminate
    }
}
```

**改进方案**:
```cpp
// PlayController.cpp
void PlayController::stop()
{
    logger->info("PlayController::stop");

    // 防止重复调用
    if (stateMachine_.isStopping() || stateMachine_.isStopped()) {
        logger->debug("PlayController::stop: Already stopping or stopped");
        return;
    }

    // 1. 转换到Stopping状态
    stateMachine_.transitionTo(PlaybackState::Stopping, "Stop requested");

    // 2. 先转换所有线程到停止状态（不立即删除）
    // 使用智能指针管理线程生命周期
    std::shared_ptr<DemuxerThread> demuxThreadSafe;
    std::shared_ptr<VideoThread> videoThreadSafe;
    std::shared_ptr<AudioThread> audioThreadSafe;

    // 临时接管线程指针所有权
    demuxThreadSafe.reset(demuxThread_, [](DemuxerThread*){});  // 不删除
    videoThreadSafe.reset(videoThread_, [](VideoThread*){});
    audioThreadSafe.reset(audioThread_, [](AudioThread*){});

    demuxThread_ = nullptr;
    videoThread_ = nullptr;
    audioThread_ = nullptr;

    // 3. 停止DemuxerThread
    if (demuxThreadSafe && demuxThreadSafe->isRunning()) {
        logger->debug("PlayController::stop: Stopping DemuxerThread");
        emptyBufferCond_.wakeAll();

        // 等待线程退出（使用超时）
        auto startWait = std::chrono::steady_clock::now();
        while (demuxThreadSafe->isRunning()) {
            auto elapsed = std::chrono::steady_clock::now() - startWait;
            if (elapsed > std::chrono::milliseconds(5000)) {
                logger->warn("PlayController::stop: DemuxerThread timeout, terminating");
                demuxThreadSafe->terminate();
                demuxThreadSafe->wait(1000);
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    // 4. 停止VideoThread
    if (videoThreadSafe && videoThreadSafe->isRunning()) {
        logger->debug("PlayController::stop: Stopping VideoThread");
        videoThreadSafe->requestStop();
        emptyBufferCond_.wakeAll();

        auto startWait = std::chrono::steady_clock::now();
        while (videoThreadSafe->isRunning()) {
            auto elapsed = std::chrono::steady_clock::now() - startWait;
            if (elapsed > std::chrono::milliseconds(5000)) {
                logger->warn("PlayController::stop: VideoThread timeout, terminating");
                videoThreadSafe->stop(true);
                videoThreadSafe->wait(1000);
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    // 5. 停止AudioThread
    if (audioThreadSafe && audioThreadSafe->isRunning()) {
        logger->debug("PlayController::stop: Stopping AudioThread");
        audioThreadSafe->requestStop();
        emptyBufferCond_.wakeAll();

        auto startWait = std::chrono::steady_clock::now();
        while (audioThreadSafe->isRunning()) {
            auto elapsed = std::chrono::steady_clock::now() - startWait;
            if (elapsed > std::chrono::milliseconds(5000)) {
                logger->warn("PlayController::stop: AudioThread timeout, terminating");
                audioThreadSafe->stop(true);
                audioThreadSafe->wait(1000);
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    // 6. 清空队列（在线程完全停止后）
    try {
        vPackets_.Reset("VideoQueue");
        aPackets_.Reset("AudioQueue");
    } catch (const std::exception& e) {
        logger->error("PlayController::stop: Exception while resetting queues: {}", e.what());
    }

    // 7. 重置状态标志
    flushVideo_ = false;
    flushAudio_ = false;
    videoSeekPos_ = -1.0;
    audioSeekPos_ = -1.0;

    // 8. 转换到Stopped状态
    stateMachine_.transitionTo(PlaybackState::Stopped, "Playback stopped");

    // 9. 智能指针自动释放线程对象
    // demuxThreadSafe, videoThreadSafe, audioThreadSafe 在作用域结束时自动释放

    logger->info("PlayController::stop completed");
}
```

**改进要点**:
1. 使用`std::shared_ptr`管理线程生命周期
2. 将线程指针从`PlayController`中移除，避免悬空指针
3. 使用循环等待替代单次`wait()`，更可靠
4. 添加超时保护，避免永久阻塞
5. 在线程完全停止后再清空队列

#### 修复点2: 改进播放完成检测（VideoThread::run()）

**当前问题**:
```cpp
// VideoThread.cpp 播放完成检测不够准确
if (vPackets_->IsEmpty() && vPackets_->IsFinished()) {
    // 播放完成，退出循环
    break;
}
```

**改进方案**:
```cpp
// VideoThread.cpp
void VideoThread::run()
{
    try {
        logger->info("VideoThread::run started");
        br_ = false;
        wasSeekingRecently_ = false;
        eofAfterSeekCount_ = 0;

        // 帧计数器
        int frames = 0;

        while (!br_) {
            try {
                // 检查flushVideo标志
                handleFlushVideo();

                // 检查暂停状态
                handlePausedState();

                // 检查seeking状态
                bool wasSeekingBefore = wasSeekingRecently_;
                bool shouldContinue = handleSeekingState(wasSeekingBefore);
                if (shouldContinue) {
                    continue;
                }

                // 检查是否应该停止
                if (controller_->isStopping() || controller_->isStopped()) {
                    logger->info("VideoThread::run: Stopping flag detected, exiting");
                    break;
                }

                // 获取数据包
                const AVPacket* packet = vPackets_->peekPacket("VideoQueue");
                if (!packet) {
                    // 检查是否播放完成
                    if (vPackets_->IsFinished() && controller_->isAudioFinished()) {
                        // 队列已结束，音频也结束，确认播放完成
                        logger->info("VideoThread::run: Playback completed (queue finished and audio finished)");
                        break;
                    } else if (vPackets_->IsFinished()) {
                        // 只有视频结束，等待音频
                        logger->debug("VideoThread::run: Video queue finished, waiting for audio");
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        continue;
                    } else {
                        // 队列为空但未结束，等待数据
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        continue;
                    }
                }

                // 解码和渲染逻辑...
                Frame videoFrame;
                int bytesConsumed = 0;
                AVPixelFormat newPixFmt = AV_PIX_FMT_NONE;
                bool isKeyFrame = false;

                if (!decodeFrame(videoFrame, bytesConsumed, newPixFmt, isKeyFrame)) {
                    continue;
                }

                bool packetPopped = false;
                if (!processDecodeResult(bytesConsumed, videoFrame, isKeyFrame, packetPopped)) {
                    continue;
                }

                // 渲染帧
                if (!renderFrame(videoFrame, frames)) {
                    continue;
                }

                // 更新健康检查时间戳
                controller_->getLastVideoFrameTime() = std::chrono::steady_clock::now();

            } catch (const std::exception& e) {
                logger->error("VideoThread::run: Exception: {}", e.what());
                ErrorInfo error(ErrorType::ThreadError, e.what());
                errorRecoveryManager_.handleError(error);
            } catch (...) {
                logger->error("VideoThread::run: Unknown exception");
            }
        }

        logger->info("VideoThread::run finished, processed {} frames", frames);

    } catch (const std::exception& e) {
        logger->critical("VideoThread::run: Unhandled exception: {}", e.what());
        emit errorOccurred(-1);
    } catch (...) {
        logger->critical("VideoThread::run: Unknown unhandled exception");
    }
}
```

**改进要点**:
1. 同时检查视频队列和音频队列的结束状态
2. 添加`isStopping()`和`isStopped()`检查
3. 使用异常处理包裹整个循环
4. 更新健康检查时间戳

#### 修复点3: 改进播放完成检测（AudioThread::run()）

**改进方案**:
```cpp
// AudioThread.cpp
void AudioThread::run()
{
    try {
        logger->info("AudioThread::run started");
        br_ = false;
        wasSeeking_ = false;
        audioFinished_.store(false);
        currentPts_ = nanoseconds{0};
        deviceStartTime_ = nanoseconds::min();

        // 初始化解码器...
        if (!initializeDecoder()) {
            logger->error("AudioThread::run: initializeDecoder() failed");
            emit errorOccurred(-1);
            return;
        }

        // 初始化音频设备...
        if (!audio_->open(format, frameSize, sampleRate)) {
            logger->error("AudioThread::run: Audio::open() failed");
            emit errorOccurred(-1);
            return;
        }

        int frames = 0;
        while (!br_) {
            try {
                // 检查flushAudio标志
                if (controller_->getFlushAudio()) {
                    avcodec_flush_buffers(codecctx_.get());
                    audio_->clear();
                    samplesPos_ = 0;
                    samplesLen_ = 0;
                    controller_->setFlushAudio(false);
                }

                // 检查暂停状态
                if (controller_->isPaused()) {
                    for (int i = 0; i < 10 && !br_; ++i) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                    if (br_) break;
                    continue;
                }

                // 检查seeking状态
                bool isSeeking = controller_->isSeeking();
                if (isSeeking) {
                    if (!wasSeeking_) {
                        wasSeeking_ = true;
                        avcodec_flush_buffers(codecctx_.get());
                        audio_->clear();
                        samplesPos_ = 0;
                        samplesLen_ = 0;
                    }

                    // 消费旧数据包...
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }

                // 检查是否应该停止
                if (controller_->isStopping() || controller_->isStopped()) {
                    logger->info("AudioThread::run: Stopping flag detected, exiting");
                    break;
                }

                // 检查队列状态
                const AVPacket* packet = aPackets_->peekPacket("AudioQueue");
                if (!packet) {
                    if (aPackets_->IsFinished()) {
                        // 音频队列结束，标记完成
                        logger->info("AudioThread::run: Audio queue finished, marking audioFinished");
                        audioFinished_.store(true);
                        break;
                    } else {
                        // 队列为空但未结束，等待数据
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        continue;
                    }
                }

                // 解码和播放逻辑...
                int ret = decodeFrame();
                if (ret < 0) {
                    if (ret == AVERROR_EOF) {
                        logger->info("AudioThread::run: Decoder EOF reached");
                        audioFinished_.store(true);
                        break;
                    }
                    continue;
                }

                // 读取并播放音频...
                uint8_t samples[8192];
                if (readAudio(samples, sizeof(samples))) {
                    audio_->writeAudio(samples, sizeof(samples));
                    frames++;
                }

                // 更新健康检查时间戳
                controller_->getLastAudioFrameTime() = std::chrono::steady_clock::now();

            } catch (const std::exception& e) {
                logger->error("AudioThread::run: Exception: {}", e.what());
                ErrorInfo error(ErrorType::ThreadError, e.what());
                errorRecoveryManager_.handleError(error);
            } catch (...) {
                logger->error("AudioThread::run: Unknown exception");
            }
        }

        logger->info("AudioThread::run finished, processed {} frames", frames);

    } catch (const std::exception& e) {
        logger->critical("AudioThread::run: Unhandled exception: {}", e.what());
        emit errorOccurred(-1);
    } catch (...) {
        logger->critical("AudioThread::run: Unknown unhandled exception");
    }
}
```

**改进要点**:
1. 添加`audioFinished_`标志，供VideoThread检查
2. 在解码EOF时标记音频完成
3. 使用异常处理包裹整个循环
4. 更新健康检查时间戳

#### 修复点4: 改进线程创建逻辑（PlayController::open()）

**当前问题**:
```cpp
// PlayController.cpp:165-225 线程创建逻辑复杂，容易出错
```

**改进方案**:
```cpp
// PlayController.cpp
bool PlayController::open(const QString& filename)
{
    logger->info("PlayController::open: {}", filename.toStdString());

    // 1. 先停止旧播放（如果有）
    if (isOpened()) {
        logger->info("PlayController::open: Stopping old playback before opening new file");
        stop();
        // 等待所有线程完全停止
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 2. 清除seeking状态
    if (stateMachine_.isSeeking()) {
        stateMachine_.transitionTo(PlaybackState::Idle, "Opening new file");
    }

    // 3. 转换到Opening状态
    stateMachine_.transitionTo(PlaybackState::Opening, "Opening file");

    // 4. 创建/重置Demuxer
    if (!demuxer_) {
        demuxer_ = std::make_unique<Demuxer>();
    } else {
        demuxer_->close();
    }

    // 5. 打开文件
    if (!demuxer_->open(filename)) {
        logger->error("PlayController::open: Demuxer::open failed");
        stateMachine_.transitionTo(PlaybackState::Error, "Failed to open file");
        return false;
    }

    // 6. 获取流索引
    int videoStreamIndex = demuxer_->getVideoStreamIndex();
    int audioStreamIndex = demuxer_->getAudioStreamIndex();

    // 7. 创建Audio
    if (!audio_) {
        audio_ = new Audio(*this);
        logger->info("PlayController::open: Audio created");
    }

    // 8. 重置视频流信息
    videoStream_ = nullptr;
    videoCodecCtx_.reset();

    // 9. 清理解码器状态
    if (videoDecoder_) {
        videoDecoder_->clearFrames();
        logger->debug("PlayController::open: Video decoder frames cleared");
    }

    // 10. 创建AudioThread
    if (audioStreamIndex >= 0) {
        audioThread_ = new AudioThread(this, audio_, this);
        logger->info("PlayController::open: AudioThread created");
    }

    // 11. 初始化解码器
    if (!initializeCodecs()) {
        logger->error("PlayController::open: initializeCodecs failed");
        stateMachine_.transitionTo(PlaybackState::Error, "Failed to initialize codecs");
        // 清理已创建的线程
        if (audioThread_) {
            delete audioThread_;
            audioThread_ = nullptr;
        }
        return false;
    }

    // 12. 创建VideoThread
    if (videoStreamIndex >= 0) {
        if (!videoDecoder_) {
            logger->error("PlayController::open: videoDecoder_ is null");
            stateMachine_.transitionTo(PlaybackState::Error, "videoDecoder_ is null");
            // 清理已创建的线程
            if (audioThread_) {
                delete audioThread_;
                audioThread_ = nullptr;
            }
            return false;
        }
        if (!videoWriter_) {
            logger->error("PlayController::open: videoWriter_ is null");
            stateMachine_.transitionTo(PlaybackState::Error, "videoWriter_ is null");
            // 清理已创建的线程
            if (audioThread_) {
                delete audioThread_;
                audioThread_ = nullptr;
            }
            return false;
        }

        videoThread_ = new VideoThread(this, &vPackets_, this);
        videoThread_->setDec(videoDecoder_.get());
        videoThread_->setVideoWriter(videoWriter_.get());
        logger->info("PlayController::open: VideoThread created");
    }

    // 13. 创建DemuxerThread
    demuxThread_ = new DemuxerThread(this, this);
    demuxThread_->setDemuxer(demuxer_.get());
    demuxThread_->setStreamIndices(
        demuxer_->getVideoStreamIndex(),
        demuxer_->getAudioStreamIndex(),
        demuxer_->getSubtitleStreamIndex());
    demuxThread_->setVideoPacketQueue(&vPackets_);
    demuxThread_->setAudioPacketQueue(&aPackets_);

    connect(demuxThread_, &DemuxerThread::seekFinished,
            this, &PlayController::onDemuxerThreadSeekFinished);
    connect(demuxThread_, &DemuxerThread::eofReached,
            this, &PlayController::onDemuxerThreadEofReached);
    connect(demuxThread_, &DemuxerThread::errorOccurred,
            this, &PlayController::onDemuxerThreadErrorOccurred);

    // 14. 启动所有线程（按顺序）
    demuxThread_->start();
    logger->info("PlayController::open: DemuxerThread started");

    if (audioThread_) {
        try {
            audioThread_->start();
            logger->info("PlayController::open: AudioThread started");
        } catch (const std::exception& e) {
            logger->error("PlayController::open: Failed to start AudioThread: {}", e.what());
            stop();
            return false;
        }
    }

    if (videoThread_) {
        videoThread_->start();
        logger->info("PlayController::open: VideoThread started");
    }

    // 15. 初始化健康检查时间戳
    auto now = std::chrono::steady_clock::now();
    lastVideoFrameTime_ = now;
    lastAudioFrameTime_ = now;
    lastDemuxTime_ = now;

    // 16. 转换到Playing状态
    stateMachine_.transitionTo(PlaybackState::Ready, "File opened");
    stateMachine_.transitionTo(PlaybackState::Playing, "Starting playback");

    logger->info("PlayController::open succeeded");
    return true;
}
```

**改进要点**:
1. 先停止旧播放，确保状态干净
2. 在创建线程之前检查所有资源
3. 在失败时清理已创建的资源
4. 按顺序启动线程：Demuxer → Audio → Video
5. 添加异常处理

---

### 1.2 修复音频播放崩溃问题 🔴

**问题描述**:
日志显示音频播放仍存在崩溃，需要增强错误处理和线程安全。

**修复方案**:

#### 修复点1: 增强OpenAL错误检查（Audio::writeAudio()）

```cpp
// audio.cc
bool Audio::writeAudio(const uint8_t *samples, unsigned int length)
{
    if (!samples || length == 0) {
        logger->warn("Audio::writeAudio: Invalid parameters (samples: {}, length: {})",
                     samples != nullptr, length);
        return false;
    }

    std::unique_lock<std::mutex> lock(srcMutex_);

    // 检查source是否有效
    if (source_ == 0) {
        logger->error("Audio::writeAudio: Invalid source (source_ == 0)");
        return false;
    }

    // 检查是否有可用缓冲区
    ALint buffersProcessed = 0;
    alGetSourcei(source_, AL_BUFFERS_PROCESSED, &buffersProcessed);

    ALenum alError = alGetError();
    if (alError != AL_NO_ERROR) {
        logger->error("Audio::writeAudio: alGetSourcei failed: {}", alGetString(alError));
        return false;
    }

    if (buffersProcessed == 0) {
        // 缓冲区满，等待
        if (logger) {
            logger->debug("Audio::writeAudio: Buffers full, waiting...");
        }

        // 使用超时等待，避免永久阻塞
        if (!srcCond_.wait_for(lock, std::chrono::milliseconds(100),
                              [this] { return br_ || source_ == 0; })) {
            logger->warn("Audio::writeAudio: Timeout waiting for free buffer");
            return false;
        }

        // 检查是否应该停止
        if (br_ || source_ == 0) {
            logger->info("Audio::writeAudio: Stopping, not writing audio");
            return false;
        }

        // 再次检查可用缓冲区
        alGetSourcei(source_, AL_BUFFERS_PROCESSED, &buffersProcessed);
        alError = alGetError();
        if (alError != AL_NO_ERROR) {
            logger->error("Audio::writeAudio: alGetSourcei failed (after wait): {}", alGetString(alError));
            return false;
        }

        if (buffersProcessed == 0) {
            logger->warn("Audio::writeAudio: Still no free buffer after wait");
            return false;
        }
    }

    // 取出已处理的缓冲区
    ALuint buffer;
    alSourceUnqueueBuffers(source_, 1, &buffer);
    alError = alGetError();
    if (alError != AL_NO_ERROR) {
        logger->error("Audio::writeAudio: alSourceUnqueueBuffers failed: {}", alGetString(alError));
        return false;
    }

    // 填充新数据
    alBufferData(buffer, format_, samples, length, sampleRate_);
    alError = alGetError();
    if (alError != AL_NO_ERROR) {
        logger->error("Audio::writeAudio: alBufferData failed: {}", alGetString(alError));
        return false;
    }

    // 队列缓冲区
    alSourceQueueBuffers(source_, 1, &buffer);
    alError = alGetError();
    if (alError != AL_NO_ERROR) {
        logger->error("Audio::writeAudio: alSourceQueueBuffers failed: {}", alGetString(alError));
        return false;
    }

    // 开始播放
    ALint state;
    alGetSourcei(source_, AL_SOURCE_STATE, &state);
    alError = alGetError();
    if (alError != AL_NO_ERROR) {
        logger->error("Audio::writeAudio: alGetSourcei failed: {}", alGetString(alError));
        return false;
    }

    if (state != AL_PLAYING && state != AL_PAUSED) {
        alSourcePlay(source_);
        alError = alGetError();
        if (alError != AL_NO_ERROR) {
            logger->error("Audio::writeAudio: alSourcePlay failed: {}", alGetString(alError));
            return false;
        }

        // 更新设备开始时间
        deviceStartTime_ = std::chrono::steady_clock::now();

        if (logger) {
            logger->debug("Audio::writeAudio: Started playback, buffer queued");
        }
    }

    return true;
}
```

#### 修复点2: 增强资源清理（Audio::close()）

```cpp
// audio.cc
void Audio::close()
{
    std::unique_lock<std::mutex> lock(srcMutex_);

    if (source_ == 0) {
        return;  // 已经关闭
    }

    logger->info("Audio::close: Closing audio device");

    // 停止播放
    alSourceStop(source_);
    ALenum alError = alGetError();
    if (alError != AL_NO_ERROR) {
        logger->warn("Audio::close: alSourceStop failed: {}", alGetString(alError));
    }

    // 取消所有缓冲区
    alSourcei(source_, AL_BUFFER, 0);
    alError = alGetError();
    if (alError != AL_NO_ERROR) {
        logger->warn("Audio::close: alSourcei failed: {}", alGetString(alError));
    }

    // 释放所有缓冲区
    for (ALuint buffer : buffers_) {
        if (buffer != 0) {
            alDeleteBuffers(1, &buffer);
            ALenum error = alGetError();
            if (error != AL_NO_ERROR) {
                logger->warn("Audio::close: alDeleteBuffers failed: {}", alGetString(error));
            }
        }
    }

    // 释放source
    alDeleteSources(1, &source_);
    alError = alGetError();
    if (alError != AL_NO_ERROR) {
        logger->warn("Audio::close: alDeleteSources failed: {}", alGetString(alError));
    }

    // 清空缓冲区数组
    buffers_.fill(0);
    bufferIdx_ = 0;
    source_ = 0;

    // 重置时钟
    currentPts_ = nanoseconds{0};
    deviceStartTime_ = nanoseconds::min();

    logger->info("Audio::close: Audio device closed");
}
```

#### 修复点3: 添加RAII包装类管理OpenAL资源

```cpp
// audio.h
// 添加RAII包装类
class ALSourceGuard {
public:
    explicit ALSourceGuard(ALuint source) : source_(source) {}
    ~ALSourceGuard() {
        if (source_ != 0) {
            alDeleteSources(1, &source_);
        }
    }
    operator ALuint() const { return source_; }
private:
    ALuint source_;
};

class ALBufferGuard {
public:
    explicit ALBufferGuard(ALuint buffer) : buffer_(buffer) {}
    ~ALBufferGuard() {
        if (buffer_ != 0) {
            alDeleteBuffers(1, &buffer_);
        }
    }
    operator ALuint() const { return buffer_; }
private:
    ALuint buffer_;
};
```

---

### 1.2.1 修复音视频同步问题 🔴（2026-01-18 新增）

**问题描述**:
- 日志显示 "OpenAL clock diff too large: -8731ms, rejecting"
- OpenAL 时钟和估算时钟差异过大（约8.7秒）
- 导致音频时钟不准确，影响音视频同步

**根本原因分析**:
1. **`currentPts_` 更新时机问题**: `AudioThread::updateClock()` 可能没有及时更新，或者更新频率不够
2. **`deviceStartTime_` 设置时机问题**: 可能在多次写入时重复设置，导致时钟基准不准确
3. **OpenAL 时钟计算问题**: `AL_SAMPLE_OFFSET` 可能在某些情况下不准确，或者与系统时间不同步

**修复方案**:

#### 修复点1: 优化 `currentPts_` 更新逻辑

```cpp
// AudioThread.cpp
void AudioThread::updateClock(nanoseconds pts)
{
    if (audio_) {
        audio_->updateClock(pts);
    }
    currentPts_ = pts;
}

// OpenALAudio.cpp
void Audio::updateClock(nanoseconds pts)
{
    std::lock_guard<std::mutex> lock(srcMutex_);
    
    // 检查时钟连续性：如果新PTS明显小于当前PTS（可能是Seeking），重置时钟
    if (deviceStartTime_ != std::chrono::steady_clock::time_point{} && 
        currentPts_ != nanoseconds::zero() &&
        pts < currentPts_ - nanoseconds{1000000000}) {  // 1秒前
        logger->warn("Audio::updateClock: PTS jumped backward ({} -> {}), resetting deviceStartTime_",
                    currentPts_.count(), pts.count());
        deviceStartTime_ = std::chrono::steady_clock::now();
    }
    
    currentPts_ = pts;
}
```

#### 修复点2: 优化 `deviceStartTime_` 设置时机

```cpp
// OpenALAudio.cpp
bool Audio::writeAudio(const uint8_t *samples, unsigned int length)
{
    // ... 写入缓冲区逻辑 ...
    
    // 只在首次成功启动播放时设置 deviceStartTime_
    if (!playbackStarted_ && queued >= 2) {
        ALint state;
        alGetSourcei(source_, AL_SOURCE_STATE, &state);
        if (state == AL_INITIAL || state == AL_STOPPED) {
            alSourcePlay(source_);
            // 等待一小段时间确认播放状态
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            alGetSourcei(source_, AL_SOURCE_STATE, &state);
            
            if (state == AL_PLAYING) {
                // 只在确认播放成功时设置 deviceStartTime_
                if (deviceStartTime_ == std::chrono::steady_clock::time_point{}) {
                    deviceStartTime_ = std::chrono::steady_clock::now();
                    playbackStarted_ = true;
                    logger->info("Audio::writeAudio: Playback started, deviceStartTime_ set");
                }
            }
        }
    }
    
    return true;
}
```

#### 修复点3: 改进 OpenAL 时钟与估算时钟的同步机制

```cpp
// OpenALAudio.cpp
nanoseconds Audio::getClock()
{
    // ... 现有逻辑 ...
    
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
            
            // 改进：使用更大的容忍范围，但添加连续性检查
            nanoseconds diff = openalClock - estimatedClock;
            nanoseconds absDiff = std::abs(diff);
            
            // 如果差异在合理范围内（500ms以内），使用OpenAL时钟
            if (absDiff < 500000000) {
                return std::max(openalClock, nanoseconds::zero());
            }
            
            // 如果差异过大，检查是否是时钟跳跃（Seeking等情况）
            if (absDiff > 2000000000) {  // 2秒
                // 可能是Seeking导致的时钟跳跃，重置 deviceStartTime_
                logger->warn("Audio::getClock: Large clock diff detected ({}ms), resetting deviceStartTime_",
                            std::chrono::duration_cast<milliseconds>(diff).count());
                deviceStartTime_ = std::chrono::steady_clock::now();
                // 重新计算估算时钟
                auto now = std::chrono::steady_clock::now();
                nanoseconds deviceElapsed = now - deviceStartTime_;
                estimatedClock = currentPts_ + deviceElapsed;
                return std::max(estimatedClock, nanoseconds::zero());
            }
            
            // 差异在500ms-2秒之间，拒绝使用OpenAL时钟，使用估算时钟
            logger->warn("Audio::getClock: OpenAL clock diff too large: {}ms, using estimated clock",
                        std::chrono::duration_cast<milliseconds>(diff).count());
        }
    }
    
    // 策略4：使用系统时间估算（最可靠的fallback）
    return std::max(estimatedClock, nanoseconds::zero());
}
```

#### 修复点4: 添加时钟连续性检查

```cpp
// OpenALAudio.cpp
class Audio {
private:
    nanoseconds lastClock_{nanoseconds::min()};  // 上次返回的时钟值
    
    nanoseconds getClock() {
        // ... 计算时钟 ...
        
        // 时钟连续性检查：如果时钟明显倒退（非Seeking情况），使用上次值
        if (lastClock_ != nanoseconds::min() && 
            clock < lastClock_ - nanoseconds{100000000}) {  // 100ms前
            logger->warn("Audio::getClock: Clock went backward ({} -> {}), using last clock",
                        lastClock_.count(), clock.count());
            return lastClock_;
        }
        
        lastClock_ = clock;
        return clock;
    }
};
```

**实施计划**:
1. 优化 `currentPts_` 更新逻辑，确保及时更新
2. 优化 `deviceStartTime_` 设置时机，只在首次成功播放时设置
3. 改进 OpenAL 时钟与估算时钟的同步机制，增加容忍范围但添加连续性检查
4. 添加时钟连续性检查，避免时钟跳跃

**状态**: 🔄 待实施

---

### 1.3 优化Seeking机制 🟡

**已完成的优化**:
- ✅ 添加`lastRenderedFrame_`机制，减少非关键帧跳过时的闪烁
- ✅ 优化队列清空时机，在`requestSeek()`时立即清空队列
- ✅ 实现KeyFrame直接跳转，使用`AVSEEK_FLAG_FRAME`
- ✅ 优化缓冲管理，在seeking期间直接丢弃满队列的数据包
- ✅ 改进Seeking同步机制，在成功解码关键帧后立即清除`wasSeekingRecently_`标志

**进一步优化建议**:

#### 优化点1: 实现自适应Seeking策略

```cpp
// PlayController.cpp
bool PlayController::seek(int64_t positionMs)
{
    // 计算seek位置（秒）
    double seekPosSec = static_cast<double>(positionMs) / 1000.0;
    int64_t positionUs = positionMs * 1000;

    // 检查当前播放位置
    int64_t currentPosMs = getCurrentPositionMs();

    // 自适应Seeking策略
    bool backward = false;

    // 如果seek距离很近（< 2秒），使用快速seek（不回退）
    if (std::abs(positionMs - currentPosMs) < 2000) {
        backward = false;  // 直接跳转
    }
    // 如果seek距离很远（> 10秒），向后seek以确保有足够的关键帧
    else if (std::abs(positionMs - currentPosMs) > 10000) {
        backward = true;   // 向后seek
    }
    // 中等距离，根据当前状态决定
    else {
        backward = (positionMs < currentPosMs);  // 向前seek
    }

    logger->info("PlayController::seek: {} ms (current: {} ms, diff: {} ms), backward: {}",
                 positionMs, currentPosMs, positionMs - currentPosMs, backward);

    // 其余seek逻辑保持不变...
}
```

#### 优化点2: 实现Seeking预加载

```cpp
// VideoThread.cpp
bool VideoThread::handleSeekingState(bool &wasSeekingBefore)
{
    bool isSeeking = controller_->isSeeking();

    if (isSeeking) {
        // Seeking刚开始时，解码目标位置附近的多个关键帧
        if (!wasSeekingBefore) {
            wasSeekingBefore = true;

            // 预解码目标位置附近的多个关键帧（减少卡顿）
            AVPixelFormat dummyPixFmt = AV_PIX_FMT_NONE;
            Frame dummyFrame;
            int keyFrameCount = 0;

            try {
                while (keyFrameCount < 3 && !br_) {  // 预解码3个关键帧
                    int ret = dec_->decodeVideo(nullptr, dummyFrame, dummyPixFmt, true, 0);
                    if (ret < 0 || dummyFrame.isEmpty()) {
                        break;
                    }

                    // 检查是否是关键帧
                    if (dummyFrame.flags() & AV_FRAME_FLAG_KEY) {
                        keyFrameCount++;
                    }

                    dummyFrame.clear();
                }
            } catch (const std::exception &e) {
                logger->error("VideoThread::handleSeekingState: Exception while pre-decoding: {}", e.what());
            }

            logger->debug("VideoThread::handleSeekingState: Pre-decoded {} keyframes", keyFrameCount);
            controller_->updateVideoClock(nanoseconds::min());
        }

        // 消费队列中的旧数据包...
        // 其余逻辑保持不变...
    }

    // 其余逻辑保持不变...
}
```

---

## 中优先级改进

### 2.1 改进音视频同步机制

**当前问题**:
- 音视频同步精度不够
- 缺乏动态同步调整

**改进方案**:

#### 改进点1: 实现动态同步阈值

```cpp
// PlayController.h
class PlayController {
private:
    // 动态同步阈值
    nanoseconds syncThreshold_{milliseconds(10)};      // 正常阈值：10ms
    nanoseconds maxSyncThreshold_{milliseconds(40)};   // 最大阈值：40ms
    nanoseconds minSyncThreshold_{milliseconds(5)};    // 最小阈值：5ms

    // 同步误差统计
    nanoseconds avgSyncError_{0};
    int syncErrorCount_{0};

public:
    // 动态调整同步阈值
    void updateSyncThreshold(nanoseconds currentError) {
        // 计算移动平均误差
        const int alpha = 10;  // 平滑因子
        avgSyncError_ = (avgSyncError_ * (alpha - 1) + currentError) / alpha;
        syncErrorCount_++;

        // 根据误差动态调整阈值
        if (std::abs(avgSyncError_.count()) > 30) {
            // 误差较大，增大阈值
            syncThreshold_ = std::min(syncThreshold_ + milliseconds(2), maxSyncThreshold_);
        } else if (std::abs(avgSyncError_.count()) < 8) {
            // 误差较小，减小阈值（更精确）
            syncThreshold_ = std::max(syncThreshold_ - milliseconds(1), minSyncThreshold_);
        }
    }
};
```

#### 改进点2: 实现视频帧重复/跳过逻辑

```cpp
// VideoThread.cpp
bool VideoThread::renderFrame(Frame& videoFrame, int& frames)
{
    // 获取主时钟
    nanoseconds masterClock = controller_->getMasterClock();

    // 获取视频帧时间戳
    nanoseconds videoFrameTs = videoFrame.ts();
    if (videoFrameTs == nanoseconds::min()) {
        // 无时间戳，立即渲染
        writer_->writeFrame(videoFrame);
        controller_->updateVideoClock(videoFrameTs);
        return true;
    }

    // 计算帧延迟
    nanoseconds delay = videoFrameTs - masterClock;

    // 记录同步误差
    controller_->updateSyncThreshold(delay);

    // 根据延迟决定渲染策略
    if (delay > controller_->getMaxSyncThreshold()) {
        // 延迟 > 40ms，跳过帧（落后太多）
        logger->debug("VideoThread::renderFrame: Skipping frame (delay: {} ms)",
                     std::chrono::duration_cast<milliseconds>(delay).count());

        // 保存为上一帧（避免黑屏）
        lastRenderedFrame_ = videoFrame;
        hasLastRenderedFrame_ = true;
        return false;

    } else if (delay > controller_->getSyncThreshold()) {
        // 延迟 > 阈值，延迟渲染
        std::this_thread::sleep_for(delay);
        writer_->writeFrame(videoFrame);
        controller_->updateVideoClock(videoFrameTs);
        return true;

    } else if (delay < -controller_->getMaxSyncThreshold()) {
        // 延迟 < -40ms，重复上一帧（超前太多）
        logger->debug("VideoThread::renderFrame: Repeating last frame (delay: {} ms)",
                     std::chrono::duration_cast<milliseconds>(delay).count());

        if (hasLastRenderedFrame_) {
            writer_->writeFrame(lastRenderedFrame_);
        } else {
            // 没有上一帧，渲染当前帧
            writer_->writeFrame(videoFrame);
        }
        controller_->updateVideoClock(videoFrameTs);
        return true;

    } else {
        // 延迟在阈值范围内，立即渲染
        writer_->writeFrame(videoFrame);
        controller_->updateVideoClock(videoFrameTs);
        return true;
    }
}
```

---

### 2.2 实现自适应缓冲管理

**当前问题**:
- 队列大小固定，无法根据网络或解码速度自适应调整

**改进方案**:

```cpp
// packet_queue.h
template<size_t InitialSizeLimit>
class AdaptivePacketQueue {
private:
    size_t currentSizeLimit_{InitialSizeLimit};
    size_t minSizeLimit_{InitialSizeLimit / 2};
    size_t maxSizeLimit_{InitialSizeLimit * 2};

    // 队列使用统计
    std::chrono::steady_clock::time_point lastFullTime_;
    int fullCount_{0};
    int emptyCount_{0};

public:
    bool put(const AVPacket *pkt, const char* queueName = nullptr) {
        std::lock_guard<std::mutex> lck(mtx_);

        // 自适应调整队列大小
        adjustSizeLimit();

        if (totalsize_ >= currentSizeLimit_) {
            fullCount_++;
            lastFullTime_ = std::chrono::steady_clock::now();
            logger->debug("PacketQueue[{}]: Full, currentSizeLimit: {}", queueName, currentSizeLimit_);
            return false;
        }

        // 放入数据包...
    }

    const AVPacket* peekPacket(const char* queueName = nullptr) {
        std::lock_guard<std::mutex> lck(mtx_);

        if (packets_.empty()) {
            emptyCount_++;
            return nullptr;
        }

        // 自适应调整队列大小
        adjustSizeLimit();

        return &packets_.front();
    }

private:
    void adjustSizeLimit() {
        auto now = std::chrono::steady_clock::now();

        // 如果队列经常满，增大队列
        if (fullCount_ > 10) {
            currentSizeLimit_ = std::min(currentSizeLimit_ * 2, maxSizeLimit_);
            fullCount_ = 0;
            logger->info("PacketQueue: Increased size limit to {}", currentSizeLimit_);
        }

        // 如果队列经常空，减小队列（释放内存）
        if (emptyCount_ > 10 && currentSizeLimit_ > minSizeLimit_) {
            currentSizeLimit_ = std::max(currentSizeLimit_ / 2, minSizeLimit_);
            emptyCount_ = 0;
            logger->info("PacketQueue: Decreased size limit to {}", currentSizeLimit_);
        }

        // 如果队列长时间不满且不空，逐步减小队列
        if (now - lastFullTime_ > std::chrono::seconds(30) &&
            !packets_.empty() &&
            currentSizeLimit_ > minSizeLimit_) {
            currentSizeLimit_ = std::max(currentSizeLimit_ * 9 / 10, minSizeLimit_);
            logger->debug("PacketQueue: Reduced size limit to {}", currentSizeLimit_);
        }
    }
};
```

---

### 2.3 完善错误恢复机制

**改进方案**:

```cpp
// ErrorRecoveryManager.cpp
RecoveryResult ErrorRecoveryManager::handleError(const ErrorInfo& error)
{
    // 记录错误
    {
        std::lock_guard<std::mutex> lock(errorCountMutex_);

        switch (error.type) {
            case ErrorType::DecodeError:
                decodeErrorCount_++;
                break;
            case ErrorType::NetworkError:
                networkErrorCount_++;
                break;
            case ErrorType::ResourceError:
                resourceErrorCount_++;
                break;
            case ErrorType::ThreadError:
                threadErrorCount_++;
                break;
            case ErrorType::UnknownError:
                unknownErrorCount_++;
                break;
        }

        logger->warn("ErrorRecoveryManager: Error occurred - Type: {}, Message: {}, Code: {}",
                     static_cast<int>(error.type), error.message, error.errorCode);
    }

    // 获取恢复动作
    RecoveryAction action = getRecoveryAction(error.type, getErrorCount(error.type));

    // 执行恢复动作
    switch (action) {
        case RecoveryAction::None:
            logger->debug("ErrorRecoveryManager: No action taken");
            break;

        case RecoveryAction::Retry:
            logger->info("ErrorRecoveryManager: Retry");
            break;

        case RecoveryAction::FlushAndRetry:
            logger->info("ErrorRecoveryManager: Flush and retry");
            break;

        case RecoveryAction::RestartThread:
            logger->warn("ErrorRecoveryManager: Restart thread");
            break;

        case RecoveryAction::StopPlayback:
            logger->error("ErrorRecoveryManager: Stop playback");
            break;
    }

    // 调用错误回调（如果有）
    if (errorCallback_) {
        errorCallback_(error, RecoveryResult(action, getErrorCount(error.type), action != RecoveryAction::StopPlayback));
    }

    return RecoveryResult(action, getErrorCount(error.type), action != RecoveryAction::StopPlayback);
}

RecoveryAction ErrorRecoveryManager::getRecoveryAction(ErrorType type, int retryCount) const
{
    switch (type) {
        case ErrorType::DecodeError:
            if (retryCount < maxDecodeRetries_) {
                return (retryCount < 2) ? RecoveryAction::Retry : RecoveryAction::FlushAndRetry;
            } else {
                return RecoveryAction::StopPlayback;
            }

        case ErrorType::NetworkError:
            if (retryCount < maxNetworkRetries_) {
                return RecoveryAction::Retry;
            } else {
                return RecoveryAction::StopPlayback;
            }

        case ErrorType::ResourceError:
            if (retryCount < maxResourceRetries_) {
                return RecoveryAction::RestartThread;
            } else {
                return RecoveryAction::StopPlayback;
            }

        case ErrorType::ThreadError:
            if (retryCount < maxThreadRetries_) {
                return RecoveryAction::RestartThread;
            } else {
                return RecoveryAction::StopPlayback;
            }

        case ErrorType::UnknownError:
            if (retryCount < maxUnknownRetries_) {
                return RecoveryAction::Retry;
            } else {
                return RecoveryAction::StopPlayback;
            }

        default:
            return RecoveryAction::StopPlayback;
    }
}
```

---

## 低优先级改进

### 3.1 实现硬件帧零拷贝渲染

**目标**: 使用WGL_NV_DX_interop扩展，直接渲染硬件帧，无需CPU拷贝

**实现步骤**:

1. **创建D3D11纹理**

```cpp
// opengl/OpenGLHWInterop.cpp
bool OpenGLHWInterop::createD3D11Texture(int width, int height)
{
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_NV12;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texDesc.MiscFlags = 0;

    HRESULT hr = d3d11Device_->CreateTexture2D(&texDesc, nullptr, &d3d11Texture_);
    if (FAILED(hr)) {
        logger->error("Failed to create D3D11 texture: 0x{:X}", hr);
        return false;
    }

    return true;
}
```

2. **创建WGL NV_DX互对象**

```cpp
bool OpenGLHWInterop::createDXInteropObject()
{
    // 创建WGL互对象
    wglDXInteropDevice_ = wglDXOpenDeviceNV(d3d11Device_);
    if (!wglDXInteropDevice_) {
        logger->error("Failed to create WGL DX interop object");
        return false;
    }

    // 注册D3D11纹理到OpenGL
    wglDXRegisterObjectNV(wglDXInteropDevice_,
                        d3d11Texture_,
                        glTexture_,
                        GL_TEXTURE_2D,
                        WGL_ACCESS_READ_WRITE_NV);

    return true;
}
```

3. **渲染硬件帧**

```cpp
bool OpenGLHWInterop::renderHWFrame(AVFrame* hwFrame)
{
    // 锁定D3D11纹理
    wglDXLockObjectsNV(wglDXInteropDevice_, 1, &wglDXInteropObject_);

    // 将硬件帧数据复制到D3D11纹理
    ID3D11Texture2D* dxTexture = nullptr;
    if (av_hwframe_transfer_data(dxTexture, hwFrame, 0) < 0) {
        logger->error("Failed to transfer hardware frame data");
        wglDXUnlockObjectsNV(wglDXInteropDevice_, 1, &wglDXInteropObject_);
        return false;
    }

    // OpenGL可以直接从D3D11纹理读取数据
    // 无需CPU拷贝

    // 解锁D3D11纹理
    wglDXUnlockObjectsNV(wglDXInteropDevice_, 1, &wglDXInteropObject_);

    return true;
}
```

---

### 3.2 添加性能监控

**实现方案**:

```cpp
// PerformanceMonitor.h
class PerformanceMonitor {
public:
    // FPS监控
    void updateVideoFPS();
    float getVideoFPS() const;

    // 解码性能
    void updateDecodeTime(std::chrono::microseconds decodeTime);
    std::chrono::microseconds getAvgDecodeTime() const;

    // 内存使用
    size_t getMemoryUsage() const;

    // 队列状态
    size_t getVideoQueueSize() const;
    size_t getAudioQueueSize() const;

    // 打印性能统计
    void printStats();

private:
    // FPS
    int frameCount_{0};
    std::chrono::steady_clock::time_point lastFpsTime_;
    float currentFPS_{0.0f};

    // 解码性能
    std::vector<std::chrono::microseconds> decodeTimes_;
    std::chrono::microseconds totalDecodeTime_{0};
    int decodeCount_{0};
};
```

---

### 3.3 实现播放速度控制

**实现方案**:

```cpp
// PlayController.cpp
bool PlayController::setPlaybackRate(double rate)
{
    if (rate <= 0.0 || rate > 4.0) {
        logger->warn("PlayController::setPlaybackRate: Invalid rate: {}", rate);
        return false;
    }

    logger->info("PlayController::setPlaybackRate: {}", rate);

    // 更新音频播放速度
    if (audio_) {
        // OpenAL不支持变速播放，需要重新采样
        // 这里只是一个示例，实际实现需要更复杂
        // audio_->setPlaybackRate(rate);
    }

    // 视频播放速度通过调整帧间隔实现
    // 在VideoThread::renderFrame()中使用
    playbackRate_ = rate;

    return true;
}

double PlayController::getPlaybackRate() const
{
    return playbackRate_;
}

// VideoThread.cpp
bool VideoThread::renderFrame(Frame& videoFrame, int& frames)
{
    // 获取主时钟
    nanoseconds masterClock = controller_->getMasterClock();

    // 获取视频帧时间戳
    nanoseconds videoFrameTs = videoFrame.ts();

    // 计算帧延迟（考虑播放速度）
    double playbackRate = controller_->getPlaybackRate();
    nanoseconds delay = (videoFrameTs - masterClock) / playbackRate;

    // 其余渲染逻辑...
}
```

---

## 架构优化建议

### 4.1 引入线程池管理

**目标**: 统一管理线程生命周期，避免线程创建销毁的开销

**实现方案**:

```cpp
// ThreadPool.h
class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads = 4);
    ~ThreadPool();

    // 提交任务
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))>;

    // 停止所有线程
    void stop();

    // 获取线程数
    size_t getThreadCount() const;

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queueMutex_;
    std::condition_variable condition_;
    bool stop_;
};

// PlayController.h
class PlayController {
private:
    std::unique_ptr<ThreadPool> threadPool_;

public:
    // 使用线程池执行任务
    void executeAsync(std::function<void()> task);
};
```

---

### 4.2 引入事件总线

**目标**: 解耦模块间的通信，使用事件驱动架构

**实现方案**:

```cpp
// EventBus.h
enum class EventType {
    PlaybackStarted,
    PlaybackStopped,
    PlaybackPaused,
    Seeking,
    SeekingFinished,
    ErrorOccurred,
    BufferUnderrun,
    BufferOverflow,
    // ...
};

struct Event {
    EventType type;
    std::any data;
    std::chrono::steady_clock::time_point timestamp;
};

class EventBus {
public:
    using EventCallback = std::function<void(const Event&)>;

    // 订阅事件
    void subscribe(EventType type, EventCallback callback);

    // 发布事件
    void publish(const Event& event);

    // 取消订阅
    void unsubscribe(EventType type);

private:
    std::unordered_map<EventType, std::vector<EventCallback>> subscribers_;
    std::mutex mutex_;
};

// 使用示例
auto eventBus = std::make_shared<EventBus>();

// 订阅事件
eventBus->subscribe(EventType::ErrorOccurred, [](const Event& event) {
    // 处理错误事件
});

// 发布事件
eventBus->publish(Event{EventType::ErrorOccurred, errorData});
```

---

### 4.3 引入依赖注入

**目标**: 降低模块间的耦合度，提高可测试性

**实现方案**:

```cpp
// ServiceLocator.h
class ServiceLocator {
public:
    // 注册服务
    template<typename T>
    static void registerService(std::shared_ptr<T> service);

    // 获取服务
    template<typename T>
    static std::shared_ptr<T> getService();

    // 清理所有服务
    static void clear();
};

// 使用示例
// 注册服务
ServiceLocator::registerService<Decoder>(std::make_shared<FFDecHW>());

// 获取服务
auto decoder = ServiceLocator::getService<Decoder>();
```

---

## 代码质量改进

### 5.1 添加单元测试

**测试框架**: Google Test

**测试示例**:

```cpp
// tests/PlayControllerTest.cpp
#include <gtest/gtest.h>
#include "PlayController.h"

class PlayControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        controller_ = std::make_unique<PlayController>();
    }

    void TearDown() override {
        controller_.reset();
    }

    std::unique_ptr<PlayController> controller_;
};

TEST_F(PlayControllerTest, TestOpenValidFile) {
    QString filename = "test.mp4";
    bool result = controller_->open(filename);
    EXPECT_TRUE(result);
    EXPECT_TRUE(controller_->isOpened());
}

TEST_F(PlayControllerTest, TestOpenInvalidFile) {
    QString filename = "invalid.mp4";
    bool result = controller_->open(filename);
    EXPECT_FALSE(result);
    EXPECT_FALSE(controller_->isOpened());
}

TEST_F(PlayControllerTest, TestPlaybackControl) {
    QString filename = "test.mp4";
    controller_->open(filename);

    controller_->play();
    EXPECT_TRUE(controller_->isPlaying());

    controller_->pause();
    EXPECT_TRUE(controller_->isPaused());

    controller_->stop();
    EXPECT_TRUE(controller_->isStopped());
}
```

---

### 5.2 添加代码文档

**使用Doxygen生成文档**:

```cpp
/**
 * @file PlayController.h
 * @brief 播放控制器类，负责协调所有播放相关组件
 *
 * PlayController是WZMediaPlayer的核心组件，负责：
 * - 管理播放状态（使用PlaybackStateMachine）
 * - 协调所有线程（DemuxerThread、VideoThread、AudioThread）
 * - 处理用户命令（play/pause/stop/seek）
 * - 管理共享数据结构（Demuxer、PacketQueue）
 *
 * @see PlaybackStateMachine
 * @see DemuxerThread
 * @see VideoThread
 * @see AudioThread
 */
class PlayController : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父QObject对象
     */
    explicit PlayController(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~PlayController();

    /**
     * @brief 打开媒体文件
     * @param filename 文件路径
     * @return 成功返回true，失败返回false
     *
     * 此方法会：
     * 1. 停止当前播放（如果有）
     * 2. 创建/重置Demuxer
     * 3. 打开文件
     * 4. 初始化解码器和渲染器
     * 5. 创建并启动所有线程
     * 6. 转换到Playing状态
     */
    bool open(const QString& filename);

    // ...
};
```

---

## 性能优化建议

### 6.1 解码性能优化

1. **使用多线程解码**

```cpp
// PlayController.cpp
int streamComponentOpen(unsigned int stream_index)
{
    // ...

    // 根据解码器类型决定线程数
    int threadCount = 8;  // 默认8线程

    // 硬件解码器通常不支持多线程
    if (enableHardwareDecoding_) {
        threadCount = 1;  // 硬件解码使用单线程
    }

    avCodecContext->thread_count = threadCount;
    avCodecContext->thread_type = FF_THREAD_FRAME;

    // ...
}
```

2. **使用零拷贝API**

```cpp
// FFDecHW.cpp
int FFDecHW::decodeVideo(...)
{
    // 使用avcodec_send_packet和avcodec_receive_frame
    // 避免使用已废弃的avcodec_decode_video2

    // 发送数据包
    int ret = avcodec_send_packet(codec_ctx_, encodedPacket);
    if (ret < 0 && ret != AVERROR(EAGAIN)) {
        return ret;
    }

    // 接收帧
    AVFrame* frame = av_frame_alloc();
    ret = avcodec_receive_frame(codec_ctx_, frame);
    if (ret < 0) {
        av_frame_free(&frame);
        return ret;
    }

    // 零拷贝：直接将AVFrame引用到输出Frame
    decoded.referenceAVFrame(frame);
    // 不需要手动释放frame，由decoded管理
}
```

---

### 6.2 渲染性能优化

1. **使用VBO/VAO**

```cpp
// OpenGLWriter.cpp
void OpenGLWriter::initializeGL()
{
    // 创建VAO
    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);

    // 创建VBO
    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);

    // 顶点数据
    float vertices[] = {
        // 位置          // 纹理坐标
        -1.0f,  1.0f,   0.0f, 0.0f,
         1.0f,  1.0f,   1.0f, 0.0f,
        -1.0f, -1.0f,   0.0f, 1.0f,
         1.0f, -1.0f,   1.0f, 1.0f
    };

    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // 设置顶点属性
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}
```

2. **使用PBO异步传输**

```cpp
// OpenGLWriter.cpp
void OpenGLWriter::writeFrame(const Frame& frame)
{
    // 使用PBO异步上传纹理数据
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo_);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, frameSize, frame.data(), GL_STREAM_DRAW);

    // 异步上传
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                    GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}
```

---

## 测试策略

### 7.1 单元测试

**目标**: 单元测试覆盖率达到70%以上

**测试范围**:
- PlayController核心方法
- PacketQueue线程安全操作
- PlaybackStateMachine状态转换
- ErrorRecoveryManager错误处理
- 音视频同步算法

---

### 7.2 集成测试

**测试场景**:
1. 正常播放流程
2. 播放/暂停/停止操作
3. Seeking操作（短距离、长距离）
4. 视频切换
5. 循环播放
6. 错误恢复

---

### 7.3 性能测试

**测试指标**:
- FPS（帧率）
- CPU使用率
- 内存使用量
- 解码延迟
- Seek响应时间

---

## 实施路线图

### 第一阶段：稳定性修复（1-2周）
- [x] 修复BUG-001: 视频切换崩溃
- [ ] 修复音频播放崩溃
- [ ] 优化Seeking机制
- [ ] 完善错误恢复机制

### 第二阶段：功能完善（2-3周）
- [ ] 改进音视频同步
- [ ] 实现自适应缓冲管理
- [ ] 添加播放速度控制
- [ ] 实现性能监控

### 第三阶段：性能优化（1-2周）
- [ ] 实现硬件帧零拷贝渲染
- [ ] 优化解码性能
- [ ] 优化渲染性能
- [ ] 内存使用优化

### 第四阶段：架构优化（2-3周）
- [ ] 引入线程池管理
- [ ] 引入事件总线
- [ ] 引入依赖注入
- [ ] 代码重构

### 第五阶段：测试和文档（1周）
- [ ] 添加单元测试
- [ ] 添加集成测试
- [ ] 性能测试
- [ ] 代码文档

---

## 总结

本文档提供了针对WZMediaPlayer项目的详细改进建议，包括：

**高优先级改进**:
1. 修复视频切换崩溃问题（使用智能指针管理线程生命周期）
2. 修复音频播放崩溃问题（增强OpenAL错误检查）
3. 优化Seeking机制（实现自适应Seeking策略）

**中优先级改进**:
1. 改进音视频同步机制（动态同步阈值）
2. 实现自适应缓冲管理
3. 完善错误恢复机制

**低优先级改进**:
1. 实现硬件帧零拷贝渲染
2. 添加性能监控
3. 实现播放速度控制

**架构优化**:
1. 引入线程池管理
2. 引入事件总线
3. 引入依赖注入

**代码质量改进**:
1. 添加单元测试
2. 添加代码文档

**性能优化**:
1. 解码性能优化
2. 渲染性能优化

建议按照实施路线图的优先级逐步实施改进，确保每个阶段都能得到充分的测试和验证。

---

**文档版本**: 1.0
**最后更新**: 2026-01-16
**维护者**: GLM（通用语言模型）
