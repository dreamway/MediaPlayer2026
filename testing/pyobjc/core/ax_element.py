# testing/pyobjc/core/ax_element.py
"""
Accessibility Element Wrapper for macOS UI Automation
"""

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
)


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