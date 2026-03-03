# 硬件解码黑画面问题分析与修复

## 问题现象
播放视频时显示黑画面，日志中出现大量错误：
```
FFDecHW::decodeVideo: sw_frame is invalid after transfer - data[0]: false, width: 0, height: 0, format: -1
```

## 日志分析

### 关键日志序列

1. **硬件解码器初始化成功**（L54-68）：
```
FFDecHW::init: Hardware decoder initialized successfully: cuda
Hardware decoder initialized successfully: h264_cuvid, device: cuda, hw_pix_fmt: 117, needs_transfer: true
```

2. **开始解码，连续返回 EAGAIN**（L114-167）：
```
FFDecHW::decodeVideo: EAGAIN with packet consumed, no output frame - returning 1
```

3. **第一次硬件帧转换失败**（L168）：
```
FFDecHW::decodeVideo: sw_frame is invalid after transfer - data[0]: false, width: 0, height: 0, format: -1
```

4. **后续持续失败**（L174-394）：所有硬件帧转换都失败

## 根本原因分析

### 问题 1：硬件帧格式不匹配

**硬件帧信息**（L68）：
- `hw_pix_fmt: 117` (AV_PIX_FMT_CUDA)
- `needs_transfer: true`

**硬件帧状态**（L318-330 in hardware_decoder.cc）：
```cpp
if (hw_type == AV_HWDEVICE_TYPE_CUDA) {
    is_hw_format = (hw_frame->format == AV_PIX_FMT_CUDA);
}
```

**问题**：硬件帧的 `format` 字段可能不是 CUDA 格式，导致 `transferFrame` 判断不需要转换，但实际上需要转换。

### 问题 2：av_frame_copy_props 失败

**代码位置**：hardware_decoder.cc:349

```cpp
ret = av_frame_copy_props(sw_frame, hw_frame);
```

**问题**：如果 `hw_frame` 是硬件帧，`av_frame_copy_props` 可能无法正确复制属性，导致：
- `sw_frame->width = 0`
- `sw_frame->height = 0`
- `sw_frame->format = -1`

### 问题 3：日志格式问题

**当前格式**：
```cpp
"[%Y-%m-%d %H:%M:%S.%e][thread %t][%@][%!][%L] : %v"
```

**问题**：`%@` 和 `%!` 格式说明符在 SPDLOG 中不支持，导致日志显示为空：
```
[thread 66132][,][info]
```

## 已完成的修复

### 1. 修复日志格式

**修改文件**：`MainWindow.cpp`

**修改内容**：
```cpp
// 修改前
file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e][thread %t][%@][%!][%L] : %v");

// 修改后
file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e][thread %t][%s:%#][%L] : %v");
```

**格式说明**：
- `%s`: 源文件名
- `%#`: 行号
- `%L`: 日志级别

### 2. 添加调试日志

**修改文件**：`FFDecHW.cpp`

**修改内容**：
```cpp
// 在 transferFrame 之前添加调试日志
logger->debug(
    "FFDecHW::decodeVideo: Before transfer - hw_frame: format={}, width={}, height={}, data[0]={}",
    static_cast<int>(hw_frame->format),
    hw_frame->width,
    hw_frame->height,
    hw_frame->data[0] != nullptr ? "valid" : "null");
```

## 建议的后续修复

### 1. 修复 hardware_decoder.cc 的 transferFrame

**问题**：`av_frame_copy_props` 无法正确处理硬件帧

**建议修复**：
```cpp
// 替换 av_frame_copy_props
// 直接设置 sw_frame 的属性
sw_frame->format = AV_PIX_FMT_NV12; // 或其他软件格式
sw_frame->width = hw_frame->width;
sw_frame->height = hw_frame->height;
sw_frame->pts = hw_frame->pts;
// ... 其他属性
```

### 2. 检查硬件帧的 format 字段

**建议修复**：
```cpp
// 在 transferFrame 中添加检查
logger->debug(
    "Hardware frame: format={}, width={}, height={}, hw_frames_ctx={}",
    static_cast<int>(hw_frame->format),
    hw_frame->width,
    hw_frame->height,
    hw_frame->hw_frames_ctx != nullptr ? "valid" : "null");
```

### 3. 考虑使用 av_hwframe_transfer_data 的替代方案

**当前方案**：
```cpp
av_hwframe_transfer_data(sw_frame, hw_frame, 0);
```

**替代方案**：使用 `av_hwframe_transfer_data` 的反向方向，或直接分配 sw_frame 并手动填充数据。

### 4. 验证硬件帧池配置

**检查点**：
- `AVHWFramesContext` 是否正确初始化
- `sw_format` 是否设置为正确的软件格式（如 NV12）
- `initial_pool_size` 是否足够

## 测试步骤

1. **编译最新代码**：
```bash
cd E:\WZMediaPlayer_2025\WZMediaPlay
./build.bat
```

2. **运行播放器并观察日志**：
- 检查新日志格式是否正确显示文件名和行号
- 检查 `Before transfer` 日志中的 hw_frame 状态
- 检查 transferFrame 是否返回错误

3. **分析 hw_frame 状态**：
- 如果 `hw_frame->data[0]` 是 null，说明硬件帧无效
- 如果 `hw_frame->format` 不是 CUDA 格式，说明格式判断有问题
- 如果 `hw_frame->width/height` 是 0，说明帧未正确解码

## 参考文件

- `FFDecHW.cpp` - 硬件解码器实现
- `hardware_decoder.cc` - 硬件帧转换逻辑
- `MainWindow.cpp` - 日志格式配置

## 更新时间
2026-01-19 17:30
