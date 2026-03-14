# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

WZMediaPlayer is a cross-platform C++ media player built with Qt 6.6.3, FFmpeg, OpenGL, and OpenAL. It supports video playback, 3D stereo rendering, and camera input.

## 核心目录结构 (Core Directories)

```
项目根目录: /Users/jingwenlai/leadwit-tech/WeiZheng/MediaPlayer2026/
├── WZMediaPlay/          # C++ 源码目录
├── build/                # CMake/Ninja 构建输出目录 (macOS)
├── testing/              # 测试相关目录
│   ├── pyobjc/           # macOS Python GUI 测试主目录
│   │   ├── tests/        # macOS 测试用例目录
│   │   ├── core/         # 测试核心模块 (app_launcher, screenshot_capture 等)
│   │   ├── config.py     # 测试配置
│   │   └── run_all_tests.py  # 综合测试入口
│   ├── pywinauto/        # Windows Python GUI 测试目录
│   └── video/            # 测试视频文件目录
│       ├── bbb_sunflower_1080p_30fps_normal.mp4    # 标准 2D 测试视频
│       ├── bbb_sunflower_1080p_30fps_stereo_abl.mp4 # 3D 立体测试视频
│       ├── test_60s.mp4  # 60秒测试视频
│       └── ...           # 其他测试视频
├── docs/                 # 文档目录
├── new-docs/             # 新文档目录 (Bug 登记表等)
└── dist/                 # 发布输出目录
```

## Build Commands

**当前运行平台: macOS**

### macOS 构建命令 (当前平台)
```bash
# 从项目根目录执行
cd /Users/jingwenlai/leadwit-tech/WeiZheng/MediaPlayer2026

# 配置 (首次或修改 CMakeLists.txt 后)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# 编译
cmake --build build

# 运行单元测试
cd build && ctest --output-on-failure
```

### Windows (MSBuild)
```batch
# Build Release (default)
build.bat

# Build Debug
build.bat Debug

# Initial CMake configuration (one-time)
cmake -B build -G "Visual Studio 17 2022" -A x64
```

## Code Style

- **Standard**: C++17
- **Formatter**: clang-format (config: `.clang-format` based on Qt Creator Coding Rules)
- **Indentation**: 4 spaces, no tabs
- **Column limit**: 160 characters
- **Pointer alignment**: Right (`int *ptr`)
- **Braces**: Left brace on new line for classes/functions, same line for control statements

### Naming Conventions
- Classes: PascalCase (`PlayController`, `VideoThread`)
- Member variables: camelCase with underscore suffix (`videoDecoder_`, `flushVideo_`)
- Functions: camelCase (`open()`, `seek()`, `getDurationMs()`)
- Constants: UPPER_SNAKE_CASE (`MAX_BUFFER_SIZE`)
- Namespaces: lower_snake_case

### Include Order
1. Corresponding header file
2. Project headers
3. Third-party library headers
4. Qt headers
5. Standard library headers

## Architecture

### Core Components

```
MainWindow
├── PlayController     # Central playback orchestration
│   ├── DemuxerThread  # Demuxes media streams
│   ├── VideoThread    # Decodes and renders video
│   ├── AudioThread    # Decodes and plays audio
│   └── AVClock        # Synchronization clock
└── StereoVideoWidget  # Video display with 3D support
    ├── VideoRenderer  # OpenGL-based rendering
    └── CameraOpenGLWidget  # Camera input display
```

### Key Classes

- **PlayController**: Central hub coordinating all playback components. Manages playback state via `PlaybackStateMachine`.
- **DemuxerThread**: Reads media packets from FFmpeg, feeds to decoder threads.
- **VideoThread/AudioThread**: Decoder threads with frame/packet buffers.
- **AVClock**: Audio-video synchronization using `std::chrono` types.
- **ThreadSyncManager**: Manages mutexes and condition variables for thread synchronization.
- **StereoVideoWidget**: Manages video display, supports 2D/3D mode switching.
- **VideoRenderer**: Abstract base with `OpenGLRenderer` and `StereoOpenGLRenderer` implementations.

### Threading Model

- **DemuxerThread**: Reads packets from media file
- **VideoThread**: Decodes video frames
- **AudioThread**: Decodes audio samples
- **Main Thread**: UI and rendering

**Locking order** (to avoid deadlocks): VideoThread → AudioThread

### Data Flow

```
Media File → DemuxerThread → PacketQueue → VideoThread/AudioThread
                                                    ↓
                                            FrameBuffer
                                                    ↓
                                        VideoRenderer/AudioOutput
```

## Testing

### macOS Python GUI 测试 (当前平台)
```bash
# 从项目根目录执行
cd /Users/jingwenlai/leadwit-tech/WeiZheng/MediaPlayer2026/testing/pyobjc

# 安装依赖
pip install -r requirements.txt

# 运行综合测试
python run_all_tests.py

# 运行单个测试
python tests/test_gui_e2e.py
python tests/test_basic_playback.py
python tests/test_3d_rendering.py
```

**主要测试文件:**
- `tests/test_gui_e2e.py` - GUI 端到端测试
- `tests/test_basic_playback.py` - 基础播放测试
- `tests/test_3d_rendering.py` - 3D 渲染测试
- `tests/test_comprehensive.py` - 综合测试
- `tests/test_playback_sync.py` - 播放同步测试

### Windows Python GUI Tests
```bash
cd testing/pywinauto
pip install -r requirements.txt
python run_all_tests.py
```

### C++ Unit Tests
```bash
cd build && ctest --output-on-failure
```

Executables: `PlaybackStateMachineTest`, `ErrorRecoveryManagerTest`, `SeekingStateMachineTest`

## Configuration

- **System config**: `WZMediaPlay/config/SystemConfig.ini` (hardware decoding, FPS display, etc.)
- **Log config**: `WZMediaPlay/config/LogConfig.ini`
- **Log files**: `WZMediaPlay/logs/MediaPlayer_*.log`

### 测试视频文件
位于 `testing/video/` 目录:
- `bbb_sunflower_1080p_30fps_normal.mp4` - 2D 标准 1080p 测试视频
- `bbb_sunflower_1080p_30fps_stereo_abl.mp4` - 3D 立体测试视频
- `test_60s.mp4` - 60秒测试视频
- `wukong_2D3D-40S.mp4` - 2D/3D 对比测试视频
- `wukong4K-40S.mp4` - 4K 测试视频

## Dependencies

- Qt 6.6.3 (Core, Gui, Widgets, OpenGL, Network, Multimedia, ShaderTools)
- FFmpeg (libavcodec, libavformat, libavutil, libswresample, libswscale, libavfilter, libavdevice)
- OpenAL (audio output)
- GLEW (OpenGL extensions)
- spdlog + fmt (logging)

## Language Preference

Default language: Chinese (中文). Use Chinese for documentation, code comments, commit messages, and specifications.

## Known Issues

- Hardware decoding (FFDecHW) has issues with frame conversion; use software decoding (FFDecSW) as fallback
- `PlaybackStateMachineTest` has a pre-existing assertion failure for "Ready → Seeking" transition
- pywinauto tests are Windows-specific

## Reference Documentation

- Architecture: `docs/lwPlayer/渲染架构关系图.md`
- Current TODO: `docs/TODO_CURRENT.md`
- Bug registry: `new-docs/FULL_BUG_REGISTRY.md`
- Testing docs: `testing/pywinauto/README.md`
- Recent Changes: `docs/plans/*.md`