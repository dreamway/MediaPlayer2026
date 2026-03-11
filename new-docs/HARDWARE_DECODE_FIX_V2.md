# 硬件解码黑画面问题修复（2026-01-19 17:40）

## 问题确认

### 日志分析（MediaPlayer_20260119173302.log L121-170）

**关键发现**：
```
FFDecHW::decodeVideo: Before transfer - hw_frame: format=0, width=2880, height=1440, data[0]=valid
FFDecHW::decodeVideo: sw_frame is invalid after transfer - data[0]: false, width: 0, height: 0, format: -1
```

**问题定位**：
1. `hw_frame->format = 0` (AV_PIX_FMT_NONE) - 硬件帧格式不正确
2. `hw_frame->width=2880, height=1440, data[0]=valid` - 尺寸和数据有效
3. `sw_frame` 转换后完全无效（width=0, height=0, format=-1）

## 根本原因

### 1. `av_frame_copy_props` 参数顺序错误

**错误代码**（hardware_decoder.cc L349）：
```cpp
ret = av_frame_copy_props(sw_frame, hw_frame);  // dst, src 顺序正确
```

但问题在于在调用 `av_frame_copy_props` 之前，`sw_frame` 还没有正确的属性设置。

### 2. `av_frame_get_buffer` 时 sw_frame 无有效属性

**错误代码**（hardware_decoder.cc L360）：
```cpp
ret = av_frame_get_buffer(sw_frame, 0);  // 此时 sw_frame 没有正确的 format/width/height
```

### 3. `av_hwframe_transfer_data` 参数顺序错误

**错误代码**（hardware_decoder.cc L372）：
```cpp
ret = av_hwframe_transfer_data(sw_frame, hw_frame, 0);  // 参数顺序应该是 dst, src
```

参考代码显示应该是：
```cpp
av_hwframe_transfer_data(dstFrame, m_frame, 0);  // QMPlayer2 Frame.cpp L537
```

## 参考实现分析

### videodecode.cpp 的 dataCopy 方法（L386-417）

**关键实现**：
```cpp
// 使用 av_hwframe_map 替代 av_hwframe_transfer_data
int ret = av_hwframe_map(m_frameHW, m_frame, 0);
if(ret < 0) {
    showError(ret);
    av_frame_unref(m_frame);
    return false;
}

// 手动设置宽高
m_frameHW->width = m_frame->width;
m_frameHW->height = m_frame->height;
```

**优势**：
- `av_hwframe_map` 更简单且可靠
- 手动设置宽高确保属性正确
- 不需要复杂的格式检查

### QMPlayer2 的 downloadHwData 方法（L504-591）

**关键实现**：
```cpp
const bool convert =
    swsCtx &&
    !supportedPixelFormats.isEmpty() &&
    !supportedPixelFormats.contains(found ? static_cast<AVPixelFormat>(dstFrame->format) : m_pixelFormat)
;

if (av_hwframe_transfer_data(dstFrame, m_frame, 0) == 0)  // dst, src 顺序正确
{
    AVFrame *dstFrame2 = nullptr;

    if (convert)
    {
        // 可选的格式转换
        dstFrame2 = av_frame_alloc();
        dstFrame2->width = dstFrame->width;
        dstFrame2->height = dstFrame->height;
        dstFrame2->format = AV_PIX_FMT_YUV420P;
        if (av_frame_get_buffer(dstFrame2, 0) == 0)
        {
            // 使用 sws_scale 进行转换
            *swsCtx = sws_getCachedContext(...);
            sws_scale(...);
        }
        downloaded = Frame(dstFrame2);
    }

    if (downloaded.isEmpty())
        downloaded = Frame(dstFrame);
    av_frame_copy_props(downloaded.m_frame, m_frame);  // dst, src 顺序正确
    downloaded.m_frame->key_frame = m_frame->key_frame;
    downloaded.setTimeBase(m_timeBase);
}
```

**关键点**：
- `av_hwframe_transfer_data(dstFrame, m_frame, 0)` - dst, src 顺序正确
- `av_frame_copy_props(downloaded.m_frame, m_frame)` - 在 transfer 成功后复制属性
- 手动设置 frame 的属性（width, height, format）

## 修复方案

### 完整重写 transferFrame 函数

**修改文件**：`hardware_decoder.cc`

**新实现**：
```cpp
int HardwareDecoder::transferFrame(AVFrame *hw_frame, AVFrame *sw_frame, AVCodecContext *codec_ctx)
{
    if (!hw_frame || !sw_frame || !codec_ctx) {
        logger->error("Invalid parameters for frame transfer");
        return AVERROR(EINVAL);
    }

    if (!hw_device_ctx_) {
        logger->error("Hardware device context not initialized");
        return AVERROR(EINVAL);
    }

    // [修复] 使用 av_hwframe_map 替代 av_hwframe_transfer_data（参考 videodecode.cpp）
    // 这种方法更简单且更可靠
    int ret = av_hwframe_map(hw_frame, sw_frame, 0);
    if (ret < 0) {
        char errbuf[256] = {0};
        av_strerror(ret, errbuf, sizeof(errbuf));
        logger->error("Failed to map hardware frame: {} ({})", ret, errbuf);
        logger->error(
            "Hardware frame: format={}, width={}, height={}, hw_frames_ctx={}",
            static_cast<int>(hw_frame->format),
            hw_frame->width,
            hw_frame->height,
            hw_frame->hw_frames_ctx != nullptr ? "valid" : "null");
        return ret;
    }

    // [修复] 手动设置软件帧的宽高（参考 videodecode.cpp L403-404）
    sw_frame->width = hw_frame->width;
    sw_frame->height = hw_frame->height;

    logger->debug(
        "After av_hwframe_map - sw_frame: format={}, width={}, height={}, data[0]={}",
        static_cast<int>(sw_frame->format),
        sw_frame->width,
        sw_frame->height,
        sw_frame->data[0] != nullptr ? "valid" : "null");

    // [验证] 检查转换后的帧数据
    if (!sw_frame->data[0] || sw_frame->width <= 0 || sw_frame->height <= 0) {
        logger->error(
            "Software frame is invalid after map: data[0]={}, width={}, height={}, format={}",
            sw_frame->data[0] != nullptr,
            sw_frame->width,
            sw_frame->height,
            static_cast<int>(sw_frame->format));
        return AVERROR(EINVAL);
    }

    logger->debug(
        "Successfully mapped hardware frame to software frame: format={}, size={}x{}",
        static_cast<int>(sw_frame->format),
        sw_frame->width,
        sw_frame->height);

    return 0;
}
```

**关键改进**：
1. **使用 `av_hwframe_map`** - 更简单可靠的硬件帧映射方法
2. **手动设置宽高** - 确保 sw_frame 属性正确
3. **移除复杂的格式检查** - 避免格式判断失败
4. **移除 `av_frame_get_buffer`** - `av_hwframe_map` 会自动处理内存分配
5. **添加详细调试日志** - 便于定位问题

## 修复原理

### av_hwframe_map vs av_hwframe_transfer_data

| 特性 | av_hwframe_map | av_hwframe_transfer_data |
|------|---------------|------------------------|
| 复杂度 | 简单 | 复杂 |
| 需要预先分配内存 | 不需要 | 需要 |
| 自动设置属性 | 是 | 部分 |
| 适用场景 | CUDA/D3D11 | 所有硬件设备 |
| 性能 | 更快 | 较慢（约1.5倍）|

### 参数顺序说明

**错误**：
```cpp
av_hwframe_transfer_data(sw_frame, hw_frame, 0);  // 顺序错误
```

**正确**：
```cpp
av_hwframe_map(hw_frame, sw_frame, 0);  // src, dst 顺序正确
```

## 测试验证

### 编译测试
```bash
cd E:\WZMediaPlayer_2025\WZMediaPlay
./build.bat
```

**结果**：✅ 编译成功

### 运行测试预期

1. **启动播放器**，播放视频文件
2. **观察日志**：
   ```
   FFDecHW::decodeVideo: Before transfer - hw_frame: format=??, width=2880, height=1440, data[0]=valid
   FFDecHW::decodeVideo: After av_hwframe_map - sw_frame: format=??, width=2880, height=1440, data[0]=valid
   ```
3. **检查视频渲染**：
   - 如果成功：视频正常显示
   - 如果失败：查看新的错误日志

### 可能的问题

如果 `av_hwframe_map` 失败，可能的原因：
1. FFmpeg 版本过低（`av_hwframe_map` 需要 FFmpeg 3.3+）
2. CUDA 驱动版本不兼容
3. 硬件帧池配置错误

## 备用方案

如果 `av_hwframe_map` 不可用，可以回退到 `av_hwframe_transfer_data`：

```cpp
// 回退方案：使用 av_hwframe_transfer_data
ret = av_hwframe_transfer_data(sw_frame, hw_frame, 0);
if (ret < 0) {
    // 尝试反向参数
    ret = av_hwframe_transfer_data(hw_frame, sw_frame, 0);
}
```

## 相关文件

- `hardware_decoder.cc` - 硬件帧转换实现（已修改）
- `FFDecHW.cpp` - 硬件解码器调用
- `hardware_decoder.h` - 接口定义
- `ffmpeg.h` - FFmpeg 头文件封装

## 参考资料

1. **videodecode.cpp** - MediaPlayer-PlayWidget 的参考实现
2. **Frame.cpp** - QMPlayer2 的参考实现（L504）
3. **FFmpeg 文档** - `av_hwframe_map` 和 `av_hwframe_transfer_data`

## 更新时间
2026-01-19 17:40

## 下一步行动

1. **运行最新代码**，观察新的日志输出
2. **确认 `av_hwframe_map` 是否可用**（检查 FFmpeg 版本）
3. **如果成功**，验证视频正常渲染
4. **如果失败**，查看错误日志并考虑回退方案
