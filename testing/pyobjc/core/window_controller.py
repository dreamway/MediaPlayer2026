# testing/pyobjc/core/window_controller.py
"""
Window Controller for macOS UI Automation
"""

import time
import sys
import os

# 添加 pyobjc 目录到路径
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from ApplicationServices import (
    AXUIElementCreateApplication,
    AXUIElementCopyAttributeValue,
    kAXWindowsAttribute,
    kAXMainWindowAttribute,
    kAXButtonRole,
    kAXSliderRole,
)

from config import TIMEOUTS
from core.ax_element import AXElement


class WindowControllerError(Exception):
    """Exception raised for WindowController operations."""
    pass


class WindowController:
    """
    Controls and interacts with application windows through Accessibility API.
    """

    def __init__(self):
        """Initialize WindowController."""
        self._pid = None
        self._ax_app = None
        self._main_window = None

    def connect(self, pid: int) -> bool:
        """
        Connect to an application by its process ID.

        Args:
            pid: The process ID of the target application.

        Returns:
            bool: True if connection successful.

        Raises:
            WindowControllerError: If connection fails.
        """
        self._pid = pid
        self._ax_app = AXUIElementCreateApplication(pid)

        # Wait for the main window to be available
        start_time = time.time()
        timeout_sec = TIMEOUTS["window_ready"] / 1000.0

        while time.time() - start_time < timeout_sec:
            # Try to get the main window
            result, main_window = AXUIElementCopyAttributeValue(
                self._ax_app, kAXMainWindowAttribute, None
            )

            if result == 0 and main_window is not None:
                self._main_window = AXElement(main_window)
                return True

            # Try to get any window
            result, windows = AXUIElementCopyAttributeValue(
                self._ax_app, kAXWindowsAttribute, None
            )

            if result == 0 and windows is not None and len(windows) > 0:
                self._main_window = AXElement(windows[0])
                return True

            time.sleep(0.1)

        raise WindowControllerError(
            "Failed to connect to application window within "
            f"{timeout_sec} seconds. "
            "可能原因：应用启动较慢/窗口尚未创建，或 macOS 未授予终端/ Python 进程“辅助功能(Accessibility)”权限。"
        )

    def find_button(self, title: str = None, timeout_ms: int = None) -> AXElement:
        """
        Find a button element in the window.

        Args:
            title: The button title to search for. If None, finds any button.
            timeout_ms: Timeout in milliseconds for the search.

        Returns:
            AXElement: The found button element.

        Raises:
            WindowControllerError: If button not found.
        """
        timeout_ms = timeout_ms or TIMEOUTS["window_ready"]
        timeout_sec = timeout_ms / 1000.0
        start_time = time.time()

        while time.time() - start_time < timeout_sec:
            if title:
                button = self._main_window.find_child_by_title(title)
            else:
                button = self._main_window.find_child_by_role(kAXButtonRole)

            if button is not None:
                return button

            time.sleep(0.1)

        raise WindowControllerError(
            f"Button '{title}' not found within {timeout_ms} milliseconds"
        )

    def find_slider(self, title: str = None, timeout_ms: int = None) -> AXElement:
        """
        Find a slider element in the window.

        Args:
            title: The slider title/label to search for. If None, finds any slider.
            timeout_ms: Timeout in milliseconds for the search.

        Returns:
            AXElement: The found slider element.

        Raises:
            WindowControllerError: If slider not found.
        """
        timeout_ms = timeout_ms or TIMEOUTS["window_ready"]
        timeout_sec = timeout_ms / 1000.0
        start_time = time.time()

        while time.time() - start_time < timeout_sec:
            if title:
                # First find by title, then check if it's a slider or has a slider child
                element = self._main_window.find_child_by_title(title)
                if element is not None:
                    if element.get_role() == kAXSliderRole:
                        return element
                    # Check if the found element has a slider child
                    slider = element.find_child_by_role(kAXSliderRole)
                    if slider is not None:
                        return slider
            else:
                slider = self._main_window.find_child_by_role(kAXSliderRole)
                if slider is not None:
                    return slider

            time.sleep(0.1)

        raise WindowControllerError(
            f"Slider '{title}' not found within {timeout_ms} milliseconds"
        )

    def click_button(self, title: str, timeout_ms: int = None) -> bool:
        """
        Find and click a button by its title.

        Args:
            title: The button title.
            timeout_ms: Timeout in milliseconds.

        Returns:
            bool: True if click was successful.

        Raises:
            WindowControllerError: If button not found or click fails.
        """
        button = self.find_button(title, timeout_ms)
        if button is None:
            raise WindowControllerError(f"Button '{title}' not found")

        if not button.is_enabled():
            raise WindowControllerError(f"Button '{title}' is not enabled")

        if not button.click():
            raise WindowControllerError(f"Failed to click button '{title}'")

        return True

    def get_window_title(self) -> str:
        """
        Get the title of the main window.

        Returns:
            str: The window title.
        """
        if self._main_window is None:
            return ""
        return self._main_window.get_title()

    def get_all_buttons(self) -> list:
        """
        Get all button elements in the window.

        Returns:
            list: List of AXElement objects representing buttons.
        """
        buttons = []

        def find_all_buttons(element):
            if element.get_role() == kAXButtonRole:
                buttons.append(element)
            for child in element.get_children():
                find_all_buttons(child)

        if self._main_window:
            find_all_buttons(self._main_window)

        return buttons

    def get_all_sliders(self) -> list:
        """
        Get all slider elements in the window.

        Returns:
            list: List of AXElement objects representing sliders.
        """
        sliders = []

        def find_all_sliders(element):
            if element.get_role() == kAXSliderRole:
                sliders.append(element)
            for child in element.get_children():
                find_all_sliders(child)

        if self._main_window:
            find_all_sliders(self._main_window)

        return sliders

    def get_main_window(self) -> AXElement:
        """
        Get the main window element.

        Returns:
            AXElement: The main window element.
        """
        return self._main_window

    # ------------------------- 播放状态 UI 读取（用于进度/按钮同步测试）-------------------------

    @staticmethod
    def _time_str_to_seconds(s: str) -> int:
        """
        将 "HH:MM:SS" 或 "H:MM:SS" 解析为秒数。

        Returns:
            int: 秒数，解析失败返回 -1。
        """
        if not s or not s.strip():
            return -1
        s = s.strip()
        import re
        m = re.match(r"^(\d+):(\d{1,2}):(\d{1,2})$", s)
        if not m:
            return -1
        h, m1, sec = int(m.group(1)), int(m.group(2)), int(m.group(3))
        if m1 >= 60 or sec >= 60:
            return -1
        return h * 3600 + m1 * 60 + sec

    def get_playback_ui_state(self) -> dict:
        """
        读取当前播放相关 UI 状态：当前时间、总时长、进度条、播放/暂停按钮状态。

        通过遍历可访问元素：时间标签（AXStaticText，格式 HH:MM:SS）、
        进度条（AXSlider）、播放/暂停按钮（AXButton，checked=正在播放）。

        Returns:
            dict: {
                "play_time_str": str,      # 当前播放时间 "00:01:23"
                "total_time_str": str,     # 总时长 "00:10:00"
                "play_time_sec": int,      # 当前秒数，-1 表示无效
                "total_time_sec": int,     # 总秒数，-1 表示无效
                "slider_value": int,       # 进度条当前值（秒），-1 表示未找到
                "slider_max": int,         # 进度条最大值（秒），-1 表示未找到
                "is_playing": bool | None # True=播放中, False=暂停, None=无法判断
            }
        """
        out = {
            "play_time_str": "",
            "total_time_str": "",
            "play_time_sec": -1,
            "total_time_sec": -1,
            "slider_value": -1,
            "slider_max": -1,
            "is_playing": None,
            "debug_time_strs": [],
            "debug_slider_raw": [],
        }
        if self._main_window is None:
            return out

        # 收集所有 AXStaticText，筛选出时间格式 "H:MM:SS" 或 "HH:MM:SS"
        import re
        time_pattern = re.compile(r"^\d{1,2}:\d{1,2}:\d{1,2}$")
        static_texts = self._main_window.find_all_by_role("AXStaticText")
        time_strs = []
        for el in static_texts:
            title = (el.get_title() or "").strip()
            if not title:
                val = (el.get_value() or "").strip()
                title = val
            if time_pattern.match(title):
                time_strs.append(title)
        out["debug_time_strs"] = list(time_strs)

        # 通常左侧为当前时间、右侧为总时长；优先按“出现顺序”取前两个，避免排序带来的误判
        if len(time_strs) >= 2:
            t_play = time_strs[0]
            t_total = time_strs[1]
            out["play_time_str"] = t_play
            out["total_time_str"] = t_total
            out["play_time_sec"] = self._time_str_to_seconds(t_play)
            out["total_time_sec"] = self._time_str_to_seconds(t_total)
        elif len(time_strs) == 1:
            t = time_strs[0]
            s = self._time_str_to_seconds(t)
            out["play_time_str"] = out["total_time_str"] = t
            out["play_time_sec"] = out["total_time_sec"] = s if s >= 0 else -1

        # 进度条：选择正确的播放进度 slider（排除音量 slider）
        # Qt 可能暴露多个 slider，需要识别哪个是播放进度 slider
        # 播放进度 slider 的 max 值通常与视频总时长接近
        sliders = self._main_window.find_all_by_role("AXSlider")
        candidates = []
        for sl in sliders:
            if not sl.is_enabled():
                continue
            v = sl.get_numeric_value()
            max_v = sl.get_max_value()
            min_v = sl.get_min_value()
            out["debug_slider_raw"].append({"min": min_v, "max": max_v, "value": v})
            candidates.append((max_v, v, sl))

        if candidates:
            # 优先选择 max 值接近 total_time_sec 的 slider
            # 如果 total_time_sec > 0，选择 max 值最接近它的 slider
            if out["total_time_sec"] > 0:
                # 按 max 值与 total_time_sec 的差距排序
                candidates.sort(key=lambda x: abs(x[0] - out["total_time_sec"]))
            else:
                # 如果没有 total_time，选择 max 值最大的 slider
                candidates.sort(key=lambda x: x[0], reverse=True)

            max_v, v, _ = candidates[0]
            out["slider_value_raw"] = int(round(v)) if v >= 0 else -1
            out["slider_max_raw"] = int(round(max_v)) if max_v and max_v > 0 else 99

            # 如果 max 值接近 total_time_sec，说明是播放进度 slider
            # 直接使用原始值（单位为秒）
            if out["total_time_sec"] > 0 and abs(max_v - out["total_time_sec"]) <= 1:
                # 播放进度 slider，值单位为秒
                out["slider_value"] = int(round(v)) if v >= 0 else -1
                out["slider_max"] = out["total_time_sec"]
            elif out["total_time_sec"] > 0 and out["slider_max_raw"] < 1000:
                # 可能是百分比 slider，转换为秒
                percentage = (v / max_v) if max_v > 0 else 0
                out["slider_value"] = int(round(percentage * out["total_time_sec"]))
                out["slider_max"] = out["total_time_sec"]
            else:
                # 其他情况，直接使用原始值
                out["slider_value"] = int(round(v)) if v >= 0 else -1
                out["slider_max"] = int(round(max_v)) if max_v and max_v > 0 else out["total_time_sec"]

        if out["slider_max"] < 0 and out["total_time_sec"] >= 0:
            out["slider_max"] = out["total_time_sec"]

        # 播放/暂停按钮：查找可检查的按钮（Qt setChecked 可能暴露为 AXValue 1/0）
        buttons = self._main_window.find_all_by_role("AXButton")
        for btn in buttons:
            title = (btn.get_title() or "").strip().lower()
            val = (btn.get_value() or "").strip()
            # 部分 Qt 版本：value "1" 表示 checked（播放中）
            if val in ("1", "true", "yes"):
                out["is_playing"] = True
                break
            if val in ("0", "false", "no"):
                out["is_playing"] = False
                break
            if "play" in title or "pause" in title or "播放" in title or "暂停" in title:
                out["is_playing"] = "pause" not in title and "暂停" not in title
                break
        # 若未从按钮判断，根据时间是否在变化可后续由测试逻辑推断
        return out