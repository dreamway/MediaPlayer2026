# 闭环测试增强 实施计划

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 完善闭环自动化测试：日志与测试步骤同步关联、音频命名管道验证、黑屏检测增强，实现测试失败时快速定位 BUG。

**Architecture:** 1) 扩展 ProcessOutputMonitor 支持带时间戳的日志缓冲，ClosedLoopTestBase 在 start_test/end_test 时记录时间窗口，失败时筛选并输出对应日志；2) 测试启动前设置 LogMode=1 使播放器输出到 console；3) C++ 端新增 TestPipeServer，测试模式下通过命名管道输出 JSON 行（volume、audio_pts、video_pts、playing、muted）；4) Python 端新增 audio_pipe_client 连接管道并验证；5) 关键步骤增加黑屏检测。

**Tech Stack:** Python (pywinauto, Pillow), C++/Qt (QTimer, Windows Named Pipe), spdlog

**设计文档:** `docs/plans/2025-03-02-closed-loop-test-enhancement-design.md`

---

## Phase 1: 日志同步闭环

### Task 1: ProcessOutputMonitor 支持时间戳缓冲

**Files:**
- Modify: `testing/pywinauto/core/process_output_monitor.py`

**Step 1: 扩展 ProcessOutputMonitor**

在 `_reader_loop` 中，每读一行时记录 `datetime.now()` 并存入 `_all_lines` 或新列表 `_timestamped_lines`，格式为 `(timestamp, line)`。新增方法 `get_lines_in_window(t_start, t_end)` 返回该时间窗口内的行。

**Step 2: 实现 get_lines_in_window**

```python
def get_lines_in_window(self, t_start: float, t_end: float) -> List[str]:
    """t_start, t_end 为 time.time() 秒"""
    with self._lock:
        return [line for ts, line in self._timestamped_lines 
                if t_start <= ts <= t_end]
```

**Step 3: 确保 _timestamped_lines 被填充**

修改 `_reader_loop`，在 `capture_all` 或始终保存时，将 `(time.time(), line)` 追加到 `_timestamped_lines`（限制最大条数如 5000，避免内存膨胀）。

**Step 4: 运行测试**

Run: `cd testing/pywinauto && python -c "from core.process_output_monitor import ProcessOutputMonitor; print('OK')"`
Expected: 无 ImportError

**Step 5: Commit**

```bash
git add testing/pywinauto/core/process_output_monitor.py
git commit -m "feat(test): ProcessOutputMonitor 支持时间窗口筛选日志"
```

---

### Task 2: ClosedLoopTestBase 集成时间窗口日志关联

**Files:**
- Modify: `testing/pywinauto/core/test_base.py`
- Modify: `testing/pywinauto/core/ui_automation.py`（确保 start_application 时 capture_process_output=True 且 _output_monitor 可访问）

**Step 1: 在 start_test 记录 t_start**

```python
def start_test(self, test_name: str):
    self.current_test_name = test_name
    self.test_start_time = time.time() * 1000
    self._test_start_wall = time.time()  # 新增：用于日志窗口
    print(f"\n[测试] {test_name} ...")
```

**Step 2: 在 end_test 失败时获取时间窗口日志**

```python
# 在 end_test 中，当 passed=False 时：
if not passed and hasattr(self, 'ui') and self.ui and getattr(self.ui, '_output_monitor', None):
    mon = self.ui._output_monitor
    if hasattr(mon, 'get_lines_in_window'):
        t_start = getattr(self, '_test_start_wall', 0)
        t_end = time.time()
        lines = mon.get_lines_in_window(t_start, t_end)
        err_lines = [l for l in lines if any(k in l.lower() for k in ['error','warn','critical','failed'])]
        if err_lines:
            details += f"\n    关联日志:\n" + "\n".join(f"      {l[:120]}" for l in err_lines[-5:])
```

**Step 3: 在 generate_report 中输出失败步骤的关联日志**

将上述 `err_lines` 存入 `TestResult` 新字段 `related_log_lines`，报告时一并输出。

**Step 4: 运行测试**

Run: `cd testing/pywinauto && python unified_closed_loop_tests.py --categories basic`
Expected: 测试可运行，失败时报告含关联日志（若 ProcessOutputMonitor 有数据）

**Step 5: Commit**

```bash
git add testing/pywinauto/core/test_base.py
git commit -m "feat(test): 失败步骤关联时间窗口内日志"
```

---

### Task 3: 测试启动前强制 LogMode=1

**Files:**
- Modify: `testing/pywinauto/core/ui_automation.py`（start_application 前设置环境变量）
- Modify: `testing/pywinauto/core/test_base.py`（setup 中调用）

**Step 1: 在 start_application 前设置环境变量**

```python
# ui_automation.py start_application 开头：
import os
os.environ['WZ_LOG_MODE'] = os.environ.get('WZ_LOG_MODE', '1')
# 若播放器支持读取 WZ_LOG_MODE，则生效；否则需修改 config
```

**Step 2: 测试前备份并修改 SystemConfig.ini**

在 test_base.setup() 中，定位 exe 同目录或上级的 config/SystemConfig.ini，备份原 LogMode，写入 LogMode=1。teardown 时恢复。

**Step 3: 验证**

确认 ApplicationSettings 或 GlobalDef 会读取 SystemConfig.ini 的 LogMode。若需代码改动，在 PlayController/MainWindow 启动时检查 `getenv("WZ_LOG_MODE")` 并覆盖 LOG_MODE。

**Step 4: 运行测试**

Run: `cd testing/pywinauto && python unified_closed_loop_tests.py --categories basic`
Expected: 播放器输出到 console，ProcessOutputMonitor 能捕获

**Step 5: Commit**

```bash
git add testing/pywinauto/core/ui_automation.py testing/pywinauto/core/test_base.py
git commit -m "feat(test): 测试时强制 LogMode=1 输出到 console"
```

---

### Task 4: 集成 LogMonitor 作为 fallback

**Files:**
- Modify: `testing/pywinauto/core/test_base.py`
- Modify: `testing/pywinauto/log_monitor.py`（如需支持 get_lines_in_window）

**Step 1: LogMonitor 增加 get_lines_in_window**

在 LogMonitor 中，若监控文件，则按时间戳解析每行，提供 `get_lines_in_window(t_start, t_end)`，从 `anomalies` 或原始行中筛选。

**Step 2: test_base 中按配置选择**

若 ProcessOutputMonitor 无数据（如未捕获到 stdout），则尝试启用 LogMonitor，传入 logs 目录和最新日志文件路径。

**Step 3: 运行测试**

Run: `cd testing/pywinauto && python unified_closed_loop_tests.py --categories basic`
Expected: 正常或 fallback 到 LogMonitor 时均能关联日志

**Step 4: Commit**

```bash
git add testing/pywinauto/log_monitor.py testing/pywinauto/core/test_base.py
git commit -m "feat(test): LogMonitor 作为日志关联 fallback"
```

---

## Phase 2: 音频命名管道

### Task 5: C++ TestPipeServer 实现

**Files:**
- Create: `WZMediaPlay/test_support/TestPipeServer.h`
- Create: `WZMediaPlay/test_support/TestPipeServer.cpp`
- Modify: `WZMediaPlay/WZMediaPlay.vcxproj`（添加新文件）
- Modify: `WZMediaPlay/MainWindow.cpp`（初始化 TestPipeServer）

**Step 1: 创建 TestPipeServer.h**

```cpp
#pragma once
#include <QObject>
#include <QTimer>
#include <QString>
class PlayController;

class TestPipeServer : public QObject {
    Q_OBJECT
public:
    explicit TestPipeServer(PlayController* controller, QObject* parent = nullptr);
    ~TestPipeServer();
    bool start();
    void stop();
private slots:
    void onTimer();
private:
    PlayController* controller_;
    QTimer* timer_;
    void* pipeHandle_;  // HANDLE on Windows
    bool writeStatus(const QString& json);
};
```

**Step 2: 创建 TestPipeServer.cpp**

```cpp
#include "TestPipeServer.h"
#include "PlayController.h"
#include "videoDecoder/AVClock.h"
#include "videoDecoder/chronons.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QDateTime>
#ifdef _WIN32
#include <windows.h>
#endif

TestPipeServer::TestPipeServer(PlayController* controller, QObject* parent)
    : QObject(parent), controller_(controller), pipeHandle_(nullptr) {
    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, &TestPipeServer::onTimer);
}

TestPipeServer::~TestPipeServer() { stop(); }

bool TestPipeServer::start() {
#ifdef _WIN32
    QString pipeName = "\\\\.\\pipe\\WZMediaPlayer_Test";
    pipeHandle_ = CreateNamedPipeA(
        pipeName.toUtf8().constData(),
        PIPE_ACCESS_OUTBOUND,
        PIPE_TYPE_BYTE,
        1, 512, 512, 0, nullptr);
    if (pipeHandle_ == INVALID_HANDLE_VALUE) return false;
    timer_->start(300);  // 300ms
    return true;
#else
    return false;
#endif
}

void TestPipeServer::stop() {
    timer_->stop();
#ifdef _WIN32
    if (pipeHandle_ && pipeHandle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(pipeHandle_);
        pipeHandle_ = nullptr;
    }
#endif
}

void TestPipeServer::onTimer() {
#ifdef _WIN32
    if (!pipeHandle_ || pipeHandle_ == INVALID_HANDLE_VALUE) return;
    if (!ConnectNamedPipe(pipeHandle_, nullptr) && GetLastError() != ERROR_PIPE_CONNECTED)
        return;
    QJsonObject obj;
    obj["ts"] = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    obj["vol"] = controller_->getVolume();
    auto* clock = controller_->getAVClock();
    obj["audio_pts"] = clock ? clock->pts() : 0;
    obj["video_pts"] = clock ? clock->videoTime() : 0;
    obj["playing"] = controller_->isPlaying();
    obj["muted"] = false;  // TODO: 从 Audio 获取
    QByteArray json = QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\n";
    DWORD written;
    WriteFile(pipeHandle_, json.constData(), json.size(), &written, nullptr);
#endif
}
```

**Step 3: 启用条件检测**

在 MainWindow 或初始化处，检查 `qEnvironmentVariable("WZ_TEST_MODE") == "1"` 或 `SystemConfig.ini` 中 `EnableTestPipe=true`，再创建并 start TestPipeServer。

**Step 4: 编译**

Run: `build.bat` 或 `MSBuild WZMediaPlay.sln /p:Configuration=Debug /p:Platform=x64`
Expected: 编译通过

**Step 5: Commit**

```bash
git add WZMediaPlay/test_support/ WZMediaPlay/MainWindow.cpp WZMediaPlay/WZMediaPlay.vcxproj
git commit -m "feat: 添加 TestPipeServer 命名管道输出音频状态"
```

---

### Task 6: Python audio_pipe_client 实现

**Files:**
- Create: `testing/pywinauto/core/audio_pipe_client.py`

**Step 1: 创建 audio_pipe_client.py**

```python
import json
import threading
import time
from typing import Optional, Dict, List
from collections import deque

class AudioPipeClient:
    def __init__(self, pipe_name: str = r"\\.\pipe\WZMediaPlayer_Test"):
        self.pipe_name = pipe_name
        self._queue: deque = deque(maxlen=500)
        self._thread: Optional[threading.Thread] = None
        self._stop = threading.Event()

    def connect(self, timeout: float = 5.0) -> bool:
        try:
            import win32file
            self._handle = win32file.CreateFile(
                self.pipe_name,
                win32file.GENERIC_READ,
                0, None,
                win32file.OPEN_EXISTING,
                0, None
            )
        except Exception as e:
            print(f"[AudioPipe] 连接失败: {e}")
            return False
        self._stop.clear()
        self._thread = threading.Thread(target=self._read_loop, daemon=True)
        self._thread.start()
        return True

    def _read_loop(self):
        import win32file
        while not self._stop.is_set():
            try:
                _, data = win32file.ReadFile(self._handle, 4096)
                for line in data.decode('utf-8', errors='ignore').strip().split('\n'):
                    if line:
                        try:
                            self._queue.append((time.time(), json.loads(line)))
                        except json.JSONDecodeError:
                            pass
            except Exception:
                break

    def disconnect(self):
        self._stop.set()
        if hasattr(self, '_handle'):
            try:
                import win32file
                win32file.CloseHandle(self._handle)
            except Exception:
                pass

    def get_latest_status(self) -> Optional[Dict]:
        if not self._queue:
            return None
        return self._queue[-1][1]

    def verify_audio_playing(self, duration_sec: float = 2.0) -> (bool, str):
        start = time.time()
        prev_pts = None
        while time.time() - start < duration_sec:
            s = self.get_latest_status()
            if s and s.get('vol', 0) > 0 and s.get('playing'):
                pts = s.get('audio_pts', 0)
                if prev_pts is not None and pts > prev_pts:
                    return True, "音频 PTS 递增，有声音"
                prev_pts = pts
            time.sleep(0.2)
        return False, "未检测到持续音频输出"

    def verify_av_sync(self, duration_sec: float, max_diff_sec: float = 0.5) -> (bool, str):
        start = time.time()
        max_seen_diff = 0
        while time.time() - start < duration_sec:
            s = self.get_latest_status()
            if s and s.get('playing'):
                diff = abs(s.get('audio_pts', 0) - s.get('video_pts', 0))
                max_seen_diff = max(max_seen_diff, diff)
            time.sleep(0.2)
        ok = max_seen_diff <= max_diff_sec
        return ok, f"音视频 PTS 最大差: {max_seen_diff:.3f}s (阈值 {max_diff_sec}s)"
```

**Step 2: 添加 pywin32 依赖**

若需 win32file，在 requirements.txt 添加 `pywin32`。

**Step 3: 运行简单测试**

```python
# 手动：启动播放器并设置 WZ_TEST_MODE=1，打开视频后运行
from core.audio_pipe_client import AudioPipeClient
c = AudioPipeClient()
if c.connect():
    import time
    time.sleep(3)
    print(c.get_latest_status())
    c.disconnect()
```

**Step 4: Commit**

```bash
git add testing/pywinauto/core/audio_pipe_client.py testing/pywinauto/requirements.txt
git commit -m "feat(test): 添加 audio_pipe_client 连接命名管道验证音频"
```

---

### Task 7: 测试用例集成音频验证

**Files:**
- Modify: `testing/pywinauto/unified_closed_loop_tests.py`
- Modify: `testing/pywinauto/core/test_base.py`（setup 中初始化 AudioPipeClient 并连接）

**Step 1: 在 test_base.setup 中初始化 AudioPipeClient**

启动播放器后 sleep(3)，尝试 `self.audio_pipe = AudioPipeClient(); self.audio_pipe.connect()`，失败则 `self.audio_pipe = None`。

**Step 2: 在 test_basic_playback 中调用 verify_audio_playing**

打开视频并验证时间标签后，若 `self.audio_pipe`，则调用 `verify_audio_playing(2.0)`，失败则 end_test(False, msg)。

**Step 3: 在 test_av_sync 中调用 verify_av_sync**

播放 30 秒的循环中，若 `self.audio_pipe`，在检查点调用 `verify_av_sync(10.0, 0.5)`。

**Step 4: 在 test_audio_features 中检查 muted/vol**

静音后检查 `get_latest_status()['muted']==True`，取消静音后检查 `vol>0`。

**Step 5: 运行测试**

Run: `cd testing/pywinauto && python unified_closed_loop_tests.py --categories basic`
Expected: 若管道可用则验证音频；不可用则跳过

**Step 6: Commit**

```bash
git add testing/pywinauto/unified_closed_loop_tests.py testing/pywinauto/core/test_base.py
git commit -m "feat(test): 集成音频管道验证到测试用例"
```

---

## Phase 3: 黑屏检测增强

### Task 8: 关键步骤增加黑屏检测

**Files:**
- Modify: `testing/pywinauto/unified_closed_loop_tests.py`

**Step 1: 在打开视频验证后增加黑屏检测**

在 test_basic_playback 打开视频且时间标签非 00:00:00 后，截屏并调用 `image_verifier.is_black_screen(play_region)`，若为黑屏则 end_test(False, "播放区域黑屏")。

**Step 2: 在 Seek 后增加黑屏检测**

在 test_progress_and_seek 每次 Seek 操作后，等待 1s，截屏检测黑屏，失败则记录。

**Step 3: 可配置阈值**

从环境变量 `WZ_BLACK_SCREEN_THRESHOLD` 读取，默认 30。

**Step 4: 运行测试**

Run: `cd testing/pywinauto && python unified_closed_loop_tests.py --categories basic`
Expected: 黑屏时失败并保存截图

**Step 5: Commit**

```bash
git add testing/pywinauto/unified_closed_loop_tests.py
git commit -m "feat(test): 关键步骤增加黑屏检测"
```

---

## 验收标准

- [ ] 测试失败时，报告输出对应时间窗口内的 error/warn 日志
- [ ] 测试时播放器日志输出到 console，ProcessOutputMonitor 可捕获
- [ ] 启用 TestPipe 时，Python 可连接并读取 JSON 行
- [ ] test_basic_playback 可验证有声音（管道可用时）
- [ ] test_av_sync 可验证音视频同步（管道可用时）
- [ ] 打开视频、Seek 后若黑屏则测试失败并保存截图

---

## 执行选项

**Plan complete and saved to `docs/plans/2025-03-02-closed-loop-test-enhancement-implementation.md`.**

**Two execution options:**

1. **Subagent-Driven (this session)** - 按任务分派子代理，逐任务审查，快速迭代
2. **Parallel Session (separate)** - 新会话使用 executing-plans，批量执行并设置检查点

**Which approach?**
