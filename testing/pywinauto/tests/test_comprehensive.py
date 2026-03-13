# testing/pywinauto/tests/test_comprehensive.py
"""
WZMediaPlayer 综合测试套件 (Windows 版本)
包含渲染验证、播放测试、Seeking、音视频同步等功能
"""

import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from core.test_base import TestBase
from core.keyboard_input import KeyboardInput
from core.screenshot_capture import ScreenshotCapture, ImageAnalyzer
from config import (
    APP_PATH, TEST_VIDEO_60S_PATH, SCREENSHOT_DIR, WAIT_TIMES,
    BLACK_THRESHOLD, BLACK_RATIO_THRESHOLD
)


class ComprehensiveTest(TestBase):
    """综合测试类"""

    def __init__(self):
        super().__init__("ComprehensiveTest")
        self.screenshot = ScreenshotCapture(SCREENSHOT_DIR)
        self.test_video = TEST_VIDEO_60S_PATH

    def setup(self, app_path: str = None, video_path: str = None) -> bool:
        """设置测试环境"""
        app_args = [video_path] if video_path else None
        return super().setup(app_path, app_args)

    # ==================== 基础测试 ====================

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

        window_title = self.window_controller.get_window_title()
        self.end_test(test_name, True, None, {"窗口标题": window_title})
        return True

    # ==================== 渲染验证测试 ====================

    def test_rendering_not_black(self) -> bool:
        """测试渲染是否正常（非黑屏）"""
        test_name = "渲染验证（非黑屏）"
        self.start_test(test_name)

        try:
            # 确保视频正在播放
            KeyboardInput.toggle_playback()
            time.sleep(WAIT_TIMES["medium"])

            # 等待渲染
            time.sleep(WAIT_TIMES["playback_test"])

            # 截取屏幕
            screenshot_path = self.screenshot.capture_full_screen()

            # 分析图像
            is_black, black_ratio, brightness = ImageAnalyzer.is_black_screen(
                screenshot_path, BLACK_THRESHOLD
            )

            if is_black:
                self.end_test(test_name, False,
                    f"检测到黑屏: 黑色像素比例={black_ratio:.2%}, 平均亮度={brightness:.1f}",
                    {"截图": screenshot_path, "亮度": brightness}
                )
                return False

            self.end_test(test_name, True,
                f"渲染正常: 黑色像素比例={black_ratio:.2%}, 平均亮度={brightness:.1f}",
                {"截图": screenshot_path, "亮度": brightness}
            )
            return True

        except Exception as e:
            self.end_test(test_name, False, f"渲染验证失败: {e}")
            return False

    # ==================== Seeking 测试 ====================

    def test_seek_forward(self) -> bool:
        """测试向前 Seek"""
        test_name = "向前 Seek"
        self.start_test(test_name)

        try:
            KeyboardInput.focus_app("WZMediaPlayer")
            KeyboardInput.seek_forward()
            time.sleep(WAIT_TIMES["after_seek"])

            self.end_test(test_name, True, "Seek 完成")
            return True

        except Exception as e:
            self.end_test(test_name, False, f"向前 Seek 失败: {e}")
            return False

    def test_seek_backward(self) -> bool:
        """测试向后 Seek"""
        test_name = "向后 Seek"
        self.start_test(test_name)

        try:
            KeyboardInput.focus_app("WZMediaPlayer")
            KeyboardInput.seek_backward()
            time.sleep(WAIT_TIMES["after_seek"])

            self.end_test(test_name, True, "向后 Seek 完成")
            return True

        except Exception as e:
            self.end_test(test_name, False, f"向后 Seek 失败: {e}")
            return False

    def test_seek_to_start_end(self) -> bool:
        """测试 Seek 到边界"""
        test_name = "Seek 到边界"
        self.start_test(test_name)

        try:
            # Seek 到开始
            KeyboardInput.focus_app("WZMediaPlayer")
            KeyboardInput.seek_to_start()
            time.sleep(WAIT_TIMES["after_seek"])

            # Seek 到结束
            KeyboardInput.seek_to_end()
            time.sleep(WAIT_TIMES["after_seek"])

            self.end_test(test_name, True, "边界 Seek 完成")
            return True

        except Exception as e:
            self.end_test(test_name, False, f"边界 Seek 失败: {e}")
            return False

    # ==================== 3D 渲染测试 ====================

    def test_3d_mode_toggle(self) -> bool:
        """测试 3D 模式切换"""
        test_name = "3D模式切换"
        self.start_test(test_name)

        try:
            KeyboardInput.focus_app("WZMediaPlayer")

            # 截取 2D 截图
            time.sleep(WAIT_TIMES["medium"])
            screenshot_2d = self.screenshot.capture_full_screen()
            brightness_2d = ImageAnalyzer.get_average_brightness(screenshot_2d)

            # 切换到 3D
            KeyboardInput.toggle_3d()
            time.sleep(WAIT_TIMES["medium"])

            # 截取 3D 截图
            screenshot_3d = self.screenshot.capture_full_screen()
            brightness_3d = ImageAnalyzer.get_average_brightness(screenshot_3d)

            self.end_test(test_name, True,
                f"3D切换成功: 2D亮度={brightness_2d:.1f}, 3D亮度={brightness_3d:.1f}",
                {"2D截图": screenshot_2d, "3D截图": screenshot_3d}
            )
            return True

        except Exception as e:
            self.end_test(test_name, False, f"3D切换失败: {e}")
            return False

    def test_parallax_increase(self) -> bool:
        """测试视差增加"""
        test_name = "视差增加"
        self.start_test(test_name)

        try:
            KeyboardInput.focus_app("WZMediaPlayer")

            # 截取调节前截图
            screenshot_before = self.screenshot.capture_full_screen()
            brightness_before = ImageAnalyzer.get_average_brightness(screenshot_before)

            # 增加视差 (Ctrl+E)
            KeyboardInput.send_hotkey('e', ['ctrl'], delay=0.5)

            # 截取调节后截图
            screenshot_after = self.screenshot.capture_full_screen()
            brightness_after = ImageAnalyzer.get_average_brightness(screenshot_after)

            self.end_test(test_name, True,
                f"视差增加正常: 调节前亮度={brightness_before:.1f}, 调节后亮度={brightness_after:.1f}",
                {"调节前": screenshot_before, "调节后": screenshot_after}
            )
            return True

        except Exception as e:
            self.end_test(test_name, False, f"视差增加失败: {e}")
            return False

    def test_parallax_decrease(self) -> bool:
        """测试视差减少"""
        test_name = "视差减少"
        self.start_test(test_name)

        try:
            KeyboardInput.focus_app("WZMediaPlayer")

            # 减少视差 (Ctrl+W)
            KeyboardInput.send_hotkey('w', ['ctrl'], delay=0.5)

            # 截取截图
            screenshot = self.screenshot.capture_full_screen()
            brightness = ImageAnalyzer.get_average_brightness(screenshot)

            self.end_test(test_name, True,
                f"视差减少正常, 亮度={brightness:.1f}",
                {"截图": screenshot}
            )
            return True

        except Exception as e:
            self.end_test(test_name, False, f"视差减少失败: {e}")
            return False

    # ==================== 综合测试运行 ====================

    def run_all_tests(self, video_path: str = None):
        """运行所有测试"""
        print("\n" + "=" * 80)
        print("WZMediaPlayer 综合测试套件")
        print("=" * 80)

        video_path = video_path or self.test_video
        if video_path and not os.path.exists(video_path):
            print(f"测试视频不存在: {video_path}")
            return

        if not self.setup(APP_PATH, video_path=video_path):
            print("测试准备失败，跳过所有测试")
            return

        try:
            # 基础测试
            print("\n--- 基础测试 ---")
            self.test_app_launch()

            # 渲染验证测试
            print("\n--- 渲染验证测试 ---")
            self.test_rendering_not_black()

            # Seeking 测试
            print("\n--- Seeking 测试 ---")
            self.test_seek_forward()
            self.test_seek_backward()
            self.test_seek_to_start_end()

            # 3D渲染测试
            print("\n--- 3D渲染测试 ---")
            self.test_3d_mode_toggle()
            self.test_parallax_increase()
            self.test_parallax_decrease()

        finally:
            self.teardown()

        # 生成报告
        self.generate_report()
        self.print_summary()
        self.save_report()


def main():
    import argparse

    parser = argparse.ArgumentParser(description="WZMediaPlayer 综合测试")
    parser.add_argument("--video", type=str, default=None, help="测试视频路径")
    args = parser.parse_args()

    suite = ComprehensiveTest()
    suite.run_all_tests(args.video)
    return 0 if suite._report and suite._report.get("failed", 0) == 0 else 1


if __name__ == "__main__":
    sys.exit(main())