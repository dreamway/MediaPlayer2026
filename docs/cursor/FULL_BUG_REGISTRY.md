# WZMediaPlayer 完整 Bug 登记表

**日期**: 2026-03-03  
**基于**: docs/ 全部 63 个文档 + 源码 Code Review  
**维护者**: Cursor Cloud Agent

---

## 重构历史概要

项目经历了 **四轮主要重构**，由不同的 AI 模型/开发者主导：

| 轮次 | 文档数 | 时间 | 主题 | 关键模型 |
|------|--------|------|------|---------|
| 第一轮 | 10 | 2025-12~2026-01 | QMPlayer2 风格架构（PlayController、Demuxer、AVThread 等） | 初始架构 |
| 第二轮 | 4 | 2026-01 | GLM 改进（AudioThread、OpenAL、音视频同步） | GLM-4 |
| 第三轮 | 4 | 2026-01 | 硬件解码修复（FFDecHW、EAGAIN、hw_frames_ctx） | 编译/硬件 |
| 第四轮 | 11 | 2026-02 | lwPlayer 架构（VideoRenderer、AVClock、Camera、Statistics） | lwPlayer |
| 补丁轮 | 16 | 2026-02~03 | TDD 稳定性、闭环测试、BUG 修复 | TDD/测试 |
| Cursor 轮 | — | 2026-03 | 跨平台 CMake、Code Review、架构优化 | Cursor |

---

## Bug 总表

### ✅ 已修复且已验证

| 编号 | 描述 | 根因 | 修复方案 | 验证方式 |
|------|------|------|---------|---------|
| BUG-001 | 播放/切换视频时崩溃 | PacketQueue Reset 与消费者线程 race | resetting_ 标志 + setFinished() 唤醒 | 自动化测试 7/7 |
| BUG-013 | 首次打开等待 ~2s | 未检查 isOpened() | openPath() 添加判断 | 代码审查 |
| BUG-016 | 播放列表切换崩溃 | 同 BUG-001 根因 | stop() 等待增强 | 合并到 BUG-001 |

### ✅ 已修复（通过 Code Review 验证）

| 编号 | 描述 | 根因 | 修复方案 | Cursor Review 结论 |
|------|------|------|---------|-------------------|
| BUG-002 | 音视频同步 lag 数十秒 | getMasterClock() 用 AVClock 而非 AL_SAMPLE_OFFSET | 改用 audioThread_->getClock() | ✅ 正确 + 修复 data race (C2) |
| BUG-003 | Seek 后无声音 | alSourceRewind 后 writeAudio 不重启 | clear() 添加 alSourceRewind + 修复 playbackStarted_ 逻辑 (H1+H7) | ✅ 修复 |
| BUG-004 | 进度条不同步 | 同 BUG-002 + blockSignals | 随 BUG-002 + 动态 max 更新 + enterSeeking/exitSeeking (C1) | ✅ 修复 |
| BUG-005 | 黑屏闪烁 | 无帧时渲染黑屏 | lastFrame_ 缓存 | ✅ 代码正确 |
| BUG-014 | 列表自动下一首 | handlePlaybackFinished 未 openPath | 添加 openPath 调用 | ✅ 代码正确 |
| BUG-015 | 退出崩溃(logger) | LOG_MODE 判断缺失 | 析构函数添加判断 | ✅ 代码正确 |
| BUG-017 | 退出崩溃(AdvWidget) | 指针未初始化 | 初始化 + 析构顺序 | ✅ 代码正确 |
| SEEK-001 | Seeking 花屏/卡顿 | 非关键帧跳过不足 | 关键帧跳过 + AVSEEK_FLAG_FRAME | ✅ 代码正确 |
| 线程系列 | Mutex/线程生命周期 | 跨线程解锁/悬空指针 | ThreadSyncManager、锁顺序 | ✅ 代码正确 |

### ⚠️ 未修复（有 workaround）

| 编号 | 描述 | 当前状态 | Workaround |
|------|------|---------|-----------|
| BUG-006 | 硬件解码黑屏 | av_hwframe_transfer_data 失败 | 配置中禁用硬件解码 |
| BUG-007 | 视频色彩错误(绿/品红) | YUV→RGB U/V 通道或 range 问题 | 部分 shader 修复已应用 |
| BUG-008 | FPS 过低 (0.5~22) | 帧过期逻辑过度丢帧 | 依赖 BUG-002 修复后改善 |
| BUG-009 | 摄像头功能 | QtMultimedia 后端 DLL 缺失 | UI 逻辑已修，后端缺失 |
| BUG-010 | 3D 切换无效 | 信号连接/渲染器更新不完整 | 未排查 |
| BUG-011 | MainLogo Slogan | LogoWidget 初始化不完整 | 未排查 |
| BUG-012 | VS 项目文件显示 | vcxproj.filters 异常 | 低优先级 |

### 📋 旧文档中提到但未编号的问题

| 问题 | 来源文档 | 状态 | 备注 |
|------|----------|------|------|
| Seek 卡死 | 重构计划、功能状态梳理 | 合并到 BUG-001/003 | PacketQueue 阻塞导致 |
| OpenAL buffer-in-use 崩溃 | GLM-progress | ✅ 已修复 | availableBuffers_ 重构 |
| 视频队列满阻塞 | GLM-progress | ✅ 已修复 | FFDecSW 返回值约定 |
| worktree exe 启动崩溃 | closed-loop-enhancement | ⚠️ 未修复 | Windows DLL 问题 |
| Camera 打开即崩溃 | lwPlayer/当前进度 | 合并到 BUG-009 | Linux 上已条件禁用 |
| MainWindow Seek 参数错误 (秒 vs 毫秒) | BUG_FIXES 补充 | ✅ 已修复 | seek(seekPosMs) |

---

## 基础设施状态

| 组件 | 状态 | Cursor 更新 |
|------|------|------------|
| PlaybackStateMachine | ✅ 正确使用 | enterSeeking/exitSeeking 已集成到 PlayController |
| ErrorRecoveryManager | ✅ 增强 | FlushAndRetry 真正执行 flush + 成功解码后重置计数 |
| SystemMonitor | ✅ 已集成 | LOG_LEVEL<=1 时自动启动，PlayController::stop() 自动停止 |
| ThreadSyncManager | ✅ 增强 | 新增 ScopedLock RAII 类简化使用 |
| crash_handler | ✅ 跨平台 | Linux 添加 signal handler (SIGSEGV/SIGABRT/SIGFPE + backtrace) |
| AVClock | ✅ 正确 | hasAudioPts_ 消除歧义 |
| chronons.h | ✅ 增强 | kInvalidTimestamp + isValidTimestamp() + safeTimestampAdd() |

---

## 文档对照索引

docs/ 下的 63 个文档与本登记表的对应关系：

- **核心参考**: BUG_FIXES.md, TODO_CURRENT.md, TODO.md
- **第一轮**: 重构计划.md, 重构计划-核心解码功能.md, 小步快跑式重构计划.md, FFmpegView改造计划.md, 新建文件完成总结.md, 现有文件修改计划.md, AVThread重构与Seeking修复计划.md, 重构反思与下一步规划.md, 功能状态梳理与重构反思.md, 与QMPlayer2相比.md
- **第二轮 (GLM)**: GLM-progress.md, GLM代码关键信息.md, GLM改进建议.md, GLM改进步骤详细规划.md
- **第三轮 (硬件)**: HARDWARE_DECODE_FIX.md, HARDWARE_DECODE_FIX_V2.md, FIX_SUMMARY_FFDecHW.md, COMPILE_ERROR_FIX.md
- **第四轮 (lwPlayer)**: lwPlayer/ 目录下全部 11 个文档
- **Bug 修复总结**: Seeking花屏.md, 稳定性修复.md, 线程同步.md, Mutex生命周期.md, 跨线程Mutex.md, 线程生命周期.md, 视频切换崩溃.md, Playback_Crashed.md, Stability_Fixes.md
- **全局优化**: 架构分析与稳定性改进方案.md, 全局优化与稳定性改进方案.md, 全局优化完成总结.md, 全局优化实施总结.md
- **测试/计划**: plans/ 目录下全部 7 个文档, TDD/Catch2.md, 单元测试与集成测试计划.md
- **其他**: arch/Demuxer_vs_Timer.md, rendering_architecture_usage.md, SHORTCUTCHANGES.md, REFACTORING_PROGRESS_PHASE1/2.md, 继续工作Prompt.md
