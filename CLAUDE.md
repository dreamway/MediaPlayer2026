# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

WZMediaPlayer is a cross-platform C++ media player built with Qt 6.6.3, FFmpeg, OpenGL, and OpenAL. It supports video playback, 3D stereo rendering, and camera input.

## Build Commands
Current Is Running On macOS Platform.
### Windows (MSBuild)
```batch
# Build Release (default)
build.bat

# Build Debug
build.bat Debug

# Initial CMake configuration (one-time)
cmake -B build -G "Visual Studio 17 2022" -A x64
```

### macOS/Linux (CMake + Ninja)
```bash
# Configure and build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Or use the convenience script
./build_ninja.bat  # Windows with Ninja
```

### Run Unit Tests
```bash
cd build && ctest --output-on-failure
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

### Python GUI Tests (Windows)
```bash
cd testing/pywinauto
pip install -r requirements.txt
python run_all_tests.py
```

Test suites: `test_basic_playback.py`, `test_progress_seek.py`, `test_3d_features.py`, `test_av_sync.py`, `test_hardware_decoding.py`, `test_audio.py`, `test_edge_cases.py`

### C++ Unit Tests
```bash
cd build
ctest --output-on-failure
```

Executables: `PlaybackStateMachineTest`, `ErrorRecoveryManagerTest`, `SeekingStateMachineTest`

## Configuration

- **System config**: `WZMediaPlay/config/SystemConfig.ini` (hardware decoding, FPS display, etc.)
- **Log config**: `WZMediaPlay/config/LogConfig.ini`
- **Log files**: `WZMediaPlay/logs/MediaPlayer_*.log`

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
- Bug registry: `docs/cursor/FULL_BUG_REGISTRY.md`
- Testing docs: `testing/pywinauto/README.md`
- Recent Changes: `docs/plans/*.md`