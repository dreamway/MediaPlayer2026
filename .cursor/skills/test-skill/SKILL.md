---
name: "test-skill"
description: "Run automated integration tests using pywinauto for WZMediaPlayer. Invoke when user needs to test seeking/playback/pause functionality or verify bug fixes."
---

# Test Skill - WZMediaPlayer Automated Testing

## Overview

This skill runs automated integration tests for WZMediaPlayer using the pywinauto framework. Supports **闭环测试** (closed-loop verification), **进程输出监控** (process output monitoring), and **图像验证** (image verification).

## Test Environment

- **Test Directory**: `E:\WZMediaPlayer_2025\testing\pywinauto\`
- **Main Script**: `unified_closed_loop_tests.py`（推荐）
- **Legacy**: `main.py`
- **Target**: `E:\WZMediaPlayer_2025\x64\Debug\WZMediaPlay.exe`
- **Test Video**: `D:\BaiduNetdiskDownload\test.mp4`

## Dependencies

```bash
pip install pywinauto Pillow
```

## 核心能力

### 1. 闭环测试（Closed-Loop Verification）
- 自动解析 Qt UI 控件
- 图像对比验证播放/暂停/停止状态
- 时间标签变化验证 Seek 效果

### 2. 进程输出监控（2026-02 新增）
- 捕获被测进程 stdout/stderr
- 检测 **关键错误**（出现即判定失败）：
  - `QMutex: destroying locked mutex`
  - `Access violation` / `0xC0000005`
  - `Exception thrown` / `crashed` / `Fatal error`
- 检测 **警告模式**（超阈值判定失败）：
  - `[ALSOFT] (WW)` 超过 10 次
  - `[ALSOFT] (EE)` 1 次
  - `Modifying storage for in-use buffer` 超过 15 次
- 进程意外退出（崩溃）检测
- 用户中断（Ctrl+C）纳入报告

### 3. 图像验证（Image Verification）
- `ImageVerifier.verify_video_playing(img1, img2)`：播放/暂停对比
- `ImageVerifier.is_black_screen(image)`：黑屏检测
- `ImageVerifier.is_playback_ui_state(screenshot, region)`：播放区域亮度分析

## Usage

### 运行完整测试

```powershell
cd E:\WZMediaPlayer_2025\testing\pywinauto
python unified_closed_loop_tests.py
```

### 运行指定类别

```powershell
# 基础播放 + Seek + 音频 + 同步（BUG 验证常用）
python unified_closed_loop_tests.py --categories basic seek audio sync

# 仅 Seek 与同步（验证 BUG 3、BUG 9）
python unified_closed_loop_tests.py --categories seek sync
```

### 修改后自动验证流程

1. **编译**：`build.bat`
2. **运行测试**：`python unified_closed_loop_tests.py --categories basic seek sync`
3. **查看报告**：`reports/closed_loop_test_report_*.txt`
4. 若报告中有 **进程输出错误检测** 或 **进程存活检测** 失败，说明存在 ALSOFT/QMutex 等运行时错误或崩溃

## 测试类别

| 类别 | 命令行 | 说明 |
|------|--------|------|
| basic | `--categories basic` | 打开、播放/暂停、停止 |
| seek | `--categories seek` | 进度条、Seek |
| audio | `--categories audio` | 音量、静音 |
| 3d | `--categories 3d` | 3D/2D 切换 |
| sync | `--categories sync` | 音视频同步 |
| hw | `--categories hw` | 硬件解码 |
| edge | `--categories edge` | 边界条件 |
| fullscreen | `--categories fullscreen` | 全屏 |

## 报告

- **路径**: `testing/pywinauto/reports/closed_loop_test_report_YYYYMMDD_HHMMSS.txt`
- **内容**: 通过/失败统计、进程输出错误、崩溃检测、用户中断

## Hotkey Mapping

| Function | Hotkey |
|----------|--------|
| Open File | Ctrl+O |
| Play/Pause | Space |
| Stop | Ctrl+C |
| Seek +5s | → |
| Seek -5s | ← |
| Seek +10s | ↑ |
| Seek -10s | ↓ |
| Volume+ | PageUp |
| Volume- | PageDown |
| Mute | M |
