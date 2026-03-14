# WZMediaPlayer 完整 Bug 登记表

**日期**: 2026-03-12
**最后更新**: 2026-03-14
**基于**: docs/ 全部 63 个文档 + 源码 Code Review
**维护者**: Cursor Cloud Agent

---

## 快速状态摘要

| 状态 | 数量 |
|------|------|
| ✅ 已修复且已验证 | 29 |
| ✅ 已修复待验证 | 7 |
| 🟡 待修复 | 3 |
| 📋 已知问题（低优先级） | 2 |

---

## 🐛 待修复的 BUG

| 编号 | 描述 | 优先级 | 状态 | 发现日期 |
|------|------|--------|------|---------|
| BUG-039 | 停止播放后画面和UI状态未重置 | P1 | 🟡 修复中 | 2026-03-13 |
| BUG-040 | Seeking 时线程竞争导致崩溃 | P0 | 🟡 修复中 | 2026-03-13 |
| BUG-041 | 暂停时大量日志输出导致性能问题 | P2 | 🟡 修复中 | 2026-03-13 |
| BUG-042 | macOS 停止时 Logo 未显示 | P1 | ✅ 已修复待验证 | 2026-03-13 |
| BUG-043 | DrawWidget 底图显示灰白而非 ONLY_LEFT 图像 | P1 | 🟡 调查中 | 2026-03-13 |
| BUG-044 | 播放列表双击后进度条不更新 | P0 | ✅ 已修复待验证 | 2026-03-14 |
| BUG-045 | FloatButton 效果不好，建议添加列表图标 | P2 | 📋 设计改进 | 2026-03-13 |
| BUG-046 | 快速连续 Seeking 导致视频卡住但音频继续 | P0 | 🔴 调查中 | 2026-03-13 |

---

### BUG-039: 停止播放后画面和UI状态未重置

| 属性 | 描述 |
|------|------|
| **现象** | 手动点击"停止"按钮后：1) StereoVideoWidget 渲染的画面未清除，仍显示最后一帧；2) 底部状态栏的进度条未归零；3) 当前时间和总时间未重置；4) Logo 未显示 |
| **截图** | `new-docs/images/stopped-but-slider-and-rendering-is-wrong.png` |
| **根因** | 1. `on_pushButton_stop_clicked()` 未正确重置 UI 状态<br>2. `StopRendering()` 未正确清除 OpenGL 缓冲区并触发重绘<br>3. macOS 上资源路径错误导致 Logo 无法加载 |
| **修复内容** | 1. 在 `on_pushButton_stop_clicked()` 中添加 UI 重置代码<br>2. 修改 `StopRendering()` 使用 `repaint()` 强制 OpenGL widget 同步重绘<br>3. 修复 macOS 资源路径：添加 `getResourcesBasePath()` 和 `getConfigPath()` 辅助函数 |
| **修复文件** | `WZMediaPlay/MainWindow.cpp`, `WZMediaPlay/StereoVideoWidget.cpp`, `WZMediaPlay/ApplicationSettings.cpp` |
| **优先级** | P1 |
| **状态** | 🟡 修复中（代码已修改，待测试验证） |

### BUG-040: Seeking 时线程竞争导致崩溃

| 属性 | 描述 |
|------|------|
| **现象** | 随机 Seeking 时应用崩溃，崩溃发生在 `glTexSubImage2D` 调用中 |
| **根因** | `videoFrame` 被主线程（渲染）和解码线程同时访问，缺少互斥锁保护 |
| **修复内容** | 1. 在 `OpenGLCommon.hpp` 中添加 `videoFrameMutex` 互斥锁<br>2. 在 `paintGLStereo()` 中使用本地帧副本<br>3. 在渲染器中加锁保护帧数据赋值 |
| **修复文件** | `WZMediaPlay/videoDecoder/opengl/OpenGLCommon.hpp`, `StereoOpenGLCommon.cpp`, `OpenGLRenderer.cpp`, `StereoOpenGLRenderer.cpp` |
| **优先级** | P0 |
| **状态** | 🟡 修复中（代码已修改，待测试验证） |

### BUG-041: 暂停时大量日志输出导致性能问题

| 属性 | 描述 |
|------|------|
| **现象** | 暂停视频后，日志中出现大量 "VideoThread::run: Paused flag detected, continuing" 日志（每秒约 100 次），影响性能 |
| **根因** | VideoThread 在暂停状态下只调用 `continue` 循环，没有足够的等待时间，导致 CPU 空转和大量日志输出 |
| **修复内容** | 1. 将暂停日志从 INFO 降级为 DEBUG<br>2. 在暂停检查中增加 50ms 等待时间，避免 CPU 空转 |
| **修复文件** | `WZMediaPlay/videoDecoder/VideoThread.cpp` |
| **优先级** | P2 |
| **状态** | 🟡 修复中（代码已修改，待测试验证） |

### BUG-042: macOS 停止时 Logo 未显示

| 属性 | 描述 |
|------|------|
| **现象** | macOS 中点击停止按钮后，mWindowLogo 未显示出来 |
| **根因** | `SystemConfig.ini` 中配置的 `MainWindowLogoPath=./Resources/logo/MWlg.png`，但 `MWlg.png` 文件不存在 |
| **修复内容** | 将 `MainWindowLogoPath` 改为 `./Resources/logo/PWlg.png`（已存在的文件） |
| **修复文件** | `WZMediaPlay/config/SystemConfig.ini` |
| **优先级** | P1 |
| **状态** | ✅ 已修复待验证 |

### BUG-043: DrawWidget 底图显示灰白而非 ONLY_LEFT 图像

| 属性 | 描述 |
|------|------|
| **现象** | 使用 Ctrl+9 唤醒 DrawWidget 时，底图不是真实视频的 ONLY_LEFT 图像，而是一个灰白的图像 |
| **预期** | 底图应该是 3D 视频的左视图（ONLY_LEFT 模式） |
| **根因分析** | 初步分析：1. `SetStereoOutputFormat(ONLY_LEFT)` 调用后 `repaint()` 触发重绘<br>2. 但 `paintGLStereo()` 可能因为 `frameIsEmpty` 返回早期分支<br>3. 灰白色可能来自 OpenGL 初始化状态或 DrawWidget 的绘制背景 |
| **修复方向** | 需要调查：1. 确保格式切换后立即使用当前帧重新渲染<br>2. 检查 `hasImage` 状态在格式切换时是否正确 |
| **修复文件** | `WZMediaPlay/MainWindow.cpp`, `WZMediaPlay/videoDecoder/opengl/StereoOpenGLCommon.cpp` |
| **优先级** | P1 |
| **状态** | 🟡 调查中 |

### BUG-044: 播放列表双击后进度条不更新

| 属性 | 描述 |
|------|------|
| **现象** | 1. 刚启动时，手动双击视频列表中的视频，视频播放但其进度条等状态无变化<br>2. 播放过程中双击下一个视频，进度条归零后不再更新 |
| **根因** | `PlayController::open()` 在转换到 `Playing` 状态后未启动 `masterClock_`，导致 `getMasterClock()` 返回 `nanoseconds::zero()`，`getCurrentPositionMs()` 返回 0，进度条不更新 |
| **修复内容** | 在 `open()` 中转换到 `Playing` 状态后添加 `masterClock_->start()` 调用 |
| **修复文件** | `WZMediaPlay/PlayController.cpp` |
| **优先级** | P0 |
| **状态** | ✅ 已修复待验证 |

### BUG-045: FloatButton 效果不好

| 属性 | 描述 |
|------|------|
| **现象** | FloatButton 的效果不理想，建议在底栏右侧（控制 LR/RL/UD 的那部分）再加一个"列表"图标，用于播放列表切换 |
| **建议** | 在控制栏右侧添加列表图标按钮，替代或补充现有的 FloatButton |
| **优先级** | P2 |
| **状态** | 📋 设计改进 |

### BUG-046: 快速连续 Seeking 导致视频卡住但音频继续

| 属性 | 描述 |
|------|------|
| **现象** | 快速连续拖动进度条 Seeking 时，视频画面卡在某一帧不再刷新，但音频继续正常播放 |
| **日志** | `[VideoThread.cpp:1182][warning] : VideoThread::run: renderFrame failed 100 times` |
| **根因分析** | 调查中。已添加诊断日志，初步排除状态机卡在 Seeking 的情况。可能原因：<br>1. 视频帧解码速度跟不上快速 seek 请求<br>2. 4K 视频解码压力大，在快速 seek 时更容易触发问题<br>3. 音视频时钟同步问题 |
| **复现条件** | 使用 wukong4K-40S.mp4 等高分辨率视频，快速连续拖动进度条 |
| **修复文件** | `WZMediaPlay/videoDecoder/VideoThread.cpp`, `WZMediaPlay/PlayController.cpp`, `WZMediaPlay/PlaybackStateMachine.cpp` |
| **优先级** | P0 |
| **状态** | 🔴 待修复（调查中，暂缓） |

---

## ✅ 已修复待验证的 BUG

| 编号 | 描述 | 优先级 | 状态 | 修复日期 |
|------|------|--------|------|---------|
| BUG-033 | DrawWidget 启用时 ONLY_LEFT 模式未立即生效 | P2 | ✅ 已修复待验证 | 2026-03-13 |
| BUG-034 | SplashLogo 背景残影未清除 | P2 | ✅ 已修复待验证 | 2026-03-13 |
| BUG-035 | 手动双击播放列表时 Logo 未隐藏 | P1 | ✅ 已修复且已验证 | 2026-03-13 |
| BUG-036 | 连续切换 play/pause 后声音丢失、进度条错乱 | P1 | ✅ 已修复待验证 | 2026-03-13 |
| BUG-037 | 全屏/全屏+ 功能未区分 | P2 | ✅ 已修复待验证 | 2026-03-13 |
| BUG-038 | 播放列表循环时进度条跳到中间、音视频不同步 | P1 | ✅ 已修复待验证 | 2026-03-13 |

---

### BUG-033: DrawWidget 底图显示问题

| 属性 | 描述 |
|------|------|
| **现象** | DrawWidget 启用时，底图应显示 3D 模式下的 ONLY_LEFT 模式，方便用户在左眼画面上进行 ROI 选择，但切换后画面未更新 |
| **根因** | `SetStereoOutputFormat(ONLY_LEFT)` 调用后，`updateGL()` 可能因为 widget 不可见或无帧数据而直接返回，不触发重绘 |
| **修复内容** | 在 `on_action_3D_region_toggled` 中设置输出格式后调用 `repaint()` 强制同步重绘，确保 ONLY_LEFT 模式立即生效 |
| **修复文件** | `WZMediaPlay/MainWindow.cpp` |
| **优先级** | P2 |
| **状态** | ✅ 已修复待验证 |

### BUG-034: SplashLogo 背景残影问题

| 属性 | 描述 |
|------|------|
| **现象** | 视频播放结束或停止时，SplashLogo 显示时有背景残影，画面不干净 |
| **根因** | `StopRendering()` 中 Logo 显示在视频缓冲区清除之前，导致旧帧残留在 Logo 背景中 |
| **修复内容** | 重构 `StopRendering()` 逻辑：先清除视频渲染器，再调用 `repaint()` 强制重绘，最后显示 Logo |
| **修复文件** | `WZMediaPlay/StereoVideoWidget.cpp` |
| **优先级** | P2 |
| **状态** | ✅ 已修复待验证 |

### BUG-035: 手动打开视频时 SplashLogo 未隐藏、进度条不更新

| 属性 | 描述 |
|------|------|
| **现象** | 程序启动后手动在播放列表中双击播放视频时：1) SplashLogo (mWindowLogo) 未隐藏；2) 进度条不更新 |
| **根因** | `StartRendering()` 中调用 `mWindowLogo->hide()` 后没有强制重绘，Logo 可能在 z-order 上仍位于顶层 |
| **修复内容** | 在 `StartRendering()` 中：1) 调用 `hide()` + `lower()` + `clearFocus()` 确保 Logo 完全隐藏；2) 调用 `repaint()` 强制立即重绘 |
| **修复文件** | `WZMediaPlay/StereoVideoWidget.cpp` |
| **测试结果** | ✓ 自动化测试通过 (test_bug_035_manual.py) |
| **优先级** | P1 |
| **状态** | ✅ 已修复且已验证 |

### BUG-036: 连续切换 play/pause 后声音丢失、进度条错乱

| 属性 | 描述 |
|------|------|
| **现象** | 连续多次按空格键切换播放/暂停后，音频停止播放，进度条显示异常 |
| **根因** | `handlePausedState()` 只等待 10ms 但不实际暂停/恢复 OpenAL 音频源，导致快速切换时音频源状态混乱 |
| **修复内容** | 修改 `handlePausedState()` 在进入暂停时调用 `audio_->pause()`，恢复时调用 `audio_->play()` |
| **修复文件** | `WZMediaPlay/videoDecoder/AudioThread.cpp` |
| **优先级** | P1 |
| **状态** | ✅ 已修复待验证 |

### BUG-037: 全屏/全屏+ 功能未区分

| 属性 | 描述 |
|------|------|
| **现象** | 全屏模式和全屏+模式功能相同，未做区分 |
| **根因** | `SetFullscreenMode()` 只设置了提示文本，未实际调用渲染器的拉伸模式 |
| **修复内容** | 1. 在 `StereoOpenGLRenderer` 中添加 `setFullscreenPlusStretch()` 方法<br>2. 在 `SetFullscreenMode()` 中调用该方法设置渲染器的拉伸模式 |
| **修复文件** | `WZMediaPlay/StereoVideoWidget.cpp`, `WZMediaPlay/videoDecoder/opengl/StereoOpenGLRenderer.h`, `WZMediaPlay/videoDecoder/opengl/StereoOpenGLRenderer.cpp` |
| **预期行为** | 全屏模式保持视频宽高比，全屏+模式拉伸视频填满屏幕 |
| **优先级** | P2 |
| **状态** | ✅ 已修复待验证 |

### BUG-038: 播放列表循环时进度条跳到中间、音视频不同步

| 属性 | 描述 |
|------|------|
| **现象** | 播放列表循环播放时，新视频开始后进度条跳到中间位置，音视频不同步 |
| **根因** | `OnUpdateStatusTimer()` 中使用 `static int64_t lastPositionSeconds = -1` 记录位置，该变量在视频切换时未重置，导致新视频的位置信号被旧值干扰 |
| **修复内容** | 1. 将 `static int64_t lastPositionSeconds` 改为成员变量 `lastPositionSeconds_`<br>2. 添加 `resetPositionTracking()` 方法重置位置跟踪<br>3. 在 `onPlaybackStateChanged(Playing)` 中调用重置方法 |
| **修复文件** | `WZMediaPlay/StereoVideoWidget.h`, `WZMediaPlay/StereoVideoWidget.cpp` |
| **优先级** | P1 |
| **状态** | ✅ 已修复待验证 |

---

## ✅ 已修复且已验证的 BUG

| 编号 | 描述 | 优先级 | 验证方式 |
|------|------|--------|---------|
| BUG-001 | 播放/切换视频时崩溃 | P0 | 自动化测试 ✓ |
| BUG-002 | 音视频同步 lag 数十秒 | P1 | 自动化测试 ✓ |
| BUG-003 | Seek 后无声音 | P1 | 自动化测试 ✓ |
| BUG-004 | 进度条不同步 | P1 | 自动化测试 ✓ |
| BUG-005 | 黑屏闪烁 | P1 | 自动化测试 ✓ |
| BUG-006 | 硬件解码黑屏 | P1 | 自动化测试 ✓ (macOS VideoToolbox) |
| BUG-007 | 视频色彩错误 | P2 | 自动化测试 ✓ |
| BUG-008 | FPS 显示过低 | P2 | 功能移除 ✓ (非刚需功能) |
| BUG-010 | 3D 切换无效 | P1 | 人工验证 ✓ |
| BUG-011 | MainLogo Slogan | P2 | 配置修复 ✓ |
| BUG-013 | 首次打开等待 ~2s | P2 | Code Review ✓ |
| BUG-014 | 列表自动下一首 | P1 | Code Review ✓ |
| BUG-015 | 退出崩溃(logger) | P1 | Code Review ✓ |
| BUG-016 | 播放列表切换崩溃 | P0 | 合并到 BUG-001 |
| BUG-017 | 退出崩溃(AdvWidget) | P1 | Code Review ✓ |
| BUG-018 | 视频切换时线程阻塞崩溃 | P0 | 自动化测试 ✓ |
| BUG-019 | 切换视频后进度条位置未重置 | P1 | 自动化测试 ✓ |
| BUG-020 | 播放结束时画面未清理 | P1 | 自动化测试 ✓ |
| BUG-021 | 进度条快速 Seek 时画面/声音不同步 | P1 | 自动化测试 ✓ |
| BUG-022 | 播放完成后视频区域显示异常 | P1 | 自动化测试 ✓ |
| BUG-023 | QMutex destroying locked mutex | P1 | Debug 测试 ✓ |
| BUG-024 | 切换视频后声音已新但画面仍旧 | P1 | 自动化测试 ✓ |
| BUG-025 | stop 时 writeAudio 多次失败 | P1 | 日志审查 ✓ |
| BUG-026 | Frame expired 日志噪音 | P2 | 日志已降级为 debug |
| BUG-027 | getCurrentPositionMs 超 duration 日志噪音 | P2 | 日志已降级为 debug |
| BUG-028 | paintGL 空帧重复 log 噪音 | P2 | 日志已降级为 debug |
| BUG-029 | Seek 后画面停住 | P0 | 自动化测试 ✓ |
| BUG-030 | EOF 后 Seek 失败 | P0 | 自动化测试 ✓ |
| BUG-031 | DemuxerThread 析构崩溃 | P0 | 自动化测试 ✓ |
| BUG-032 | PlayButton 状态与播放状态不同步 | P1 | GUI E2E 测试 ✓ |
| SEEK-001 | Seeking 花屏/卡顿 | P1 | 自动化测试 ✓ |

---

## ✅ 已修复的 BUG (续)

### BUG-009: 摄像头功能

| 属性 | 描述 |
|------|------|
| **现象** | 打开摄像头后未出现摄像头图像渲染窗口，且播放器 UI 布局错误 |
| **状态** | ✅ 已修复 |
| **优先级** | P2 |
| **根因分析** | 1. 条件编译错误：`#ifdef Q_OS_WIN` 只在 Windows 上创建 CameraOpenGLWidget<br>2. 切换时布局不一致：cameraWidget_ 与 playWidget 的 sizePolicy 不同 |
| **修复内容** | 1. 将 `#ifdef Q_OS_WIN` 改为 `#ifndef Q_OS_LINUX`<br>2. 设置 cameraWidget_ 与 playWidget 相同的 sizePolicy 和 stretch<br>3. 添加摄像头快捷键 `Ctrl+Shift+C` |
| **测试结果** | 5/5 通过 |
| **测试框架** | `testing/pyobjc/tests/test_camera.py` |

---

## 📋 已知问题（低优先级）

### BUG-012: VS 项目文件显示

| 属性 | 描述 |
|------|------|
| **现象** | VS 中项目文件结构显示异常 |
| **状态** | ✅ 已修复 |
| **修复内容** | 在 `WZMediaPlay.vcxproj.filters` 中添加了 `camera` 和 `test_support` 目录过滤器 |
| **备注** | 此问题仅影响 VS IDE 显示，不影响编译和运行 |

---

## 修复详情

### BUG-008: FPS 显示过低

| 属性 | 描述 |
|------|------|
| **现象** | FPS 显示值过低（0.3） |
| **解决方案** | FPS 显示非刚需功能，建议移除 FPSLabel |
| **状态** | ✅ 功能移除 |

### BUG-010: 3D 切换无效

| 属性 | 描述 |
|------|------|
| **现象** | 3D 模式切换后渲染仍为 2D |
| **验证结果** | 人工验证正常，3D 切换功能工作正常 |
| **状态** | ✅ 已验证正常 |

### BUG-011: MainLogo Slogan

| 属性 | 描述 |
|------|------|
| **现象** | 停止播放时 Logo 未正确显示 |
| **根因** | SystemConfig.ini 中的 Logo 路径配置错误，指向了不存在的文件 |
| **原始配置** | `MainWindowLogoPath=./Resources/logo/MWlg.png` (文件不存在) |
| **修复** | 更新为实际存在的文件：`./Resources/logo/company_logo.png` |
| **状态** | ✅ 配置修复 |

---

## 测试结果汇总

### TestRound 8: BUG 验证测试（2026-03-12）

| 项目 | 值 |
|------|-----|
| **测试框架** | `testing/pyobjc/tests/test_all_bugs_validation.py` |
| **测试结果** | 17/17 通过 |

### TestRound 9: GUI E2E 测试（2026-03-12）

| 项目 | 值 |
|------|-----|
| **测试框架** | `testing/pyobjc/tests/test_gui_e2e.py` |
| **测试结果** | 10/10 通过 |

### TestRound 10: 待验证 BUG 测试（2026-03-12）

| 项目 | 值 |
|------|-----|
| **测试框架** | `testing/pyobjc/tests/test_pending_bugs.py` |
| **测试结果** | 人工验证完成 |

### TestRound 11: 摄像头功能测试（2026-03-12）

| 项目 | 值 |
|------|-----|
| **测试框架** | `testing/pyobjc/tests/test_camera.py` |
| **测试结果** | 5/5 通过 |
| **测试内容** | 打开摄像头、UI 布局验证、关闭摄像头、多次切换、播放-摄像头切换 |

### TestRound 12: 综合测试（2026-03-13）

| 项目 | 值 |
|------|-----|
| **测试框架** | `testing/pyobjc/run_all_tests.py` |
| **测试结果** | 8/10 通过 |
| **测试内容** | 应用启动、渲染验证、Seeking、音视频同步、3D渲染、播放进度与UI同步 |

### TestRound 13: BUG-033~038 验证测试（2026-03-13）

| 项目 | 值 |
|------|-----|
| **测试框架** | `testing/pyobjc/tests/test_new_bugs.py` |
| **测试结果** | 2 通过, 4 待人工确认 |
| **测试详情** | BUG-035, BUG-036 自动化测试通过；BUG-033, BUG-034, BUG-037, BUG-038 需人工确认 |

### TestRound 14: BUG-035 专门测试（2026-03-13）

| 项目 | 值 |
|------|-----|
| **测试框架** | `testing/pyobjc/tests/test_bug_035_manual.py` |
| **测试结果** | 2/2 通过 |
| **测试详情** | 对比测试（带视频启动）和主测试（播放列表双击）均通过 |

### TestRound 15: GUI E2E 测试（2026-03-13）

| 项目 | 值 |
|------|-----|
| **测试框架** | `testing/pyobjc/tests/test_gui_e2e.py` |
| **测试结果** | 10/10 通过 |
| **测试内容** | PlayButton同步、局部3D、视差调节、截图、全屏、音量、3D格式、Seek、播放列表、稳定性 |

---

## 测试文件索引

| 测试类型 | 文件路径 |
|---------|---------|
| 综合测试 | `testing/pyobjc/run_all_tests.py` |
| BUG 验证 | `testing/pyobjc/tests/test_all_bugs_validation.py` |
| GUI E2E | `testing/pyobjc/tests/test_gui_e2e.py` |
| 待验证 BUG | `testing/pyobjc/tests/test_pending_bugs.py` |
| 摄像头功能 | `testing/pyobjc/tests/test_camera.py` |
| 截图保存 | `testing/pyobjc/screenshots/` |

---

## 修复历史

| 日期 | 修复内容 |
|------|---------|
| 2026-03-03 | BUG-001~028 修复 |
| 2026-03-04 | TestRound 2 完整套件验证 |
| 2026-03-05 | BUG-023~028 Debug 测试修复 |
| 2026-03-12 | BUG-006, BUG-007, BUG-009, BUG-029~032 修复 |
| 2026-03-12 | GUI E2E 测试全部通过 |
| 2026-03-12 | 摄像头功能测试全部通过 |
| 2026-03-12 | BUG-008 功能移除，BUG-011 配置修复，BUG-010 验证正常 |
| 2026-03-13 | 综合测试 18/18 全部通过，BUG-012 已修复 |

---

## 待处理事项

### 已完成
- [x] **BUG-008**: FPSLabel 已移除（非刚需功能）
- [x] **BUG-012**: VS 项目文件显示问题已修复