# testing/pywinauto/core/app_launcher.py
"""
Windows 应用启动器
用于自动化测试中的应用启动和管理
"""

import os
import sys
import time
import subprocess

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from config import TIMEOUTS


class AppLauncher:
    """Windows 应用启动器"""

    def __init__(self, app_path: str):
        """
        初始化应用启动器

        Args:
            app_path: 应用可执行文件路径
        """
        self.app_path = app_path
        self._process = None
        self._pid = None

    def start(self, args: list = None) -> bool:
        """
        启动应用

        Args:
            args: 命令行参数

        Returns:
            是否启动成功
        """
        try:
            cmd = [self.app_path]
            if args:
                cmd.extend(args)

            self._process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                creationflags=subprocess.CREATE_NEW_CONSOLE if sys.platform == 'win32' else 0
            )
            self._pid = self._process.pid

            # 等待应用启动
            time.sleep(TIMEOUTS["app_start"] / 1000.0)

            return True

        except Exception as e:
            print(f"Failed to start application: {e}")
            return False

    def stop(self) -> bool:
        """
        停止应用

        Returns:
            是否停止成功
        """
        try:
            if self._process:
                self._process.terminate()
                try:
                    self._process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    self._process.kill()

            self._process = None
            self._pid = None
            return True

        except Exception as e:
            print(f"Failed to stop application: {e}")
            return False

    def is_running(self) -> bool:
        """
        检查应用是否正在运行

        Returns:
            是否正在运行
        """
        if self._process is None:
            return False

        return self._process.poll() is None

    def get_pid(self) -> int:
        """
        获取进程ID

        Returns:
            进程ID
        """
        return self._pid

    def get_process(self):
        """获取进程对象"""
        return self._process