# testing/pywinauto/core/window_controller.py
"""
Windows 窗口控制器
用于自动化测试中的窗口操作
"""

import os
import sys
import time
import re

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

try:
    import pywinauto
    from pywinauto import Application
    PYWINAUTO_AVAILABLE = True
except ImportError:
    PYWINAUTO_AVAILABLE = False

from config import TIMEOUTS


class WindowControllerError(Exception):
    """窗口控制器异常"""
    pass


class WindowController:
    """Windows 窗口控制器"""

    def __init__(self):
        """初始化窗口控制器"""
        self._pid = None
        self._app = None
        self._main_window = None

    def connect(self, pid: int) -> bool:
        """
        连接到应用程序

        Args:
            pid: 进程ID

        Returns:
            是否连接成功
        """
        self._pid = pid

        timeout_sec = TIMEOUTS["window_ready"] / 1000.0
        start_time = time.time()

        while time.time() - start_time < timeout_sec:
            try:
                if PYWINAUTO_AVAILABLE:
                    self._app = Application(backend="uia").connect(process=pid, timeout=5)
                    self._main_window = self._app.top_window()
                    return True
            except Exception:
                time.sleep(0.1)

        raise WindowControllerError(
            f"Failed to connect to application window within {timeout_sec} seconds"
        )

    def get_main_window(self):
        """获取主窗口"""
        return self._main_window

    def get_window_title(self) -> str:
        """获取窗口标题"""
        if self._main_window:
            return self._main_window.window_text()
        return ""

    def find_button(self, title: str = None, timeout_ms: int = None):
        """
        查找按钮

        Args:
            title: 按钮标题
            timeout_ms: 超时时间

        Returns:
            按钮元素
        """
        timeout_ms = timeout_ms or TIMEOUTS["window_ready"]
        timeout_sec = timeout_ms / 1000.0
        start_time = time.time()

        while time.time() - start_time < timeout_sec:
            try:
                if title:
                    btn = self._main_window.child_window(title=title, control_type="Button")
                else:
                    btn = self._main_window.child_window(control_type="Button")

                if btn.exists():
                    return btn
            except Exception:
                pass
            time.sleep(0.1)

        raise WindowControllerError(f"Button '{title}' not found")

    def click_button(self, title: str, timeout_ms: int = None) -> bool:
        """
        点击按钮

        Args:
            title: 按钮标题
            timeout_ms: 超时时间

        Returns:
            是否成功
        """
        btn = self.find_button(title, timeout_ms)
        if btn:
            btn.click()
            return True
        return False

    def get_all_buttons(self) -> list:
        """获取所有按钮"""
        buttons = []
        try:
            for btn in self._main_window.children(control_type="Button"):
                buttons.append(btn)
        except Exception:
            pass
        return buttons

    def get_all_sliders(self) -> list:
        """获取所有滑块"""
        sliders = []
        try:
            for slider in self._main_window.children(control_type="Slider"):
                sliders.append(slider)
        except Exception:
            pass
        return sliders

    @staticmethod
    def _time_str_to_seconds(s: str) -> int:
        """
        将时间字符串转换为秒数

        Args:
            s: 时间字符串 "HH:MM:SS"

        Returns:
            秒数
        """
        if not s or not s.strip():
            return -1
        s = s.strip()
        m = re.match(r"^(\d+):(\d{1,2}):(\d{1,2})$", s)
        if not m:
            return -1
        h, m1, sec = int(m.group(1)), int(m.group(2)), int(m.group(3))
        if m1 >= 60 or sec >= 60:
            return -1
        return h * 3600 + m1 * 60 + sec

    def get_playback_ui_state(self) -> dict:
        """
        读取当前播放相关 UI 状态

        Returns:
            状态字典
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

        if not self._main_window:
            return out

        try:
            # 收集所有文本控件
            time_pattern = re.compile(r"^\d{1,2}:\d{1,2}:\d{1,2}$")
            texts = self._main_window.children(control_type="Text")
            time_strs = []

            for text in texts:
                try:
                    title = text.window_text().strip()
                    if time_pattern.match(title):
                        time_strs.append(title)
                except Exception:
                    pass

            out["debug_time_strs"] = list(time_strs)

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

            # 获取滑块
            sliders = self.get_all_sliders()
            for slider in sliders:
                try:
                    v = slider.value()
                    max_v = slider.max_value()
                    min_v = slider.min_value()
                    out["debug_slider_raw"].append({"min": min_v, "max": max_v, "value": v})
                except Exception:
                    pass

            if out["debug_slider_raw"]:
                # 选择第一个滑块作为播放进度
                slider_data = out["debug_slider_raw"][0]
                out["slider_value"] = int(slider_data.get("value", -1))
                out["slider_max"] = int(slider_data.get("max", -1))

        except Exception as e:
            print(f"Failed to get playback UI state: {e}")

        return out