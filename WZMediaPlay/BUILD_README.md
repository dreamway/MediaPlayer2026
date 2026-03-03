# WZMediaPlayer 编译指南

## 脚本说明

### 1. `build.ps1` - 完整项目编译
用于完整编译整个WZMediaPlayer项目。

#### 使用方法：
```powershell
# 调试版本编译
.\build.ps1

# 发布版本编译
.\build.ps1 -Configuration Release

# 清理后重新编译
.\build.ps1 -Clean

# 详细输出
.\build.ps1 -Verbose
```

#### 参数：
- `-Configuration <Debug|Release>`: 编译配置，默认Debug
- `-Clean`: 先清理再编译
- `-Verbose`: 显示详细编译信息

### 2. `quick_build.ps1` - 快速语法检查
仅编译新增的core模块，用于快速验证新代码语法。

#### 使用方法：
```powershell
# 快速语法检查
.\quick_build.ps1

# 详细输出
.\quick_build.ps1 -Verbose
```

## 手动编译方法

如果脚本无法正常工作，可以手动执行以下步骤：

### 1. 设置VS环境
```cmd
call "D:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
```

### 2. 使用MSBuild编译
```cmd
cd WZMediaPlay
MSBuild WZMediaPlay.vcxproj /p:Configuration=Debug /p:Platform=x64 /t:Build
```

## 常见问题

### 1. 找不到MSBuild
确保VS 2022已正确安装，并且MSBuild在PATH中。

### 2. MSBuild找不到
确保VS 2022的MSBuild在PATH中，或使用完整路径：
```cmd
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
```

### 3. 权限问题
以管理员身份运行PowerShell：
```powershell
Start-Process powershell -Verb RunAs
```

### 4. 脚本执行策略
如果无法运行PowerShell脚本，执行：
```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

## 新增文件验证

重构过程中新增的文件：
- `core/Packet.h` & `core/Packet.cpp` - 媒体数据包封装
- `core/Frame.h` & `core/Frame.cpp` - 解码后视频帧封装
- `core/StreamInfo.h` & `core/StreamInfo.cpp` - 媒体流信息
- `core/PacketBuffer.h` & `core/PacketBuffer.cpp` - 线程安全缓冲区
- `core/Demuxer.h` & `core/Demuxer.cpp` - 解复用器接口和实现
- `core/DemuxerThr.h` & `core/DemuxerThr.cpp` - 解复用线程
- `core/MediaInfo.h` - 媒体信息（前向声明）

### 验证方法：
1. **快速语法检查**：运行 `syntax_check.bat`
2. **完整编译**：运行 `build.bat`
3. **手动检查**：使用cl.exe的/Zs选项进行语法检查

## 脚本文件总结

- `build.bat` - 完整项目编译
- `syntax_check.bat` - 快速语法检查（推荐）
- `build.ps1` - PowerShell版本完整编译
- `quick_build.ps1` - PowerShell版本快速编译
- `BUILD_README.md` - 本文档