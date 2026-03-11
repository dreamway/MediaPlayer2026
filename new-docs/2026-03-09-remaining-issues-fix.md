# WZMediaPlayer 剩余问题修复实施计划

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 修复 macOS 测试框架、色彩、3D、Seeking 和硬件解码问题，实现稳定可用的 3D 播放器。

**Architecture:** macOS 测试框架采用 PyQt + pyobjc；色彩修复恢复旧版本 YUV 采样；3D 功能检查信号连接；Seeking 强化状态保护；硬件解码采用 FFmpeg hwaccel。

**Tech Stack:** Python 3, pyobjc, C++17, Qt 6.6.3, FFmpeg, OpenGL

---

## Task 1: 色彩问题修复（最简单，立即见效）

**Files:**
- Modify: `WZMediaPlay/Shader/fragment.glsl:669-682`

**Step 1: 修改 YUV 采样代码**

找到第 669-682 行：

```glsl
float y = stereo_display(textureY, TexCoord, iStereoFlag, iStereoInputFormat, iStereoOutputFormat, bEnableRegion, region, iParallaxOffset).r;
float u = stereo_display(textureU, TexCoord, iStereoFlag, iStereoInputFormat, iStereoOutputFormat, bEnableRegion, region, iParallaxOffset).r;
float v = stereo_display(textureV, TexCoord, iStereoFlag, iStereoInputFormat, iStereoOutputFormat, bEnableRegion, region, iParallaxOffset).r;

// YUV到RGB转换（BT.601标准）
// 注意：GL_LUMINANCE格式已经将0-255归一化到0-1
// Limited range: Y在16-235，U/V在16-240（centered at 128）
// Full range: Y/U/V都在0-255
// 这里尝试两种方式：先假设limited range，如果颜色不对，可能需要调整
// 如果视频是full range，Y不需要减去0.0625，U/V不需要减去0.5
yuv.x = y - 0.0625;  // Y: limited range (16-235) -> 减去16/256
// BUG 2 修复：部分源（FFmpeg/驱动）输出的 U/V 与纹理命名相反，交换后可纠正绿/洋红偏色
yuv.y = v - 0.5;     // 用 textureV 填 U 位
yuv.z = u - 0.5;     // 用 textureU 填 V 位
```

修改为：

```glsl
float y = stereo_display(textureY, TexCoord, iStereoFlag, iStereoInputFormat, iStereoOutputFormat, bEnableRegion, region, iParallaxOffset).r;
float u = stereo_display(textureU, TexCoord, iStereoFlag, iStereoInputFormat, iStereoOutputFormat, bEnableRegion, region, iParallaxOffset).r;
float v = stereo_display(textureV, TexCoord, iStereoFlag, iStereoInputFormat, iStereoOutputFormat, bEnableRegion, region, iParallaxOffset).r;

// YUV到RGB转换（BT.601标准）
// 注意：GL_LUMINANCE格式已经将0-255归一化到0-1
// Limited range: Y在16-235，U/V在16-240（centered at 128）
// Full range: Y/U/V都在0-255
yuv.x = y - 0.0625;  // Y: limited range (16-235) -> 减去16/256
yuv.y = u - 0.5;     // U 分量
yuv.z = v - 0.5;     // V 分量
```

**Step 2: 验证编译**

Run: `cd /Users/jingwenlai/leadwit-tech/WeiZheng/MediaPlayer2026 && cmake --build build 2>&1 | head -50`
Expected: 编译成功，无错误

**Step 3: 验证色彩**

Run: `cd /Users/jingwenlai/leadwit-tech/WeiZheng/MediaPlayer2026/build && ./WZMediaPlayer.app/Contents/MacOS/WZMediaPlayer`
Expected: 播放视频，色彩正常（不偏绿）

**Step 4: 提交**

```bash
git add WZMediaPlay/Shader/fragment.glsl
git commit -m "$(cat <<'EOF'
fix: 修复视频色彩偏绿问题

恢复为旧版本 (v1.0.8) 的 YUV 采样方式：
- yuv.y = u - 0.5 (U 分量)
- yuv.z = v - 0.5 (V 分量)

之前错误地交换了 U/V 分量导致整体偏绿

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: macOS 自动测试框架 - 基础结构

**Files:**
- Create: `testing/pyobjc/`
- Create: `testing/pyobjc/core/__init__.py`
- Create: `testing/pyobjc/core/ax_element.py`
- Create: `testing/pyobjc/core/app_launcher.py`
- Create: `testing/pyobjc/core/window_controller.py`
- Create: `testing/pyobjc/core/test_base.py`
- Create: `testing/pyobjc/config.py`
- Create: `testing/pyobjc/requirements.txt`

**Step 1: 创建目录结构**

Run: `mkdir -p /Users/jingwenlai/leadwit-tech/WeiZheng/MediaPlayer2026/testing/pyobjc/core /Users/jingwenlai/leadwit-tech/WeiZheng/MediaPlayer2026/testing/pyobjc/tests`

**Step 2: 创建 requirements.txt**

```text
pyobjc-core>=9.0
pyobjc-framework-Cocoa>=9.0
pyobjc-framework-ApplicationServices>=9.0
Pillow>=9.0
```

**Step 3: 创建 config.py**

```python
# testing/pyobjc/config.py
import os

# 播放器可执行文件路径
APP_PATH = os.path.join(
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
    "build", "WZMediaPlayer.app", "Contents", "MacOS", "WZMediaPlayer"
)

# 测试视频文件路径
TEST_VIDEO_PATH = os.path.join(
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
    "testing", "video", "test.mp4"
)

# 超时设置（毫秒）
TIMEOUTS = {
    "app_start": 5000,
    "video_load": 5000,
    "seek_complete": 2000,
    "window_ready": 2000,
}

# 等待时间（秒）
WAIT_TIMES = {
    "after_open": 2.0,
    "after_seek": 1.0,
    "playback_test": 5.0,
}
```

**Step 4: 创建 core/__init__.py**

```python
# testing/pyobjc/core/__init__.py
from .ax_element import AXElement
from .app_launcher import AppLauncher
from .window_controller import WindowController
from .test_base import TestBase

__all__ = ['AXElement', 'AppLauncher', 'WindowController', 'TestBase']
```

**Step 5: 创建 ax_element.py**

```python
# testing/pyobjc/core/ax_element.py
"""
macOS Accessibility 元素封装
使用 pyobjc 调用 macOS Accessibility API
"""

from ApplicationServices import (
    AXUIElementRef,
    AXUIElementCopyAttributeValue,
    AXUIElementCopyAttributeNames,
    AXUIElementPerformAction,
    AXUIElementSetAttributeValue,
    kAXChildrenAttribute,
    kAXTitleAttribute,
    kAXValueAttribute,
    kAXRoleAttribute,
    kAXEnabledAttribute,
    kAXPressAction,
    kAXIncrementAction,
    kAXDecrementAction,
    AXValueGetType,
    kAXValueAXErrorType,
    kAXValueIllegal,
)
from AppKit import NSLog
from typing import Optional, List, Any


class AXElement:
    """macOS Accessibility 元素封装类"""

    def __init__(self, ref: AXUIElementRef):
        """
        初始化 AXElement

        Args:
            ref: AXUIElementRef 对象
        """
        self.ref = ref

    def get_title(self) -> Optional[str]:
        """获取元素标题"""
        result, value = AXUIElementCopyAttributeValue(self.ref, kAXTitleAttribute, None)
        if result == 0 and value:
            return str(value)
        return None

    def get_role(self) -> Optional[str]:
        """获取元素角色"""
        result, value = AXUIElementCopyAttributeValue(self.ref, kAXRoleAttribute, None)
        if result == 0 and value:
            return str(value)
        return None

    def get_value(self) -> Any:
        """获取元素值"""
        result, value = AXUIElementCopyAttributeValue(self.ref, kAXValueAttribute, None)
        if result == 0:
            return value
        return None

    def set_value(self, value: Any) -> bool:
        """设置元素值"""
        result = AXUIElementSetAttributeValue(self.ref, kAXValueAttribute, value)
        return result == 0

    def is_enabled(self) -> bool:
        """检查元素是否启用"""
        result, value = AXUIElementCopyAttributeValue(self.ref, kAXEnabledAttribute, None)
        if result == 0 and value:
            return bool(value)
        return False

    def get_children(self) -> List['AXElement']:
        """获取所有子元素"""
        result, children = AXUIElementCopyAttributeValue(self.ref, kAXChildrenAttribute, None)
        if result == 0 and children:
            return [AXElement(child) for child in children]
        return []

    def find_child_by_title(self, title: str) -> Optional['AXElement']:
        """按标题查找子元素"""
        for child in self.get_children():
            if child.get_title() == title:
                return child
            # 递归查找
            found = child.find_child_by_title(title)
            if found:
                return found
        return None

    def find_child_by_role(self, role: str) -> Optional['AXElement']:
        """按角色查找子元素"""
        for child in self.get_children():
            if child.get_role() == role:
                return child
            # 递归查找
            found = child.find_child_by_role(role)
            if found:
                return found
        return None

    def click(self) -> bool:
        """点击元素（执行 Press 动作）"""
        result = AXUIElementPerformAction(self.ref, kAXPressAction)
        return result == 0

    def increment(self) -> bool:
        """增加元素值（滑块）"""
        result = AXUIElementPerformAction(self.ref, kAXIncrementAction)
        return result == 0

    def decrement(self) -> bool:
        """减少元素值（滑块）"""
        result = AXUIElementPerformAction(self.ref, kAXDecrementAction)
        return result == 0

    def __repr__(self):
        return f"AXElement(title={self.get_title()}, role={self.get_role()})"
```

**Step 6: 创建 app_launcher.py**

```python
# testing/pyobjc/core/app_launcher.py
"""
macOS 应用启动器
"""

import subprocess
import time
from typing import Optional
from AppKit import NSWorkspace, NSRunningApplication
from ApplicationServices import AXUIElementCreateApplication, AXUIElementCopyAttributeValue, kAXWindowsAttribute

from .ax_element import AXElement


class AppLauncher:
    """macOS 应用启动器"""

    def __init__(self, app_path: str):
        """
        初始化应用启动器

        Args:
            app_path: 应用程序路径（.app 或可执行文件路径）
        """
        self.app_path = app_path
        self.process: Optional[subprocess.Popen] = None
        self.pid: Optional[int] = None
        self.ns_app: Optional[NSRunningApplication] = None

    def launch(self, timeout: float = 10.0) -> bool:
        """
        启动应用

        Args:
            timeout: 等待应用启动的超时时间（秒）

        Returns:
            是否启动成功
        """
        try:
            # 使用 NSWorkspace 启动应用
            workspace = NSWorkspace.sharedWorkspace()
            config = {
                'NSWorkspaceLaunchConfigurationArguments': []
            }

            self.ns_app = workspace.launchApplicationAtURL_options_configuration_error_(
                self.app_path if self.app_path.endswith('.app') else self._get_app_bundle(),
                0,  # NSWorkspaceLaunchDefault
                config,
                None
            )

            if self.ns_app:
                self.pid = self.ns_app.processIdentifier()
                # 等待应用窗口出现
                return self._wait_for_window(timeout)
            else:
                # 回退到 subprocess 启动
                self.process = subprocess.Popen(
                    [self.app_path],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE
                )
                self.pid = self.process.pid
                return self._wait_for_window(timeout)

        except Exception as e:
            print(f"启动应用失败: {e}")
            return False

    def _get_app_bundle(self) -> str:
        """从可执行文件路径获取 .app 包路径"""
        if '.app/Contents/MacOS/' in self.app_path:
            return self.app_path.split('/Contents/MacOS/')[0]
        return self.app_path

    def _wait_for_window(self, timeout: float) -> bool:
        """等待应用窗口出现"""
        start_time = time.time()
        while time.time() - start_time < timeout:
            if self.pid:
                app_ref = AXUIElementCreateApplication(self.pid)
                result, windows = AXUIElementCopyAttributeValue(app_ref, kAXWindowsAttribute, None)
                if result == 0 and windows and len(windows) > 0:
                    return True
            time.sleep(0.5)
        return False

    def quit(self) -> bool:
        """关闭应用"""
        try:
            if self.ns_app:
                self.ns_app.terminate()
                return True
            elif self.process:
                self.process.terminate()
                self.process.wait(timeout=5)
                return True
        except Exception as e:
            print(f"关闭应用失败: {e}")
            return False
        return False

    def is_running(self) -> bool:
        """检查应用是否在运行"""
        if self.ns_app:
            return self.ns_app.isRunning()
        elif self.process:
            return self.process.poll() is None
        return False
```

**Step 7: 创建 window_controller.py**

```python
# testing/pyobjc/core/window_controller.py
"""
macOS 窗口控制器
"""

import time
from typing import Optional, List
from ApplicationServices import (
    AXUIElementCreateApplication,
    AXUIElementCopyAttributeValue,
    kAXWindowsAttribute,
    kAXMainWindowAttribute,
    kAXFocusedWindowAttribute,
    kAXTitleAttribute,
    kAXRoleAttribute,
    kAXButtonRole,
    kAXSliderRole,
    kAXTextFieldRole,
    kAXCheckBoxRole,
)

from .ax_element import AXElement
from .app_launcher import AppLauncher


class WindowController:
    """macOS 窗口控制器"""

    def __init__(self, app_launcher: AppLauncher):
        """
        初始化窗口控制器

        Args:
            app_launcher: 应用启动器实例
        """
        self.launcher = app_launcher
        self.app_element: Optional[AXElement] = None
        self.main_window: Optional[AXElement] = None

    def connect(self, timeout: float = 5.0) -> bool:
        """
        连接到应用

        Args:
            timeout: 连接超时时间（秒）

        Returns:
            是否连接成功
        """
        if not self.launcher.pid:
            return False

        start_time = time.time()
        while time.time() - start_time < timeout:
            app_ref = AXUIElementCreateApplication(self.launcher.pid)
            self.app_element = AXElement(app_ref)

            # 获取主窗口
            result, main_win = AXUIElementCopyAttributeValue(app_ref, kAXMainWindowAttribute, None)
            if result == 0 and main_win:
                self.main_window = AXElement(main_win)
                return True

            # 尝试获取第一个窗口
            result, windows = AXUIElementCopyAttributeValue(app_ref, kAXWindowsAttribute, None)
            if result == 0 and windows and len(windows) > 0:
                self.main_window = AXElement(windows[0])
                return True

            time.sleep(0.5)

        return False

    def find_button(self, title: str) -> Optional[AXElement]:
        """查找按钮"""
        if not self.main_window:
            return None
        return self._find_element_by_role_and_title(self.main_window, kAXButtonRole, title)

    def find_slider(self, title: str = None) -> Optional[AXElement]:
        """查找滑块"""
        if not self.main_window:
            return None
        if title:
            return self._find_element_by_role_and_title(self.main_window, kAXSliderRole, title)
        return self.main_window.find_child_by_role(kAXSliderRole)

    def find_text_field(self, title: str = None) -> Optional[AXElement]:
        """查找文本框"""
        if not self.main_window:
            return None
        if title:
            return self._find_element_by_role_and_title(self.main_window, kAXTextFieldRole, title)
        return self.main_window.find_child_by_role(kAXTextFieldRole)

    def find_checkbox(self, title: str) -> Optional[AXElement]:
        """查找复选框"""
        if not self.main_window:
            return None
        return self._find_element_by_role_and_title(self.main_window, kAXCheckBoxRole, title)

    def _find_element_by_role_and_title(self, parent: AXElement, role: str, title: str) -> Optional[AXElement]:
        """按角色和标题查找元素"""
        for child in parent.get_children():
            if child.get_role() == role:
                if title is None or title in (child.get_title() or ""):
                    return child
            # 递归查找
            found = self._find_element_by_role_and_title(child, role, title)
            if found:
                return found
        return None

    def click_button(self, title: str) -> bool:
        """点击按钮"""
        button = self.find_button(title)
        if button:
            return button.click()
        return False

    def set_slider_value(self, title: str, value: float) -> bool:
        """设置滑块值"""
        slider = self.find_slider(title)
        if slider:
            return slider.set_value(value)
        return False

    def get_window_title(self) -> Optional[str]:
        """获取窗口标题"""
        if self.main_window:
            return self.main_window.get_title()
        return None
```

**Step 8: 创建 test_base.py**

```python
# testing/pyobjc/core/test_base.py
"""
macOS 测试基类
"""

import os
import time
from typing import List, Optional
from dataclasses import dataclass, field
from datetime import datetime

from .app_launcher import AppLauncher
from .window_controller import WindowController


@dataclass
class TestResult:
    """测试结果"""
    name: str
    passed: bool
    details: str = ""
    duration_ms: int = 0
    timestamp: str = field(default_factory=lambda: datetime.now().strftime('%H:%M:%S'))


class TestBase:
    """macOS 测试基类"""

    def __init__(self, app_path: str, test_video_path: str = None):
        """
        初始化测试基类

        Args:
            app_path: 播放器可执行文件路径
            test_video_path: 测试视频路径
        """
        self.app_path = app_path
        self.test_video_path = test_video_path
        self.launcher: Optional[AppLauncher] = None
        self.controller: Optional[WindowController] = None
        self.test_results: List[TestResult] = []
        self.current_test_name: str = ""
        self.test_start_time: float = 0

    def setup(self) -> bool:
        """
        测试准备 - 启动应用

        Returns:
            是否准备成功
        """
        print("\n" + "=" * 80)
        print("WZMediaPlayer macOS 自动化测试")
        print("=" * 80)

        # 启动应用
        self.launcher = AppLauncher(self.app_path)
        if not self.launcher.launch():
            self.start_test("启动应用")
            self.end_test(False, "应用启动失败")
            return False

        # 连接到应用
        self.controller = WindowController(self.launcher)
        if not self.controller.connect():
            self.start_test("连接应用")
            self.end_test(False, "无法连接到应用窗口")
            return False

        return True

    def teardown(self):
        """测试清理 - 关闭应用"""
        print("\n" + "=" * 80)
        print("测试完成，关闭应用")
        print("=" * 80)

        if self.launcher:
            self.launcher.quit()

    def start_test(self, test_name: str):
        """开始一个测试用例"""
        self.current_test_name = test_name
        self.test_start_time = time.time() * 1000
        print(f"\n[测试] {test_name} ...")

    def end_test(self, passed: bool, details: str = "") -> TestResult:
        """结束当前测试用例"""
        duration = int(time.time() * 1000 - self.test_start_time)

        result = TestResult(
            name=self.current_test_name,
            passed=passed,
            details=details,
            duration_ms=duration
        )

        self.test_results.append(result)

        symbol = "[PASS]" if passed else "[FAIL]"
        print(f"  [{symbol}] {self.current_test_name} ({duration}ms)")
        if details:
            print(f"      {details}")

        return result

    def wait_for(self, condition_func, timeout: float = 10.0, interval: float = 0.5, description: str = "") -> bool:
        """
        等待条件满足

        Args:
            condition_func: 条件函数
            timeout: 超时时间（秒）
            interval: 检查间隔（秒）
            description: 条件描述

        Returns:
            条件是否满足
        """
        start_time = time.time()
        while time.time() - start_time < timeout:
            try:
                if condition_func():
                    return True
            except Exception:
                pass
            time.sleep(interval)

        if description:
            print(f"    等待超时: {description}")
        return False

    def generate_report(self) -> str:
        """生成测试报告"""
        lines = []
        lines.append("=" * 80)
        lines.append("WZMediaPlayer macOS 自动化测试报告")
        lines.append("=" * 80)
        lines.append(f"测试时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        lines.append(f"测试视频: {self.test_video_path or 'N/A'}")
        lines.append(f"应用路径: {self.app_path}")
        lines.append("")

        # 统计
        total = len(self.test_results)
        passed = sum(1 for r in self.test_results if r.passed)
        failed = total - passed
        total_duration = sum(r.duration_ms for r in self.test_results)

        lines.append(f"总计: {total} | 通过: {passed} | 失败: {failed} | 总耗时: {total_duration}ms")
        lines.append("")

        # 详细结果
        lines.append("-" * 80)
        lines.append("测试详情:")
        lines.append("-" * 80)

        for i, result in enumerate(self.test_results, 1):
            symbol = "PASS" if result.passed else "FAIL"
            lines.append(f"\n{i}. [{symbol}] {result.name} ({result.duration_ms}ms)")
            if result.details:
                lines.append(f"   详情: {result.details}")

        lines.append("\n" + "=" * 80)

        return "\n".join(lines)

    def save_report(self, report: str = None):
        """保存测试报告"""
        if report is None:
            report = self.generate_report()

        try:
            timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
            filename = f"macos_test_report_{timestamp}.txt"

            script_dir = os.path.dirname(os.path.abspath(__file__))
            report_path = os.path.join(script_dir, '..', 'reports', filename)
            os.makedirs(os.path.dirname(report_path), exist_ok=True)

            with open(report_path, 'w', encoding='utf-8') as f:
                f.write(report)

            print(f"\n[报告] 测试报告已保存: {report_path}")

        except Exception as e:
            print(f"\n[错误] 保存报告失败: {e}")
```

**Step 9: 提交**

```bash
git add testing/pyobjc/
git commit -m "$(cat <<'EOF'
feat: 添加 macOS 自动测试框架基础结构

基于 PyQt + pyobjc 方案：
- AXElement: Accessibility 元素封装
- AppLauncher: 应用启动器
- WindowController: 窗口控制器
- TestBase: 测试基类

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: macOS 自动测试框架 - 基础测试用例

**Files:**
- Create: `testing/pyobjc/tests/__init__.py`
- Create: `testing/pyobjc/tests/test_basic_playback.py`
- Create: `testing/pyobjc/run_all_tests.py`

**Step 1: 创建 tests/__init__.py**

```python
# testing/pyobjc/tests/__init__.py
from .test_basic_playback import BasicPlaybackTest

__all__ = ['BasicPlaybackTest']
```

**Step 2: 创建 test_basic_playback.py**

```python
# testing/pyobjc/tests/test_basic_playback.py
"""
基础播放测试用例
"""

import os
import sys
import time

# 添加父目录到路径
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from core.test_base import TestBase
from core.config import APP_PATH, TEST_VIDEO_PATH


class BasicPlaybackTest(TestBase):
    """基础播放测试类"""

    def __init__(self):
        super().__init__(APP_PATH, TEST_VIDEO_PATH)

    def test_app_launch(self) -> bool:
        """测试应用启动"""
        self.start_test("应用启动")

        if not self.launcher or not self.launcher.is_running():
            self.end_test(False, "应用未运行")
            return False

        if not self.controller or not self.controller.main_window:
            self.end_test(False, "无法获取主窗口")
            return False

        self.end_test(True, f"窗口标题: {self.controller.get_window_title()}")
        return True

    def test_open_file_dialog(self) -> bool:
        """测试打开文件对话框"""
        self.start_test("打开文件对话框")

        # 点击打开按钮
        if not self.controller.click_button("open"):
            self.end_test(False, "未找到打开按钮")
            return False

        # 等待文件对话框出现
        time.sleep(1)

        self.end_test(True, "文件对话框已打开")
        return True

    def run_all_tests(self):
        """运行所有测试"""
        if not self.setup():
            print("测试准备失败，跳过所有测试")
            return

        try:
            self.test_app_launch()
            # 其他测试可以在这里添加
        finally:
            self.teardown()

        # 保存报告
        report = self.generate_report()
        print(report)
        self.save_report(report)


def main():
    test = BasicPlaybackTest()
    test.run_all_tests()


if __name__ == "__main__":
    main()
```

**Step 3: 创建 run_all_tests.py**

```python
# testing/pyobjc/run_all_tests.py
"""
macOS 自动测试入口
"""

import os
import sys

# 确保可以找到 core 模块
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tests.test_basic_playback import BasicPlaybackTest


def main():
    """运行所有测试"""
    print("=" * 80)
    print("WZMediaPlayer macOS 自动化测试套件")
    print("=" * 80)
    print()
    print("注意：首次运行需要在「系统偏好设置 > 安全性与隐私 > 隐私 > 辅助功能」")
    print("      中授权终端或 Python 解释器")
    print()

    test = BasicPlaybackTest()
    test.run_all_tests()


if __name__ == "__main__":
    main()
```

**Step 4: 测试框架运行**

Run: `cd /Users/jingwenlai/leadwit-tech/WeiZheng/MediaPlayer2026/testing/pyobjc && python run_all_tests.py`
Expected: 测试框架能够启动应用并执行基础测试

**Step 5: 提交**

```bash
git add testing/pyobjc/tests/ testing/pyobjc/run_all_tests.py
git commit -m "$(cat <<'EOF'
feat: 添加 macOS 测试框架基础测试用例

- test_basic_playback.py: 应用启动测试、打开文件对话框测试
- run_all_tests.py: 测试入口

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: 3D 功能修复 - 信号连接检查

**Files:**
- Modify: `WZMediaPlay/StereoVideoWidget.cpp`
- Modify: `WZMediaPlay/videoDecoder/opengl/StereoOpenGLCommon.cpp`

**Step 1: 检查 StereoVideoWidget 中的信号连接**

在 `StereoVideoWidget.cpp` 中查找 3D 模式切换的信号连接代码，确保：
1. 3D 开关信号正确连接到 `setStereoEnabled` 槽
2. 输入格式选择信号正确连接到 `setStereoInputFormat` 槽
3. 输出格式选择信号正确连接到 `setStereoOutputFormat` 槽
4. 每次切换后调用 `update()` 触发重绘

如果信号连接缺失或不完整，添加类似以下代码：

```cpp
// 在 StereoVideoWidget 构造函数或初始化函数中
void StereoVideoWidget::setupStereoConnections() {
    // 确保 3D 开关信号正确连接
    // 这里假设有对应的 UI 控件
    // 实际代码需要根据 UI 结构调整
}
```

**Step 2: 验证 StereoOpenGLCommon 中的 uniform 更新**

在 `StereoOpenGLCommon.cpp` 中确保以下方法正确更新 uniform 并触发重绘：

```cpp
void StereoOpenGLCommon::setStereoEnabled(bool enabled) {
    iStereoFlag = enabled ? 1 : 0;
    if (shaderProgramStereo && shaderProgramStereo->isLinked()) {
        shaderProgramStereo->bind();
        shaderProgramStereo->setUniformValue("iStereoFlag", iStereoFlag);
        shaderProgramStereo->release();
    }
    update();  // 触发重绘
}

void StereoOpenGLCommon::setStereoInputFormat(int format) {
    iStereoInputFormat = format;
    if (shaderProgramStereo && shaderProgramStereo->isLinked()) {
        shaderProgramStereo->bind();
        shaderProgramStereo->setUniformValue("iStereoInputFormat", iStereoInputFormat);
        shaderProgramStereo->release();
    }
    update();  // 触发重绘
}

void StereoOpenGLCommon::setStereoOutputFormat(int format) {
    iStereoOutputFormat = format;
    if (shaderProgramStereo && shaderProgramStereo->isLinked()) {
        shaderProgramStereo->bind();
        shaderProgramStereo->setUniformValue("iStereoOutputFormat", iStereoOutputFormat);
        shaderProgramStereo->release();
    }
    update();  // 触发重绘
}
```

**Step 3: 验证编译**

Run: `cd /Users/jingwenlai/leadwit-tech/WeiZheng/MediaPlayer2026 && cmake --build build 2>&1 | head -50`
Expected: 编译成功

**Step 4: 验证 3D 功能**

Run: 播放 3D 视频，切换 2D/3D 模式、输入格式、输出格式
Expected: 画面正确显示，切换后立即更新

**Step 5: 提交**

```bash
git add WZMediaPlay/StereoVideoWidget.cpp WZMediaPlay/videoDecoder/opengl/StereoOpenGLCommon.cpp
git commit -m "$(cat <<'EOF'
fix: 修复 3D 功能切换无效问题

- 确保信号连接正确传递到渲染器
- 每次切换后更新 uniform 并触发重绘

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Seeking 问题修复 - 时钟同步

**Files:**
- Modify: `WZMediaPlay/PlayController.cpp`

**Step 1: 在 Seek 完成回调中同步时钟**

找到 `onDemuxerThreadSeekFinished` 方法，确保时钟同步到目标位置：

```cpp
void PlayController::onDemuxerThreadSeekFinished(qint64 targetPts) {
    if (logger) {
        logger->info("Seek completed to PTS: {}", targetPts);
    }

    // 同步音视频时钟到目标位置
    if (audioThread_) {
        audioThread_->getClock()->setPts(targetPts);
    }
    if (videoThread_) {
        videoThread_->resetClock(targetPts);
    }

    // 重置主时钟基准
    basePts_ = targetPts;

    // 退出 Seeking 状态
    stateMachine_.exitSeeking("Seek completed successfully");
}
```

**Step 2: 验证编译**

Run: `cd /Users/jingwenlai/leadwit-tech/WeiZheng/MediaPlayer2026 && cmake --build build 2>&1 | head -50`
Expected: 编译成功

**Step 3: 验证 Seeking**

手动测试：播放视频，多次拖动进度条
Expected: 音视频同步，画面正确更新

**Step 4: 提交**

```bash
git add WZMediaPlay/PlayController.cpp
git commit -m "$(cat <<'EOF'
fix: 修复 Seek 后音视频不同步问题

在 seek 完成回调中同步音视频时钟到目标位置：
- 重置音频时钟
- 重置视频时钟
- 更新主时钟基准

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Task 6: Seeking 问题修复 - 首帧强制刷新

**Files:**
- Modify: `WZMediaPlay/PlayController.cpp`
- Modify: `WZMediaPlay/videoDecoder/VideoThread.cpp`（如需要）

**Step 1: 在 Seek 时清空渲染器**

找到 `seek` 方法，确保清空渲染器：

```cpp
bool PlayController::seek(qint64 posMs) {
    // 只有在 Playing 或 Paused 状态才能 Seek
    if (!stateMachine_.canSeek()) {
        if (logger) {
            logger->warn("Cannot seek in current state: {}", stateMachine_.getState());
        }
        return false;
    }

    stateMachine_.enterSeeking("User requested seek");

    // 清空渲染器，避免 Seek 后显示旧帧
    if (videoRenderer_) {
        videoRenderer_->clear();
    }

    // 执行 seek...
    // （后续代码保持不变）

    return true;
}
```

**Step 2: 验证编译**

Run: `cd /Users/jingwenlai/leadwit-tech/WeiZheng/MediaPlayer2026 && cmake --build build 2>&1 | head -50`
Expected: 编译成功

**Step 3: 验证**

手动测试：播放视频，快速多次拖动进度条
Expected: 画面正确更新，无残留旧帧

**Step 4: 提交**

```bash
git add WZMediaPlay/PlayController.cpp
git commit -m "$(cat <<'EOF'
fix: 修复 Seek 后画面不更新问题

在 seek 时清空渲染器，避免 Seek 后显示旧帧

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Task 7: 硬件解码修复 - 参考旧版本实现

**Files:**
- Modify: `WZMediaPlay/videoDecoder/FFDecHW.cpp`
- Reference: `v1.0.8-srcs/WZMediaPlay/FFmpegView.cc`

**Step 1: 分析旧版本硬件解码实现**

参考 `v1.0.8-srcs/WZMediaPlay/FFmpegView.cc` 中的硬件解码逻辑：
- 硬件设备上下文创建
- 硬件帧传输
- 错误处理

**Step 2: 修复 FFDecHW 初始化**

在 `FFDecHW::init()` 中确保：
1. 正确创建 `hw_device_ctx`
2. 正确设置 `codec_ctx->hw_device_ctx`
3. 添加错误处理和回退逻辑

**Step 3: 修复硬件帧传输**

在 `FFDecHW::decode()` 中确保：
1. 正确调用 `av_hwframe_transfer_data`
2. 处理传输失败的情况
3. 回退到软件解码

**Step 4: 验证编译**

Run: `cd /Users/jingwenlai/leadwit-tech/WeiZheng/MediaPlayer2026 && cmake --build build 2>&1 | head -50`
Expected: 编译成功

**Step 5: 验证硬件解码**

在 Windows 上启用硬件解码配置，播放 H.264/H.265 视频
Expected: 硬件解码正常工作，无黑屏

**Step 6: 提交**

```bash
git add WZMediaPlay/videoDecoder/FFDecHW.cpp
git commit -m "$(cat <<'EOF'
fix: 修复硬件解码黑屏问题

参考 v1.0.8 旧版本实现：
- 正确创建硬件设备上下文
- 修复硬件帧传输逻辑
- 添加错误处理和回退机制

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## 回归测试清单

- [ ] macOS 编译成功
- [ ] 程序启动不崩溃
- [ ] 色彩正常（不偏绿）
- [ ] 3D 功能切换正常
- [ ] Seeking 音视频同步
- [ ] Seeking 画面正确更新
- [ ] 硬件解码正常（Windows）
- [ ] macOS 测试框架运行正常

---

## 实施顺序

1. **Task 1: 色彩问题** - 最简单，立即见效
2. **Task 2-3: macOS 测试框架** - 为后续修复提供验证保障
3. **Task 4: 3D 功能** - 信号连接检查
4. **Task 5-6: Seeking 问题** - 时钟同步和首帧刷新
5. **Task 7: 硬件解码** - 最复杂，需要参考旧版本代码