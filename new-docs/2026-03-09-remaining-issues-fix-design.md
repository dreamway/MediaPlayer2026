# WZMediaPlayer 剩余问题修复设计

**日期**: 2026-03-09
**状态**: 已批准
**优先级**: P0-P2

## 问题概览

| 优先级 | 模块 | 问题 | 影响 |
|--------|------|------|------|
| P0 | macOS 测试框架 | 缺少自动化测试 | 无法验证修复效果 |
| P1 | 硬件解码 | 黑屏、帧转换失败 | 无法使用 GPU 加速 |
| P1 | Seeking | 音视频不同步、画面不更新、偶发崩溃 | 播放体验差 |
| P2 | 3D 功能 | 切换无效 | 3D 播放功能不可用 |
| P2 | 色彩 | 整体偏绿 | 视频色彩异常 |

## 参考资源

- **旧版本代码**: `v1.0.8-srcs/` - 硬解码、色彩、3D 功能正常
- **现有测试框架**: `testing/pywinauto/` - Windows 自动化测试

---

## 第一部分：macOS 自动测试框架设计

### 方案选择

采用 **PyQt + pyobjc** 方案：
- PyQt 编写测试框架主体
- pyobjc 调用 macOS Accessibility API 实现 GUI 操作
- 与 pywinauto API 相似，便于迁移

### 架构设计

```
testing/
├── pywinauto/           # Windows 测试框架（现有）
│   ├── core/
│   ├── tests/
│   └── run_all_tests.py
├── pyobjc/              # macOS 测试框架（新增）
│   ├── core/
│   │   ├── app_launcher.py      # 应用启动器
│   │   ├── ax_element.py        # Accessibility 元素封装
│   │   ├── window_controller.py # 窗口控制器
│   │   └── test_base.py         # 测试基类
│   ├── tests/
│   │   ├── test_basic_playback.py
│   │   ├── test_seek.py
│   │   └── test_3d.py
│   ├── config.py
│   └── run_all_tests.py
└── shared/              # 共享测试逻辑（可选）
    └── test_cases.py
```

### 核心组件

#### 1. AXElement (Accessibility 元素封装)

```python
# core/ax_element.py
from AppKit import NSObject
from ApplicationServices import (
    AXUIElementCreateApplication,
    AXUIElementCopyAttributeValue,
    AXUIElementPerformAction,
    kAXChildrenAttribute,
    kAXTitleAttribute,
    kAXValueAttribute,
    kAXPressAction,
)

class AXElement:
    def __init__(self, ref):
        self.ref = ref

    def find_child(self, title=None, role=None):
        """查找子元素"""
        pass

    def click(self):
        """点击元素"""
        AXUIElementPerformAction(self.ref, kAXPressAction)

    def get_value(self):
        """获取值"""
        pass
```

#### 2. WindowController (窗口控制器)

```python
# core/window_controller.py
class WindowController:
    def __init__(self, app_path):
        self.app_path = app_path
        self.pid = None

    def launch(self):
        """启动应用"""
        pass

    def get_main_window(self):
        """获取主窗口"""
        pass

    def find_button(self, name):
        """查找按钮"""
        pass

    def find_slider(self, name):
        """查找滑块"""
        pass
```

#### 3. TestBase (测试基类)

```python
# core/test_base.py
class TestBase:
    def setup(self):
        """启动应用"""
        self.controller = WindowController(APP_PATH)
        self.controller.launch()

    def teardown(self):
        """关闭应用"""
        self.controller.quit()

    def open_video(self, video_path):
        """打开视频（共享逻辑）"""
        pass

    def wait_for_playback(self, timeout=5000):
        """等待播放开始"""
        pass
```

### 与 pywinauto 对比

| 功能 | pywinauto (Windows) | pyobjc (macOS) |
|------|---------------------|----------------|
| 启动应用 | `Application().start()` | `NSWorkspace.sharedWorkspace().launchApplication()` |
| 查找控件 | `app.Window.child_window(title="...")` | `AXUIElementCopyAttributeValue()` |
| 点击 | `button.click()` | `AXUIElementPerformAction(ref, kAXPressAction)` |
| 获取属性 | `element.window_text()` | `AXUIElementCopyAttributeValue()` |

### macOS 权限要求

需要在「系统偏好设置 > 安全性与隐私 > 隐私 > 辅助功能」中授权终端或测试脚本。

---

## 第二部分：硬件解码修复设计

### 方案选择

采用 **FFmpeg hwaccel** 方案：
- 使用 FFmpeg 内置硬件加速 API
- 通过 `AV_HWDEVICE_TYPE_CUDA` 启用 NVDEC
- 跨平台设计，未来可扩展 VideoToolbox

### 当前问题

```
av_hwframe_map: -40 (Function not implemented)
av_hwframe_transfer_data: 失败
```

### 根因分析

1. `hw_frames_ctx` 未正确初始化
2. 像素格式不匹配
3. CUDA 上下文管理问题

### 修复策略

#### 1. 检查硬件解码器可用性

```cpp
// FFDecHW.cpp
bool FFDecHW::init() {
    // 检查硬件解码器
    AVCodecHWConfig* config = avcodec_get_hw_config(codec, 0);
    if (!config || !(config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)) {
        logger->warn("Hardware decoder not supported");
        return false;
    }

    // 创建硬件设备上下文
    int ret = av_hwdevice_ctx_create(&hw_device_ctx,
                                      AV_HWDEVICE_TYPE_CUDA,
                                      NULL, NULL, 0);
    if (ret < 0) {
        logger->error("Failed to create hardware device context: {}", ret);
        return false;
    }

    // 设置到 codec 上下文
    codec_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
    return true;
}
```

#### 2. 正确处理硬件帧

```cpp
// FFDecHW.cpp
int FFDecHW::decode(const AVPacket* packet, AVFrame* frame) {
    int ret = avcodec_receive_frame(codec_ctx, hw_frame);

    if (ret == 0 && hw_frame->format == AV_PIX_FMT_CUDA) {
        // 硬件帧需要传输到系统内存
        ret = av_hwframe_transfer_data(frame, hw_frame, 0);
        if (ret < 0) {
            logger->error("hwframe_transfer failed: {}", ret);
            return ret;
        }
    }

    return ret;
}
```

#### 3. 参考旧版本实现

旧版本 `v1.0.8-srcs/WZMediaPlay/` 中硬件解码正常工作，可参考其 `FFmpegView.cc` 中的实现。

### 错误恢复

当硬件解码失败时，自动回退到软件解码：

```cpp
if (!hardware_decoder->init()) {
    logger->warn("Hardware decoder init failed, falling back to software");
    decoder_ = std::make_unique<FFDecSW>();
    decoder_->init();
}
```

---

## 第三部分：Seeking 问题修复设计

### 问题症状

| 问题 | 频率 | 症状 |
|------|------|------|
| 音视频不同步 | 中频 | Seek 后声音和画面位置不一致 |
| 画面不更新 | 低频 | Seek 后只有声音，画面无变化 |
| 偶发崩溃 | 低频 | Seek 后程序崩溃 |

### 修复策略

#### 1. 音视频不同步

**根因**：Seek 完成后时钟未正确重置。

**修复**：在 `onDemuxerThreadSeekFinished` 中同步音视频时钟：

```cpp
void PlayController::onDemuxerThreadSeekFinished(qint64 targetPts) {
    // 同步音视频时钟到目标位置
    if (audioThread_) {
        audioThread_->getClock()->setPts(targetPts);
    }
    if (videoThread_) {
        videoThread_->resetClock(targetPts);
    }

    // 重置主时钟
    basePts_ = targetPts;

    stateMachine_.exitSeeking("Seek completed");
}
```

#### 2. 画面不更新

**根因**：Seek 后首帧未触发渲染。

**修复**：Seek 完成后请求关键帧并强制刷新：

```cpp
void PlayController::seek(qint64 posMs) {
    stateMachine_.enterSeeking("User requested seek");

    // 清空队列和渲染器
    videoRenderer_->clear();

    // 请求 Seek 到最近关键帧
    demuxerThread_->seekTo(posMs, AVSEEK_FLAG_BACKWARD);

    // 在 seek 完成回调中触发首帧渲染
}
```

#### 3. 偶发崩溃

**根因**：多线程竞态条件。

**修复**：利用 `PlaybackStateMachine` 强化状态保护：

```cpp
bool PlayController::seek(qint64 posMs) {
    // 只有在 Playing 或 Paused 状态才能 Seek
    if (!stateMachine_.canSeek()) {
        logger->warn("Cannot seek in current state: {}", stateMachine_.getState());
        return false;
    }

    stateMachine_.enterSeeking("Seek requested");

    // 暂停解码线程
    pauseDecoding();

    // ... 执行 Seek ...

    return true;
}
```

---

## 第四部分：3D 功能修复设计

### 问题

3D 模式切换后画面无变化。

### 根因分析

着色器已有完整实现（`fragment.glsl`），问题可能在于：
1. UI 控件状态变化未正确传递到着色器 uniform
2. 切换后未调用 `update()` 触发重绘

### 支持的格式

**输入格式**：
- `STEREO_INPUT_FORMAT_LR` (0) - 左右格式
- `STEREO_INPUT_FORMAT_RL` (1) - 右左格式
- `STEREO_INPUT_FORMAT_UD` (2) - 上下格式

**输出格式**：
- `STEREO_OUTPUT_FORMAT_VERTICAL` (0) - 垂直隔行
- `STEREO_OUTPUT_FORMAT_HORIZONTAL` (1) - 水平隔行
- `STEREO_OUTPUT_FORMAT_CHESS` (2) - 棋盘格
- `STEREO_OUTPUT_FORMAT_ONLY_LEFT` (3) - 只显示左眼

### 修复策略

#### 1. 检查信号连接

```cpp
// StereoVideoWidget.cpp 或 MainWindow.cpp
void setup3DConnections() {
    // 确保 3D 开关信号正确连接
    connect(switchButton3D, &SwitchButton::toggled, this, [this](bool checked) {
        stereoWidget_->setStereoEnabled(checked);
        stereoWidget_->update();  // 强制重绘
    });

    // 输入格式选择
    connect(inputFormatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        stereoWidget_->setStereoInputFormat(index);
        stereoWidget_->update();
    });

    // 输出格式选择
    connect(outputFormatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        stereoWidget_->setStereoOutputFormat(index);
        stereoWidget_->update();
    });
}
```

#### 2. 确保 uniform 更新

```cpp
// StereoOpenGLCommon.cpp
void StereoOpenGLCommon::setStereoEnabled(bool enabled) {
    iStereoFlag = enabled ? 1 : 0;
    if (shaderProgramStereo && shaderProgramStereo->isLinked()) {
        shaderProgramStereo->bind();
        shaderProgramStereo->setUniformValue("iStereoFlag", iStereoFlag);
        shaderProgramStereo->release();
    }
    update();  // 触发重绘
}
```

---

## 第五部分：色彩问题修复设计

### 问题

视频整体偏绿。

### 根因分析

对比旧版本 `v1.0.8-srcs/WZMediaPlay/Shader/fragment.glsl`：

**旧版本 (正常)**:
```glsl
yuv.x = ...textureY... - 0.0625;
yuv.y = ...textureU... - 0.5;   // textureU -> yuv.y (U 分量)
yuv.z = ...textureV... - 0.5;   // textureV -> yuv.z (V 分量)
```

**当前版本 (偏绿)**:
```glsl
yuv.x = ...textureY... - 0.0625;
yuv.y = ...textureV... - 0.5;   // 错误：交换了 U/V
yuv.z = ...textureU... - 0.5;
```

当前版本错误地交换了 U/V 分量，导致色彩偏绿。

### 修复策略

恢复为旧版本的 YUV 采样方式：

```glsl
// fragment.glsl main() 函数中
yuv.x = stereo_display(textureY, TexCoord, ...).r - 0.0625;
yuv.y = stereo_display(textureU, TexCoord, ...).r - 0.5;  // U 分量
yuv.z = stereo_display(textureV, TexCoord, ...).r - 0.5;  // V 分量
```

### 验证方法

1. 播放测试视频，检查肤色是否正常（不偏绿）
2. 检查白色区域是否纯白（不偏品红）
3. 对比旧版本播放效果

---

## 测试验证

### macOS 测试框架验证

```bash
# 运行 macOS 测试
cd testing/pyobjc
python run_all_tests.py
```

### 硬件解码验证

1. 启用硬件解码配置
2. 播放 H.264/H.265 视频
3. 检查日志确认使用 NVDEC
4. 验证画面正常、无黑屏

### Seeking 验证

1. 播放视频
2. 多次快速拖动进度条
3. 验证音视频同步
4. 验证无崩溃

### 3D 功能验证

1. 打开 3D 视频
2. 切换 2D/3D 模式
3. 切换输入格式 (LR/RL/UD)
4. 切换输出格式 (垂直/水平/棋盘/单眼)
5. 验证画面正确显示

### 色彩验证

1. 播放测试视频
2. 对比旧版本播放效果
3. 检查肤色、白色、彩色是否正常

---

## 实施顺序

1. **macOS 测试框架** - 为后续修复提供验证保障
2. **色彩问题** - 最简单的修复，立即见效
3. **3D 功能** - 信号连接检查，修改量小
4. **Seeking 问题** - 需要仔细测试多线程逻辑
5. **硬件解码** - 最复杂，需要参考旧版本代码

---

## 后续建议

1. 考虑将 macOS 测试框架和 Windows 测试框架抽象为统一接口
2. 硬件解码修复后，可扩展支持 VideoToolbox (macOS)
3. 添加 CI/CD 自动化测试，覆盖 macOS 和 Windows