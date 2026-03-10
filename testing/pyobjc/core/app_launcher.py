# testing/pyobjc/core/app_launcher.py
"""
Application Launcher for macOS Applications
"""

import subprocess
import time
import sys
import os

# 添加 pyobjc 目录到路径
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from AppKit import NSWorkspace, NSRunningApplication
from ApplicationServices import (
    AXUIElementCreateApplication,
    AXUIElementCopyAttributeValue,
    kAXWindowsAttribute,
)

from config import TIMEOUTS, WAIT_TIMES, APP_PATH
from core.ax_element import AXElement


class AppLauncherError(Exception):
    """Exception raised for AppLauncher operations."""
    pass


class AppLauncher:
    """
    Handles launching and quitting macOS applications for testing.
    """

    def __init__(self, app_path: str = None):
        """
        Initialize AppLauncher.

        Args:
            app_path: Path to the application executable.
                      If None, uses the default from config.
        """
        self.app_path = app_path or APP_PATH
        self._process = None
        self._pid = None
        self._running_app = None

    def launch(self) -> bool:
        """
        Launch the application.

        Returns:
            bool: True if launch was successful.

        Raises:
            AppLauncherError: If launch fails.
        """
        try:
            # Launch the application using subprocess
            self._process = subprocess.Popen(
                [self.app_path],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                start_new_session=True
            )

            # Wait for the app to start
            start_time = time.time()
            timeout_sec = TIMEOUTS["app_start"] / 1000.0

            while time.time() - start_time < timeout_sec:
                if self.is_running():
                    # Wait a bit more for the window to be ready
                    time.sleep(WAIT_TIMES["after_open"])
                    return True
                time.sleep(0.1)

            raise AppLauncherError(
                f"Application failed to start within {timeout_sec} seconds"
            )

        except FileNotFoundError:
            raise AppLauncherError(f"Application not found at: {self.app_path}")
        except Exception as e:
            raise AppLauncherError(f"Failed to launch application: {e}")

    def quit(self) -> bool:
        """
        Quit the application.

        Returns:
            bool: True if quit was successful.
        """
        if self._running_app is not None:
            self._running_app.terminate()
            # Wait for the app to quit
            start_time = time.time()
            while time.time() - start_time < 5.0:
                if not self.is_running():
                    return True
                time.sleep(0.1)
            # Force kill if still running
            self._running_app.forceTerminate()

        if self._process is not None:
            try:
                self._process.terminate()
                self._process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self._process.kill()
            except Exception:
                pass

        self._process = None
        self._pid = None
        self._running_app = None
        return True

    def is_running(self) -> bool:
        """
        Check if the application is currently running.

        Returns:
            bool: True if the application is running.
        """
        # First check our subprocess
        if self._process is not None and self._process.poll() is None:
            self._pid = self._process.pid
            return True

        # Get the app name from path
        app_name = self.app_path.split("/")[-1]

        # Find running applications with this name
        workspace = NSWorkspace.sharedWorkspace()
        running_apps = workspace.runningApplications()

        for app in running_apps:
            if app.localizedName() == app_name or app.bundleIdentifier() == app_name:
                self._running_app = app
                self._pid = app.processIdentifier()
                return True

        return False

    def get_pid(self) -> int:
        """
        Get the process ID of the running application.

        Returns:
            int: The process ID, or None if not running.
        """
        if self._pid is None:
            self.is_running()  # This will set _pid if app is running
        return self._pid

    def _wait_for_window(self, timeout_ms: int = None) -> AXElement:
        """
        Wait for the main window to become available.

        Args:
            timeout_ms: Timeout in milliseconds.

        Returns:
            AXElement: The main window element.

        Raises:
            AppLauncherError: If window not found within timeout.
        """
        timeout_ms = timeout_ms or TIMEOUTS["window_ready"]
        timeout_sec = timeout_ms / 1000.0
        start_time = time.time()

        while time.time() - start_time < timeout_sec:
            pid = self.get_pid()
            if pid is None:
                time.sleep(0.1)
                continue

            # Create AXUIElement for the application
            ax_app = AXUIElementCreateApplication(pid)
            result, windows = AXUIElementCopyAttributeValue(
                ax_app, kAXWindowsAttribute, None
            )

            if result == 0 and windows is not None and len(windows) > 0:
                return AXElement(windows[0])

            time.sleep(0.1)

        raise AppLauncherError(
            f"No window found within {timeout_ms} milliseconds"
        )