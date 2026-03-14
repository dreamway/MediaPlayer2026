# 自动打包脚本修复说明

## 问题描述

自动打包脚本在第 5 步报错：
```
Error: Could not find app bundle "WZMediaPlayer"
```

## 根本原因

macdeployqt 需要 macOS .app bundle 格式的输入，但之前编译的是单独的可执行文件（`WZMediaPlayer`）。

## 修复方法

### 1. 修改 CMakeLists.txt 生成 .app bundle

在 `qt_add_executable(WZMediaPlayer ...)` 之后添加：

```cmake
# 为 macOS 创建 .app bundle
if(APPLE)
    set_target_properties(WZMediaPlayer PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.leadwit.WZMediaPlayer"
        MACOSX_BUNDLE_BUNDLE_VERSION "1.0.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.0"
    )
endif()
```

这样编译后会生成 `WZMediaPlayer.app/Contents/MacOS/WZMediaPlayer` 而不是单独的 `WZMediaPlayer`。

### 2. 更新 package_mac.sh 脚本

脚本需要支持两种格式：
- .app bundle（新）
- 单独可执行文件（旧）

修改部分：
- **第 2 步**：检查两种格式是否存在
- **第 3 步**：复制 .app bundle 或可执行文件
- **第 5 步**：对 .app bundle 或可执行文件运行 macdeployqt
- **第 6 步**：验证 .app bundle 内部的可执行文件

### 3. 修复配置文件中的 Logo 路径

原配置文件中的 Logo 文件不存在：
- `SplashLogo.png` → 不存在
- `Slogan.png` → 不存在
- `MWlg.png` → 不存在

修复为实际存在的文件：
- `company_logo.png` → 存在

修改 `config/SystemConfig.ini`：
```ini
SplashLogoPath=./Resources/logo/company_logo.png
PlayWindowLogoPath=./Resources/logo/company_logo.png
MainWindowLogoPath=./Resources/logo/company_logo.png
```

### 4. 更新 run.sh 脚本

由于编译输出改为 .app bundle，需要更新 run.sh：

```bash
# 运行程序
if [ -f "WZMediaPlayer.app/Contents/MacOS/WZMediaPlayer" ]; then
    open WZMediaPlayer.app
else
    ./WZMediaPlayer
fi
```

## 验证结果

### 打包成功
```bash
cd dist
ls -lh
# total 0
# drwxr-xr-x@ 4 ...  config
# drwxr-xr-x@ 3 ...  Resources
# drwxr-xr-x@ 2 ...  Snapshots
# drwxr-xr-x@ 3 ...  WZMediaPlayer.app
```

### 依赖库正确打包
```bash
ls -lh WZMediaPlayer.app/Contents/Frameworks/
# libavcodec.62.dylib
# libopenal.1.dylib
# libGLEW.2.3.dylib
# Qt6/ 框架目录
```

### 程序可运行
```bash
open WZMediaPlayer.app
# 程序启动成功 ✓
```

## 修改的文件列表

1. `CMakeLists.txt` - 添加 MACOSX_BUNDLE 配置
2. `scripts/package_mac.sh` - 支持 .app bundle
3. `build/run.sh` - 支持打开 .app
4. `dist/config/SystemConfig.ini` - 修复 Logo 路径
5. `docs/PACKAGE_FIX_SUMMARY.md` - 本文档（新增）

## 使用说明

### 开发环境（build 目录）

```bash
cd build
./run.sh  # 自动打开 .app bundle
```

### 打包分发（dist 目录）

```bash
cd build
cmake ..
make WZMediaPlayer
cd ..
./scripts/package_mac.sh
cd dist
open WZMediaPlayer.app
```

## 注意事项

1. **运行方式**：
   - 开发环境：使用 `./run.sh` 或 `open WZMediaPlayer.app`
   - 打包后：使用 `open WZMediaPlayer.app`（双击也可）

2. **调试信息**：
   - 使用 `DYLD_PRINT_LIBRARIES=1` 查看库加载详情
   - 使用 `log show --predicate 'process == "WZMediaPlayer"' --last 2m` 查看系统日志

3. **资源文件**：
   - `config/` - 配置文件
   - `Resources/logo/` - Logo 图片
   - `Snapshots/` - 截图保存目录（空）
   - 所有路径相对于 .app bundle 的可执行文件

## 下一步建议

1. **测试程序功能**：验证播放、暂停、seek 等核心功能
2. **性能测试**：检查内存使用、CPU 占用
3. **兼容性测试**：在不同 macOS 版本上测试
4. **代码签名**：考虑添加 codesign 以便分发
5. **创建 DMG**：使用 `hdiutil` 创建 DMG 镜像
