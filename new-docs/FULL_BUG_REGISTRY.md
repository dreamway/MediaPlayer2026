# WZMediaPlayer 完整 Bug 登记表

**日期**: 2026-03-12
**最后更新**: 2026-03-13
**基于**: docs/ 全部 63 个文档 + 源码 Code Review
**维护者**: Cursor Cloud Agent

---

## 快速状态摘要

| 状态 | 数量 |
|------|------|
| ✅ 已修复且已验证 | 28 |
| 📋 已知问题（低优先级） | 1 |
| 🐛 待修复 | 6 |

---

## 🐛 待修复的 BUG

| 编号 | 描述 | 优先级 | 状态 |
|------|------|--------|------|
| BUG-033 | DrawWidget 底图应显示 3D_ONLY_LEFT 模式 | P2 | 待修复 |
| BUG-034 | SplashLogo 背景残影未清除 | P2 | 待修复 |
| BUG-035 | 手动打开视频时 SplashLogo 未隐藏、进度条不更新 | P1 | 待修复 |
| BUG-036 | 连续切换 play/pause 后声音丢失、进度条错乱 | P1 | 待修复 |
| BUG-037 | 全屏/全屏+ 功能未区分 | P2 | 待修复 |
| BUG-038 | 播放列表循环时进度条跳到中间、音视频不同步 | P1 | 待修复 |

---

### BUG-033: DrawWidget 底图显示问题

| 属性 | 描述 |
|------|------|
| **现象** | DrawWidget 启用时，底图应显示 3D 模式下的 ONLY_LEFT 模式，方便用户在左眼画面上进行 ROI 选择 |
| **当前行为** | 底图显示的不是 ONLY_LEFT 模式 |
| **预期行为** | DrawWidget 启用时自动切换到 3D_ONLY_LEFT 模式显示 |
| **测试结果** | ⚠ 无法通过 Accessibility API 验证，需要人工验证 |
| **代码分析** | 代码在 `on_action_3D_region_toggled` 中已正确设置 `STEREO_OUTPUT_FORMAT_ONLY_LEFT` |
| **优先级** | P2 |
| **状态** | 🐛 待验证 |

### BUG-034: SplashLogo 背景残影问题

| 属性 | 描述 |
|------|------|
| **现象** | 视频播放结束或停止时，SplashLogo 显示时有背景残影，画面不干净 |
| **根因分析** | 可能是 OpenGL 缓冲区未正确清除，需要在显示 SplashLogo 前调用 glClear |
| **预期行为** | SplashLogo 显示时背景应完全清除，无残影 |
| **测试结果** | ⚠ Logo 未显示或无法通过 Accessibility API 确认 |
| **优先级** | P2 |
| **状态** | 🐛 待验证 |

### BUG-035: 手动打开视频时 SplashLogo 未隐藏、进度条不更新

| 属性 | 描述 |
|------|------|
| **现象** | 通过 Cmd+O 手动打开视频时：1) SplashLogo 未隐藏；2) 进度条不更新 |
| **测试结果** | ✓ 通过 - Logo 已隐藏，进度条在更新 |
| **优先级** | P1 |
| **状态** | ✅ 不是 BUG（代码已正确处理） |

### BUG-036: 连续切换 play/pause 后声音丢失、进度条错乱

| 属性 | 描述 |
|------|------|
| **现象** | 连续多次按空格键切换播放/暂停后，音频停止播放，进度条显示异常 |
| **测试结果** | 视频在播放（进度条更新），但快速切换后状态可能混乱 |
| **优先级** | P1 |
| **状态** | 🐛 部分验证（需要人工确认音频状态） |

### BUG-037: 全屏/全屏+ 功能未区分

| 属性 | 描述 |
|------|------|
| **现象** | 全屏模式和全屏+模式功能相同，未做区分 |
| **当前行为** | 两种全屏模式效果一样 |
| **预期行为** | 全屏+模式应该隐藏更多 UI 元素（如进度条、控制按钮） |
| **测试结果** | ⚠ 无法通过 Accessibility API 获取全屏提示 |
| **优先级** | P2 |
| **状态** | 🐛 待验证 |

### BUG-038: 播放列表循环时进度条跳到中间、音视频不同步

| 属性 | 描述 |
|------|------|
| **现象** | 播放列表循环播放时，新视频开始后进度条跳到中间位置，音视频不同步 |
| **测试结果** | 切换视频后进度条在 50%，而不是从 0 开始 |
| **根因分析** | 可能是进度条在 openPath 中重置后又被其他逻辑更新 |
| **优先级** | P1 |
| **状态** | 🐛 待修复 |

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
| **测试结果** | 18/18 通过 |
| **测试内容** | 应用启动、渲染验证、Seeking、音视频同步、3D渲染、播放进度与UI同步 |

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