# WZMediaPlayer AGENTS.md

此文件包含WZMediaPlayer项目的构建、测试、代码风格和代理使用说明。

## 构建和测试命令

### 构建项目
```bash
# Windows (MSBuild)
build.bat

# 手动构建（需要配置Visual Studio环境）
MSBuild WZMediaPlay.sln /p:Configuration=Debug /p:Platform=x64 /m /v:minimal

# Release构建
MSBuild WZMediaPlay.sln /p:Configuration=Release /p:Platform=x64 /m /v:minimal
```

### 代码质量检查
```bash
# 格式化代码（遵循.clang-format配置）
clang-format -i <file.cpp> <file.h>

# 格式化整个项目
find WZMediaPlay -name "*.cpp" -o -name "*.h" | xargs clang-format -i

# 检查格式（不修改文件，仅报告差异）
find WZMediaPlay -name "*.cpp" -o -name "*.h" | xargs clang-format --dry-run --Werror
```

### 运行测试
```bash
# Python自动化测试（完整套件）
cd testing/pywinauto && python run_all_tests.py

# Python单个测试套件
cd testing/pywinauto && python test_basic_playback.py    # 基础播放测试
cd testing/pywinauto && python test_seeking.py            # Seeking测试
cd testing/pywinauto && python test_3d_features.py        # 3D功能测试
cd testing/pywinauto && python test_av_sync.py            # 音视频同步测试
cd testing/pywinauto && python test_hardware_decoding.py   # 硬件解码测试

# C++单元测试（独立编译，无测试框架）
cd WZMediaPlay/tests
cl PlaybackStateMachineTest.cpp /I.. /std:c++17 /Fe:test.exe
test.exe
```

## 代码风格规范

### 命名约定
- 类名：PascalCase（如 `PlayController`, `VideoThread`）
- 成员变量：camelCase，私有成员加下划线后缀（如 `videoDecoder_`, `flushVideo_`）
- 函数名：camelCase（如 `open()`, `seek()`, `getDurationMs()`）
- 常量：UPPER_SNAKE_CASE（如 `MAX_BUFFER_SIZE`）
- 命名空间：lower_snake_case

### 导入顺序
1. 对应头文件（如 `PlayController.h` 对应 `PlayController.cpp`）
2. 项目头文件
3. 第三方库头文件
4. Qt头文件
5. 标准库头文件

示例：`#include "PlayController.h"`, `#include "GlobalDef.h"`, `#include <QApplication>`, `#include <chrono>`

### 格式化规范
- 配置文件：`.clang-format`（基于Qt Creator Coding Rules）
- 缩进：4个空格（不使用Tab）
- 列宽限制：160字符
- 指针对齐：右对齐（`int *ptr`）
- 大括号：类/函数左大括号换行，控制语句左大括号不换行
- 标准：C++17

### 类型规范
- 时间类型：`std::chrono`（`nanoseconds`, `microseconds`, `milliseconds`）
- 智能指针：`std::unique_ptr`, `std::shared_ptr` 管理对象生命周期
- Qt类型：优先使用Qt类型（`QString`, `QList`）而非标准库

### 错误处理
- 日志：spdlog（`logger->trace/debug/info/warn/error/critical()`）
- 返回值：`bool`表示成功/失败，或返回错误码
- 异常：关键操作使用try-catch捕获

### 线程安全与内存管理
- 使用`PlaybackStateMachine`管理播放状态，避免状态不一致
- 使用`ThreadSyncManager`管理互斥锁和条件变量
- 使用QWaitCondition唤醒等待线程
- 锁定顺序：先VideoThread，后AudioThread，避免死锁
- Qt对象：设置parent，由Qt自动管理
- RAII：资源获取即初始化，避免裸delete

## 项目结构与代理

### 项目结构
```
WZMediaPlayer_2025/
├── WZMediaPlay/          # 主应用程序（C++/Qt）
│   ├── MainWindow.*      # 主窗口UI
│   ├── PlayController.*  # 播放控制器（核心）
│   ├── videoDecoder/     # 视频/音频解码模块
│   └── tests/            # C++单元测试
├── testing/pywinauto/    # Python GUI自动化测试
├── docs/                 # 文档（架构、BUG修复）
└── reference/            # 参考代码（QtAV等）
```

### Cursor技能
路径：`.cursor/skills/`，包含编译、测试、Git、文档查询、项目规划等技能。
- **build-skill**: 编译构建，检查编译错误
- **test-skill**: pywinauto自动化测试，验证Bug修复
- **git-skill**: Git版本控制，查看代码变更
- **docs-skill**: FFmpeg/Qt/OpenGL API查询
- **project-planning-skill**: 更新TODO，制定开发计划

### 代理使用模式
- **General Agent**: 复杂多步骤任务，并行执行多个工作单元
- **Explore Agent**: 代码库探索（quick/medium/very thorough级别）
- **文件操作**: 使用Read/Glob，无需代理

## 测试框架说明

### Python自动化测试（主要）
- 框架：pywinauto
- 位置：`testing/pywinauto/`
- 测试套件：7个（基础播放、3D功能、边界条件、音视频同步、进度条Seeking、音频、硬件解码）
- 运行：`cd testing/pywinauto && python run_all_tests.py`
- 报告：自动生成详细测试报告（reports/目录）

### C++单元测试（原型）
- 框架：简单自定义宏（`TEST_ASSERT`）
- 状态：无测试运行器，需独立编译
- 测试文件：`PlaybackStateMachineTest.cpp`, `ErrorRecoveryManagerTest.cpp`, `SeekingAutomatedTest.cpp`
- 注意：建议集成Google Test或Catch2以支持自动化测试

## 关键配置与语言偏好

### 配置文件
- 系统配置：`WZMediaPlay/config/SystemConfig.ini`（硬件解码、FPS显示等）
- 日志配置：`WZMediaPlay/config/LogConfig.ini`
- 日志文件：`WZMediaPlay/logs/MediaPlayer_*.log`
- 硬件解码：默认启用，但当前临时禁用（见`docs/TODO_CURRENT.md`）

### 语言偏好
- 默认使用中文：文档、代码注释、提交信息、规范说明
- Qt编码规范：https://wiki.qt.io/Qt_Coding_Style
- 格式化规则：`.clang-format`（120行配置）

## 开发工作流

### 开发流程
1. **编码**：遵循代码风格规范，使用clang-format格式化
2. **构建**：运行`build.bat`编译项目
3. **测试**：运行Python自动化测试验证功能
4. **修复**：根据测试结果修复BUG，重新构建测试
5. **提交**：编写中文提交信息，参考历史提交记录

## 关键依赖

Qt 6.6.3、FFmpeg、OpenGL、OpenAL、spdlog、fmt

## 文档参考

### 重要文档
- `docs/`：架构文档、BUG修复记录、TODO列表
- `docs/BUG_FIXES.md`：已知BUG修复历史（包含Seeking同步修复等）
- `.cursor/plan/`：项目开发计划（`.plan.md`格式）
- `testing/pywinauto/README.md`：自动化测试框架详细文档

## 会话管理

代理可以使用会话ID保持跨多次调用的状态，用于持续对话。

## Cursor Cloud specific instructions

### Cross-platform build (CMake, Ubuntu/Linux)

The project now supports cross-platform building via CMake. On Ubuntu:

```bash
# Install dependencies (one-time)
sudo apt-get install -y qt6-base-dev qt6-multimedia-dev qt6-shadertools-dev \
  libqt6opengl6-dev qt6-l10n-tools qt6-tools-dev qt6-tools-dev-tools \
  libgl1-mesa-dev libglu1-mesa-dev libglew-dev libopenal-dev \
  libavcodec-dev libavformat-dev libavutil-dev libswresample-dev \
  libswscale-dev libavfilter-dev libavdevice-dev \
  libspdlog-dev libfmt-dev cmake ninja-build pkg-config clang-format

# Build
mkdir -p build && cd build
cmake .. -G Ninja -DCMAKE_CXX_COMPILER=g++
ninja

# Run unit tests
ctest --output-on-failure

# Run the application (needs display)
cd build && ./WZMediaPlayer
```

### Running the application in Cloud VM
- The Cloud VM has no GPU, so OpenGL rendering shows a black viewport. The application launches, initializes, and responds to input — only the video rendering area is black.
- Launch: `cd /workspace/build && ./WZMediaPlayer`
- Test video: `testing/video/test.mp4` (56MB)

### Lint and format
- **Check format:** `find WZMediaPlay -name "*.cpp" -o -name "*.h" | grep -v 3rdparty | xargs clang-format --dry-run --Werror`
- **Apply format:** `find WZMediaPlay -name "*.cpp" -o -name "*.h" | grep -v 3rdparty | xargs clang-format -i`

### Known issues
- `PlaybackStateMachineTest.cpp` line 32 has a pre-existing test logic bug: expects `Playing -> Seeking` to fail, but state machine correctly allows it.
- OpenGL video rendering requires a real GPU; Cloud VM renders black viewport due to software GL.
- pywinauto test suite (`testing/pywinauto/`) is Windows-specific (uses `pywinauto` + `pywin32`); cannot run on Linux as-is.
- The `WZMediaPlay.sln` solution file is missing from the repo (Windows-only build artifact).
