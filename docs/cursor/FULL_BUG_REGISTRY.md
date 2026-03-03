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


## TestRound 1: 2026/03/03 21:34

---

## 🆕 新发现 Bug (2026-03-03 手动测试)

### 🔴 BUG-018: 视频切换时线程阻塞导致崩溃

| 属性 | 描述 |
|------|------|
| **现象** | 切换视频时程序崩溃，关闭上一个视频的解码线程时无限等待，最终强制关闭时崩溃 |
| **根因分析** | 线程同步问题：VideoThread/AudioThread 在 stop() 时等待消费者线程退出，但消费者线程可能阻塞在条件变量等待上，导致死锁 |
| **相关组件** | ThreadSyncManager, VideoThread, AudioThread, DemuxerThread |
| **优先级** | P0 - 致命 |
| **状态** | ⚠️ 未修复 |
| **建议方案** | 1. 使用 ThreadSyncManager 统一管理线程生命周期 2. 添加超时机制避免无限等待 3. 确保 stop() 时先 signal 再 wait |

### 🔴 BUG-019: 切换视频后进度条位置未重置

| 属性 | 描述 |
|------|------|
| **现象** | 手动切换打开新视频后，进度条位置未从头开始，仍停留在上一个视频播放位置附近 |
| **根因分析** | PlayController::openPath() 或 MainWindow 未正确重置进度条 slider 位置 |
| **相关组件** | MainWindow, PlayController, CustomSlider |
| **优先级** | P1 - 高 |
| **状态** | ⚠️ 未修复 |
| **建议方案** | 在 openPath() 成功后调用 slider->setValue(0) 并更新 duration 显示 |

### 🔴 BUG-020: 播放结束时画面未清理且UI状态异常

| 属性 | 描述 |
|------|------|
| **现象** | 播放结束时：1) 视频画面未清空为黑屏/Logo，残留旧帧图像 2) 播放控件未切换至"播放完成"状态 3) 进度条未重置 |
| **根因分析** | handlePlaybackFinished() 未正确清理 VideoWidget 显示，未重置 UI 控件状态 |
| **相关组件** | MainWindow::handlePlaybackFinished(), VideoWidget, StereoVideoWidget |
| **优先级** | P1 - 高 |
| **状态** | ⚠️ 未修复 |
| **建议方案** | 1. 播放结束时调用 videoWidget->clearFrame() 2. 重置 playButton 图标状态 3. 重置 slider 到 0 位置 |

### 🔴 BUG-021: 进度条快速Seek时画面/声音不同步

| 属性 | 描述 |
|------|------|
| **现象** | 进度条快速拖动时两种异常：1) 声音跟上新位置但画面仍显示旧帧 2) 声音和画面都未切换到新位置 |
| **根因分析** | Seeking 时 flush 不彻底，或 seek 完成后未正确触发首帧解码和显示更新 |
| **相关组件** | PlayController::seek(), DemuxerThread::seek(), VideoThread, AudioThread |
| **优先级** | P1 - 高 |
| **状态** | ⚠️ 未修复 |
| **建议方案** | 1. seek() 后强制刷新首帧 2. 确保 flush 后 decoder 状态正确重置 3. 添加 seek 完成回调通知 UI 更新 |

---

## 自动化测试结果

### 测试环境与路径（2026-03-03 更新）

| 项目 | 值 |
|------|-----|
| **EXE 路径** | `D:\2026Github\build\Release\WZMediaPlayer.exe`（CMake 构建产物） |
| **测试入口** | `testing/pywinauto/run_all_tests.py` |
| **测试视频** | `D:\2026Github\testing\video\test.mp4`（若不存在需先准备或修改脚本中的路径） |

以下脚本中的 EXE 路径已统一为上述路径：`run_all_tests.py`、`main.py`、`config.py`、`run_closed_loop_tests.py`、`unified_closed_loop_tests.py`、`test_av_sync.py`、`test_audio.py`、`test_progress_seek.py`、`test_3d_features.py`、`test_edge_cases.py`、`test_hardware_decoding.py`、`test_qt6_direct.py`、`ui_inspector.py`、`debug_ui.py`、`tests/closed_loop_tests.py`。

### 自动化测试发现的 BUG（测试框架/环境）

#### TEST-001: Windows 控制台 GBK 下 Unicode 符号导致测试中断

| 属性 | 描述 |
|------|------|
| **现象** | 运行 `run_all_tests.py` 时，各阶段在打印结果时抛出 `UnicodeEncodeError: 'gbk' codec can't encode character '\u2713' in position ...`，导致 7 个阶段均未完成实际用例，总计 0 通过/0 失败 |
| **根因** | 测试脚本中使用了 Unicode 符号 `✓`(U+2713)、`✗`(U+2717)，在 Windows 默认 GBK 控制台下无法编码 |
| **修复** | 已将 `main.py`、`run_all_tests.py` 中的 `✓`/`✗` 替换为 ASCII `[PASS]`/`[FAIL]`、`[OK]` |
| **状态** | 已修复（脚本已更新，需重新运行完整套件验证） |

#### 关键日志路径说明

- **LogMonitor 当前逻辑**：由 `exe_path` 推导日志目录：`dirname(exe)/../../WZMediaPlay/logs` 或 `dirname(exe)/../logs`。即 EXE 为 `build/Release/WZMediaPlayer.exe` 时，会查找 `D:\2026Github\WZMediaPlay\logs` 或 `D:\2026Github\build\logs`。
- 若 CMake 构建将日志写到 `build/Release/logs/` 等位置，需在 `main.py` 的 `_setup_log_monitor()` 中增加对应候选路径，否则会提示「未找到日志文件」。

### TestRound 1 首次运行摘要（2026-03-03）

- **运行命令**：`python run_all_tests.py`（EXE：`D:\2026Github\build\Release\WZMediaPlayer.exe`）
- **结果**：因 TEST-001 未修复前即运行，各阶段在打印时异常退出，**未执行到实际播放/Seek 等用例**；汇总为 总计: 0 | 通过: 0 | 失败: 0；耗时约 110 秒（多阶段启动/异常/退出）。
- **后续**：修复 TEST-001 后，请在本机再次执行 `cd testing/pywinauto && python run_all_tests.py`，将生成的报告与失败用例补充到本节；若发现新的播放器 BUG（如 BUG-018～021 的复现或新问题），请追加到上方「新发现 Bug」表格并注明由自动化测试发现。
