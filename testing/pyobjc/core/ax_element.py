# testing/pyobjc/core/ax_element.py
"""
Accessibility Element Wrapper for macOS UI Automation
"""

import ctypes
from ctypes import Structure, c_double, POINTER

from ApplicationServices import (
    AXUIElementCopyAttributeValue,
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
    kAXMinValueAttribute,
    kAXMaxValueAttribute,
    kAXPositionAttribute,
    kAXSizeAttribute,
)


# Define CGPoint and CGSize structures for ctypes
class CGPoint(Structure):
    _fields_ = [("x", c_double), ("y", c_double)]


class CGSize(Structure):
    _fields_ = [("width", c_double), ("height", c_double)]


class AXElementError(Exception):
    """Exception raised for AXElement operations."""
    pass


class AXElement:
    """
    Wrapper class for macOS Accessibility UI Elements.
    Provides methods to interact with UI elements through the Accessibility API.
    """

    def __init__(self, ax_element):
        """
        Initialize AXElement wrapper.

        Args:
            ax_element: The underlying AXUIElementRef
        """
        self._element = ax_element

    def get_title(self) -> str:
        """
        Get the title of the element.

        Returns:
            str: The element's title, or empty string if not available.
        """
        result, value = AXUIElementCopyAttributeValue(
            self._element, kAXTitleAttribute, None
        )
        if result == 0 and value is not None:
            return str(value)
        return ""

    def get_role(self) -> str:
        """
        Get the role of the element (e.g., 'AXButton', 'AXSlider').

        Returns:
            str: The element's role, or empty string if not available.
        """
        result, value = AXUIElementCopyAttributeValue(
            self._element, kAXRoleAttribute, None
        )
        if result == 0 and value is not None:
            return str(value)
        return ""

    def get_position(self) -> tuple:
        """
        Get the position of the element (x, y).

        Returns:
            tuple: (x, y) position, or (0, 0) if not available.
        """
        result, value = AXUIElementCopyAttributeValue(
            self._element, kAXPositionAttribute, None
        )
        if result == 0 and value is not None:
            try:
                import Quartz
                # Use ctypes to properly handle the output parameter
                point = CGPoint(0.0, 0.0)
                # Get the AXValueGetValue function with proper signature
                ax_value_get_value = Quartz.AXValueGetValue
                ax_value_get_value.restype = bool
                ax_value_get_value.argtypes = [ctypes.c_void_p, ctypes.c_int, POINTER(CGPoint)]

                if ax_value_get_value(int(value), Quartz.kAXValueCGPointType, ctypes.byref(point)):
                    return (int(point.x), int(point.y))
            except (TypeError, AttributeError, ImportError, OSError) as e:
                pass
        return (0, 0)

    def get_size(self) -> tuple:
        """
        Get the size of the element (width, height).

        Returns:
            tuple: (width, height), or (0, 0) if not available.
        """
        result, value = AXUIElementCopyAttributeValue(
            self._element, kAXSizeAttribute, None
        )
        if result == 0 and value is not None:
            try:
                import Quartz
                # Use ctypes to properly handle the output parameter
                size = CGSize(0.0, 0.0)
                # Get the AXValueGetValue function with proper signature
                ax_value_get_value = Quartz.AXValueGetValue
                ax_value_get_value.restype = bool
                ax_value_get_value.argtypes = [ctypes.c_void_p, ctypes.c_int, POINTER(CGSize)]

                if ax_value_get_value(int(value), Quartz.kAXValueCGSizeType, ctypes.byref(size)):
                    return (int(size.width), int(size.height))
            except (TypeError, AttributeError, ImportError, OSError) as e:
                pass
        return (0, 0)

    def get_frame(self) -> tuple:
        """
        Get the frame of the element (x, y, width, height).

        Returns:
            tuple: (x, y, width, height), or (0, 0, 0, 0) if not available.
        """
        x, y = self.get_position()
        w, h = self.get_size()
        return (x, y, w, h)

    def get_value(self) -> str:
        """
        Get the value of the element (e.g., slider value, text field content).

        Returns:
            str: The element's value, or empty string if not available.
        """
        result, value = AXUIElementCopyAttributeValue(
            self._element, kAXValueAttribute, None
        )
        if result == 0 and value is not None:
            return str(value)
        return ""

    def get_numeric_value(self) -> float:
        """
        Get the value as number (for sliders). Returns 0.0 if not a number.
        """
        result, value = AXUIElementCopyAttributeValue(
            self._element, kAXValueAttribute, None
        )
        if result == 0 and value is not None:
            try:
                return float(value)
            except (TypeError, ValueError):
                pass
        return 0.0

    def get_min_value(self) -> float:
        """Slider/range 最小值。"""
        result, value = AXUIElementCopyAttributeValue(
            self._element, kAXMinValueAttribute, None
        )
        if result == 0 and value is not None:
            try:
                return float(value)
            except (TypeError, ValueError):
                pass
        return 0.0

    def get_max_value(self) -> float:
        """Slider/range 最大值。"""
        result, value = AXUIElementCopyAttributeValue(
            self._element, kAXMaxValueAttribute, None
        )
        if result == 0 and value is not None:
            try:
                return float(value)
            except (TypeError, ValueError):
                pass
        return 0.0

    def set_value(self, value) -> bool:
        """
        Set the value of the element.

        Args:
            value: The value to set (string for text, number for sliders).

        Returns:
            bool: True if successful, False otherwise.
        """
        result = AXUIElementSetAttributeValue(
            self._element, kAXValueAttribute, value
        )
        return result == 0

    def is_enabled(self) -> bool:
        """
        Check if the element is enabled.

        Returns:
            bool: True if enabled, False otherwise.
        """
        result, value = AXUIElementCopyAttributeValue(
            self._element, kAXEnabledAttribute, None
        )
        if result == 0 and value is not None:
            return bool(value)
        return False

    def get_children(self) -> list:
        """
        Get all child elements of this element.

        Returns:
            list: List of AXElement objects representing children.
        """
        result, value = AXUIElementCopyAttributeValue(
            self._element, kAXChildrenAttribute, None
        )
        if result == 0 and value is not None:
            children = []
            for i in range(len(value)):
                children.append(AXElement(value[i]))
            return children
        return []

    def find_child_by_title(self, title: str) -> 'AXElement':
        """
        Find a child element by its title.

        Args:
            title: The title to search for.

        Returns:
            AXElement: The found element, or None if not found.
        """
        for child in self.get_children():
            if child.get_title() == title:
                return child
            # Recursively search in children
            found = child.find_child_by_title(title)
            if found is not None:
                return found
        return None

    def find_child_by_role(self, role: str) -> 'AXElement':
        """
        Find a child element by its role.

        Args:
            role: The role to search for (e.g., 'AXButton', 'AXSlider').

        Returns:
            AXElement: The found element, or None if not found.
        """
        for child in self.get_children():
            if child.get_role() == role:
                return child
            # Recursively search in children
            found = child.find_child_by_role(role)
            if found is not None:
                return found
        return None

    def find_all_by_role(self, role: str) -> list:
        """
        Find all descendant elements with the given role.

        Args:
            role: The role to search for (e.g., 'AXStaticText', 'AXSlider').

        Returns:
            list: List of AXElement objects.
        """
        result = []
        for child in self.get_children():
            if child.get_role() == role:
                result.append(child)
            result.extend(child.find_all_by_role(role))
        return result

    def click(self) -> bool:
        """
        Perform a click/press action on the element.

        Returns:
            bool: True if successful, False otherwise.
        """
        result = AXUIElementPerformAction(self._element, kAXPressAction)
        return result == 0

    def increment(self) -> bool:
        """
        Perform an increment action on the element (e.g., slider increment).

        Returns:
            bool: True if successful, False otherwise.
        """
        result = AXUIElementPerformAction(self._element, kAXIncrementAction)
        return result == 0

    def decrement(self) -> bool:
        """
        Perform a decrement action on the element (e.g., slider decrement).

        Returns:
            bool: True if successful, False otherwise.
        """
        result = AXUIElementPerformAction(self._element, kAXDecrementAction)
        return result == 0

    def __repr__(self):
        return f"AXElement(title='{self.get_title()}', role='{self.get_role()}')"


class AXElementHelper:
    """
    Helper class for common UI element queries in WZMediaPlayer.
    提供针对 WZMediaPlayer 的常用 UI 元素查询方法。
    """

    def __init__(self):
        """Initialize helper and find WZMediaPlayer window."""
        self.app_element = None
        self.window_element = None
        self._find_app()

    def _find_app(self):
        """Find WZMediaPlayer application window."""
        from ApplicationServices import (
            AXUIElementCreateApplication,
            AXUIElementCopyAttributeValue,
            kAXChildrenAttribute,
        )

        # Find WZMediaPlayer PID
        import subprocess
        result = subprocess.run(['pgrep', '-x', 'WZMediaPlayer'], capture_output=True, text=True)
        if result.returncode != 0:
            return

        pid = int(result.stdout.strip())
        self.app_element = AXUIElementCreateApplication(pid)

        # Get main window
        result, children = AXUIElementCopyAttributeValue(
            self.app_element, kAXChildrenAttribute, None
        )
        if result == 0 and children:
            for child in children:
                elem = AXElement(child)
                if elem.get_role() == 'AXWindow':
                    self.window_element = elem
                    break

    def get_fps_value(self) -> float:
        """
        获取 FPS 显示值。

        Returns:
            float: FPS 值，如果无法获取则返回 None
        """
        if not self.window_element:
            return None

        # 查找包含 "FPS" 的静态文本
        texts = self.window_element.find_all_by_role('AXStaticText')
        for text in texts:
            value = text.get_value()
            if value and 'FPS' in value:
                # 解析 "FPS: 30.0" 格式
                try:
                    parts = value.split(':')
                    if len(parts) == 2:
                        return float(parts[1].strip())
                except (ValueError, IndexError):
                    pass
        return None

    def is_3d_mode_enabled(self) -> bool:
        """
        检查 3D 模式是否启用。

        Returns:
            bool: True 如果 3D 模式启用
        """
        if not self.window_element:
            return False

        # 查找 3D 相关的 UI 元素
        # 可以通过菜单状态或工具栏按钮状态判断
        texts = self.window_element.find_all_by_role('AXStaticText')
        for text in texts:
            value = text.get_value()
            if value and ('3D' in value or '立体' in value):
                return True
        return False

    def is_logo_visible(self) -> bool:
        """
        检查 Logo 是否可见。

        Returns:
            bool: True 如果 Logo 可见
        """
        if not self.window_element:
            return False

        # 查找 Logo 相关元素
        # 通常是一个 image 或 static text
        texts = self.window_element.find_all_by_role('AXStaticText')
        for text in texts:
            title = text.get_title()
            value = text.get_value()
            if (title and ('Logo' in title or 'WZ' in title)) or \
               (value and ('Logo' in value or 'WZ' in value)):
                return True
        return False

    def get_playbutton_state(self) -> str:
        """
        获取播放按钮状态。

        Returns:
            str: 'playing' 或 'paused' 或 'unknown'
        """
        if not self.window_element:
            return 'unknown'

        # 查找播放按钮
        buttons = self.window_element.find_all_by_role('AXButton')
        for btn in buttons:
            title = btn.get_title()
            if title and ('Play' in title or 'play' in title or '播放' in title):
                # 根据按钮状态判断
                if 'Pause' in title or 'pause' in title or '暂停' in title:
                    return 'playing'
                else:
                    return 'paused'
        return 'unknown'

    def get_volume_value(self) -> float:
        """
        获取音量滑块值。

        Returns:
            float: 音量值 (0-100)
        """
        if not self.window_element:
            return 0.0

        # 查找音量滑块
        sliders = self.window_element.find_all_by_role('AXSlider')
        for slider in sliders:
            title = slider.get_title()
            if title and ('Volume' in title or '音量' in title):
                return slider.get_numeric_value()
        return 0.0

    def find_element_by_title(self, title: str) -> AXElement:
        """
        按标题查找元素。

        Args:
            title: 元素标题

        Returns:
            AXElement 或 None
        """
        if not self.window_element:
            return None
        return self.window_element.find_child_by_title(title)

    def get_progress_value(self) -> float:
        """
        获取播放进度条值（百分比）。

        Returns:
            float: 进度百分比 (0-100)，如果无法获取则返回 None
        """
        if not self.window_element:
            return None

        # 查找进度滑块
        sliders = self.window_element.find_all_by_role('AXSlider')
        found_sliders = []
        for slider in sliders:
            max_val = slider.get_max_value()
            current_val = slider.get_numeric_value()
            found_sliders.append((max_val, current_val))
            # 进度滑块通常有较大的最大值（视频时长秒数，通常 > 60）
            # 音量滑块最大值通常是 100 或更小
            if max_val > 60:  # 假设进度滑块最大值大于60秒
                if max_val > 0:
                    return (current_val / max_val) * 100

        # 如果没有找到合适的滑块，打印调试信息
        # print(f"  [DEBUG] Found sliders: {found_sliders}")
        return None

    def get_progress_seconds(self) -> float:
        """
        获取当前播放位置（秒）。

        Returns:
            float: 当前播放秒数，如果无法获取则返回 None
        """
        if not self.window_element:
            return None

        sliders = self.window_element.find_all_by_role('AXSlider')
        for slider in sliders:
            max_val = slider.get_max_value()
            if max_val > 60:
                return slider.get_numeric_value()
        return None

    def is_play_button_checked(self) -> bool:
        """
        检查播放按钮是否处于播放状态。

        Returns:
            bool: True 如果正在播放
        """
        if not self.window_element:
            return False

        # 查找播放/暂停按钮
        buttons = self.window_element.find_all_by_role('AXButton')
        for btn in buttons:
            title = btn.get_title()
            if title and ('播放' in title or '暂停' in title or 'Play' in title or 'Pause' in title):
                # 尝试获取按钮的 value 属性（checked 状态）
                value = btn.get_value()
                # 对于 checkable 按钮，value 通常表示 checked 状态
                if value and value.lower() in ['1', 'true', 'checked']:
                    return True
                # 如果无法获取 value，尝试根据标题判断
                if title and ('暂停' in title or 'Pause' in title):
                    return True
                if title and ('播放' in title or 'Play' in title):
                    return False
        return False

    def get_stereo_output_format(self) -> int:
        """
        获取当前 3D 输出格式。

        Returns:
            int: 输出格式值（0=VERTICAL, 1=HORIZONTAL, 2=CHESS, 3=ONLY_LEFT），如果无法获取则返回 None
        """
        if not self.window_element:
            return None

        # 尝试从静态文本中解析
        texts = self.window_element.find_all_by_role('AXStaticText')
        for text in texts:
            value = text.get_value()
            if value:
                if 'ONLY_LEFT' in value or 'OnlyLeft' in value or 'only_left' in value or '仅左眼' in value:
                    return 3  # STEREO_OUTPUT_FORMAT_ONLY_LEFT
                if 'VERTICAL' in value or 'Vertical' in value or 'vertical' in value or '垂直' in value:
                    return 0  # STEREO_OUTPUT_FORMAT_VERTICAL
                if 'HORIZONTAL' in value or 'Horizontal' in value or 'horizontal' in value or '水平' in value:
                    return 1  # STEREO_OUTPUT_FORMAT_HORIZONTAL
                if 'CHESS' in value or 'Chess' in value or 'chess' in value or '棋盘' in value:
                    return 2  # STEREO_OUTPUT_FORMAT_CHESS
        return None

    def get_fullscreen_tip(self) -> str:
        """
        获取全屏提示文本。

        Returns:
            str: 全屏提示文本，如果无法获取则返回 None
        """
        if not self.window_element:
            return None

        texts = self.window_element.find_all_by_role('AXStaticText')
        for text in texts:
            value = text.get_value()
            if value and ('Fullscreen' in value or '全屏' in value):
                return value
        return None

    def get_current_time_label(self) -> str:
        """
        获取当前时间标签文本。

        Returns:
            str: 当前时间（如 "00:01:23"），如果无法获取则返回 None
        """
        if not self.window_element:
            return None

        texts = self.window_element.find_all_by_role('AXStaticText')
        for text in texts:
            value = text.get_value()
            if value and ':' in value:
                # 时间格式通常是 HH:MM:SS 或 MM:SS
                parts = value.split(':')
                if len(parts) >= 2:
                    try:
                        # 尝试解析为时间
                        for part in parts:
                            int(part.strip())
                        return value
                    except ValueError:
                        continue
        return None

    def get_total_time_label(self) -> str:
        """
        获取总时间标签文本。

        Returns:
            str: 总时间（如 "00:05:00"），如果无法获取则返回 None
        """
        if not self.window_element:
            return None

        # 通常总时间标签会显示在进度条后面，格式为 /HH:MM:SS 或类似
        texts = self.window_element.find_all_by_role('AXStaticText')
        for text in texts:
            value = text.get_value()
            if value and '/' in value:
                # 格式可能是 "00:01:23/00:05:00"
                parts = value.split('/')
                if len(parts) == 2:
                    return parts[1].strip()
        return None