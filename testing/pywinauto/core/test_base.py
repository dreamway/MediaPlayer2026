# testing/pywinauto/core/test_base.py
"""
Windows 自动化测试基类
"""

import os
import sys
import time
import json
from datetime import datetime
from typing import Dict, List, Optional

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from config import (
    APP_PATH, TEST_VIDEO_PATH, LOG_DIR, SCREENSHOT_DIR, REPORT_DIR,
    TIMEOUTS, WAIT_TIMES
)
from core.app_launcher import AppLauncher
from core.window_controller import WindowController
from core.screenshot_capture import ScreenshotCapture, ImageAnalyzer


class TestBase:
    """自动化测试基类"""

    def __init__(self, test_suite_name: str):
        """
        初始化测试基类

        Args:
            test_suite_name: 测试套件名称
        """
        self.test_suite_name = test_suite_name
        self.results: List[Dict] = []
        self._report: Dict = {}
        self._test_start_time: float = 0

        # 核心组件
        self.app_launcher: Optional[AppLauncher] = None
        self.window_controller: Optional[WindowController] = None
        self.screenshot = ScreenshotCapture(SCREENSHOT_DIR)

    def setup(self, app_path: str = None, app_args: list = None) -> bool:
        """
        测试准备

        Args:
            app_path: 应用路径
            app_args: 应用参数

        Returns:
            是否准备成功
        """
        app_path = app_path or APP_PATH

        # 启动应用
        self.app_launcher = AppLauncher(app_path)
        if not self.app_launcher.start(app_args):
            print("Failed to start application")
            return False

        # 连接窗口
        self.window_controller = WindowController()
        try:
            self.window_controller.connect(self.app_launcher.get_pid())
        except Exception as e:
            print(f"Failed to connect to window: {e}")
            return False

        return True

    def teardown(self) -> bool:
        """
        测试清理

        Returns:
            是否清理成功
        """
        if self.app_launcher:
            self.app_launcher.stop()

        return True

    def start_test(self, test_name: str):
        """
        开始测试

        Args:
            test_name: 测试名称
        """
        self._test_start_time = time.time() * 1000
        print(f"[TEST] Starting: {test_name}")

    def end_test(self, test_name: str, passed: bool, error_message: str = None, details: Dict = None):
        """
        结束测试

        Args:
            test_name: 测试名称
            passed: 是否通过
            error_message: 错误消息
            details: 详细信息
        """
        duration_ms = time.time() * 1000 - self._test_start_time

        result = {
            "name": test_name,
            "passed": passed,
            "duration_ms": duration_ms,
            "error_message": error_message,
            "details": details or {}
        }

        self.results.append(result)

        status = "PASSED" if passed else "FAILED"
        print(f"[TEST] {status}: {test_name} ({duration_ms:.2f}ms)")
        if error_message:
            print(f"        {error_message}")

    def generate_report(self):
        """生成测试报告"""
        total = len(self.results)
        passed = sum(1 for r in self.results if r["passed"])
        failed = total - passed

        self._report = {
            "test_suite": self.test_suite_name,
            "start_time": datetime.now().isoformat(),
            "end_time": datetime.now().isoformat(),
            "total_tests": total,
            "passed": passed,
            "failed": failed,
            "skipped": 0,
            "results": self.results
        }

    def print_summary(self):
        """打印测试摘要"""
        total = len(self.results)
        passed = sum(1 for r in self.results if r["passed"])

        print("=" * 50)
        print(f"Test Suite: {self.test_suite_name}")
        print(f"Total: {total}")
        print(f"Passed: {passed}")
        print(f"Failed: {total - passed}")
        print("=" * 50)

        failed_tests = [r for r in self.results if not r["passed"]]
        if failed_tests:
            print("\nFailed Tests:")
            for test in failed_tests:
                print(f"  - {test['name']}: {test['error_message']}")

    def save_report(self):
        """保存测试报告"""
        os.makedirs(REPORT_DIR, exist_ok=True)

        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"{self.test_suite_name}_{timestamp}.json"
        filepath = os.path.join(REPORT_DIR, filename)

        with open(filepath, 'w', encoding='utf-8') as f:
            json.dump(self._report, f, indent=2, ensure_ascii=False)

        print(f"[REPORT] Saved to: {filepath}")