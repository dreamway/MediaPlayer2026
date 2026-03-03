# 与QMPlayer2相比，修改成更优雅稳定的代码

## 概述

本文档对比了当前 WZMediaPlayer 实现与 QMPlayer2 的架构差异，识别出可以改进的地方，并提供了具体的改进规划。

## 核心问题分析

### 1. Frame 封装不统一

**当前问题**：
- `FrameBuffer::Frame` 包含三种帧类型：`frame`（硬件帧）、`sw_frame`（软件帧）、`yuv420p_frame`（转换后帧）
- 需要 `selectFrameToUse()` 函数来选择使用哪个帧
- 多处需要验证帧的有效性（width>0, height>0, format有效等）
- 硬件解码转换逻辑分散在 `Video::start()` 中

**QMPlayer2 的做法**：
- `Frame` 类统一封装 `AVFrame`，隐藏硬件/软件细节
- 提供 `downloadHwData()` 方法自动处理硬件帧下载
- 帧的有效性检查封装在 `Frame` 类内部
- 硬件解码通过 `HWDecContext` 和 `VideoFilter` 统一管理

**改进方向**：
1. 创建统一的 `Frame` 类（参考 `QMPlayer2/src/qmplay2/Frame.hpp`）
2. 将硬件帧转换逻辑封装在 `Frame` 类中
3. 移除 `selectFrameToUse()` 函数，直接使用 `Frame` 对象
4. 在 `Frame` 类内部进行有效性检查

### 2. Decoder 接口不清晰

**当前问题**：
- `Video` 和 `Audio` 类直接操作 `AVCodecContext`
- 硬件解码逻辑与解码逻辑混合
- 解码器初始化、硬件解码尝试、回退逻辑都在 `Video` 类中

**QMPlayer2 的做法**：
- `Decoder` 接口统一管理解码逻辑
- `hasHWDecContext()` 和 `hwAccelFilter()` 分离硬件解码关注点
- `decodeVideo()` 和 `decodeAudio()` 接口清晰
- 硬件解码通过 `HWDecContext` 和 `VideoFilter` 管理

**改进方向**：
1. 创建 `Decoder` 接口（参考 `QMPlayer2/src/qmplay2/Decoder.hpp`）
2. 实现 `FFDecSW`（软件解码）和 `FFDecHW`（硬件解码）子类
3. 将硬件解码逻辑从 `Video` 类中分离
4. 通过 `VideoFilter` 链处理硬件帧转换

### 3. VideoWriter 接口缺失

**当前问题**：
- `FFmpegView` 直接操作 `AVFrame` 进行渲染
- 硬件帧转换逻辑在 `Video::start()` 中
- 渲染逻辑与解码逻辑耦合

**QMPlayer2 的做法**：
- `VideoWriter` 接口统一管理视频渲染
- `OpenGLWriter` 实现 OpenGL 渲染
- `setHWDecContext()` 支持硬件互操作
- `supportedPixelFormats()` 声明支持的像素格式

**改进方向**：
1. 创建 `VideoWriter` 接口（参考 `QMPlayer2/src/qmplay2/VideoWriter.hpp`）
2. 将 `FFmpegView` 重构为 `OpenGLWriter` 实现
3. 通过 `VideoWriter::writeVideo()` 统一渲染接口
4. 硬件互操作通过 `setHWDecContext()` 管理

### 4. PacketBuffer 设计复杂

**当前问题**：
- `FrameBuffer` 包含复杂的音视频同步逻辑
- `getFrame()` 方法需要 `GetFrameOptions` 参数
- 帧过期检测、跳帧逻辑都在 `FrameBuffer` 中
- 多处需要验证帧的有效性

**QMPlayer2 的做法**：
- `PacketBuffer` 使用 `std::deque<Packet>`，逻辑简单
- `seekTo()` 方法处理 seek 操作
- `fetch()` 方法简单获取下一个数据包
- 音视频同步在 `VideoThr` 中处理，不在缓冲区中

**改进方向**：
1. 简化 `FrameBuffer`，只负责帧的存储和基本同步
2. 将音视频同步逻辑移到 `VideoThread` 中
3. 使用 `std::deque` 替代环形缓冲区（如果不需要固定大小）
4. 帧选择逻辑在 `VideoThread` 中处理

### 5. 线程职责不清晰

**当前问题**：
- `Video::start()` 包含解码、格式转换、硬件解码处理、音视频同步
- `Audio::start()` 包含解码、重采样、播放
- 线程逻辑与业务逻辑混合

**QMPlayer2 的做法**：
- `VideoThr` 负责：从 `PacketBuffer` 获取数据包、调用 `Decoder::decodeVideo()`、音视频同步、调用 `VideoWriter::writeVideo()`
- `AudioThr` 负责：从 `PacketBuffer` 获取数据包、调用 `Decoder::decodeAudio()`、重采样、调用 `Writer::write()`
- 职责清晰，易于维护

**改进方向**：
1. `VideoThread` 只负责：获取数据包、调用解码器、音视频同步、调用渲染器
2. `AudioThread` 只负责：获取数据包、调用解码器、重采样、调用音频输出
3. 格式转换、硬件解码处理通过 `Frame` 类和 `VideoFilter` 处理
4. 移除 `Video::start()` 中的格式转换逻辑

### 6. 硬件解码处理分散

**当前问题**：
- 硬件解码尝试在 `PlayController::initializeCodecs()` 中
- 硬件帧转换在 `Video::start()` 中（NV12 转 YUV420P）
- 硬件解码器管理在 `PlayController` 中
- 多处需要检查是否是硬件格式

**QMPlayer2 的做法**：
- 硬件解码通过 `Decoder` 接口的 `hasHWDecContext()` 和 `hwAccelFilter()` 管理
- 硬件帧转换通过 `VideoFilter` 链处理
- `OpenGLWriter` 通过 `setHWDecContext()` 支持硬件互操作
- 硬件帧通过 `Frame::downloadHwData()` 自动下载

**改进方向**：
1. 创建 `HWDecContext` 基类（参考 `QMPlayer2/src/qmplay2/HWDecContext.hpp`）
2. 硬件解码器实现 `HWDecContext` 接口
3. 硬件帧转换通过 `VideoFilter` 处理
4. `VideoWriter` 通过 `setHWDecContext()` 支持硬件互操作

### 7. 音视频同步逻辑复杂

**当前问题**：
- 音视频同步逻辑在 `FrameBuffer::getFrame()` 中
- `Video::currentFrame()` 需要计算时钟、处理同步
- 多处需要检查音频是否完成、时钟是否卡住

**QMPlayer2 的做法**：
- 音视频同步在 `VideoThr::run()` 中处理
- 计算视频帧时间戳与音频时钟的差值
- 根据差值调整延迟或跳过帧
- 同步逻辑集中在一个地方

**改进方向**：
1. 将音视频同步逻辑从 `FrameBuffer` 移到 `VideoThread` 中
2. `FrameBuffer` 只负责帧的存储和基本的时间戳匹配
3. `VideoThread` 负责计算同步差值、调整延迟、跳过帧
4. 简化 `Video::currentFrame()` 方法

### 8. 帧验证逻辑重复

**当前问题**：
- `selectFrameToUse()` 需要验证帧的有效性
- `FrameBuffer::getFrame()` 需要验证帧的有效性
- `Video::currentFrame()` 需要验证帧的有效性
- `FFmpegView::OnRenderTimer()` 需要验证帧的有效性
- 验证逻辑重复，容易遗漏

**QMPlayer2 的做法**：
- `Frame` 类封装有效性检查
- `isEmpty()` 方法检查帧是否有效
- 渲染器只需要检查 `Frame::isEmpty()`

**改进方向**：
1. 在 `Frame` 类中封装有效性检查
2. 提供 `isEmpty()` 或 `isValid()` 方法
3. 移除各处的重复验证逻辑
4. 统一使用 `Frame` 类的验证方法

## 具体改进规划

### 阶段1：创建统一的 Frame 类（优先级：高）

**目标**：统一帧的封装，隐藏硬件/软件细节

**任务**：
1. 创建 `Frame` 类（参考 `QMPlayer2/src/qmplay2/Frame.hpp`）
2. 封装 `AVFrame`，提供统一的接口
3. 实现 `downloadHwData()` 方法处理硬件帧下载
4. 实现 `isEmpty()` 方法检查帧的有效性
5. 移除 `FrameBuffer::Frame` 中的三种帧类型，统一使用 `Frame`

**修改文件**：
- `WZMediaPlay/videoDecoder/Frame.h`（新建）
- `WZMediaPlay/videoDecoder/Frame.cpp`（新建）
- `WZMediaPlay/videoDecoder/FrameBuffer.h`（修改）
- `WZMediaPlay/videoDecoder/FrameBuffer.cpp`（修改）
- `WZMediaPlay/videoDecoder/video.cc`（修改）
- `WZMediaPlay/FFmpegView.cc`（修改）

**预期效果**：
- 移除 `selectFrameToUse()` 函数
- 简化帧的有效性检查
- 硬件帧转换逻辑封装在 `Frame` 类中

### 阶段2：创建 Decoder 接口（优先级：高）

**目标**：统一解码接口，分离硬件解码关注点

**任务**：
1. 创建 `Decoder` 接口（参考 `QMPlayer2/src/qmplay2/Decoder.hpp`）
2. 实现 `FFDecSW`（软件解码）子类
3. 实现 `FFDecHW`（硬件解码）子类
4. 将 `Video` 和 `Audio` 类的解码逻辑移到 `Decoder` 子类中
5. 通过 `hasHWDecContext()` 和 `hwAccelFilter()` 管理硬件解码

**修改文件**：
- `WZMediaPlay/videoDecoder/Decoder.h`（新建）
- `WZMediaPlay/videoDecoder/Decoder.cpp`（新建）
- `WZMediaPlay/videoDecoder/FFDecSW.h`（新建）
- `WZMediaPlay/videoDecoder/FFDecSW.cpp`（新建）
- `WZMediaPlay/videoDecoder/FFDecHW.h`（新建）
- `WZMediaPlay/videoDecoder/FFDecHW.cpp`（新建）
- `WZMediaPlay/videoDecoder/video.cc`（修改）
- `WZMediaPlay/videoDecoder/audio.cc`（修改）

**预期效果**：
- 解码逻辑与线程逻辑分离
- 硬件解码逻辑统一管理
- 易于添加新的解码器实现

### 阶段3：创建 VideoWriter 接口（优先级：中）

**目标**：统一视频渲染接口，支持硬件互操作

**任务**：
1. 创建 `VideoWriter` 接口（参考 `QMPlayer2/src/qmplay2/VideoWriter.hpp`）
2. 将 `FFmpegView` 重构为 `OpenGLWriter` 实现
3. 实现 `writeVideo()` 方法
4. 实现 `setHWDecContext()` 方法支持硬件互操作
5. 实现 `supportedPixelFormats()` 方法
6. 特别要留意FFmpegView，因为包含了很多自定义的渲染逻辑，可以先用你自带的VideoWriter或OpenGLWriter，先确保2D能够正常渲染，后续再考虑把3D渲染（即FFmpegView中核心内容）再做整合。

**修改文件**：
- `WZMediaPlay/videoDecoder/VideoWriter.h`（新建）
- `WZMediaPlay/videoDecoder/VideoWriter.cpp`（新建）
- `WZMediaPlay/FFmpegView.h`（修改）
- `WZMediaPlay/FFmpegView.cc`（修改）

**预期效果**：
- 渲染逻辑与解码逻辑分离
- 支持硬件互操作
- 易于添加新的渲染器实现

### 阶段4：简化 FrameBuffer（优先级：中）

**目标**：简化帧缓冲区，将同步逻辑移到线程中

**任务**：
1. 简化 `FrameBuffer`，只负责帧的存储和基本的时间戳匹配
2. 移除 `GetFrameOptions` 参数，简化 `getFrame()` 方法
3. 将音视频同步逻辑移到 `VideoThread` 中
4. 使用 `std::deque` 替代环形缓冲区（如果不需要固定大小）

**修改文件**：
- `WZMediaPlay/videoDecoder/FrameBuffer.h`（修改）
- `WZMediaPlay/videoDecoder/FrameBuffer.cpp`（修改）
- `WZMediaPlay/videoDecoder/VideoThread.cpp`（修改）

**预期效果**：
- `FrameBuffer` 逻辑简单清晰
- 音视频同步逻辑集中在一个地方
- 易于理解和维护

### 阶段5：重构 VideoThread 和 AudioThread（优先级：高）

**目标**：清晰线程职责，分离业务逻辑

**任务**：
1. `VideoThread` 只负责：获取数据包、调用解码器、音视频同步、调用渲染器
2. `AudioThread` 只负责：获取数据包、调用解码器、重采样、调用音频输出
3. 移除 `Video::start()` 中的格式转换逻辑
4. 格式转换通过 `Frame` 类和 `VideoFilter` 处理

**修改文件**：
- `WZMediaPlay/videoDecoder/VideoThread.cpp`（修改）
- `WZMediaPlay/videoDecoder/AudioThread.cpp`（修改）
- `WZMediaPlay/videoDecoder/video.cc`（修改）
- `WZMediaPlay/videoDecoder/audio.cc`（修改）

**预期效果**：
- 线程职责清晰
- 业务逻辑与线程逻辑分离
- 易于测试和维护

### 阶段6：创建 HWDecContext 和 VideoFilter（优先级：中）

**目标**：统一硬件解码管理，支持硬件帧转换

**任务**：
1. 创建 `HWDecContext` 基类（参考 `QMPlayer2/src/qmplay2/HWDecContext.hpp`）
2. 硬件解码器实现 `HWDecContext` 接口
3. 创建 `VideoFilter` 接口处理硬件帧转换
4. 实现 `hwAccelFilter` 处理硬件帧下载和转换

**修改文件**：
- `WZMediaPlay/videoDecoder/HWDecContext.h`（新建）
- `WZMediaPlay/videoDecoder/HWDecContext.cpp`（新建）
- `WZMediaPlay/videoDecoder/VideoFilter.h`（新建）
- `WZMediaPlay/videoDecoder/VideoFilter.cpp`（新建）
- `WZMediaPlay/videoDecoder/hardware_decoder.h`（修改）
- `WZMediaPlay/videoDecoder/hardware_decoder.cc`（修改）

**预期效果**：
- 硬件解码逻辑统一管理
- 硬件帧转换通过 `VideoFilter` 处理
- 易于添加新的硬件解码器

## 实施建议

### 优先级排序

1. **阶段1（Frame 类）**：这是基础，其他改进都依赖它
2. **阶段5（重构线程）**：这是核心，直接影响代码质量
3. **阶段2（Decoder 接口）**：分离解码逻辑，提高可维护性
4. **阶段4（简化 FrameBuffer）**：简化同步逻辑，提高可读性
5. **阶段3（VideoWriter 接口）**：分离渲染逻辑，提高可扩展性
6. **阶段6（HWDecContext 和 VideoFilter）**：统一硬件解码管理，提高可维护性

### 实施原则

1. **小步快跑**：每个阶段都是独立的、可验证的小改动
2. **保持功能**：每个阶段完成后，功能必须完全正常
3. **渐进式**：不一次性大改，而是逐步改进
4. **可回滚**：每个阶段都可以独立回滚
5. **测试驱动**：每个阶段完成后进行功能验证和回归测试

### 注意事项

1. **兼容性**：在重构过程中保持向后兼容，避免破坏现有功能
2. **性能**：确保重构后性能不下降，甚至有所提升
3. **可测试性**：每个组件都应该可以独立测试
4. **文档**：及时更新文档，记录架构变化

## 总结

通过参考 QMPlayer2 的成熟架构，我们可以显著改进代码质量：

1. **统一封装**：`Frame` 类统一封装帧，隐藏硬件/软件细节
2. **清晰接口**：`Decoder` 和 `VideoWriter` 接口清晰，职责明确
3. **简化逻辑**：`FrameBuffer` 简化，同步逻辑移到线程中
4. **分离关注点**：硬件解码、格式转换、音视频同步分离
5. **易于维护**：代码结构清晰，易于理解和维护

这些改进将显著提高代码的可维护性、可扩展性和稳定性。
