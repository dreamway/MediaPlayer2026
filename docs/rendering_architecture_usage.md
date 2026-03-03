# WZMediaPlayer 新渲染架构使用指南

## 概述

新的渲染架构基于抽象基类 `VideoRenderer` 和 `VideoWidgetBase`，实现了渲染逻辑与 GUI 的解耦。

## 核心组件

### 1. VideoRenderer（渲染器接口）

```cpp
// 创建 2D 渲染器
auto renderer = RendererFactory::createOpenGLRenderer();

// 创建 3D 立体渲染器
auto renderer = RendererFactory::createStereoRenderer();

// 根据格式自动选择
auto renderer = RendererFactory::createRenderer(STEREO_FORMAT_3D);
```

### 2. VideoWidgetBase（视频窗口基类）

`StereoVideoWidget` 已经继承自 `VideoWidgetBase`，可以直接使用：

```cpp
// 在 MainWindow 中
ui.playWidget->setRenderer(renderer);
```

### 3. PlayController 集成

```cpp
// 设置渲染器
playController_->setVideoRenderer(renderer);

// 设置视频窗口
playController_->setVideoWidgetBase(ui.playWidget);
```

## 使用示例

### 基本使用（2D 视频）

```cpp
// 1. 创建渲染器
auto renderer = RendererFactory::createOpenGLRenderer();

// 2. 设置到视频窗口
ui.playWidget->setRenderer(renderer);

// 3. 设置到播放控制器
playController_->setVideoRenderer(renderer);
playController_->setVideoWidgetBase(ui.playWidget);

// 4. 打开并播放视频
playController_->open("video.mp4");
playController_->play();
```

### 3D 视频播放

```cpp
// 1. 创建 3D 渲染器
auto renderer = RendererFactory::createStereoRenderer();

// 2. 设置到视频窗口
ui.playWidget->setRenderer(renderer);

// 3. 配置 3D 参数
ui.playWidget->setStereoFormat(STEREO_FORMAT_3D);
ui.playWidget->setStereoInputFormat(STEREO_INPUT_FORMAT_LR);
ui.playWidget->setStereoOutputFormat(STEREO_OUTPUT_FORMAT_HORIZONTAL);

// 4. 播放视频
playController_->open("3d_video.mp4");
playController_->play();
```

### 动态切换 2D/3D

```cpp
// 切换到 2D
void MainWindow::switchTo2D()
{
    auto renderer = RendererFactory::createOpenGLRenderer();
    ui.playWidget->setRenderer(renderer);
    ui.playWidget->setStereoFormat(STEREO_FORMAT_NORMAL_2D);
}

// 切换到 3D
void MainWindow::switchTo3D()
{
    auto renderer = RendererFactory::createStereoRenderer();
    ui.playWidget->setRenderer(renderer);
    ui.playWidget->setStereoFormat(STEREO_FORMAT_3D);
}
```

## 架构优势

### 1. 解耦设计

- **VideoRenderer**：只负责渲染，不关心 GUI
- **VideoWidgetBase**：只负责显示，不关心渲染细节
- **PlayController**：协调播放，不直接操作渲染

### 2. 易于扩展

添加新的渲染器类型只需：
1. 继承 `VideoRenderer`
2. 实现纯虚函数
3. 在工厂中注册

### 3. 向后兼容

- `StereoVideoWidget` 同时支持新旧架构
- 旧代码继续使用 `StereoWriter`
- 新代码使用 `VideoRenderer`

## 文件结构

```
videoDecoder/
├── VideoRenderer.h              # 渲染器抽象基类
├── VideoWidgetBase.h            # 视频窗口抽象基类
├── RendererFactory.h/cpp        # 渲染器工厂
└── opengl/
    ├── OpenGLRenderer.h/cpp     # OpenGL 2D 渲染器
    ├── StereoOpenGLRenderer.h/cpp # OpenGL 3D 渲染器
    └── OpenGLVideoWidget.h/cpp  # OpenGL 视频窗口（可选）
```

## 迁移指南

### 从旧架构迁移

1. **替换渲染器创建**
   ```cpp
   // 旧代码
   auto writer = std::make_shared<StereoWriter>();
   playController_->setVideoWriter(writer);

   // 新代码
   auto renderer = RendererFactory::createStereoRenderer();
   playController_->setVideoRenderer(renderer);
   ```

2. **更新视频窗口设置**
   ```cpp
   // 新代码
   ui.playWidget->setRenderer(renderer);
   ```

3. **配置 3D 参数**
   ```cpp
   // 新旧代码相同
   ui.playWidget->setStereoFormat(STEREO_FORMAT_3D);
   ui.playWidget->setStereoInputFormat(STEREO_INPUT_FORMAT_LR);
   ```

## 注意事项

1. **线程安全**：渲染器的 `render()` 方法在 VideoThread 中调用
2. **OpenGL 上下文**：确保在正确的 OpenGL 上下文中操作
3. **资源清理**：渲染器会自动清理资源，无需手动释放

## 调试技巧

```cpp
// 检查渲染器状态
if (renderer->isReady()) {
    logger->info("Renderer is ready");
}

// 获取渲染统计
auto stats = renderer->stats();
logger->info("Rendered: {}, Dropped: {}, FPS: {}",
             stats.renderedFrames,
             stats.droppedFrames,
             stats.currentFps);
```
