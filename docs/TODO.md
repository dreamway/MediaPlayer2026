# WZMediaPlayer TODO List

**更新时间**: 2026-02-03
**维护者**: GLM（通用语言模型）
**版本**: v1.3

---

## P0: 关键 Bug 修复（立即执行）

### P0.1 Seeking 后音频无声 🔴
**优先级**: 最高
**状态**: ✅ 已修复 (2026-02-03)
**问题描述**: Seeking 后视频正常播放，但音频无声音

**根因分析**:
- `Audio::clear()` 重置了 `playbackStarted_ = false` 和 `deviceStartTime_ = {}`
- `clear()` 将所有 OpenAL 缓冲区放回 `availableBuffers_` 队列
- `writeAudio()` 中的播放启动条件 `!playbackStarted_ && queued >= 2` 不满足
- `alSourcePlay()` 永远不被调用，导致音频无声

**修复方案**:
- [x] 修改 `OpenALAudio::writeAudio()` 的播放启动逻辑
- [x] 条件从 `queued >= 2` 改为检查 `availableBuffers_.empty()`
- [x] 确保 Seeking 后有缓冲区时立即启动播放
- [x] 测试验证: 验证 Seeking 后音频能正常播放

**修复详情** (`OpenALAudio.cpp` 第 276-310 行):
```cpp
// 修改前:
if (!playbackStarted_ && queued >= 2) {

// 修改后:
if (!playbackStarted_) {
    bool shouldStart = false;
    if (queued >= 2) {
        shouldStart = true;
    } else if (!availableBuffers_.empty()) {
        // seeking 后只要有可用缓冲区就启动
        logger->info("Audio::writeAudio: Starting playback after seek (queued={}, available={})", queued, availableBuffers_.size());
        shouldStart = true;
    }
    if (shouldStart) {
        // ... 启动播放
    }
}
```

**相关文件**:
- `WZMediaPlay/videoDecoder/OpenALAudio.cpp` (第 276-310 行)
- `WZMediaPlay/videoDecoder/AudioThread.cpp` (第 136 行 - audio_->clear())

### P0.2 CustomSlider 进度条不同步 🔴
**优先级**: 最高
**状态**: ✅ 已修复 (2026-02-03)
**问题描述**: Seeking 后 customSlider 进度条未正确更新

**根因分析**:
- `onSeekingFinished` 调用 `setValue()` 时会触发 `valueChanged` 信号
- `StereoVideoWidget` 的定时器也会发送 `updatePlayProcess` 信号更新进度条
- 两者信号冲突，导致进度条显示不正确

**修复方案**:
- [x] 在 `onSeekingFinished` 中使用 `blockSignals(true/false)` 避免信号循环
- [x] 防止与 `StereoVideoWidget::updatePlayProcess` 信号冲突
- [x] 验证 slider 实际值与期望值一致
- [x] 测试验证: 验证 Seeking 后进度条正确更新

**修复详情** (`MainWindow.cpp` 第 2940-2948 行):
```cpp
// 修改前:
ui.horizontalSlider_playProgress->setValue(posInSeconds);

// 修改后:
// 关键修复：使用 blockSignals 避免与 valueChanged 信号循环触发
// 同时防止与 StereoVideoWidget::updatePlayProcess 信号冲突
ui.horizontalSlider_playProgress->blockSignals(true);
ui.horizontalSlider_playProgress->setValue(posInSeconds);
ui.horizontalSlider_playProgress->blockSignals(false);
```

**相关文件**:
- `WZMediaPlay/MainWindow.cpp` (第 2929-2958 行)

### P0.3 Seeking 后时钟返回错误值 🔴
**优先级**: 最高
**状态**: ✅ 已修复 (2026-02-03)
**问题描述**: Seeking 后 `getCurrentPositionMs()` 返回 `-9223372036737 ms`，进度条卡住

**根因分析**:
- 日志显示: `getCurrentPositionMs: Invalid position (-9223372036737 ms)`
- 这是 `nanoseconds::min()` 转换后的值，说明 `getVideoClock()` 返回了无效值
- `PlayController::seek()` 设置了 `videoClock_ = seekBasePts`
- **但未重置 `videoClockTime_`**
- `getVideoClock()` 中 `videoClockTime_ != time_point{}` 为 true（因为是旧值）
- 计算 `delta = now - videoClockTime_` 得到非常大的值

**修复方案**:
- [x] 在 `PlayController::seek()` 中重置 `videoClockTime_ = {}`
- [x] 这确保 seeking 后第一次 `getVideoClock()` 返回正确的 seeking 位置

**修复详情** (`PlayController.cpp` 第 961-963 行):
```cpp
// 修改前:
videoClock_ = seekBasePts;
threadSyncManager_.unlock(videoClockMutex_);

// 修改后:
videoClock_ = seekBasePts;
videoClockTime_ = {}; // 重置时间点，标记为"未更新"
threadSyncManager_.unlock(videoClockMutex_);
```

**相关文件**:
- `WZMediaPlay/PlayController.cpp` (第 955-968 行)
- `WZMediaPlay/PlayController.h` (第 231 行 - videoClockTime_ 声明)

---

## P1: 音视频同步修复（Priority 1）

### 1.1 问题诊断 ✅ 已完成
- [x] 分析 currentPts_ 更新逻辑（AudioThread::decodeFrame 中更新）
- [x] 检查 deviceStartTime_ 设置时机（Audio::writeAudio 中设置）
- [x] 分析 OpenAL 时钟与估算时钟差异原因（currentPts_ 已在播放时较大）

### 1.2 修复方案实施 🔄 进行中
- [x] 添加全局 basePts_ 到 PlayController（统一基准 PTS）
- [x] 在播放开始时同步设置 basePts_ 到 VideoThread 和 AudioThread
- [x] 优化 Audio::getClock() 使用 basePts_ 作为基准
- [ ] 添加时钟连续性检查，避免时钟跳跃
- [ ] 改进 OpenAL 时钟与估算时钟的同步机制（扩大容忍范围）

### 1.3 测试验证
- [ ] 测试音视频同步准确性（误差 < 40ms）
- [ ] 测试长时间播放时钟稳定性（无 "diff too large" 警告）
- [ ] 测试 Seeking 后时钟重置（时钟连续性）

---

## P2: FFDecHW 硬解码优化（Priority 2）

### 2.1 硬解码验证
**优先级**: 中
**状态**: 待验证
- [ ] 验证硬件解码器初始化（检查 CUDA 解码器）
- [ ] 验证硬件帧转换（hwframe_transfer_data）
- [ ] 实现硬解码自动回退到软件解码机制
- [ ] 测试硬件解码性能（CPU 占用降低）

### 2.2 性能优化
- [ ] 优化硬件解码器切换逻辑（减少切换延迟）
- [ ] 优化硬件帧转换性能（减少内存拷贝）
- [ ] 添加硬件解码器状态监控（日志和统计）
- [ ] 改进硬件解码器错误恢复（超时检测）

**相关文件**:
- `WZMediaPlay/videoDecoder/FFDecHW.cpp`
- `WZMediaPlay/videoDecoder/FFDecHW.h`
- `WZMediaPlay/videoDecoder/hardware_decoder.h`

---

## P3: Seeking 优化确认（Priority 2）

### 3.1 Seeking 同步验证
**优先级**: 中
**状态**: 待验证
- [ ] 验证 Seeking 后音视频时钟重置（basePts_ 正确设置）
- [ ] 验证 Seeking 后队列清空（PacketQueue 完全清空）
- [ ] 验证 Seeking 后关键帧处理（wasSeekingRecently 正确清除）

### 3.2 Seeking 性能优化
- [ ] 优化 Seeking 响应速度（目标 < 500ms）
- [ ] 优化 Seeking 后的缓冲策略（预缓冲）
- [ ] 添加 Seeking 进度反馈（UI 显示）

**相关文件**:
- `WZMediaPlay/PlayController.cpp` (seek() 方法，第 700-1000 行)
- `WZMediaPlay/videoDecoder/AudioThread.cpp` (handleSeekingState)
- `WZMediaPlay/videoDecoder/VideoThread.cpp` (handleSeekingState)

---

## P4: UI功能还原（Priority 3）

### 4.1 FFmpegView功能迁移分析
**优先级**: 低
**状态**: 待分析
- [ ] 分析原 FFmpegView.cc 类的功能列表
- [ ] 识别需要迁移的 UI 功能组件

### 4.2 3D 功能实现
- [ ] 修复 OpenGLWidget 中的 3D 渲染
- [ ] 实现 3D 格式自动检测
- [ ] 实现 3D 视差调节滑块
- [ ] 实现 CameraRenderer 控制

### 4.3 UI 组件整合
- [ ] 整合所有 UI 功能到主界面
- [ ] 确保 UI 操作流畅无卡顿
- [ ] 测试所有 UI 功能

**相关文件**:
- `WZMediaPlay/FFmpegView.cc`
- `WZMediaPlay/FFmpegView.h`
- `WZMediaPlay/CameraRenderer.cpp`
- `WZMediaPlay/CameraRenderer.h`
- `WZMediaPlay/videoDecoder/opengl/OpenGLWidget.cpp`

---

## P5: 代码优化与测试（Priority 4）

### 5.1 代码结构优化
**优先级**: 低
**状态**: 后续优化
- [ ] 整理 videoDecoder 目录结构
- [ ] 完善代码注释
- [ ] 移除冗余代码
- [ ] 统一日志格式

### 5.2 测试框架搭建
- [ ] 添加单元测试框架
- [ ] 编写核心模块的单元测试
- [ ] 编写集成测试

### 5.3 性能优化
- [ ] 优化内存使用
- [ ] 优化 CPU 占用
- [ ] 优化渲染性能

---

## 架构说明

### 当前架构
```
MainWindow (UI 层)
    ↓
PlayController (播放控制层 - 核心控制器)
    ├── DemuxerThread (解复用线程)
    ├── VideoThread (视频解码线程) → VideoWriter → OpenGLWidget
    └── AudioThread (音频解码线程) → OpenALAudio
```

### 核心组件职责
| 组件 | 职责 | 位置 |
|------|------|------|
| PlayController | 协调所有线程、管理播放状态、处理用户命令 | `PlayController.cpp/h` |
| DemuxerThread | 解复用媒体文件，分发数据包 | `DemuxerThread.cpp/h` |
| VideoThread | 视频解码、渲染、时钟更新 | `VideoThread.cpp/h` |
| AudioThread | 音频解码、输出、时钟管理 | `AudioThread.cpp/h` |
| OpenALAudio | OpenAL 音频输出、时钟计算 | `OpenALAudio.cpp/h` |

### 关键数据结构
- **PacketQueue**: 数据包队列（视频 50MB，音频 4MB）
- **PlaybackStateMachine**: 播放状态机（Idle/Opening/Playing/Paused/Seeking/Stopping/Stopped/Error）
- **basePts_**: 全局基准 PTS（用于音视频同步）

---

## 已知问题清单

### 🔴 高优先级（影响核心功能）
1. [P0.1] Seeking 后音频无声
2. [P0.2] CustomSlider 进度条不同步
3. [1.2] 时钟跳跃问题

### 🟡 中优先级（影响用户体验）
1. [2.1] 硬解码回退机制不完善
2. [3.2] Seeking 响应速度慢

### 🟢 低优先级（功能增强）
1. [4.x] 3D 功能迁移
2. [5.x] 代码优化

---

## 开发计划

### 短期目标（v1.2 - 2026-01）
- [ ] 修复 P0.1 Seeking 后音频无声
- [ ] 修复 P0.2 CustomSlider 进度条不同步
- [ ] 验证音视频同步准确性

### 中期目标（v1.3 - 2026-02）
- [ ] 完成 Seeking 优化
- [ ] 实现硬解码自动回退
- [ ] 优化 Seeking 响应速度

### 长期目标（v2.0）
- [ ] 完成 UI 功能还原
- [ ] 添加单元测试
- [ ] 代码重构优化

---

**下次更新**: 2026-02-01
**维护者**: GLM（通用语言模型）