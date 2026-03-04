# Mac 编译与运行问题修复总结

## 问题描述

1. **OpenAL 头文件找不到**：编译时报错 `'AL/al.h' file not found`
2. **程序启动即崩溃**：运行 WZMediaPlayer 时立即退出
3. **缺少资源文件**：程序依赖的配置文件、Logo 等资源未就位

## 根本原因

### 1. OpenAL 头文件问题

**原因：**
- CMake 默认找到系统 OpenAL 框架（`/Library/Developer/.../OpenAL.framework`）
- 系统框架的头文件在 `Headers/` 下，而代码使用 `#include <AL/al.h>`
- Homebrew 的 OpenAL 结构正确（`include/AL/al.h`），但 CMake 优先选择了系统框架

**修复：**
- 添加 `CMAKE_PREFIX_PATH` 指向 Homebrew 的 OpenAL
- 使用 `find_package(OpenAL REQUIRED NO_MODULE)` 优先使用 Homebrew 的 CMake 配置
- 使用 modern CMake target `OpenAL::OpenAL`（自动处理 include directories）

### 2. fmt 版本冲突

**原因：**
- spdlog 需要 fmt v12
- CMake 找到了 anaconda3 的 fmt v11

**修复：**
- 添加 `CMAKE_PREFIX_PATH` 指向 Homebrew 的 fmt（v12）

### 3. 程序崩溃

**原因：**
- 程序启动时需要加载 Logo 文件（`Resources/logo/company_logo.png`）
- 配置文件（`config/SystemConfig.ini`）和播放列表（`config/playList.json`）缺失
- 所有资源文件都在 `WZMediaPlay/` 目录下，但程序在 `build/` 目录运行

**修复：**
- 复制必要的资源文件到运行目录

## 修改的文件

### CMakeLists.txt

```cmake
# 添加 CMAKE_PREFIX_PATH 指定 Homebrew 路径
if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH
        "/opt/homebrew/opt/openal-soft"
        "/opt/homebrew/opt/fmt"
    )
endif()

# 使用 NO_MODULE 优先使用 Homebrew 的 CMake 配置
find_package(OpenAL REQUIRED NO_MODULE)

# 添加 FFmpeg 库目录
target_link_directories(WZMediaPlayer PRIVATE
    ${AVCODEC_LIBRARY_DIRS}
    ${AVFORMAT_LIBRARY_DIRS}
    # ... 其他 FFmpeg 库目录
)

# 使用 modern CMake target
target_link_libraries(WZMediaPlayer PRIVATE
    OpenAL::OpenAL
    # ... 其他库
)
```

## 新增的文件

### 1. `scripts/package_mac.sh`

Mac 打包脚本，功能包括：
- 清理旧的打包目录
- 编译验证
- 复制可执行文件
- 复制资源文件（config、Resources/logo、Snapshots）
- 使用 macdeployqt 打包 Qt 框架
- 验证依赖库

**使用方法：**
```bash
cd build
cmake ..
make WZMediaPlayer
cd ..
./scripts/package_mac.sh
cd dist
./WZMediaPlayer
```

### 2. `build/run.sh`

快速启动脚本，用于开发环境：
- 自动检查并复制资源文件
- 直接运行程序

**使用方法：**
```bash
cd build
./run.sh
```

### 3. `docs/PACKAGING_MAC.md`

Mac 打包文档，包含：
- 快速开始指南
- 打包脚本详解
- Mac 与 Windows 依赖管理的区别
- 打包后的目录结构
- 分发程序的方法
- 故障排查指南

## 编译与运行流程

### 开发环境（build 目录）

```bash
# 1. 编译
cd build
cmake ..
make WZMediaPlayer

# 2. 运行（使用自动脚本）
./run.sh

# 或手动复制资源后运行
mkdir -p config Resources/logo Snapshots
cp ../WZMediaPlay/config/* config/
cp -r ../WZMediaPlay/Resources/logo/* Resources/logo/
./WZMediaPlayer
```

### 打包分发（dist 目录）

```bash
# 1. 编译
cd build
cmake ..
make WZMediaPlayer

# 2. 运行打包脚本
cd ..
./scripts/package_mac.sh

# 3. 验证运行
cd dist
./WZMediaPlayer

# 4. 压缩分发
tar -czf WZMediaPlayer-macOS.tar.gz WZMediaPlayer*
# 或
zip -r WZMediaPlayer-macOS.zip WZMediaPlayer*
```

## Mac 依赖管理说明

### 与 Windows 的差异

**Windows:**
- 使用 DLL（动态链接库）
- DLL 必须放在可执行文件同级目录
- 使用 `windeployqt` 打包
- 系统依赖（.NET Framework 等）需要单独安装

**macOS:**
- 使用 `.dylib`（动态库）
- 使用 `macdeployqt` 创建 `.app` bundle 结构
- 动态库被打包到 `WZMediaPlayer.app/Contents/Frameworks/`
- 通过 `@rpath` 机制自动找到打包后的库
- 系统依赖（系统框架）无需安装

### macOS App Bundle 结构

```
WZMediaPlayer.app/
├── Contents/
│   ├── Info.plist          # App 元数据
│   ├── MacOS/
│   │   └── WZMediaPlayer # 可执行文件
│   ├── Resources/
│   │   └── qt.conf       # Qt 配置
│   └── Frameworks/       # 打包的框架和库
│       ├── Qt6/
│       │   ├── QtCore.framework/
│       │   ├── QtGui.framework/
│       │   └── ...
│       ├── libopenal.1.dylib
│       ├── libavcodec.62.dylib
│       └── ...
```

## 验证结果

### 编译成功

```bash
cd build
cmake ..
make WZMediaPlayer
# [100%] Built target WZMediaPlayer ✓
```

### 程序运行正常

```bash
./run.sh
# 程序启动，进程正常运行 ✓
```

### 依赖库验证

```bash
otool -L WZMediaPlayer | grep -E "opt/homebrew|openal|ffmpeg"
# 所有库都正确链接到 Homebrew 或打包后的路径 ✓
```

## 注意事项

1. **LSP 诊断错误**：可以忽略，这是 LSP 无法正确解析 CMake 配置导致的。实际编译和运行都是正常的。

2. **资源文件路径**：程序使用相对于可执行文件的路径（`./Resources/`、`./config/`），确保这些目录存在。

3. **macdeployqt 路径**：使用 Homebrew 安装的版本（`/opt/homebrew/bin/macdeployqt`），不要使用 anaconda3 的版本。

4. **分发版本**：打包后的 `dist` 目录可以在其他 Mac 上直接运行，无需额外安装依赖。

## 相关文件清单

- **修改：** `CMakeLists.txt`
- **新增：**
  - `scripts/package_mac.sh` - 打包脚本
  - `build/run.sh` - 快速启动脚本
  - `docs/PACKAGING_MAC.md` - 打包文档
  - `docs/MAC_FIX_SUMMARY.md` - 本文档

## 后续建议

1. **集成到 CMake**：将资源文件复制和 macdeployqt 步骤集成到 CMake 的 `install` 目标中
2. **CI/CD**：添加自动打包步骤到持续集成流程
3. **代码签名**：考虑添加代码签名（codesign）以便分发
4. **DMG 创建**：使用 `create-dmg` 或 `hdiutil` 创建 DMG 镜像
