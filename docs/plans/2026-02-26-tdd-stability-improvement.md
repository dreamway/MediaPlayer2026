# TDD 稳定性改进实施计划

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 使用 TDD 方式修复 WZMediaPlayer 关键 BUG，提升 3D 播放器稳定性，维持现有功能。

**Architecture:** 验证优先 + TDD 循环。先建立测试基线，再按 P0→P1 优先级逐个 BUG 处理。每个 BUG：RED（确认失败测试）→ GREEN（最小修复）→ REFACTOR（清理）。结合 systematic-debugging 进行根因分析。

**Tech Stack:** C++/Qt 6.6.3, FFmpeg, OpenAL, OpenGL, pywinauto, spdlog

---

## Phase 0: 建立基线（必做）

### Task 1: 编译项目

**Files:**
- 无代码修改

**Step 1: 执行编译**

```powershell
cd E:\WZMediaPlayer_2025
.\build.bat
```

**Step 2: 验证可执行文件存在**

```powershell
Test-Path E:\WZMediaPlayer_2025\x64\Release\WZMediaPlay.exe
# 或 Debug: x64\Debug\WZMediaPlay.exe
```

Expected: 编译成功，exe 存在

---

### Task 2: 运行测试套件并记录基线

**Files:**
- 无修改，仅运行

**Step 1: 运行基础 + Seek + 同步测试（P0/P1 相关）**

```powershell
cd E:\WZMediaPlayer_2025\testing\pywinauto
python unified_closed_loop_tests.py --categories basic seek sync
```

**Step 2: 记录结果**

- 通过用例数 / 总用例数
- 失败用例名称及原因
- 进程输出错误（QMutex、Access violation、ALSOFT 等）
- 进程崩溃情况

**Step 3: 保存基线报告**

创建 `docs/plans/baseline_test_report_20260226.txt`，记录上述结果。

---

### Task 3: 分析基线并确定修复顺序

**Files:**
- `docs/plans/baseline_test_report_20260226.txt`
- `docs/BUG_FIXES.md`

**Step 1: 将失败用例映射到 BUG**

| 失败场景 | 对应 BUG |
|----------|----------|
| 播放列表切换/播完切换崩溃 | BUG-001 |
| 音视频同步、长时间播放异常 | BUG-002 |
| Seek 后无声音/视频停止 | BUG-003 |
| 进度条/时间标签异常 | BUG-004 |
| 黑屏、画面闪烁 | BUG-005 |

**Step 2: 按 P0 → P1 排序待修复列表**

---

## Phase 1: P0 - BUG-001 播放/切换崩溃

### Task 4: 确认 BUG-001 的失败测试

**Files:**
- `testing/pywinauto/unified_closed_loop_tests.py`
- `testing/pywinauto/core/test_base.py`

**Step 1: 检查现有 test_basic_playback 是否覆盖「播完切换」**

若 `test_basic_playback` 包含「停止播放后重新打开」或播放列表切换，则已有覆盖。否则需新增：

```python
# 在 test_basic_playback 或新建 test_playlist_switch 中
def test_playlist_switch_no_crash(self) -> bool:
    """播放列表切换不崩溃 - BUG-001 回归测试"""
    # 1. 打开视频A，播放
    # 2. 打开视频B（或播完自动切换）
    # 3. 验证：进程存活、无 Access violation
```

**Step 2: 运行该测试**

```powershell
python unified_closed_loop_tests.py --categories basic
```

**Step 3: 确认失败原因**

- 若崩溃：记录崩溃点（栈、日志）
- 若进程输出错误：记录 QMutex、0xC0000005 等

Expected: 测试失败，且失败原因与 BUG-001 描述一致

---

### Task 5: BUG-001 根因分析（若 Task 4 失败）

**Files:**
- `WZMediaPlay/videoDecoder/packet_queue.h`
- `WZMediaPlay/PlayController.cpp`
- `WZMediaPlay/MainWindow.cpp`
- `WZMediaPlay/logs/MediaPlayer_*.log`

**Step 1: 按 systematic-debugging Phase 1 收集证据**

- 读取最新日志，查找 Reset、notify、QMutex、Access violation
- 检查 `packet_queue.h` 中 `resetting_`、`setFinished()` 使用
- 检查 `PlayController::stop()`、`stopThread()` 的线程等待逻辑

**Step 2: 形成假设**

例如：「`Reset()` 在仍有线程等待时被调用，导致 condition_variable 访问违规」

**Step 3: 设计最小修复**

参考 BUG_FIXES.md 中 BUG-001 的修复说明，确认 `resetting_`、`setFinished()` 等是否已正确实现。

---

### Task 6: BUG-001 实现修复

**Files:**
- `WZMediaPlay/videoDecoder/packet_queue.h`
- `WZMediaPlay/PlayController.cpp`

**Step 1: 实施修复（根据 Task 5 分析）**

若 packet_queue 已有 `resetting_`：检查 `getPacket()`、`waitForSpace()` 是否正确检查。  
若 PlayController 已有 `setFinished()` 调用：检查调用时机是否在 `Reset()` 之前。

**Step 2: 编译**

```powershell
cd E:\WZMediaPlayer_2025
.\build.bat
```

**Step 3: 运行测试**

```powershell
cd testing\pywinauto
python unified_closed_loop_tests.py --categories basic
```

Expected: 无崩溃，无进程输出错误，测试通过

**Step 4: 提交**

```bash
git add WZMediaPlay/videoDecoder/packet_queue.h WZMediaPlay/PlayController.cpp
git commit -m "fix(BUG-001): 播放/切换崩溃 - 增强 packet_queue 与 stop 线程同步"
```

---

## Phase 2: P0 - BUG-002 音视频同步

### Task 7: 确认 BUG-002 的失败测试

**Files:**
- `testing/pywinauto/unified_closed_loop_tests.py`（test_av_sync）

**Step 1: 运行 sync 测试**

```powershell
python unified_closed_loop_tests.py --categories sync
```

**Step 2: 分析失败**

- 「Seek 后音视频同步」失败 → 可能 BUG-002/003
- 「长时间播放同步」失败 → 可能 BUG-002
- 日志中 Frame expired、lag=18000ms 等

Expected: 明确 sync 相关失败场景

---

### Task 8: BUG-002 根因分析与修复

**Files:**
- `WZMediaPlay/PlayController.cpp`（getMasterClock）
- `WZMediaPlay/videoDecoder/AVClock.cpp`
- `WZMediaPlay/videoDecoder/OpenALAudio.cpp`（getClock）

**Step 1: 按 BUG_FIXES.md 检查 getMasterClock、AVClock、OpenAL getClock**

确认是否已使用 `audioThread_->getClock()` 及 `AL_SAMPLE_OFFSET`。

**Step 2: 若未修复，实施最小改动**

**Step 3: 编译、运行 sync 测试、提交**

```bash
git add WZMediaPlay/PlayController.cpp WZMediaPlay/videoDecoder/AVClock.cpp WZMediaPlay/videoDecoder/OpenALAudio.cpp
git commit -m "fix(BUG-002): 音视频同步 - 主时钟使用 OpenAL 实际播放位置"
```

---

## Phase 3: P1 - BUG-003 Seek 后无声音

### Task 9: 确认 BUG-003 失败测试并修复

**Files:**
- `testing/pywinauto/unified_closed_loop_tests.py`（test_av_sync, test_progress_and_seek）
- `WZMediaPlay/videoDecoder/OpenALAudio.cpp`

**Step 1: 运行 seek + sync**

```powershell
python unified_closed_loop_tests.py --categories seek sync
```

**Step 2: 若 Seek 后播放异常，检查 OpenALAudio::clear()**

确认是否有 `alSourceRewind(source_)` 在 `alSourceStop()` 之后。

**Step 3: 实施修复、编译、测试、提交**

```bash
git add WZMediaPlay/videoDecoder/OpenALAudio.cpp
git commit -m "fix(BUG-003): Seek 后无声音 - OpenAL source rewind"
```

---

## Phase 4: P1 - BUG-004 进度条不同步

### Task 10: BUG-004 验证与修复

**Files:**
- `WZMediaPlay/MainWindow.cpp`（onUpdatePlayProcess, openPath）
- `WZMediaPlay/PlayController.cpp`（getMasterClock）

**Step 1: 运行 seek 测试，观察进度条/时间标签**

**Step 2: 确认 getMasterClock 与 BUG-002 一致**

进度条依赖 getMasterClock，BUG-002 修复后 BUG-004 可能一并解决。

**Step 3: 若仍异常，检查 onUpdatePlayProcess、duration 更新逻辑**

**Step 4: 修复、测试、提交**

---

## Phase 5: P1 - BUG-005 黑屏闪烁

### Task 11: BUG-005 验证与修复

**Files:**
- `WZMediaPlay/videoDecoder/VideoRenderer.h`
- `WZMediaPlay/videoDecoder/opengl/OpenGLRenderer.cpp`
- `WZMediaPlay/videoDecoder/VideoThread.cpp`

**Step 1: 运行 edge 测试（高码率/快速操作）**

```powershell
python unified_closed_loop_tests.py --categories edge
```

**Step 2: 检查 last frame 缓存**

确认 `renderLastFrame()`、`hasLastFrame()` 是否在 decode 失败时被调用。

**Step 3: 修复、测试、提交**

---

## Phase 6: 全量回归与文档更新

### Task 12: 全量测试与 BUG_FIXES 更新

**Files:**
- `docs/BUG_FIXES.md`

**Step 1: 运行完整测试**

```powershell
python unified_closed_loop_tests.py --categories basic seek audio sync 3d edge
```

**Step 2: 更新 BUG_FIXES.md 验证状态**

将已修复并验证的 BUG 从「待确认」改为「已验证」。

**Step 3: 提交**

```bash
git add docs/BUG_FIXES.md docs/plans/
git commit -m "docs: 更新 BUG 验证状态与 TDD 稳定性改进计划"
```

---

## 执行选项

**计划已保存至** `docs/plans/2026-02-26-tdd-stability-improvement.md`

**两种执行方式：**

1. **Subagent-Driven（本会话）**：按任务派发子代理，每任务完成后审查，快速迭代  
2. **Parallel Session（新会话）**：在新会话中用 executing-plans 技能，批量执行并设置检查点  

**请选择执行方式。**
