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
from config import APP_PATH, TEST_VIDEO_PATH


class BasicPlaybackTest(TestBase):
    """基础播放测试类"""

    def __init__(self):
        super().__init__("BasicPlaybackTest")

    def test_app_launch(self) -> bool:
        """测试应用启动"""
        test_name = "应用启动"
        self.start_test(test_name)

        if not self.app_launcher or not self.app_launcher.is_running():
            self.end_test(test_name, False, "应用未运行")
            return False

        if not self.window_controller or not self.window_controller.get_main_window():
            self.end_test(test_name, False, "无法获取主窗口")
            return False

        self.end_test(test_name, True, None, {"窗口标题": self.window_controller.get_window_title()})
        return True

    def test_open_file_dialog(self) -> bool:
        """测试打开文件对话框"""
        test_name = "打开文件对话框"
        self.start_test(test_name)

        # 点击打开按钮
        if not self.window_controller.click_button("open"):
            self.end_test(test_name, False, "未找到打开按钮")
            return False

        # 等待文件对话框出现
        time.sleep(1)

        self.end_test(test_name, True, "文件对话框已打开")
        return True

    def run_all_tests(self):
        """运行所有测试"""
        if not self.setup(APP_PATH):
            print("测试准备失败，跳过所有测试")
            return

        try:
            self.test_app_launch()
            # 其他测试可以在这里添加
        finally:
            self.teardown()

        # 保存报告
        self.generate_report()
        self.print_summary()
        self.save_report()


def main():
    test = BasicPlaybackTest()
    test.run_all_tests()


if __name__ == "__main__":
    main()