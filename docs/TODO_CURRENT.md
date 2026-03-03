# TODO 列表（2026-01-19 17:50）

## 🔴 高优先级（阻塞其他功能）

### 1. 硬件解码黑画面问题 ⚠️
**状态**：待解决

**问题描述**：
- 硬件解码器初始化成功（h264_cuvid, cuda)
- `avcodec_receive_frame` 返回成功，hw_frame 有效（width=2880, height=1440, data[0]=valid）
- `av_hwframe_map` 返回错误 -40 (Function not implemented)，当前FFmpeg版本不支持
- 硬件帧转换失败，导致 sw_frame 无效

**已尝试的修复**：
1. ✅ 修复 EAGAIN 处理逻辑（参考 FFDecSW）
2. ✅ 修复日志格式（显示文件名和行号）
3. ✅ 尝试使用 `av_hwframe_map`（不支持）
4. ✅ 回退到 `av_hwframe_transfer_data`（修复参数顺序）
5. ✅ 参考 videodecode.cpp 和 QMPlayer2 的实现

**当前状态**：
- ❌ 硬件解码：黑画面，帧转换失败
- ✅ 软件解码：正常工作（可作为回退方案）

**可能的解决方案**：
1. **升级FFmpeg版本** - `av_hwframe_map` 需要 FFmpeg 3.3+（当前可能较低）
2. **检查硬件帧池配置** - `hw_frames_ctx` 可能配置错误
3. **检查CUDA驱动版本** - 可能不兼容
4. **检查硬件帧格式** - `hw_frame->format = 0` 可能不正确

**参考资源**：
- `reference/MediaPlayer-PlayWidget/VideoPlay/videodecode.cpp` - working implementation
- `reference/QMPlayer2/src/qmplay2/Frame.cpp` - working implementation
- FFmpeg 文档 - `av_hwframe_map` vs `av_hwframe_transfer_data`

**备注**：
- 暂时可使用软件解码（FFDecSW）作为回退方案
- 优先修复其他问题，后续有时间再深入研究

---

## 🟡 中优先级（影响用户体验）

### 2. 音视频同步优化
**状态**：部分完成

**已完成**：
- ✅ 添加全局 basePts_ 到 PlayController
- ✅ 在播放开始时同步设置 basePts_
- ✅ 优化 Audio::getClock() 使用 basePts_

**待完成**：
- [ ] 添加时钟连续性检查，避免时钟跳跃
- [ ] 改进 OpenAL 时钟与估算时钟的同步机制（扩大容忍范围）

**参考**：`docs/TODO.md` Phase 1.2

---

### 3. Seeking 优化
**状态**：待验证

**待完成**：
- [ ] 验证 Seeking 后音视频时钟重置
- [ ] 验证 Seeking 后队列清空
- [ ] 验证 Seeking 后关键帧处理
- [ ] 优化 Seeking 响应速度（目标 < 500ms）
- [ ] 优化 Seeking 后的缓冲策略（预缓冲）

**参考**：`docs/TODO.md` Phase 3

---

### 4. 重构前功能还原
**状态**：待规划

**需还原的功能**：
1. **3D 功能**
   - [ ] 3D 视频格式检测
   - [ ] 3D 视差调节
   - [ ] 3D 模式切换

2. **UI 功能**
   - [ ] FPS 显示优化
   - [ ] 播放进度显示
   - [ ] 音量控制
   - [ ] 字幕显示

3. **播放控制**
   - [ ] 播放/暂停/停止
   - [ ] 快进/快退
   - [ ] 速度控制
   - [ ] A-B 循环

4. **媒体信息**
   - [ ] 视频信息显示（分辨率、帧率、编码格式等）
   - [ ] 音频信息显示（采样率、声道数等）
   - [ ] 媒体时长显示

**参考**：
- 旧代码：`reference/` 目录下的原实现
- TODO：需要对比新旧代码，逐个还原功能

---

## 🟢 低优先级（优化和改进）

### 5. 代码优化
**状态**：待完成

**待完成**：
- [ ] 整理 videoDecoder 目录结构
- [ ] 完善代码注释
- [ ] 移除冗余代码
- [ ] 统一日志格式（✅ 已部分完成）

**参考**：`docs/TODO.md` Phase 5.1

---

### 6. 性能优化
**状态**：待完成

**待完成**：
- [ ] 优化内存使用
- [ ] 优化 CPU 占用
- [ ] 优化渲染性能
- [ ] 优化硬件解码器切换逻辑

**参考**：`docs/TODO.md` Phase 5.3

---

### 7. 测试框架搭建
**状态**：待完成

**待完成**：
- [ ] 添加单元测试框架
- [ ] 编写核心模块的单元测试
- [ ] 编写集成测试

**参考**：`docs/TODO.md` Phase 5.2

---

### 8. 闭环测试增强（closed-loop-enhancement）
**状态**：Phase 1–2 已完成，Phase 3 待实现

**已完成**（已合并到 trae）：
- [x] ProcessOutputMonitor 时间窗口筛选日志
- [x] 失败步骤关联时间窗口内日志
- [x] 测试时强制 LogMode=1
- [x] LogMonitor 作为 fallback
- [x] C++ TestPipeServer + Python audio_pipe_client
- [x] 测试用例集成音频验证

**待完成**：
- [ ] **Task 8**：关键步骤增加黑屏检测（test_basic_playback 打开视频后、test_progress_and_seek Seek 后）
- [ ] 排查 worktree 构建 exe 从 subprocess 启动崩溃（0xC0000135）问题

**参考**：`docs/plans/2025-03-02-closed-loop-test-enhancement-summary.md`

---

## 📋 重构前功能还原规划

### 阶段 1：核心播放功能（优先）
1. **播放控制**
   - 播放/暂停/停止
   - 快进/快退
   - 速度控制
   - 进度条拖动

2. **音视频同步**
   - 确保当前实现稳定
   - 优化同步精度

3. **Seeking**
   - 确保当前实现稳定
   - 优化响应速度

### 阶段 2：高级功能
1. **3D 功能**
   - 3D 格式检测
   - 视差调节
   - 模式切换

2. **字幕功能**
   - 字幕文件加载
   - 字幕显示
   - 字幕同步

3. **音视频信息**
   - 媒体信息显示
   - 编码格式显示
   - 性能统计显示

### 阶段 3：UI 功能
1. **播放列表**
   - 播放列表管理
   - 播放顺序控制

2. **设置界面**
   - 硬件解码开关
   - 日志级别设置
   - 其他播放器设置

3. **快捷键**
   - 全局快捷键支持
   - 快捷键自定义

---

## 📊 问题分析总结

### 硬件解码问题根本原因
1. **FFmpeg 版本兼容性**：
   - `av_hwframe_map` 需要 FFmpeg 3.3+
   - 当前版本可能较低
   - `av_hwframe_transfer_data` 参数复杂

2. **硬件帧配置问题**：
   - `hw_frame->format = 0` (AV_PIX_FMT_NONE) 不正确
   - 可能是 `get_format` 回调函数问题
   - 可能是硬件帧池配置错误

3. **CUDA 驱动兼容性**：
   - CUDA 驱动版本可能不兼容
   - 需要检查 CUDA 版本

### 软件解码验证
- ✅ 软件解码（FFDecSW）正常工作
- ✅ 可作为硬解码的回退方案
- ✅ 音视频同步基本正常

---

## 🔧 临时解决方案

### 禁用硬件解码
**方法 1**：修改配置文件
```ini
[System]
HardwareDecoding=false
```

**方法 2**：在代码中强制使用软件解码
```cpp
// FFDecHW::init 中
use_hw_decoder_ = false;  // 强制禁用硬件解码
```

**方法 3**：使用 FFDecSW 替代 FFDecHW
```cpp
// PlayController 中
decoder_ = std::make_unique<FFDecSW>();  // 直接使用软件解码
```

---

## 📝 更新记录

**2026-01-19 17:50** - 创建 TODO 列表
- 记录硬件解码问题
- 规划重构前功能还原
- 设置优先级

**2026-01-19 17:30** - 修复日志格式
- 将 `%@,%!` 改为 `%s:%#`
- 正确显示文件名和行号

**2026-01-19 17:00** - 修复 EAGAIN 处理
- 参考 FFDecSW 的实现
- 统一返回值约定

---

## 下一步行动

### 立即执行（本周）
1. ✅ **使用软件解码** - 确保基本播放功能
2. [ ] **完善音视频同步** - 优化同步精度
3. [ ] **优化 Seeking** - 提升响应速度

### 短期目标（下周）
1. [ ] **还原播放控制功能** - 播放/暂停/停止、快进/快退
2. [ ] **还原 3D 功能** - 3D 格式检测、视差调节
3. [ ] **还原字幕功能** - 字幕加载和显示

### 中期目标（本月）
1. [ ] **深入研究硬件解码问题**
2. [ ] **考虑升级 FFmpeg 版本**
3. [ ] **测试 CUDA 驱动兼容性**
4. [ ] **完善错误恢复机制**

### 长期目标（下月）
1. [ ] **性能优化**
2. [ ] **代码重构**
3. [ ] **测试框架搭建**
4. [ ] **文档完善**

---

## 备注

1. **硬件解码问题暂时搁置**，因为：
   - 软件解码可用
   - 问题复杂，需要深入研究
   - 优先还原其他功能

2. **功能还原策略**：
   - 先还原核心播放功能
   - 再还原高级功能（3D、字幕等）
   - 最后优化性能

3. **测试策略**：
   - 每还原一个功能就立即测试
   - 确保稳定性后再继续下一个功能
   - 记录测试结果

---

## 参考资料

- `docs/TODO.md` - 原 TODO 列表
- `docs/FIX_SUMMARY_FFDecHW.md` - EAGAIN 处理修复总结
- `docs/HARDWARE_DECODE_FIX.md` - 硬件解码问题分析
- `docs/HARDWARE_DECODE_FIX_V2.md` - 硬件解码修复方案
- `reference/MediaPlayer-PlayWidget/` - 参考实现
- `reference/QMPlayer2/` - 参考实现

---

**维护者**：OpenCode
**最后更新**：2026-01-19 17:50
