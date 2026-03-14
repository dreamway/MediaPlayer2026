#!/usr/bin/env python3
"""
BUG-035/047 诊断测试
- BUG-035: 启动时黑屏区域应该显示 Logo
- BUG-047: Logo 显示时机问题（播放过程中 Logo 一直显示，播放结束后 GL 背景未清除）
"""

import os
import sys
import time
import subprocess
import numpy as np

# 添加父目录到路径
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from core.test_base import TestBase
from core.keyboard_input import KeyboardInput
from core.screenshot_capture import ScreenshotCapture
from core.log_monitor import LogMonitor
from config import (
    APP_PATH, TEST_VIDEO_PATH, LOG_DIR, SCREENSHOT_DIR,
    TIMEOUTS, WAIT_TIMES
)


class LogoTest(TestBase):
    """Logo 显示测试类"""

    def __init__(self):
        super().__init__("LogoTest")
        self.screenshot = ScreenshotCapture(SCREENSHOT_DIR)
        self.test_video = TEST_VIDEO_PATH

    # ==================== BUG-035: 启动时 Logo 显示测试 ====================

    def test_logo_display_at_startup(self) -> bool:
        """
        BUG-035: 测试启动时黑屏区域应该显示 Logo

        预期:
        1. 应用启动后，视频播放区域应该是黑色背景
        2. Logo 应该居中显示在黑色背景上
        3. 不应该显示白色或其他颜色
        """
        test_name = "BUG-035: 启动时 Logo 显示"
        self.start_test(test_name)

        try:
            # 先关闭现有应用
            subprocess.run(['pkill', '-x', 'WZMediaPlayer'], capture_output=True)
            time.sleep(1)

            # 启动应用（不播放视频）
            if not self.setup(APP_PATH):
                self.end_test(test_name, False, "应用启动失败")
                return False

            # 等待窗口显示和 geometry 初始化
            time.sleep(3)

            # 获取窗口位置和大小
            window = self.window_controller.get_main_window()
            if window:
                frame = window.get_frame()
                print(f"  窗口 frame: {frame}")
                x, y, w, h = frame
                if w > 0 and h > 0:
                    # 截取应用窗口
                    screenshot_path = self.screenshot.capture_region(x, y, w, h, "bug035_startup_logo.png")
                else:
                    # 如果无法获取窗口大小，使用全屏
                    screenshot_path = self.screenshot.capture_full_screen("bug035_startup_logo.png")
            else:
                screenshot_path = self.screenshot.capture_full_screen("bug035_startup_logo.png")
            print(f"  截图保存: {screenshot_path}")

            # 分析截图 - 检查顶部区域（视频播放区域）
            from PIL import Image
            img = Image.open(screenshot_path)
            img_array = np.array(img)

            # 获取窗口上半部分（播放区域）
            height, width = img_array.shape[:2]
            print(f"  截图尺寸: {width}x{height}")

            play_area = img_array[:height//2, :]

            # 计算上半部分平均颜色
            avg_color = np.mean(play_area, axis=(0, 1))
            print(f"  上半部分平均颜色 (RGB): {avg_color}")

            # 检查是否为黑色背景（允许一定的灰度范围）
            is_dark_background = avg_color[0] < 80 and avg_color[1] < 80 and avg_color[2] < 80

            # 检查是否有非黑色的内容（Logo）
            # Logo 通常会有一些非黑色的像素
            non_black_pixels = np.sum(np.any(play_area > 30, axis=2))
            total_pixels = play_area.shape[0] * play_area.shape[1]
            non_black_ratio = non_black_pixels / total_pixels
            print(f"  非黑色像素比例: {non_black_ratio:.2%}")

            # 预期：黑色背景 + Logo（非黑色像素比例应该在一定范围内）
            # Logo 应该占画面的 1%-50% 左右
            has_logo = 0.01 < non_black_ratio < 0.5

            if is_dark_background:
                if has_logo:
                    self.end_test(test_name, True, None, {
                        "背景颜色": f"RGB({avg_color[0]:.1f}, {avg_color[1]:.1f}, {avg_color[2]:.1f})",
                        "Logo像素比例": f"{non_black_ratio:.2%}",
                        "结果": "黑色背景 + Logo 显示正常"
                    })
                    return True
                else:
                    # 可能是全黑，没有 Logo
                    self.end_test(test_name, False, "黑色背景但没有检测到 Logo", {
                        "背景颜色": f"RGB({avg_color[0]:.1f}, {avg_color[1]:.1f}, {avg_color[2]:.1f})",
                        "非黑色像素比例": f"{non_black_ratio:.2%}"
                    })
                    return False
            else:
                # 不是黑色背景，可能是白色或其他问题
                self.end_test(test_name, False, f"背景不是黑色: RGB({avg_color[0]:.1f}, {avg_color[1]:.1f}, {avg_color[2]:.1f})", {
                    "非黑色像素比例": f"{non_black_ratio:.2%}"
                })
                return False

        except Exception as e:
            import traceback
            traceback.print_exc()
            self.end_test(test_name, False, f"异常: {str(e)}")
            return False
        finally:
            self.teardown()

    # ==================== BUG-047: 播放时 Logo 不应该显示 ====================

    def test_logo_hidden_during_playback(self) -> bool:
        """
        BUG-047: 测试播放过程中 Logo 应该隐藏

        预期:
        1. 播放视频时，Logo 应该隐藏
        2. 视频内容应该正常显示
        """
        test_name = "BUG-047: 播放时 Logo 隐藏"
        self.start_test(test_name)

        try:
            # 先关闭现有应用
            subprocess.run(['pkill', '-x', 'WZMediaPlayer'], capture_output=True)
            time.sleep(1)

            # 启动应用
            if not self.setup(APP_PATH):
                self.end_test(test_name, False, "应用启动失败")
                return False

            time.sleep(2)

            # 使用命令行打开视频
            KeyboardInput.open_video_file(self.test_video, delay=3)

            # 等待视频开始播放
            time.sleep(WAIT_TIMES["medium"])

            # 获取窗口位置和大小
            window = self.window_controller.get_main_window()
            if window:
                frame = window.get_frame()
                x, y, w, h = frame
                if w > 0 and h > 0:
                    screenshot_path = self.screenshot.capture_region(x, y, w, h, "bug047_during_playback.png")
                else:
                    screenshot_path = self.screenshot.capture_full_screen("bug047_during_playback.png")
            else:
                screenshot_path = self.screenshot.capture_full_screen("bug047_during_playback.png")
            print(f"  截图保存: {screenshot_path}")

            # 分析截图 - 视频播放区域应该有视频内容
            from PIL import Image
            img = Image.open(screenshot_path)
            img_array = np.array(img)

            height, width = img_array.shape[:2]
            play_area = img_array[:height//2, :]

            # 计算颜色变化
            std_color = np.std(play_area, axis=(0, 1))
            print(f"  颜色标准差: {std_color}")

            # 视频内容应该有较大的颜色变化
            has_video_content = np.mean(std_color) > 20

            if has_video_content:
                self.end_test(test_name, True, None, {
                    "颜色标准差": f"({std_color[0]:.1f}, {std_color[1]:.1f}, {std_color[2]:.1f})",
                    "结果": "视频内容正常显示"
                })
                return True
            else:
                self.end_test(test_name, False, "可能只显示 Logo 或黑屏", {
                    "颜色标准差": f"({std_color[0]:.1f}, {std_color[1]:.1f}, {std_color[2]:.1f})"
                })
                return False

        except Exception as e:
            import traceback
            traceback.print_exc()
            self.end_test(test_name, False, f"异常: {str(e)}")
            return False
        finally:
            self.teardown()

    # ==================== BUG-047: 停止后 Logo 应该显示 ====================

    def test_logo_shown_after_stop(self) -> bool:
        """
        BUG-047: 测试停止播放后 Logo 应该显示

        预期:
        1. 停止播放后，视频区域应该显示 Logo
        2. 背景应该是黑色
        """
        test_name = "BUG-047: 停止后 Logo 显示"
        self.start_test(test_name)

        try:
            # 先关闭现有应用
            subprocess.run(['pkill', '-x', 'WZMediaPlayer'], capture_output=True)
            time.sleep(1)

            # 启动应用
            if not self.setup(APP_PATH):
                self.end_test(test_name, False, "应用启动失败")
                return False

            time.sleep(2)

            # 打开视频
            KeyboardInput.open_video_file(self.test_video, delay=3)
            time.sleep(WAIT_TIMES["medium"])

            # 停止播放
            KeyboardInput.send_key("s")  # 's' 是停止快捷键
            time.sleep(WAIT_TIMES["short"])

            # 获取窗口位置和大小
            window = self.window_controller.get_main_window()
            if window:
                frame = window.get_frame()
                x, y, w, h = frame
                if w > 0 and h > 0:
                    screenshot_path = self.screenshot.capture_region(x, y, w, h, "bug047_after_stop.png")
                else:
                    screenshot_path = self.screenshot.capture_full_screen("bug047_after_stop.png")
            else:
                screenshot_path = self.screenshot.capture_full_screen("bug047_after_stop.png")
            print(f"  截图保存: {screenshot_path}")

            # 分析截图 - 应该显示 Logo
            from PIL import Image
            img = Image.open(screenshot_path)
            img_array = np.array(img)

            height, width = img_array.shape[:2]
            play_area = img_array[:height//2, :]

            avg_color = np.mean(play_area, axis=(0, 1))
            print(f"  上半部分平均颜色 (RGB): {avg_color}")

            # 检查黑色背景
            is_dark_background = avg_color[0] < 80 and avg_color[1] < 80 and avg_color[2] < 80

            # 检查 Logo
            non_black_pixels = np.sum(np.any(play_area > 30, axis=2))
            total_pixels = play_area.shape[0] * play_area.shape[1]
            non_black_ratio = non_black_pixels / total_pixels
            has_logo = 0.01 < non_black_ratio < 0.5

            if is_dark_background and has_logo:
                self.end_test(test_name, True, None, {
                    "背景颜色": f"RGB({avg_color[0]:.1f}, {avg_color[1]:.1f}, {avg_color[2]:.1f})",
                    "Logo像素比例": f"{non_black_ratio:.2%}"
                })
                return True
            else:
                self.end_test(test_name, False, "停止后未正确显示 Logo", {
                    "背景颜色": f"RGB({avg_color[0]:.1f}, {avg_color[1]:.1f}, {avg_color[2]:.1f})",
                    "Logo像素比例": f"{non_black_ratio:.2%}"
                })
                return False

        except Exception as e:
            import traceback
            traceback.print_exc()
            self.end_test(test_name, False, f"异常: {str(e)}")
            return False
        finally:
            self.teardown()


def main():
    """运行测试"""
    print("=" * 60)
    print("BUG-035/047 Logo 显示测试")
    print("=" * 60)

    test = LogoTest()

    tests = [
        ("BUG-035: 启动时 Logo 显示", test.test_logo_display_at_startup),
        ("BUG-047: 播放时 Logo 隐藏", test.test_logo_hidden_during_playback),
        ("BUG-047: 停止后 Logo 显示", test.test_logo_shown_after_stop),
    ]

    results = []
    for name, test_func in tests:
        try:
            result = test_func()
            results.append((name, "通过" if result else "失败"))
        except Exception as e:
            print(f"  ✗ 错误: {e}")
            import traceback
            traceback.print_exc()
            results.append((name, f"错误: {e}"))

    print("\n" + "=" * 60)
    print("测试结果汇总")
    print("=" * 60)
    for name, result in results:
        status = "✓" if result == "通过" else "✗"
        print(f"  {status} {name}: {result}")


if __name__ == "__main__":
    main()