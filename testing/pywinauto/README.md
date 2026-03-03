# WZMediaPlayer 闭环自动化测试框架

## 概述

这是WZMediaPlayer 3D播放器的**闭环自动化测试框架**，使用pywinauto实现完全自动化的UI测试。与之前的开环测试不同，本框架能够：

- **自动解析Qt UI控件** - 从C++源码提取控件automation_id
- **精确控制UI元素** - 使用UIA后端直接操作控件（非仅快捷键）
- **闭环验证结果** - 通过图像对比、UI状态检查自动验证测试效果
- **智能等待机制** - 自动等待操作完成，避免固定sleep

## 架构设计

```
┌─────────────────────────────────────────────────────────────┐
│                    统一测试入口                              │
│              unified_closed_loop_tests.py                    │
├─────────────────────────────────────────────────────────────┤
│  基础播放  │  进度控制  │  音频功能  │  3D功能  │  其他测试   │
│  test_basic │ test_seek │ test_audio │ test_3d  │  ...      │
├─────────────────────────────────────────────────────────────┤
│                      闭环测试基类                            │
│              ClosedLoopTestBase (test_base.py)               │
├─────────────────────────────────────────────────────────────┤
│  UI自动化控制器    │   图像验证器    │   Qt UI解析器         │
│  ui_automation.py  │ image_verifier.py │  qt_ui_parser.py   │
└─────────────────────────────────────────────────────────────┘
```

## 安装

### 前置要求
- Python 3.8 或更高版本
- Windows 10/11 操作系统
- WZMediaPlayer 可执行文件（Debug或Release版本）

### 安装依赖

```bash
# 进入测试目录
cd testing/pywinauto

# 安装依赖
pip install pywinauto Pillow

# 或者从requirements.txt安装
pip install -r requirements.txt
```

## 路径配置

**所有脚本的 EXE 与测试视频路径统一由 `config.ini` 管理**，无需改代码：

- 编辑同目录下的 **`config.ini`**，修改 `[paths]` 中的：
  - `exe_path`：播放器可执行文件路径（如 `D:\2026Github\build\Release\WZMediaPlayer.exe`）
  - `test_video_path`：默认测试视频路径
  - `test_3d_video_path`：3D 测试视频（留空则使用 `test_video_path`）
- `config.py` 会优先从 `config.ini` 读取上述项，未配置时使用内置默认值。
- 命令行仍可通过 `--exe-path` / `--video-path` 等覆盖（部分脚本支持）。

## 快速开始

### 1. 运行完整测试套件

```bash
python unified_closed_loop_tests.py
```

### 2. 运行特定测试类别

```bash
# 只运行基础播放和音频测试
python unified_closed_loop_tests.py --categories basic audio

# 运行除边界情况外的所有测试
python unified_closed_loop_tests.py --categories basic seek audio 3d sync fullscreen

# 查看所有可用类别
python unified_closed_loop_tests.py --help
```

### 3. 指定自定义路径

```bash
python unified_closed_loop_tests.py \
    --exe-path "E:\WZMediaPlayer_2025\x64\Release\WZMediaPlay.exe" \
    --video-path "D:\test_videos\sample.mp4" \
    --video-3d-path "D:\test_videos\3d_video.mp4"
```

## 已实现功能 ✅

### Phase 1: 闭环基础架构
- [x] **Qt UI自动解析** - 从ui_MainWindow.h提取51个控件的automation_id
- [x] **UIA后端支持** - 使用pywinauto UIA后端精确控制Qt控件
- [x] **图像验证** - 通过PIL对比截图验证视频播放状态
- [x] **智能控件查找** - 支持完整路径和部分匹配的automation_id
- [x] **测试报告生成** - 自动生成详细的测试报告和截图

### Phase 2: 基础播放控制测试
- [x] **打开视频文件** - 闭环验证：时间标签从00:00:00变为实际时间
- [x] **播放/暂停切换** - 闭环验证：图像对比确认画面停止变化
- [x] **停止播放** - 闭环验证：图像对比确认视频停止播放

### Phase 3: 进度控制测试
- [x] **小幅前进Seek** - 闭环验证：时间标签变化
- [x] **小幅后退Seek** - 闭环验证：时间标签变化
- [x] **大幅Seek** - 闭环验证：视频仍在播放状态
- [x] **进度条拖动** - 闭环验证：点击滑块并确认响应

### Phase 4: 音频功能测试
- [x] **音量控制** - 使用快捷键调整音量（避免滑块读取阻塞）
- [x] **静音切换** - 使用快捷键切换静音状态

### Phase 5: 其他测试
- [x] **3D功能测试** - 3D/2D模式切换、输入格式切换
- [x] **音视频同步测试** - Seek后同步、长时间播放同步
- [x] **硬件解码测试** - 配置检查和日志分析
- [x] **边界条件测试** - 快速操作压力测试
- [x] **全屏功能测试** - 窗口尺寸变化验证

## 进程输出监控（2026-02 新增）✅

测试框架会捕获被测进程的 stdout/stderr，检测以下模式并纳入失败判定：

| 类型 | 模式示例 | 判定 |
|------|----------|------|
| 关键错误 | QMutex destroying locked mutex, Access violation, 0xC0000005 | 出现即失败 |
| 警告超阈值 | [ALSOFT] (WW) 超过 10 次 | 失败 |
| 进程崩溃 | 进程意外退出 | 失败 |
| 用户中断 | Ctrl+C | 失败 |

---

## 待实现功能 (TODO) 🔧

### 高优先级
- [ ] **视频重新打开修复** - 音频测试后视频关闭，重新打开失败
  - 问题：`_ensure_video_opened()` 在测试间重新打开视频时时间标签为None
  - 可能原因：应用状态异常或文件对话框未正确处理
  - 建议：检查应用日志，确认是否有对话框阻塞

- [ ] **静音按钮精确控制** - 当前使用快捷键，需要验证是否可用按钮点击
  - 问题：`pushButton_volume` 点击可能触发其他行为
  - 建议：检查按钮实际功能，可能需要使用右键菜单或其他方式

### 中优先级
- [ ] **进度条精确控制** - 当前只能点击，需要支持拖动到指定位置
  - 问题：`set_slider_value()` 返回"value should be bigger than 0.0 and smaller than 42.0"
  - 建议：检查滑块实际范围，可能需要使用鼠标拖动而非set_value

- [ ] **音量滑块值读取优化** - 当前跳过读取避免阻塞，需要更可靠的读取方式
  - 问题：`get_slider_value()` 可能阻塞数百秒
  - 建议：使用Qt的property系统或其他非阻塞方式读取

### 低优先级
- [ ] **多显示器测试** - 全屏在不同显示器上的行为
- [ ] **性能基准测试** - CPU/GPU使用率监控
- [ ] **内存泄漏检测** - 长时间播放内存稳定性

## 核心组件说明

### 1. UI自动化控制器 (core/ui_automation.py)

```python
class UIAutomationController:
    def click_button(self, automation_id, wait_after=0.5) -> bool
    def get_slider_value(self, automation_id, timeout=2.0) -> Optional[int]
    def set_slider_value(self, automation_id, value, timeout=3.0) -> bool
    def get_label_text(self, automation_id) -> Optional[str]
    def capture_window_screenshot(self) -> PIL.Image
```

**特点**：
- 支持部分匹配automation_id（处理Qt完整路径）
- 滑块操作带超时保护，避免阻塞
- 自动处理控件缓存和失效

### 2. 图像验证器 (core/image_verifier.py)

```python
class ImageVerifier:
    def verify_video_playing(self, img1, img2, threshold=500) -> ImageVerificationResult
    def is_black_screen(self, image, threshold=10) -> bool
    def is_playback_ui_state(self, screenshot, play_region) -> Dict
```

**用途**：
- 对比两帧截图判断视频是否在播放
- 检测黑屏状态
- 分析播放区域亮度

### 3. Qt UI解析器 (core/qt_ui_parser.py)

```python
class QtUIParser:
    def parse_ui_header(self, header_path) -> Dict[str, UIControl]
    def get_playback_controls(self) -> Dict[str, str]
    def get_shortcut_map(self) -> Dict[str, str]
```

**功能**：
- 自动解析ui_MainWindow.h提取控件信息
- 映射Qt控件类型到UIA control type
- 提供播放控制相关的控件映射

### 4. 闭环测试基类 (core/test_base.py)

```python
class ClosedLoopTestBase:
    def setup(self) -> bool          # 启动应用
    def teardown(self)               # 关闭应用
    def start_test(self, name)       # 开始测试用例
    def end_test(self, passed, details) -> TestResult
    def verify_video_playing(self, check_duration=2.0) -> (bool, str)
    def generate_report(self) -> str
```

**特性**：
- 自动管理应用生命周期
- 支持测试重试机制
- 自动生成测试报告

## 测试类别说明

| 类别 | 命令行参数 | 说明 | 闭环验证方式 |
|------|-----------|------|-------------|
| 基础播放 | `basic` | 打开、播放/暂停、停止 | 时间标签变化、图像对比 |
| 进度控制 | `seek` | Seek、进度条拖动 | 时间标签前后对比 |
| 音频功能 | `audio` | 音量、静音 | 操作完成验证 |
| 3D功能 | `3d` | 3D/2D切换、格式切换 | 图像帧对比 |
| 音视频同步 | `sync` | Seek后同步、长时间播放 | 视频播放状态验证 |
| 硬件解码 | `hw` | 配置检查、日志分析 | 配置文件和日志检查 |
| 边界条件 | `edge` | 快速操作压力测试 | 稳定性验证 |
| 全屏功能 | `fullscreen` | 全屏切换 | 窗口尺寸变化验证 |

## 常见问题

### 1. 控件找不到

**现象**：`[UIAuto] ✗ 未找到按钮: pushButton_xxx`

**解决**：
- 框架已支持部分匹配，通常能自动处理
- 如仍失败，运行调试脚本查看实际UI结构：
```bash
python debug_ui.py
```

### 2. 视频重新打开失败

**现象**：`✗ 视频打开失败，时间标签: None`

**临时解决**：
- 单独运行测试类别，避免测试间依赖
```bash
python unified_closed_loop_tests.py --categories basic
python unified_closed_loop_tests.py --categories audio
```

**根本解决**：
- 需要修复应用状态或文件对话框处理（列入TODO）

### 3. 滑块操作阻塞

**现象**：测试卡住不动，耗时数百秒

**解决**：
- 框架已添加超时机制（2-3秒）
- 如仍有问题，检查Qt控件是否响应

### 4. QMutex 与 ALSOFT 错误

**现象**：`QMutex: destroying locked mutex` 或 `[ALSOFT] (WW) Error generated on context...`

**说明**（2026-02 更新）：
- 测试框架已集成**进程输出监控**，会捕获上述错误并**纳入测试失败判定**
- `QMutex: destroying locked mutex` 通常伴随崩溃，会直接判定测试失败
- `[ALSOFT]` 警告超过阈值（如 10 次）也会判定失败
- 报告中将出现「进程输出错误检测」或「进程存活检测」失败项

## 扩展开发

### 添加新的测试类别

1. 在 `UnifiedClosedLoopTests` 类中添加测试方法：

```python
def test_new_feature(self) -> bool:
    """新功能测试"""
    print("\n" + "="*60)
    print("【新功能测试】")
    print("="*60)
    
    results = []
    
    # 确保视频已打开
    if not self._ensure_video_opened():
        print("[错误] 无法打开视频")
        return False
    
    # 测试步骤1
    self.start_test("测试步骤1")
    try:
        # 执行操作
        self.ui.click_button(automation_id=self.control_map['some_button'])
        
        # 闭环验证
        time.sleep(1)
        is_playing, msg = self.verify_video_playing(check_duration=1.0)
        
        self.end_test(is_playing, msg)
        results.append(is_playing)
    except Exception as e:
        self.end_test(False, f"异常: {e}")
        results.append(False)
    
    return all(results)
```

2. 在 `run_all_tests()` 中注册新类别：

```python
all_categories = {
    # ... 现有类别 ...
    'new_feature': ('新功能', self.test_new_feature),
}
```

3. 运行新测试：

```bash
python unified_closed_loop_tests.py --categories new_feature
```

## 测试报告

测试完成后自动生成报告，保存在 `reports/` 目录：

```
reports/
  └── closed_loop_test_report_20260210_173013.txt
```

报告内容包括：
- 测试时间和环境信息
- 每个测试用例的通过/失败状态
- 详细执行时间和验证信息
- 失败用例的截图（如有）

## 持续集成

示例CI配置：

```yaml
# .github/workflows/test.yml
name: Automated Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v2
      
      - name: Build Project
        run: build.bat
      
      - name: Install Dependencies
        run: pip install pywinauto Pillow
      
      - name: Run Closed Loop Tests
        run: python testing/pywinauto/unified_closed_loop_tests.py --categories basic seek audio
      
      - name: Upload Report
        uses: actions/upload-artifact@v2
        with:
          name: test-report
          path: testing/pywinauto/reports/*.txt
```

## 贡献指南

欢迎贡献新的测试用例和改进！

1. Fork项目并创建分支
2. 添加测试代码并确保通过
3. 更新README文档
4. 提交Pull Request

## 许可证

与WZMediaPlayer项目保持一致。

---

**最后更新**: 2026-02-10  
**版本**: v2.0 - 闭环自动化测试框架
