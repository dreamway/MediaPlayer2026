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
            f"Failed to connect to application window within {timeout_sec} seconds"
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