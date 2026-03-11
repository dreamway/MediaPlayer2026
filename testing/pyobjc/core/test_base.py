# testing/pyobjc/core/test_base.py
"""
Base Test Framework for macOS UI Automation Testing
"""

import json
import os
import sys
import time
from dataclasses import dataclass, field
from datetime import datetime
from typing import List, Optional, Any, Callable

# 添加 pyobjc 目录到路径
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from core.app_launcher import AppLauncher
from core.window_controller import WindowController


@dataclass
class TestResult:
    """Represents the result of a single test."""
    name: str
    passed: bool
    duration_ms: float
    error_message: Optional[str] = None
    details: dict = field(default_factory=dict)


@dataclass
class TestReport:
    """Represents a complete test report."""
    test_suite: str
    start_time: str
    end_time: str
    total_tests: int
    passed: int
    failed: int
    skipped: int
    results: List[TestResult]

    def to_dict(self) -> dict:
        """Convert to dictionary for JSON serialization."""
        return {
            "test_suite": self.test_suite,
            "start_time": self.start_time,
            "end_time": self.end_time,
            "total_tests": self.total_tests,
            "passed": self.passed,
            "failed": self.failed,
            "skipped": self.skipped,
            "results": [
                {
                    "name": r.name,
                    "passed": r.passed,
                    "duration_ms": r.duration_ms,
                    "error_message": r.error_message,
                    "details": r.details
                }
                for r in self.results
            ]
        }


class TestBase:
    """
    Base class for macOS UI automation tests.
    Provides setup, teardown, and utility methods for testing.
    """

    def __init__(self, test_suite_name: str = "DefaultTestSuite"):
        """
        Initialize the test base.

        Args:
            test_suite_name: Name of the test suite for reporting.
        """
        self.test_suite_name = test_suite_name
        self.app_launcher = None
        self.window_controller = None
        self.results: List[TestResult] = []
        self._current_test_start = None
        self._report: Optional[TestReport] = None

    def setup(self, app_path: str = None, app_args: list = None) -> bool:
        """
        Set up the test environment by launching the application.

        Args:
            app_path: Optional path to the application executable.
            app_args: Optional list of arguments passed to the app.

        Returns:
            bool: True if setup successful.
        """
        try:
            self.app_launcher = AppLauncher(app_path)
            self.app_launcher.launch(app_args=app_args)

            self.window_controller = WindowController()
            self.window_controller.connect(self.app_launcher.get_pid())

            return True
        except Exception as e:
            print(f"Setup failed: {e}")
            return False

    def teardown(self) -> bool:
        """
        Tear down the test environment by quitting the application.

        Returns:
            bool: True if teardown successful.
        """
        try:
            if self.app_launcher:
                self.app_launcher.quit()
            return True
        except Exception as e:
            print(f"Teardown failed: {e}")
            return False

    def start_test(self, test_name: str):
        """
        Mark the start of a test.

        Args:
            test_name: Name of the test being started.
        """
        self._current_test_start = time.time()
        print(f"[TEST] Starting: {test_name}")

    def end_test(self, test_name: str, passed: bool, error_message: str = None, details: dict = None):
        """
        Mark the end of a test and record the result.

        Args:
            test_name: Name of the test.
            passed: Whether the test passed.
            error_message: Optional error message if test failed.
            details: Optional additional details about the test.
        """
        duration_ms = (time.time() - self._current_test_start) * 1000 if self._current_test_start else 0

        result = TestResult(
            name=test_name,
            passed=passed,
            duration_ms=duration_ms,
            error_message=error_message,
            details=details or {}
        )
        self.results.append(result)

        status = "PASSED" if passed else "FAILED"
        print(f"[TEST] {status}: {test_name} ({duration_ms:.2f}ms)")
        if (not passed) and error_message:
            print(f"        Error: {error_message}")

    def wait_for(self, condition: Callable[[], bool], timeout_ms: int = 5000, poll_interval_ms: int = 100) -> bool:
        """
        Wait for a condition to be true.

        Args:
            condition: A callable that returns True when the condition is met.
            timeout_ms: Timeout in milliseconds.
            poll_interval_ms: Polling interval in milliseconds.

        Returns:
            bool: True if condition was met, False if timeout.
        """
        timeout_sec = timeout_ms / 1000.0
        poll_interval_sec = poll_interval_ms / 1000.0
        start_time = time.time()

        while time.time() - start_time < timeout_sec:
            try:
                if condition():
                    return True
            except Exception:
                pass
            time.sleep(poll_interval_sec)

        return False

    def generate_report(self) -> TestReport:
        """
        Generate a test report from all recorded results.

        Returns:
            TestReport: The generated report.
        """
        passed = sum(1 for r in self.results if r.passed)
        failed = sum(1 for r in self.results if not r.passed)
        skipped = 0  # Not implemented yet

        self._report = TestReport(
            test_suite=self.test_suite_name,
            start_time=datetime.now().isoformat(),
            end_time=datetime.now().isoformat(),
            total_tests=len(self.results),
            passed=passed,
            failed=failed,
            skipped=skipped,
            results=self.results
        )

        return self._report

    def save_report(self, filepath: str = None) -> str:
        """
        Save the test report to a JSON file.

        Args:
            filepath: Optional path for the report file.
                      If None, uses default path.

        Returns:
            str: Path to the saved report file.
        """
        if self._report is None:
            self.generate_report()

        if filepath is None:
            # Default to reports directory
            report_dir = os.path.join(
                os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                "reports"
            )
            os.makedirs(report_dir, exist_ok=True)
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            filepath = os.path.join(
                report_dir,
                f"{self.test_suite_name}_{timestamp}.json"
            )

        with open(filepath, 'w', encoding='utf-8') as f:
            json.dump(self._report.to_dict(), f, indent=2, ensure_ascii=False)

        print(f"[REPORT] Saved to: {filepath}")
        return filepath

    def print_summary(self):
        """Print a summary of test results."""
        if self._report is None:
            self.generate_report()

        print("\n" + "=" * 50)
        print(f"Test Suite: {self._report.test_suite}")
        print(f"Total: {self._report.total_tests}")
        print(f"Passed: {self._report.passed}")
        print(f"Failed: {self._report.failed}")
        print(f"Skipped: {self._report.skipped}")
        print("=" * 50)

        if self._report.failed > 0:
            print("\nFailed Tests:")
            for result in self.results:
                if not result.passed:
                    print(f"  - {result.name}: {result.error_message}")

    def assert_true(self, condition: bool, message: str = "Assertion failed"):
        """
        Assert that a condition is true.

        Args:
            condition: The condition to check.
            message: Error message if assertion fails.

        Raises:
            AssertionError: If condition is False.
        """
        if not condition:
            raise AssertionError(message)

    def assert_equal(self, actual: Any, expected: Any, message: str = None):
        """
        Assert that two values are equal.

        Args:
            actual: The actual value.
            expected: The expected value.
            message: Optional error message.

        Raises:
            AssertionError: If values are not equal.
        """
        if actual != expected:
            msg = message or f"Expected {expected}, got {actual}"
            raise AssertionError(msg)

    def assert_not_equal(self, actual: Any, unexpected: Any, message: str = None):
        """
        Assert that two values are not equal.

        Args:
            actual: The actual value.
            unexpected: The value that should not match.
            message: Optional error message.

        Raises:
            AssertionError: If values are equal.
        """
        if actual == unexpected:
            msg = message or f"Value should not be {unexpected}"
            raise AssertionError(msg)

    def assert_contains(self, container: Any, item: Any, message: str = None):
        """
        Assert that a container contains an item.

        Args:
            container: The container to search.
            item: The item to find.
            message: Optional error message.

        Raises:
            AssertionError: If item not in container.
        """
        if item not in container:
            msg = message or f"{item} not found in container"
            raise AssertionError(msg)