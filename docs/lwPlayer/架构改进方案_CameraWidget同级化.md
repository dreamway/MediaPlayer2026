# Camera Widget 架构改进方案

## 问题分析

### 当前架构问题

1. **职责不清**：`CameraOpenGLWidget` 作为 `StereoVideoWidget` 的子组件，但两者功能独立
2. **3D 功能未实现**：Camera 原本计划支持 3D 显示，但实际未实现，shader 也是分开的
3. **架构耦合**：Camera 渲染与视频文件渲染混在一起，不利于维护

### 改进目标

- 将 `CameraOpenGLWidget` 提升为与 `StereoVideoWidget` 同级的 Widget
- 在 `MainWindow` 中统一管理两个 Widget，通过 hide/show 切换
- 简化 `StereoVideoWidget` 的职责，专注于视频文件渲染和 UI 组件管理

## 改进方案

### 新架构设计

```
MainWindow
├── playWidget: StereoVideoWidget* (视频文件渲染 + UI组件)
│   ├── videoRenderer_: VideoRendererPtr
│   ├── mWindowLogo: QLabel*
│   ├── mSubtitleWidget: SubtitleWidget*
│   ├── mFPSLabel: QLabel*
│   └── butWidget: FloatButton*
│
└── cameraWidget: CameraOpenGLWidget* (Camera渲染，同级)
    └── CameraRenderer (内部管理)
```

### 切换逻辑

在 `MainWindow` 中统一管理切换：

```cpp
// MainWindow.h
class StereoVideoWidget;
class CameraOpenGLWidget;

private:
    StereoVideoWidget *playWidget_;
    CameraOpenGLWidget *cameraWidget_;
    RenderInputSource currentRenderSource_;

// MainWindow.cpp
void MainWindow::switchToVideoFile() {
    cameraWidget_->hide();
    playWidget_->show();
    currentRenderSource_ = RIS_VIDEO_FILE;
}

void MainWindow::switchToCamera() {
    playWidget_->hide();
    cameraWidget_->show();
    currentRenderSource_ = RIS_CAMERA;
}
```

## 实施步骤

### 阶段 1：准备（低优先级，支线任务）

1. **在 MainWindow 中创建 CameraOpenGLWidget**
   ```cpp
   // MainWindow.h
   CameraOpenGLWidget *cameraWidget_;
   
   // MainWindow.cpp
   cameraWidget_ = new CameraOpenGLWidget(this);
   cameraWidget_->hide(); // 默认隐藏
   ```

2. **从 StereoVideoWidget 中移除 CameraOpenGLWidget**
   - 移除 `cameraOpenGLWidget_` 成员
   - 移除 `cameraWidget()` 访问方法
   - 移除布局中添加 CameraOpenGLWidget 的代码

3. **更新切换逻辑**
   - 在 `MainWindow` 中实现 `switchToVideoFile()` 和 `switchToCamera()`
   - 修改 `on_actionOpenCamera_toggled()` 使用新的切换逻辑

### 阶段 2：清理（低优先级，支线任务）

1. **移除 StereoVideoWidget 中的 Camera 接口**
   - 移除 `getCamerasInfo()`, `openCamera()`, `startCamera()`, `stopCamera()`, `closeCamera()`
   - 移除 `mCamera`, `mCameraSession`, `mCameraSink` 成员

2. **完善 CameraManager**
   - 确保 `CameraManager` 完全独立管理 Camera 生命周期
   - 连接信号/槽，通知 `MainWindow` 切换 Widget

## 优先级说明

**Camera 架构改进是支线任务**，优先级低于以下核心任务：

1. **硬件解码修复**（高优先级）
2. **基础播放器功能修复**（高优先级）
   - 色彩问题
   - 音频问题
   - Seeking 问题
   - AV 同步问题
3. **阶段四、六实施**（中优先级）
   - 统计信息系统
   - 播放列表优化
4. **测试闭环完善**（中优先级）

**建议**：先完成核心功能修复和阶段四、六实施，再考虑 Camera 架构改进。

## 当前状态

- ✅ `CameraManager` 已创建
- ✅ `MainWindow` 中已部分集成 `CameraManager`
- ⏸️ Camera Widget 同级化（待实施，低优先级）
- ⏸️ 移除 `StereoVideoWidget` 中的 Camera 接口（待实施，低优先级）
