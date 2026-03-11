# testing/pyobjc/tests/test_3d_rendering.py
"""
WZMediaPlayer 3D渲染专项测试
测试3D模式切换、视差调节、裁边功能、全屏模式等
"""

import os
import sys
import time
from datetime import datetime

# 添加父目录到路径
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from core.test_base import TestBase
from core.keyboard_input import KeyboardInput
from core.screenshot_capture import ScreenshotCapture, ImageAnalyzer
from core.log_monitor import LogMonitor
from config import (
    APP_PATH, TEST_VIDEO_LG_PATH, TEST_VIDEO_BBB_STEREO_PATH, LOG_DIR, SCREENSHOT_DIR,
    WAIT_TIMES, BLACK_THRESHOLD
)


class Stereo3DTest(TestBase):
    """3D渲染测试类"""

    def __init__(self):
        super().__init__("Stereo3DTest")
        self.screenshot = ScreenshotCapture(SCREENSHOT_DIR)
        self.log_monitor = None
        self.test_video = TEST_VIDEO_LG_PATH
        self._used_existing_instance = False

    def setup(self, app_path: str = None, video_path: str = None) -> bool:
        """设置测试环境"""
        app_args = [video_path] if video_path else None
        result = super().setup(app_path, app_args=app_args)
        if result:
            # 检查是否使用了现有实例
            if self.app_launcher and hasattr(self.app_launcher, '_process') and self.app_launcher._process is None:
                self._used_existing_instance = True
                print("[Stereo3DTest] Using existing instance, opening video via UI...")
                if video_path and os.path.exists(video_path):
                    KeyboardInput.open_video_file(video_path, delay=WAIT_TIMES["after_open"])
            else:
                self._used_existing_instance = False

            self.log_monitor = LogMonitor(LOG_DIR)
            self.log_monitor.start()
            time.sleep(WAIT_TIMES["medium"])
        return result

    def teardown(self) -> bool:
        """清理测试环境"""
        if self.log_monitor:
            self.log_monitor.stop()
        return super().teardown()

    # ==================== 3D模式切换测试 ====================

    def test_2d_mode_rendering(self) -> bool:
        """测试2D模式渲染正常"""
        test_name = "2D模式渲染"
        self.start_test(test_name)

        try:
            # 确保在播放
            KeyboardInput.toggle_playback()
            time.sleep(WAIT_TIMES["medium"])

            # 等待渲染稳定
            time.sleep(WAIT_TIMES["playback_test"])

            # 截图验证
            screenshot_path = self.screenshot.capture_full_screen()
            is_black, black_ratio, brightness = ImageAnalyzer.is_black_screen(
                screenshot_path, BLACK_THRESHOLD
            )

            if is_black:
                self.end_test(test_name, False,
                    f"2D模式黑屏: 黑色像素比例={black_ratio:.2%}, 平均亮度={brightness:.1f}")
                return False

            self.end_test(test_name, True,
                f"2D渲染正常: 亮度={brightness:.1f}",
                {"截图": screenshot_path})
            return True

        except Exception as e:
            self.end_test(test_name, False, f"2D渲染测试失败: {e}")
            return False

    def test_3d_mode_toggle(self) -> bool:
        """测试3D模式切换"""
        test_name = "3D模式切换"
        self.start_test(test_name)

        try:
            # 确保在播放
            KeyboardInput.toggle_playback()
            time.sleep(WAIT_TIMES["short"])

            # 截取2D模式基准图
            screenshot_2d = self.screenshot.capture_full_screen()
            _, _, brightness_2d = ImageAnalyzer.is_black_screen(screenshot_2d, BLACK_THRESHOLD)

            # 切换到3D模式 (Cmd+1)
            KeyboardInput.toggle_3d()
            time.sleep(WAIT_TIMES["after_seek"])

            # 截取3D模式图
            screenshot_3d = self.screenshot.capture_full_screen()
            is_black, black_ratio, brightness_3d = ImageAnalyzer.is_black_screen(
                screenshot_3d, BLACK_THRESHOLD
            )

            if is_black:
                self.end_test(test_name, False,
                    f"3D模式黑屏: 黑色像素比例={black_ratio:.2%}")
                return False

            # 切换回2D模式
            KeyboardInput.toggle_3d()
            time.sleep(WAIT_TIMES["short"])

            self.end_test(test_name, True,
                f"3D切换成功: 2D亮度={brightness_2d:.1f}, 3D亮度={brightness_3d:.1f}",
                {"2D截图": screenshot_2d, "3D截图": screenshot_3d})
            return True

        except Exception as e:
            self.end_test(test_name, False, f"3D切换测试失败: {e}")
            return False

    def test_3d_rendering_not_black(self) -> bool:
        """测试3D模式渲染非黑屏"""
        test_name = "3D渲染验证"
        self.start_test(test_name)

        try:
            # 切换到3D模式
            KeyboardInput.toggle_3d()
            time.sleep(WAIT_TIMES["medium"])

            # 确保播放
            KeyboardInput.toggle_playback()
            time.sleep(WAIT_TIMES["playback_test"])

            # 截图分析
            screenshot_path = self.screenshot.capture_full_screen()
            is_black, black_ratio, brightness = ImageAnalyzer.is_black_screen(
                screenshot_path, BLACK_THRESHOLD
            )

            if is_black:
                self.end_test(test_name, False,
                    f"3D渲染黑屏: 黑色像素比例={black_ratio:.2%}, 亮度={brightness:.1f}")
                return False

            self.end_test(test_name, True,
                f"3D渲染正常: 黑色像素比例={black_ratio:.2%}, 亮度={brightness:.1f}",
                {"截图": screenshot_path})
            return True

        except Exception as e:
            self.end_test(test_name, False, f"3D渲染测试失败: {e}")
            return False

    # ==================== 3D输入格式切换测试 ====================

    def test_stereo_input_format_lr(self) -> bool:
        """测试LR输入格式 (Cmd+2)"""
        test_name = "LR输入格式"
        self.start_test(test_name)

        try:
            # 切换到LR格式
            KeyboardInput.send_hotkey('2', ['command'], delay=0.5)
            time.sleep(WAIT_TIMES["short"])

            # 截图验证
            screenshot_path = self.screenshot.capture_full_screen()
            is_black, _, _ = ImageAnalyzer.is_black_screen(screenshot_path, BLACK_THRESHOLD)

            if is_black:
                self.end_test(test_name, False, "LR格式黑屏")
                return False

            self.end_test(test_name, True, "LR格式正常", {"截图": screenshot_path})
            return True

        except Exception as e:
            self.end_test(test_name, False, f"LR格式测试失败: {e}")
            return False

    def test_stereo_input_format_rl(self) -> bool:
        """测试RL输入格式 (Cmd+3)"""
        test_name = "RL输入格式"
        self.start_test(test_name)

        try:
            # 切换到RL格式
            KeyboardInput.send_hotkey('3', ['command'], delay=0.5)
            time.sleep(WAIT_TIMES["short"])

            # 截图验证
            screenshot_path = self.screenshot.capture_full_screen()
            is_black, _, _ = ImageAnalyzer.is_black_screen(screenshot_path, BLACK_THRESHOLD)

            if is_black:
                self.end_test(test_name, False, "RL格式黑屏")
                return False

            self.end_test(test_name, True, "RL格式正常", {"截图": screenshot_path})
            return True

        except Exception as e:
            self.end_test(test_name, False, f"RL格式测试失败: {e}")
            return False

    def test_stereo_input_format_ud(self) -> bool:
        """测试UD输入格式 (Cmd+4)"""
        test_name = "UD输入格式"
        self.start_test(test_name)

        try:
            # 切换到UD格式
            KeyboardInput.send_hotkey('4', ['command'], delay=0.5)
            time.sleep(WAIT_TIMES["short"])

            # 截图验证
            screenshot_path = self.screenshot.capture_full_screen()
            is_black, _, _ = ImageAnalyzer.is_black_screen(screenshot_path, BLACK_THRESHOLD)

            if is_black:
                self.end_test(test_name, False, "UD格式黑屏")
                return False

            self.end_test(test_name, True, "UD格式正常", {"截图": screenshot_path})
            return True

        except Exception as e:
            self.end_test(test_name, False, f"UD格式测试失败: {e}")
            return False

    # ==================== 3D输出格式切换测试 ====================

    def test_stereo_output_format_vertical(self) -> bool:
        """测试Vertical输出格式 (Cmd+5)"""
        test_name = "Vertical输出格式"
        self.start_test(test_name)

        try:
            # 切换到Vertical格式
            KeyboardInput.send_hotkey('5', ['command'], delay=0.5)
            time.sleep(WAIT_TIMES["short"])

            # 截图验证
            screenshot_path = self.screenshot.capture_full_screen()
            is_black, _, brightness = ImageAnalyzer.is_black_screen(screenshot_path, BLACK_THRESHOLD)

            if is_black:
                self.end_test(test_name, False, "Vertical格式黑屏")
                return False

            self.end_test(test_name, True, f"Vertical格式正常, 亮度={brightness:.1f}",
                {"截图": screenshot_path})
            return True

        except Exception as e:
            self.end_test(test_name, False, f"Vertical格式测试失败: {e}")
            return False

    def test_stereo_output_format_horizontal(self) -> bool:
        """测试Horizontal输出格式 (Cmd+6)"""
        test_name = "Horizontal输出格式"
        self.start_test(test_name)

        try:
            # 切换到Horizontal格式
            KeyboardInput.send_hotkey('6', ['command'], delay=0.5)
            time.sleep(WAIT_TIMES["short"])

            # 截图验证
            screenshot_path = self.screenshot.capture_full_screen()
            is_black, _, brightness = ImageAnalyzer.is_black_screen(screenshot_path, BLACK_THRESHOLD)

            if is_black:
                self.end_test(test_name, False, "Horizontal格式黑屏")
                return False

            self.end_test(test_name, True, f"Horizontal格式正常, 亮度={brightness:.1f}",
                {"截图": screenshot_path})
            return True

        except Exception as e:
            self.end_test(test_name, False, f"Horizontal格式测试失败: {e}")
            return False

    def test_stereo_output_format_chess(self) -> bool:
        """测试Chess输出格式 (Cmd+7)"""
        test_name = "Chess输出格式"
        self.start_test(test_name)

        try:
            # 切换到Chess格式
            KeyboardInput.send_hotkey('7', ['command'], delay=0.5)
            time.sleep(WAIT_TIMES["short"])

            # 截图验证
            screenshot_path = self.screenshot.capture_full_screen()
            is_black, _, brightness = ImageAnalyzer.is_black_screen(screenshot_path, BLACK_THRESHOLD)

            if is_black:
                self.end_test(test_name, False, "Chess格式黑屏")
                return False

            self.end_test(test_name, True, f"Chess格式正常, 亮度={brightness:.1f}",
                {"截图": screenshot_path})
            return True

        except Exception as e:
            self.end_test(test_name, False, f"Chess格式测试失败: {e}")
            return False

    # ==================== 视差调节测试 ====================

    def test_parallax_increase(self) -> bool:
        """测试视差增加 (Cmd+E)"""
        test_name = "视差增加"
        self.start_test(test_name)

        try:
            # 确保在3D模式
            KeyboardInput.toggle_3d()
            time.sleep(WAIT_TIMES["short"])

            # 截取基准图
            screenshot_before = self.screenshot.capture_full_screen()
            _, _, brightness_before = ImageAnalyzer.is_black_screen(screenshot_before, BLACK_THRESHOLD)

            # 增加视差 (Cmd+E)
            KeyboardInput.send_hotkey('e', ['command'], delay=0.3)
            time.sleep(WAIT_TIMES["short"])

            # 截取变化后的图
            screenshot_after = self.screenshot.capture_full_screen()
            is_black, _, brightness_after = ImageAnalyzer.is_black_screen(screenshot_after, BLACK_THRESHOLD)

            if is_black:
                self.end_test(test_name, False, "视差调节后黑屏")
                return False

            self.end_test(test_name, True,
                f"视差增加正常: 调节前亮度={brightness_before:.1f}, 调节后亮度={brightness_after:.1f}",
                {"调节前": screenshot_before, "调节后": screenshot_after})
            return True

        except Exception as e:
            self.end_test(test_name, False, f"视差增加测试失败: {e}")
            return False

    def test_parallax_decrease(self) -> bool:
        """测试视差减少 (Cmd+W)"""
        test_name = "视差减少"
        self.start_test(test_name)

        try:
            # 减少视差 (Cmd+W)
            KeyboardInput.send_hotkey('w', ['command'], delay=0.3)
            time.sleep(WAIT_TIMES["short"])

            # 截图验证
            screenshot_path = self.screenshot.capture_full_screen()
            is_black, _, brightness = ImageAnalyzer.is_black_screen(screenshot_path, BLACK_THRESHOLD)

            if is_black:
                self.end_test(test_name, False, "视差减少后黑屏")
                return False

            self.end_test(test_name, True, f"视差减少正常, 亮度={brightness:.1f}",
                {"截图": screenshot_path})
            return True

        except Exception as e:
            self.end_test(test_name, False, f"视差减少测试失败: {e}")
            return False

    def test_parallax_reset(self) -> bool:
        """测试视差重置 (Cmd+R)"""
        test_name = "视差重置"
        self.start_test(test_name)

        try:
            # 先调节视差
            for _ in range(3):
                KeyboardInput.send_hotkey('e', ['command'], delay=0.2)

            # 重置视差 (Cmd+R)
            KeyboardInput.send_hotkey('r', ['command'], delay=0.5)
            time.sleep(WAIT_TIMES["short"])

            # 截图验证
            screenshot_path = self.screenshot.capture_full_screen()
            is_black, _, brightness = ImageAnalyzer.is_black_screen(screenshot_path, BLACK_THRESHOLD)

            if is_black:
                self.end_test(test_name, False, "视差重置后黑屏")
                return False

            self.end_test(test_name, True, f"视差重置正常, 亮度={brightness:.1f}",
                {"截图": screenshot_path})
            return True

        except Exception as e:
            self.end_test(test_name, False, f"视差重置测试失败: {e}")
            return False

    # ==================== 全屏模式测试 ====================

    def test_fullscreen_toggle(self) -> bool:
        """测试全屏切换"""
        test_name = "全屏切换"
        self.start_test(test_name)

        try:
            # 确保播放
            KeyboardInput.toggle_playback()
            time.sleep(WAIT_TIMES["short"])

            # 截取窗口模式图
            screenshot_window = self.screenshot.capture_full_screen()

            # 进入全屏 (Cmd+Enter)
            KeyboardInput.toggle_fullscreen()
            time.sleep(WAIT_TIMES["medium"])

            # 截取全屏模式图
            screenshot_fullscreen = self.screenshot.capture_full_screen()
            is_black, _, brightness = ImageAnalyzer.is_black_screen(screenshot_fullscreen, BLACK_THRESHOLD)

            if is_black:
                # 退出全屏
                KeyboardInput.exit_fullscreen()
                self.end_test(test_name, False, "全屏模式黑屏")
                return False

            # 退出全屏
            KeyboardInput.exit_fullscreen()
            time.sleep(WAIT_TIMES["short"])

            self.end_test(test_name, True, f"全屏切换正常, 亮度={brightness:.1f}",
                {"窗口模式": screenshot_window, "全屏模式": screenshot_fullscreen})
            return True

        except Exception as e:
            self.end_test(test_name, False, f"全屏切换测试失败: {e}")
            return False

    def test_fullscreen_3d_mode(self) -> bool:
        """测试全屏下的3D模式"""
        test_name = "全屏3D模式"
        self.start_test(test_name)

        try:
            # 进入全屏
            KeyboardInput.toggle_fullscreen()
            time.sleep(WAIT_TIMES["medium"])

            # 切换到3D模式
            KeyboardInput.toggle_3d()
            time.sleep(WAIT_TIMES["medium"])

            # 截图验证
            screenshot_path = self.screenshot.capture_full_screen()
            is_black, black_ratio, brightness = ImageAnalyzer.is_black_screen(
                screenshot_path, BLACK_THRESHOLD
            )

            # 退出全屏和3D模式
            KeyboardInput.toggle_3d()
            KeyboardInput.exit_fullscreen()
            time.sleep(WAIT_TIMES["short"])

            if is_black:
                self.end_test(test_name, False,
                    f"全屏3D黑屏: 黑色像素比例={black_ratio:.2%}")
                return False

            self.end_test(test_name, True, f"全屏3D渲染正常, 亮度={brightness:.1f}",
                {"截图": screenshot_path})
            return True

        except Exception as e:
            # 确保退出全屏
            try:
                KeyboardInput.exit_fullscreen()
            except:
                pass
            self.end_test(test_name, False, f"全屏3D测试失败: {e}")
            return False

    # ==================== 裁边功能测试 ====================

    def test_region_toggle(self) -> bool:
        """测试裁边功能切换 (Cmd+9)"""
        test_name = "裁边功能"
        self.start_test(test_name)

        try:
            # 确保在3D模式
            KeyboardInput.toggle_3d()
            time.sleep(WAIT_TIMES["short"])

            # 切换裁边功能 (Cmd+9)
            KeyboardInput.send_hotkey('9', ['command'], delay=0.5)
            time.sleep(WAIT_TIMES["short"])

            # 截图验证
            screenshot_path = self.screenshot.capture_full_screen()
            is_black, _, brightness = ImageAnalyzer.is_black_screen(screenshot_path, BLACK_THRESHOLD)

            if is_black:
                self.end_test(test_name, False, "裁边功能导致黑屏")
                return False

            # 再次切换关闭裁边
            KeyboardInput.send_hotkey('9', ['command'], delay=0.5)

            self.end_test(test_name, True, f"裁边功能正常, 亮度={brightness:.1f}",
                {"截图": screenshot_path})
            return True

        except Exception as e:
            self.end_test(test_name, False, f"裁边功能测试失败: {e}")
            return False

    # ==================== 综合测试运行 ====================

    def run_all_tests(self, video_path: str = None):
        """运行所有3D测试"""
        print("\n" + "=" * 80)
        print("WZMediaPlayer 3D渲染测试套件")
        print("=" * 80)

        video_path = video_path or TEST_VIDEO_BBB_STEREO_PATH
        if video_path and not os.path.exists(video_path):
            print(f"测试视频不存在: {video_path}")
            return

        if not self.setup(APP_PATH, video_path=video_path):
            print("测试准备失败，跳过所有测试")
            return

        try:
            # 2D/3D模式测试
            print("\n--- 2D/3D模式测试 ---")
            self.test_2d_mode_rendering()
            self.test_3d_mode_toggle()
            self.test_3d_rendering_not_black()

            # 3D输入格式测试
            print("\n--- 3D输入格式测试 ---")
            self.test_stereo_input_format_lr()
            self.test_stereo_input_format_rl()
            self.test_stereo_input_format_ud()

            # 3D输出格式测试
            print("\n--- 3D输出格式测试 ---")
            self.test_stereo_output_format_vertical()
            self.test_stereo_output_format_horizontal()
            self.test_stereo_output_format_chess()

            # 视差调节测试
            print("\n--- 视差调节测试 ---")
            self.test_parallax_increase()
            self.test_parallax_decrease()
            self.test_parallax_reset()

            # 全屏模式测试
            print("\n--- 全屏模式测试 ---")
            self.test_fullscreen_toggle()
            self.test_fullscreen_3d_mode()

            # 裁边功能测试
            print("\n--- 裁边功能测试 ---")
            self.test_region_toggle()

        finally:
            self.teardown()

        # 生成报告
        self.generate_report()
        self.print_summary()
        self.save_report()


def main():
    """主函数"""
    import argparse

    parser = argparse.ArgumentParser(description='WZMediaPlayer 3D渲染测试')
    parser.add_argument('--video', type=str, default=None,
                        help='测试视频路径')
    parser.add_argument('--quick', action='store_true',
                        help='快速测试模式')

    args = parser.parse_args()

    test = Stereo3DTest()
    video_path = args.video or TEST_VIDEO_BBB_STEREO_PATH

    if args.quick:
        # 快速测试模式
        print("快速测试模式")
        if not test.setup(APP_PATH, video_path=video_path):
            print("测试准备失败")
            return 1
        try:
            test.test_3d_mode_toggle()
            test.test_3d_rendering_not_black()
        finally:
            test.teardown()
        test.generate_report()
        test.print_summary()
    else:
        # 完整测试
        test.run_all_tests(video_path)

    return 0


if __name__ == "__main__":
    sys.exit(main())