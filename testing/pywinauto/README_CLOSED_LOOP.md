# WZMediaPlayer 闭环自动化测试框架

## 概述

这是一个完全闭环的自动化测试框架，基于 `pywinauto` 的 UIA 后端实现。与之前的测试方式不同，本框架能够：

1. **自动识别UI控件** - 通过解析Qt源码和UI文件，自动获取控件信息
2. **精确控件操作** - 使用UIA后端直接操作控件，而非依赖坐标
3. **图像验证闭环** - 通过截图和图像比较自动判断程序状态
4. **智能等待机制** - 基于控件状态而非固定sleep

## 架构

```
testing/pywinauto/
├── core/                          # 核心模块
│   ├── __init__.py
│   ├── qt_ui_parser.py           # Qt UI解析器
│   ├── ui_automation.py          # UI自动化控制器
│   ├── image_verifier.py         # 图像验证器
│   └── test_base.py              # 测试基类
├── tests/                         # 测试用例
│   ├── __init__.py
│   └── closed_loop_tests.py      # 闭环测试用例
├── run_closed_loop_tests.py      # 测试入口
└── README_CLOSED_LOOP.md         # 本文档
```

## 核心特性

### 1. Qt UI解析器 (qt_ui_parser.py)

自动解析Qt生成的UI头文件，提取控件信息：

```python
from core import QtUIParser

parser = QtUIParser(project_root)
parser.parse_ui_header("path/to/ui_MainWindow.h")
parser.print_control_summary()

# 获取控件映射
controls = parser.get_playback_controls()
# controls['play_pause'] -> 'pushButton_playPause'
```

### 2. UI自动化控制器 (ui_automation.py)

基于pywinauto UIA后端的精确控件操作：

```python
from core import UIAutomationController

ui = UIAutomationController(backend="uia")
ui.start_application("path/to/exe")

# 精确获取控件
control = ui.get_control(automation_id="pushButton_playPause", control_type="Button")

# 点击按钮
ui.click_button(automation_id="pushButton_playPause")

# 获取滑块值
value = ui.get_slider_value("horizontalSlider_volume")

# 获取标签文本
text = ui.get_label_text("label_playTime")

# 截图
image = ui.capture_window_screenshot()
```

### 3. 图像验证器 (image_verifier.py)

基于PIL的图像验证：

```python
from core import ImageVerifier

verifier = ImageVerifier()

# 验证视频是否在播放（比较两帧）
result = verifier.verify_video_playing(image1, image2)

# 检测黑屏
is_black = verifier.is_black_screen(image)

# 验证像素颜色
result = verifier.verify_pixel_color(image, (100, 100), (255, 0, 0))

# 分析播放状态
state = verifier.is_playback_ui_state(image, play_region)
# state['is_playing'] -> True/False
```

### 4. 闭环测试基类 (test_base.py)

提供完整的测试框架：

```python
from core import ClosedLoopTestBase

class MyTests(ClosedLoopTestBase):
    def test_example(self):
        self.start_test("示例测试")
        
        # 执行操作
        self.ui.click_button(automation_id="pushButton_playPause")
        
        # 闭环验证
        is_playing, msg = self.verify_video_playing()
        
        self.end_test(is_playing, msg)
```

## 使用方法

### 1. 运行所有测试

```bash
cd testing/pywinauto
python run_closed_loop_tests.py
```

### 2. 列出UI控件

```bash
python run_closed_loop_tests.py --list-controls
```

### 3. 检查运行中的UI

```bash
python run_closed_loop_tests.py --inspect-ui
```

### 4. 运行指定测试

```bash
python run_closed_loop_tests.py --test-case test_open_video_file
```

### 5. 指定路径

```bash
python run_closed_loop_tests.py --exe-path "C:\Path\To\Player.exe" --video-path "C:\Path\To\Video.mp4"
```

## 测试用例说明

### 基础播放测试

- `test_open_video_file()` - 打开视频文件
  - 闭环验证：时间标签更新、播放区域非黑屏
  
- `test_play_pause_toggle()` - 播放/暂停切换
  - 闭环验证：画面停止/恢复变化
  
- `test_stop_playback()` - 停止播放
  - 闭环验证：时间重置、黑屏检测

### Seek测试

- `test_seek_forward_small()` - 小幅前进Seek
  - 闭环验证：时间标签前进
  
- `test_seek_backward_small()` - 小幅后退Seek
  - 闭环验证：时间标签后退
  
- `test_seek_to_position()` - 进度条Seek
  - 闭环验证：时间变化

### 音量控制测试

- `test_volume_control()` - 音量调整
  - 闭环验证：滑块值变化
  
- `test_mute_toggle()` - 静音切换
  - 闭环验证：按钮状态

### 3D功能测试

- `test_3d_mode_toggle()` - 3D/2D切换
  - 闭环验证：画面变化（图像比较）
  
- `test_input_format_change()` - 输入格式切换
  - 闭环验证：画面变化

### 全屏测试

- `test_fullscreen_toggle()` - 全屏切换
  - 闭环验证：窗口尺寸变化

## 闭环验证原理

### 视频播放验证

```python
def verify_video_playing(self, check_duration=2.0):
    # 1. 截取第一帧
    img1 = self.ui.capture_window_screenshot()
    
    # 2. 等待一段时间
    time.sleep(check_duration)
    
    # 3. 截取第二帧
    img2 = self.ui.capture_window_screenshot()
    
    # 4. 比较两帧差异
    # 如果差异大，说明视频在播放
    # 如果差异小，说明视频暂停或停止
```

### 黑屏检测

```python
def is_black_screen(self, image):
    gray = image.convert('L')
    avg_brightness = sum(gray.getdata()) / len(pixels)
    return avg_brightness < threshold
```

### UI状态验证

```python
def verify_control_property(self, automation_id, property_name, expected_value):
    control = self.ui.get_control(automation_id)
    actual_value = control.get_property(property_name)
    return actual_value == expected_value
```

## 扩展测试用例

添加新的测试用例非常简单：

```python
class WZMediaPlayerClosedLoopTests(ClosedLoopTestBase):
    
    def test_my_new_feature(self):
        """测试新功能"""
        self.start_test("新功能测试")
        
        try:
            # 1. 执行操作
            self.ui.click_button(automation_id="myButton")
            
            # 2. 等待状态变化
            time.sleep(1)
            
            # 3. 闭环验证
            screenshot = self.ui.capture_window_screenshot()
            state = self.image_verifier.is_playback_ui_state(screenshot, region)
            
            # 4. 记录结果
            self.end_test(state['is_playing'], f"亮度: {state['brightness']}")
            
        except Exception as e:
            self.end_test(False, f"异常: {e}", screenshot=True)
```

## 注意事项

1. **UIA后端要求**：Qt应用需要支持UI Automation（Qt 5.3+）
2. **管理员权限**：某些操作可能需要管理员权限
3. **屏幕分辨率**：测试期间保持屏幕分辨率稳定
4. **焦点问题**：确保测试过程中应用窗口能获得焦点

## 运行说明（含 worktree）

exe 路径会**根据脚本位置自动推断**，支持 main 工作区和 worktree：

- **main 工作区**：`d:\MediaPlayer_2025\x64\Debug\WZMediaPlay.exe`
- **worktree**：`d:\MediaPlayer_2025\.worktrees\closed-loop-enhancement\WZMediaPlay\x64\Debug\WZMediaPlay.exe`

在 worktree 中运行测试时，请确保：

1. 在 worktree 根目录下编译：`MSBuild WZMediaPlay\WZMediaPlay.vcxproj /p:Configuration=Debug /p:Platform=x64`
2. 在 `testing/pywinauto` 下执行：`python unified_closed_loop_tests.py --categories basic`
3. 若 exe 目录缺少 `config/SystemConfig.ini`，测试框架会自动从项目复制

显式指定 exe 路径：

```bash
python unified_closed_loop_tests.py --categories basic --exe-path "path\to\WZMediaPlay.exe"
```

## 故障排除

### 控件无法找到

1. 运行 `--list-controls` 检查控件映射
2. 运行 `--inspect-ui` 检查实际UI结构
3. 确认应用使用Qt 5.3+编译

### 图像验证失败

1. 检查屏幕分辨率是否变化
2. 确认播放区域坐标正确
3. 调整亮度/差异阈值

### 测试不稳定

1. 增加等待时间
2. 使用智能等待替代固定sleep
3. 添加重试机制

### 进程启动后立即退出（Process not found）

1. 确认 exe 路径正确（运行时会打印 `[信息] 使用播放器: ...`）
2. 检查 exe 同目录是否有 `config/SystemConfig.ini`（测试框架会自动尝试复制）
3. 确认所需 DLL（如 OpenAL32.dll）存在于 exe 同目录
4. 若使用 worktree 构建，请确保在 worktree 内完成编译

## 参考

- [pywinauto文档](https://pywinauto.readthedocs.io/)
- [UIA后端说明](https://pywinauto.readthedocs.io/en/latest/code/pywinauto.uia_defines.html)
- [Qt UI Automation](https://doc.qt.io/qt-6/windows-accessibility.html)
