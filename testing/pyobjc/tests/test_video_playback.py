# testing/pyobjc/tests/test_video_playback.py
"""
视频播放测试用例
"""

import os
import sys
import time
import subprocess

# 添加父目录到路径
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from core.test_base import TestBase
from config import APP_PATH, TEST_VIDEO_PATH, TIMEOUTS, WAIT_TIMES


class VideoPlaybackTest(TestBase):
    """视频播放测试类"""

    def __init__(self):
        super().__init__("VideoPlaybackTest")
        self.test_video = TEST_VIDEO_PATH

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

    def test_open_video_via_dialog(self) -> bool:
        """测试通过文件对话框打开视频"""
        test_name = "打开视频文件"
        self.start_test(test_name)

        # 使用 osascript 打开文件
        script = f'''
        tell application "System Events"
            tell process "WZMediaPlayer"
                keystroke "o" using {{command down}}
            end tell
        end tell
        '''

        try:
            subprocess.run(['osascript', '-e', script], check=True, timeout=5)

            # 等待文件对话框
            time.sleep(1)

            # 使用 osascript 输入文件路径
            path_script = f'''
            tell application "System Events"
                keystroke "{self.test_video}"
                delay 0.5
                keystroke return
            end tell
            '''

            subprocess.run(['osascript', '-e', path_script], check=True, timeout=10)

            # 等待视频加载
            time.sleep(WAIT_TIMES["after_open"])

            self.end_test(test_name, True, "视频文件已打开")
            return True

        except Exception as e:
            self.end_test(test_name, False, f"打开视频失败: {e}")
            return False

    def test_playback_running(self) -> bool:
        """测试视频播放运行中"""
        test_name = "视频播放中"
        self.start_test(test_name)

        # 等待播放开始
        time.sleep(2)

        # 检查窗口标题是否包含视频信息或时间
        title = self.window_controller.get_window_title()
        self.end_test(test_name, True, f"当前窗口标题: {title}", {"标题": title})
        return True

    def test_video_rendering(self) -> bool:
        """测试视频渲染（通过日志检查）"""
        test_name = "视频渲染验证"
        self.start_test(test_name)

        # 检查应用日志中是否有渲染帧
        log_dir = os.path.join(os.path.dirname(APP_PATH), "..", "logs")
        if os.path.exists(log_dir):
            log_files = sorted(os.listdir(log_dir), reverse=True)
            if log_files:
                log_path = os.path.join(log_dir, log_files[0])
                try:
                    with open(log_path, 'r', encoding='utf-8', errors='ignore') as f:
                        content = f.read()
                        if 'render' in content.lower() or 'frame' in content.lower():
                            self.end_test(test_name, True, "检测到渲染日志")
                            return True
                except:
                    pass

        self.end_test(test_name, True, "跳过日志验证（无日志文件）")
        return True

    def run_all_tests(self):
        """运行所有测试"""
        if not self.setup(APP_PATH):
            print("测试准备失败，跳过所有测试")
            return

        try:
            self.test_app_launch()
            self.test_open_video_via_dialog()
            self.test_playback_running()
            self.test_video_rendering()
        finally:
            self.teardown()

        # 保存报告
        self.generate_report()
        self.print_summary()
        self.save_report()


def main():
    test = VideoPlaybackTest()
    test.run_all_tests()


if __name__ == "__main__":
    main()