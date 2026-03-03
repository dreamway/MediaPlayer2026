# WZMediaPlayer 闭环测试增强设计

> **日期**: 2025-03-02  
> **目标**: 完善闭环自动化测试，实现日志同步、音频验证、黑屏检测闭环，快速定位 BUG  
> **状态**: 已批准

## 一、背景与问题

### 1.1 当前状态

- **测试框架**: pywinauto + UIA + ImageVerifier，已有 unified_closed_loop_tests.py
- **日志**: spdlog 写入 `./logs/MediaPlayer_<timestamp>.log`，格式含时间戳、线程、文件:行号、级别
- **已有但未完全集成**: LogMonitor（实时 tail 日志）、ProcessOutputMonitor（捕获 stdout/stderr）

### 1.2 核心问题

1. **黑屏渲染**: 图像验证存在，但未在所有关键步骤闭环验证
2. **音视频同步/无声音**: 无音频输出验证，无法自动检测无声或不同步
3. **日志与测试不同步**: 跑完测试再查看日志，无法将 BUG 与对应日志实时关联
4. **定位效率低**: 测试失败后难以快速定位到 WZMediaPlayer 源码中的问题点

### 1.3 用户目标

- 通过自动化测试验证播放器所有功能
- 根据测试情况快速定位到原播放器代码中的问题点并进行修复

---

## 二、方案选择

### 日志同步

- **采用**: 时间窗口 + 可选 Marker 结合
- **主通道**: Console 输出 + ProcessOutputMonitor（避免文件写入延迟）
- **备用**: LogMonitor tail 日志文件（当无法使用 console 时）

### 音频验证

- **采用**: 播放器测试模式 + 命名管道（Named Pipe）
- **输出通道**: `\\.\pipe\WZMediaPlayer_Test`，JSON 行格式
- **测试脚本**: 连接管道，解析状态，验证 volume、PTS、音视频同步

### 测试时日志模式

- 通过环境变量或配置，使播放器在测试时输出到 console（LOG_MODE=0 或 1）
- ProcessOutputMonitor 已能捕获 stdout，可实时获取日志

---

## 三、详细设计

### 3.1 日志同步闭环

#### 3.1.1 架构

```
┌─────────────────┐     stdout/stderr      ┌──────────────────────┐
│  WZMediaPlayer  │ ─────────────────────►│ ProcessOutputMonitor  │
│  (LOG_MODE=0/1  │                        │  + 时间戳缓冲         │
│   测试时)       │                        └──────────┬───────────┘
└─────────────────┘                                  │
        │                                             │ 每个 end_test 时
        │ 备用：文件日志                              │ 筛选 [t_start, t_end]
        ▼                                             ▼
┌─────────────────┐     tail -f 风格      ┌──────────────────────┐
│  logs/*.log     │ ◄─────────────────── │ LogMonitor           │
└─────────────────┘   (若用文件模式)      │  (fallback)           │
                                          └──────────────────────┘
```

#### 3.1.2 测试步骤与日志关联

- `start_test(name)` 时记录 `t_start`
- `end_test(...)` 时记录 `t_end`
- 失败时：从 ProcessOutputMonitor 缓冲中筛选 `[t_start, t_end]` 内 error/warn/critical 行
- 报告输出：`失败步骤 X：对应日志片段 [时间戳] 内容...`

#### 3.1.3 测试时日志模式

- 测试启动前：设置环境变量 `WZ_LOG_MODE=1` 或修改 `SystemConfig.ini` 中 `LogMode=1`（console）
- 或增加 `--test-mode` 命令行参数，内部强制 console 日志

#### 3.1.4 LogMonitor 集成

- 当无法使用 console 时（如某些构建只写文件），启用 LogMonitor 作为 fallback
- 集成到 ClosedLoopTestBase，在 setup 时根据配置选择 ProcessOutputMonitor 或 LogMonitor
- 两者均支持按时间窗口筛选

---

### 3.2 音频命名管道

#### 3.2.1 架构

```
┌─────────────────────────────────────────────────────────────────┐
│  WZMediaPlayer (测试模式: EnableTestPipe=true 或 WZ_TEST_MODE=1) │
│                                                                  │
│  ┌──────────────┐   每 200–500ms    ┌─────────────────────────┐│
│  │ PlayController│ ───────────────► │ TestPipeServer           ││
│  │ + AVClock     │   采集状态        │ (独立模块/单例)           ││
│  │ + Audio       │                  │ - volume, audio_pts,     ││
│  └──────────────┘                  │   video_pts, is_playing  ││
│                                     └────────────┬────────────┘│
└─────────────────────────────────────────────────────────────────┘
                                                   │
                                    JSON 行 (每行一条)
                                                   ▼
                                    \\.\pipe\WZMediaPlayer_Test
                                                   │
                                                   ▼
┌─────────────────────────────────────────────────────────────────┐
│  Python 测试脚本 (core/audio_pipe_client.py)                      │
│  - 连接命名管道                                                   │
│  - 读取 JSON 行                                                   │
│  - 校验: volume>0, audio_pts 递增, |audio_pts - video_pts| < 阈值 │
└─────────────────────────────────────────────────────────────────┘
```

#### 3.2.2 数据格式（JSON 行）

每行一条 JSON，便于流式解析：

```json
{"ts":1709123456.789,"vol":0.49,"audio_pts":12.34,"video_pts":12.31,"playing":true,"muted":false}
```

| 字段       | 类型  | 含义                     |
|------------|-------|--------------------------|
| ts         | float | 采集时间戳（秒）         |
| vol        | float | 音量 0–1                 |
| audio_pts  | float | 音频 PTS（秒）           |
| video_pts  | float | 视频 PTS（秒）           |
| playing    | bool  | 是否在播放               |
| muted      | bool  | 是否静音                 |

#### 3.2.3 C++ 端：TestPipeServer

**位置**: `WZMediaPlay/test_support/TestPipeServer.h`、`TestPipeServer.cpp`（新建）

**职责**:

- 测试模式下创建命名管道 `\\.\pipe\WZMediaPlayer_Test`
- 使用 QTimer（200–500ms）定期从 PlayController 采集状态
- 序列化为 JSON 行写入管道
- 仅在有客户端连接时写入，避免阻塞

**数据来源**:

- `volume`: `PlayController::getVolume()` 或 `Audio::getVolume()`
- `audio_pts`: `AVClock::pts()` 或 `Audio::getClock()`
- `video_pts`: `AVClock::videoTime()`
- `playing`: `PlayController::isPlaying()`
- `muted`: 通过 Audio 或 PlayController 的静音接口

**启用条件**:

- 环境变量 `WZ_TEST_MODE=1`，或
- `SystemConfig.ini` 中 `[System] EnableTestPipe=true`

**生命周期**:

- MainWindow 或 PlayController 初始化时创建
- 测试模式启用时启动
- 应用退出时关闭管道

#### 3.2.4 Python 端：audio_pipe_client.py

**位置**: `testing/pywinauto/core/audio_pipe_client.py`（新建）

**职责**:

- 连接 `\\.\pipe\WZMediaPlayer_Test`
- 后台线程读取 JSON 行，写入线程安全队列
- 提供接口:
  - `get_latest_status()`: 最近一条状态
  - `verify_audio_playing(duration_sec)`: 指定时间内是否有持续音频输出（volume>0 且 audio_pts 递增）
  - `verify_av_sync(duration_sec, max_diff_sec)`: 音视频 PTS 差是否在阈值内

**连接时机**:

- setup() 启动播放器后，等待 2–3 秒再连接（等待管道创建）
- 连接失败时回退到「无音频验证」模式并记录警告

#### 3.2.5 测试用例集成

- **test_basic_playback**: 打开视频后调用 `verify_audio_playing(2.0)` 验证有声音
- **test_av_sync**: 播放一段时间后调用 `verify_av_sync(10.0, 0.5)` 验证音视频同步
- **test_audio_features**: 静音后检查 `muted==true`，取消静音后检查 `vol>0`

管道不可用时（未启用测试模式或连接失败），这些检查跳过并记录「跳过（无音频管道）」。

---

### 3.3 黑屏检测增强

- 在 `ImageVerifier::is_black_screen(threshold=30)` 基础上，于「打开视频」「Seek 后」等关键步骤后增加黑屏检测
- 失败时保存截图并写入报告
- 可配置阈值（如环境变量 `WZ_BLACK_SCREEN_THRESHOLD`）

---

## 四、配置变更

### 4.1 SystemConfig.ini 新增项

```ini
[System]
LogMode=1
LogLevel=2
EnableTestPipe=false
```

测试时由测试脚本在启动前将 `EnableTestPipe=true`、`LogMode=1` 写入（或通过环境变量覆盖）。

### 4.2 环境变量

| 变量                    | 含义                         |
|-------------------------|------------------------------|
| WZ_TEST_MODE=1          | 启用测试管道                 |
| WZ_LOG_MODE=0/1         | 覆盖 LogMode（0=console, 1=console+AllocConsole） |
| WZ_BLACK_SCREEN_THRESHOLD | 黑屏亮度阈值（默认 30）    |

---

## 五、实施顺序建议

1. **日志同步**: 扩展 ProcessOutputMonitor 支持时间戳缓冲，集成到 ClosedLoopTestBase，测试时强制 LogMode=1
2. **LogMonitor fallback**: 集成 LogMonitor，支持时间窗口筛选
3. **TestPipeServer**: C++ 端实现命名管道服务
4. **audio_pipe_client**: Python 端实现管道客户端
5. **测试用例集成**: 在 unified_closed_loop_tests 中接入音频验证
6. **黑屏增强**: 在关键步骤增加黑屏检测

---

## 六、后续步骤

- 调用 writing-plans 技能，生成详细实施计划（任务拆解、验收标准、依赖关系）
