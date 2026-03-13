# testing/pywinauto/tests/test_camera.py
"""
WZMediaPlayer 摄像头功能测试 (Windows 版本)
"""

import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from core.test_base import TestBase
from core.keyboard_input import KeyboardInput
from core.screenshot_capture import ScreenshotCapture, ImageAnalyzer
from config import (
    APP_PATH, TEST_VIDEO_PATH, SCREENSHOT_DIR, WAIT_TIMES
)


class CameraTest(TestBase):
    """摄像头功能测试类"""

    def __init__(self):
        super().__init__("CameraTest")
        self.screenshot = ScreenshotCapture(SCREENSHOT_DIR)
        self.test_video = TEST_VIDEO_PATH

    def setup(self, app_path: str = None, video_path: str = None) -> bool:
        return super().setup(app_path)

    def test_open_camera(self) -> bool:
        """测试打开摄像头"""
        test_name = "打开摄像头"
        self.start_test(test_name)

        try:
            KeyboardInput.focus_app("WZMediaPlayer")

            # 打开摄像头 (Ctrl+Shift+C)
            KeyboardInput.toggle_camera()
            time.sleep(WAIT_TIMES["after_open"])

            # 截图验证
            screenshot = self.screenshot.capture_full_screen()
            brightness = ImageAnalyzer.get_average_brightness(screenshot)

            self.end_test(test_name, True,
                f"摄像头已打开, 亮度={brightness:.1f}",
                {"截图": screenshot}
            )
            return True

        except Exception as e:
            self.end_test(test_name, False, f"打开摄像头失败: {e}")
            return False

    def test_close_camera(self) -> bool:
        """测试关闭摄像头"""
        test_name = "关闭摄像头"
        self.start_test(test_name)

        try:
            KeyboardInput.focus_app("WZMediaPlayer")

            # 关闭摄像头 (Ctrl+Shift+C)
            KeyboardInput.toggle_camera()
            time.sleep(WAIT_TIMES["medium"])

            self.end_test(test_name, True, "摄像头已关闭")
            return True

        except Exception as e:
            self.end_test(test_name, False, f"关闭摄像头失败: {e}")
            return False

    def test_camera_toggle_multiple(self) -> bool:
        """测试多次切换摄像头"""
        test_name = "多次切换摄像头"
        self.start_test(test_name)

        try:
            KeyboardInput.focus_app("WZMediaPlayer")

            for i in range(3):
                KeyboardInput.toggle_camera()
                time.sleep(WAIT_TIMES["medium"])

            self.end_test(test_name, True, "多次切换完成")
            return True

        except Exception as e:
            self.end_test(test_name, False, f"多次切换失败: {e}")
            return False

    def test_video_to_camera_switch(self) -> bool:
        """测试播放-摄像头切换"""
        test_name = "播放-摄像头切换"
        self.start_test(test_name)

        try:
            KeyboardInput.focus_app("WZMediaPlayer")

            # 确保摄像头关闭
            KeyboardInput.toggle_camera()
            time.sleep(WAIT_TIMES["medium"])

            # 切换到摄像头
            KeyboardInput.toggle_camera()
            time.sleep(WAIT_TIMES["after_open"])

            # 切换回视频
            KeyboardInput.toggle_camera()
            time.sleep(WAIT_TIMES["medium"])

            self.end_test(test_name, True, "播放-摄像头切换完成")
            return True

        except Exception as e:
            self.end_test(test_name, False, f"播放-摄像头切换失败: {e}")
            return False

    def run_all_tests(self):
        """运行所有摄像头测试"""
        print("\n" + "=" * 80)
        print("WZMediaPlayer 摄像头功能测试")
        print("=" * 80)

        if not self.setup(APP_PATH):
            print("测试准备失败，跳过所有测试")
            return

        try:
            print("\n--- 摄像头测试 ---")
            self.test_open_camera()
            self.test_close_camera()
            self.test_camera_toggle_multiple()
            self.test_video_to_camera_switch()

        finally:
            self.teardown()

        self.generate_report()
        self.print_summary()
        self.save_report()


def main():
    suite = CameraTest()
    suite.run_all_tests()
    return 0 if suite._report and suite._report.get("failed", 0) == 0 else 1


if __name__ == "__main__":
    sys.exit(main())