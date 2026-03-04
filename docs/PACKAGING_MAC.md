# WZMediaPlayer Mac 打包脚本使用说明

## 快速开始

### 方式一：直接运行（推荐用于开发）

```bash
# 1. 编译程序
cd build
cmake ..
make WZMediaPlayer

# 2. 复制资源文件
mkdir -p config Resources/logo Snapshots
cp ../WZMediaPlay/config/* config/
cp -r ../WZMediaPlay/Resources/logo/* Resources/logo/

# 3. 运行程序
./WZMediaPlayer
```

### 方式二：使用打包脚本（推荐用于分发）

```bash
# 1. 编译程序（如果尚未编译）
cd build
cmake ..
make WZMediaPlayer

# 2. 运行打包脚本
cd ..
./scripts/package_mac.sh

# 3. 进入打包目录运行
cd dist
./WZMediaPlayer
```

## 打包脚本详解

脚本位置：`scripts/package_mac.sh`

### 功能说明

脚本会自动执行以下步骤：

1. **清理旧的打包目录** - 删除之前的 `dist` 目录
2. **编译程序** - 验证可执行文件是否存在
3. **复制可执行文件** - 将 `build/WZMediaPlayer` 复制到 `dist/`
4. **复制资源文件** - 包括配置文件、Logo、截图目录等
5. **使用 macdeployqt 打包** - 将所有 Qt 框架和依赖库打包到 `dist/`
6. **验证依赖库** - 检查主要依赖库是否正确链接

### 打包后的目录结构

```
dist/
├── WZMediaPlayer              # 可执行文件
├── WZMediaPlayer.app/        # macOS App Bundle（由 macdeployqt 创建）
│   └── Contents/
│       ├── MacOS/
│       │   └── WZMediaPlayer
│       ├── Resources/
│       │   └── qt.conf
│       └── Frameworks/      # Qt 框架和依赖库
│           ├── Qt6/
│           │   └── ...      # Qt 模块
│           └── ...          # 第三方库（openal, ffmpeg 等）
├── config/                  # 配置文件目录
│   ├── SystemConfig.ini
│   └── playList.json
├── Resources/               # 资源文件目录
│   └── logo/
│       ├── company_logo.png
│       ├── company_logo_s.png
│       ├── film-color.svg
│       ├── film-gray.svg
│       ├── mpv-vlc.svg
│       └── triangle-circle.svg
└── Snapshots/               # 截图保存目录（空）
```

## Mac 依赖说明

### 与 Windows 的区别

**Windows:**
- 需要将所有 DLL 放在可执行文件同级目录
- 使用 `windeployqt` 打包 Qt 依赖
- 资源文件通常放在可执行文件同级目录

**macOS:**
- 使用 `macdeployqt` 打包，会创建 `.app` bundle 结构
- 动态库（.dylib）被打包到 `WZMediaPlayer.app/Contents/Frameworks/`
- Qt 框架被打包到 `WZMediaPlayer.app/Contents/Frameworks/Qt6/`
- 可执行文件有 `@rpath` 设置，可以找到打包后的库

### 运行时路径

程序会从 `config/SystemConfig.ini` 读取配置，包括：
- Logo 路径：`./Resources/logo/xxx.png`
- 截图保存目录：`./Snapshots`
- 其他用户设置

所有路径都是相对于 `WZMediaPlayer` 可执行文件的。

## 分发程序

打包完成后，可以将整个 `dist` 目录压缩：

```bash
cd dist
tar -czf WZMediaPlayer-macOS.tar.gz WZMediaPlayer*
```

或者创建 .zip 压缩包：

```bash
cd dist
zip -r WZMediaPlayer-macOS.zip WZMediaPlayer*
```

接收方解压后，直接运行 `WZMediaPlayer` 即可，无需额外安装依赖。

## 故障排查

### 程序启动崩溃

1. 检查资源文件是否完整：
   ```bash
   ls -R config Resources/
   ```

2. 检查配置文件是否存在：
   ```bash
   cat config/SystemConfig.ini
   ```

3. 查看系统崩溃日志：
   ```bash
   log show --predicate 'process == "WZMediaPlayer"' --last 5m
   ```

### 依赖库问题

检查可执行文件的依赖：
```bash
cd dist
otool -L WZMediaPlayer
```

所有非系统库都应该指向 `WZMediaPlayer.app/Contents/Frameworks/`。

### Qt 插件缺失

如果程序运行但某些功能不正常，可能是 Qt 插件未正确打包。检查：
```bash
ls -R WZMediaPlayer.app/Contents/Plugins/
```

常见插件：
- `platforms/` - 平台插件
- `imageformats/` - 图像格式插件
- `mediaservice/` - 媒体服务插件

## 开发者信息

- **编译器**: AppleClang 17.0.0
- **Qt 版本**: 6.10.2
- **架构**: arm64 (Apple Silicon)
- **CMake**: 3.16+
- **Homebrew**: 用于安装依赖

### 主要依赖

- Qt6 (Core, Gui, Widgets, OpenGL, OpenGLWidgets, Network, Multimedia, MultimediaWidgets, ShaderTools)
- FFmpeg (avcodec, avformat, avutil, swresample, swscale, avfilter, avdevice)
- OpenAL (音频输出)
- GLEW (OpenGL 扩展)
- spdlog (日志)
- fmt (格式化库)

## 许可证

请参考项目根目录的 LICENSE 文件。
