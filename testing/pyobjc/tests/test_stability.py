# testing/pyobjc/tests/test_stability.py
"""
WZMediaPlayer 稳定性测试套件
包含视频切换、Seeking切换、快速操作、Monkey测试等
用于验证播放器在各种操作下的稳定性
"""

import os
import sys
import time
import random
from datetime import datetime

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from core.test_base import TestBase
from core.keyboard_input import KeyboardInput
from core.screenshot_capture import ScreenshotCapture, ImageAnalyzer
from core.log_monitor import LogMonitor
from config import (
    APP_PATH, LOG_DIR, SCREENSHOT_DIR, WAIT_TIMES, BLACK_THRESHOLD,
    TEST_VIDEO_60S_PATH, TEST_VIDEO_BBB_STEREO_PATH, TEST_VIDEO_BBB_NORMAL_PATH,
    TEST_VIDEO_SMTPTE_PATH, DEFAULT_TEST_VIDEOS
)


class StabilityTest(TestBase):
    """稳定性测试类"""

    # 测试视频列表
    TEST_VIDEOS = [
        TEST_VIDEO_60S_PATH,
        TEST_VIDEO_BBB_NORMAL_PATH,
        TEST_VIDEO_BBB_STEREO_PATH,
        TEST_VIDEO_SMTPTE_PATH,
    ]

    def __init__(self):
        super().__init__("StabilityTest")
        self.screenshot = ScreenshotCapture(SCREENSHOT_DIR)
        self.log_monitor = None
        self._used_existing_instance = False

    def setup(self, app_path: str = None, video_path: str = None) -> bool:
        """设置测试环境"""
        app_args = [video_path] if video_path else None
        result = super().setup(app_path, app_args=app_args)
        if result:
            if self.app_launcher and hasattr(self.app_launcher, '_process') and self.app_launcher._process is None:
                self._used_existing_instance = True
                print("[StabilityTest] Using existing instance, opening video via UI...")
                if video_path and os.path.exists(video_path):
                    KeyboardInput.open_video_file(video_path, delay=WAIT_TIMES["after_open"])
            else:
                self._used_existing_instance = False

            self.log_monitor = LogMonitor(LOG_DIR)
            self.log_monitor.start()
            time.sleep(WAIT_TIMES["medium"])
        return result

    def teardown(self) -> bool:
        if self.log_monitor:
            self.log_monitor.stop()
        return super().teardown()

    # ==================== 视频切换测试 ====================

    def test_video_switch_normal(self) -> bool:
        """测试正常视频切换"""
        test_name = "正常视频切换"
        self.start_test(test_name)

        try:
            videos = [v for v in self.TEST_VIDEOS if os.path.exists(v)]
            if len(videos) < 2:
                self.end_test(test_name, True, "跳过: 需要至少2个测试视频")
                return True

            switch_count = 0
            for i, video_path in enumerate(videos[:3]):  # 最多切换3个视频
                print(f"  切换到视频 {i+1}/{len(videos[:3])}: {os.path.basename(video_path)}")
                KeyboardInput.open_video_file(video_path, delay=WAIT_TIMES["after_open"])

                # 验证视频加载成功（非黑屏）
                screenshot_path = self.screenshot.capture_full_screen()
                is_black, _, brightness = ImageAnalyzer.is_black_screen(screenshot_path, BLACK_THRESHOLD)

                if is_black:
                    self.end_test(test_name, False, f"视频 {os.path.basename(video_path)} 切换后黑屏")
                    return False

                switch_count += 1
                time.sleep(1.0)

            self.end_test(test_name, True, f"成功切换 {switch_count} 个视频")
            return True

        except Exception as e:
            self.end_test(test_name, False, f"视频切换测试异常: {e}")
            return False

    def test_video_switch_after_seek(self) -> bool:
        """测试Seek后视频切换（回归测试：此前手动测试曾崩溃）"""
        test_name = "Seek后视频切换"
        self.start_test(test_name)

        try:
            videos = [v for v in self.TEST_VIDEOS if os.path.exists(v)]
            if len(videos) < 2:
                self.end_test(test_name, True, "跳过: 需要至少2个测试视频")
                return True

            # 确保播放
            KeyboardInput.focus_app("WZMediaPlayer", delay=0.3)
            KeyboardInput.toggle_playback()
            time.sleep(WAIT_TIMES["short"])

            for i in range(3):
                # 执行Seek操作
                KeyboardInput.focus_app("WZMediaPlayer", delay=0.1)
                KeyboardInput.seek_forward()
                time.sleep(0.5)

            time.sleep(WAIT_TIMES["after_seek"])

            # 切换到新视频
            video_path = videos[1] if len(videos) > 1 else videos[0]
            print(f"  Seek后切换到视频: {os.path.basename(video_path)}")
            KeyboardInput.open_video_file(video_path, delay=WAIT_TIMES["after_open"])

            # 验证新视频正常播放
            KeyboardInput.toggle_playback()
            time.sleep(WAIT_TIMES["medium"])

            screenshot_path = self.screenshot.capture_full_screen()
            is_black, _, brightness = ImageAnalyzer.is_black_screen(screenshot_path, BLACK_THRESHOLD)

            if is_black:
                self.end_test(test_name, False, "Seek后视频切换导致黑屏")
                return False

            self.end_test(test_name, True, f"Seek后视频切换成功, 亮度={brightness:.1f}")
            return True

        except Exception as e:
            self.end_test(test_name, False, f"Seek后视频切换测试异常: {e}")
            return False

    def test_video_switch_during_playback(self) -> bool:
        """测试播放中切换视频"""
        test_name = "播放中视频切换"
        self.start_test(test_name)

        try:
            videos = [v for v in self.TEST_VIDEOS if os.path.exists(v)]
            if len(videos) < 2:
                self.end_test(test_name, True, "跳过: 需要至少2个测试视频")
                return True

            # 开始播放
            KeyboardInput.focus_app("WZMediaPlayer", delay=0.3)
            KeyboardInput.toggle_playback()
            time.sleep(2.0)

            # 在播放中切换视频
            for i in range(2):
                video_path = random.choice(videos)
                print(f"  播放中切换到: {os.path.basename(video_path)}")
                KeyboardInput.open_video_file(video_path, delay=WAIT_TIMES["after_open"])

                # 继续播放
                KeyboardInput.toggle_playback()
                time.sleep(1.5)

                # 验证渲染正常
                screenshot_path = self.screenshot.capture_full_screen()
                is_black, _, _ = ImageAnalyzer.is_black_screen(screenshot_path, BLACK_THRESHOLD)
                if is_black:
                    self.end_test(test_name, False, f"播放中切换视频导致黑屏")
                    return False

            self.end_test(test_name, True, "播放中视频切换成功")
            return True

        except Exception as e:
            self.end_test(test_name, False, f"播放中视频切换测试异常: {e}")
            return False

    # ==================== 快速操作测试 ====================

    def test_rapid_seek(self) -> bool:
        """测试快速连续Seek操作"""
        test_name = "快速连续Seek"
        self.start_test(test_name)

        try:
            KeyboardInput.focus_app("WZMediaPlayer", delay=0.3)
            KeyboardInput.toggle_playback()
            time.sleep(WAIT_TIMES["short"])

            # 快速连续Seek 20次
            for i in range(20):
                KeyboardInput.focus_app("WZMediaPlayer", delay=0.05)
                KeyboardInput.seek_forward()
                time.sleep(0.1)  # 100ms间隔

            time.sleep(WAIT_TIMES["after_seek"])

            # 验证渲染正常
            screenshot_path = self.screenshot.capture_full_screen()
            is_black, _, brightness = ImageAnalyzer.is_black_screen(screenshot_path, BLACK_THRESHOLD)

            if is_black:
                self.end_test(test_name, False, "快速Seek后黑屏")
                return False

            self.end_test(test_name, True, f"快速Seek 20次后正常, 亮度={brightness:.1f}")
            return True

        except Exception as e:
            self.end_test(test_name, False, f"快速Seek测试异常: {e}")
            return False

    def test_rapid_play_pause(self) -> bool:
        """测试快速连续播放/暂停操作"""
        test_name = "快速播放/暂停"
        self.start_test(test_name)

        try:
            KeyboardInput.focus_app("WZMediaPlayer", delay=0.3)

            # 快速连续播放/暂停 15次
            for i in range(15):
                KeyboardInput.toggle_playback()
                time.sleep(0.1)  # 100ms间隔

            time.sleep(WAIT_TIMES["medium"])

            # 验证渲染正常
            screenshot_path = self.screenshot.capture_full_screen()
            is_black, _, brightness = ImageAnalyzer.is_black_screen(screenshot_path, BLACK_THRESHOLD)

            if is_black:
                self.end_test(test_name, False, "快速播放/暂停后黑屏")
                return False

            self.end_test(test_name, True, f"快速播放/暂停 15次后正常, 亮度={brightness:.1f}")
            return True

        except Exception as e:
            self.end_test(test_name, False, f"快速播放/暂停测试异常: {e}")
            return False

    def test_seek_to_boundaries(self) -> bool:
        """测试Seek到边界位置"""
        test_name = "Seek到边界"
        self.start_test(test_name)

        try:
            KeyboardInput.focus_app("WZMediaPlayer", delay=0.3)
            KeyboardInput.toggle_playback()
            time.sleep(WAIT_TIMES["short"])

            # Seek到开头
            KeyboardInput.seek_to_start()
            time.sleep(WAIT_TIMES["after_seek"])

            screenshot1 = self.screenshot.capture_full_screen()
            is_black1, _, _ = ImageAnalyzer.is_black_screen(screenshot1, BLACK_THRESHOLD)
            if is_black1:
                self.end_test(test_name, False, "Seek到开头后黑屏")
                return False

            # Seek到结尾
            KeyboardInput.seek_to_end()
            time.sleep(WAIT_TIMES["after_seek"])

            screenshot2 = self.screenshot.capture_full_screen()
            is_black2, _, brightness = ImageAnalyzer.is_black_screen(screenshot2, BLACK_THRESHOLD)
            if is_black2:
                self.end_test(test_name, False, "Seek到结尾后黑屏")
                return False

            # 再Seek到开头
            KeyboardInput.seek_to_start()
            time.sleep(WAIT_TIMES["after_seek"])

            self.end_test(test_name, True, f"边界Seek测试通过, 亮度={brightness:.1f}")
            return True

        except Exception as e:
            self.end_test(test_name, False, f"边界Seek测试异常: {e}")
            return False

    # ==================== 3D模式切换稳定性测试 ====================

    def test_3d_switch_during_playback(self) -> bool:
        """测试播放中3D模式切换"""
        test_name = "播放中3D切换"
        self.start_test(test_name)

        try:
            KeyboardInput.focus_app("WZMediaPlayer", delay=0.3)
            KeyboardInput.toggle_playback()
            time.sleep(WAIT_TIMES["short"])

            # 播放中切换3D模式
            for i in range(5):
                KeyboardInput.toggle_3d()
                time.sleep(0.5)

                # 验证渲染正常
                screenshot_path = self.screenshot.capture_full_screen()
                is_black, _, _ = ImageAnalyzer.is_black_screen(screenshot_path, BLACK_THRESHOLD)
                if is_black:
                    self.end_test(test_name, False, f"3D切换 #{i+1} 后黑屏")
                    return False

            self.end_test(test_name, True, "播放中3D切换正常")
            return True

        except Exception as e:
            self.end_test(test_name, False, f"3D切换测试异常: {e}")
            return False

    def test_seek_during_3d_mode(self) -> bool:
        """测试3D模式下Seek操作"""
        test_name = "3D模式Seek"
        self.start_test(test_name)

        try:
            # 进入3D模式
            KeyboardInput.focus_app("WZMediaPlayer", delay=0.3)
            KeyboardInput.toggle_3d()
            time.sleep(WAIT_TIMES["short"])

            KeyboardInput.toggle_playback()
            time.sleep(WAIT_TIMES["short"])

            # 3D模式下Seek
            for i in range(5):
                KeyboardInput.focus_app("WZMediaPlayer", delay=0.1)
                KeyboardInput.seek_forward()
                time.sleep(0.5)

            time.sleep(WAIT_TIMES["after_seek"])

            # 验证渲染正常
            screenshot_path = self.screenshot.capture_full_screen()
            is_black, _, brightness = ImageAnalyzer.is_black_screen(screenshot_path, BLACK_THRESHOLD)

            if is_black:
                self.end_test(test_name, False, "3D模式Seek后黑屏")
                return False

            # 切回2D模式
            KeyboardInput.toggle_3d()
            time.sleep(WAIT_TIMES["short"])

            self.end_test(test_name, True, f"3D模式Seek测试通过, 亮度={brightness:.1f}")
            return True

        except Exception as e:
            self.end_test(test_name, False, f"3D模式Seek测试异常: {e}")
            return False

    # ==================== 视差调节稳定性测试 ====================

    def test_rapid_parallax_adjust(self) -> bool:
        """测试快速视差调节"""
        test_name = "快速视差调节"
        self.start_test(test_name)

        try:
            # 进入3D模式
            KeyboardInput.focus_app("WZMediaPlayer", delay=0.3)
            KeyboardInput.toggle_3d()
            time.sleep(WAIT_TIMES["short"])

            KeyboardInput.toggle_playback()
            time.sleep(WAIT_TIMES["short"])

            # 快速调节视差
            for i in range(10):
                if i % 2 == 0:
                    KeyboardInput.send_hotkey('e', ['command'], delay=0.1)  # 增加视差
                else:
                    KeyboardInput.send_hotkey('w', ['command'], delay=0.1)  # 减少视差
                time.sleep(0.2)

            # 重置视差
            KeyboardInput.send_hotkey('r', ['command'], delay=0.3)
            time.sleep(WAIT_TIMES["short"])

            # 验证渲染正常
            screenshot_path = self.screenshot.capture_full_screen()
            is_black, _, brightness = ImageAnalyzer.is_black_screen(screenshot_path, BLACK_THRESHOLD)

            if is_black:
                self.end_test(test_name, False, "快速视差调节后黑屏")
                return False

            # 切回2D模式
            KeyboardInput.toggle_3d()
            time.sleep(WAIT_TIMES["short"])

            self.end_test(test_name, True, f"快速视差调节测试通过, 亮度={brightness:.1f}")
            return True

        except Exception as e:
            self.end_test(test_name, False, f"视差调节测试异常: {e}")
            return False

    # ==================== 压力测试 ====================

    def test_mixed_stress_operations(self) -> bool:
        """测试混合压力操作"""
        test_name = "混合压力操作"
        self.start_test(test_name)

        try:
            operations = [
                ("播放/暂停", lambda: KeyboardInput.toggle_playback()),
                ("Seek前进", lambda: KeyboardInput.seek_forward()),
                ("Seek后退", lambda: KeyboardInput.seek_backward()),
                ("音量增加", lambda: KeyboardInput.increase_volume()),
                ("音量减少", lambda: KeyboardInput.decrease_volume()),
                ("3D切换", lambda: KeyboardInput.toggle_3d()),
            ]

            KeyboardInput.focus_app("WZMediaPlayer", delay=0.3)

            # 执行30次随机操作
            for i in range(30):
                op_name, op_func = random.choice(operations)
                KeyboardInput.focus_app("WZMediaPlayer", delay=0.05)
                op_func()
                time.sleep(random.uniform(0.1, 0.3))

            time.sleep(WAIT_TIMES["medium"])

            # 验证渲染正常
            screenshot_path = self.screenshot.capture_full_screen()
            is_black, _, brightness = ImageAnalyzer.is_black_screen(screenshot_path, BLACK_THRESHOLD)

            if is_black:
                self.end_test(test_name, False, "混合压力操作后黑屏")
                return False

            self.end_test(test_name, True, f"混合压力操作测试通过, 亮度={brightness:.1f}")
            return True

        except Exception as e:
            self.end_test(test_name, False, f"混合压力操作测试异常: {e}")
            return False

    # ==================== Monkey测试 ====================

    def test_monkey_test(self, iterations: int = 50) -> bool:
        """Monkey测试：随机操作"""
        test_name = f"Monkey测试({iterations}次)"
        self.start_test(test_name)

        try:
            # 定义所有可能的操作
            all_operations = [
                ("toggle_playback", 0.3, lambda: KeyboardInput.toggle_playback()),
                ("seek_forward", 0.2, lambda: KeyboardInput.seek_forward()),
                ("seek_backward", 0.1, lambda: KeyboardInput.seek_backward()),
                ("seek_to_start", 0.05, lambda: KeyboardInput.seek_to_start()),
                ("seek_to_end", 0.05, lambda: KeyboardInput.seek_to_end()),
                ("toggle_3d", 0.1, lambda: KeyboardInput.toggle_3d()),
                ("increase_volume", 0.1, lambda: KeyboardInput.increase_volume()),
                ("decrease_volume", 0.1, lambda: KeyboardInput.decrease_volume()),
                ("toggle_mute", 0.05, lambda: KeyboardInput.toggle_mute()),
                ("parallax_increase", 0.05, lambda: KeyboardInput.send_hotkey('e', ['command'], delay=0.1)),
                ("parallax_decrease", 0.05, lambda: KeyboardInput.send_hotkey('w', ['command'], delay=0.1)),
                ("parallax_reset", 0.05, lambda: KeyboardInput.send_hotkey('r', ['command'], delay=0.1)),
            ]

            error_count = 0
            last_brightness = 0

            print(f"  执行 {iterations} 次随机操作...")

            for i in range(iterations):
                # 随机选择操作
                op_name, weight, op_func = random.choices(
                    all_operations,
                    weights=[op[1] for op in all_operations],
                    k=1
                )[0]

                try:
                    KeyboardInput.focus_app("WZMediaPlayer", delay=0.05)
                    op_func()
                    time.sleep(random.uniform(0.1, 0.3))

                    # 每10次操作检查一次渲染状态
                    if (i + 1) % 10 == 0:
                        screenshot_path = self.screenshot.capture_full_screen()
                        is_black, _, brightness = ImageAnalyzer.is_black_screen(screenshot_path, BLACK_THRESHOLD)

                        if is_black:
                            error_count += 1
                            print(f"    警告: 操作 {i+1} 后黑屏")

                        last_brightness = brightness

                except Exception as op_error:
                    print(f"    操作 {op_name} 异常: {op_error}")
                    error_count += 1

            # 验证最终状态
            screenshot_path = self.screenshot.capture_full_screen()
            is_black, _, brightness = ImageAnalyzer.is_black_screen(screenshot_path, BLACK_THRESHOLD)

            if error_count > iterations * 0.1:  # 超过10%错误率
                self.end_test(test_name, False, f"错误率过高: {error_count}/{iterations}")
                return False

            if is_black:
                self.end_test(test_name, False, f"Monkey测试后黑屏")
                return False

            self.end_test(test_name, True, f"Monkey测试通过, 错误={error_count}, 亮度={brightness:.1f}")
            return True

        except Exception as e:
            self.end_test(test_name, False, f"Monkey测试异常: {e}")
            return False

    # ==================== 音视频同步验证 ====================

    def test_av_sync_after_seek(self) -> bool:
        """测试Seek后音视频同步"""
        test_name = "Seek后音视频同步"
        self.start_test(test_name)

        try:
            KeyboardInput.focus_app("WZMediaPlayer", delay=0.3)
            KeyboardInput.toggle_playback()
            time.sleep(WAIT_TIMES["short"])

            # 多次Seek并检查同步
            for i in range(5):
                # Seek操作
                KeyboardInput.seek_forward()
                time.sleep(WAIT_TIMES["after_seek"])

                # 检查日志中的同步信息
                if self.log_monitor:
                    stats = self.log_monitor.get_stats()
                    errors = stats.get('errors', [])
                    sync_errors = [e for e in errors if 'sync' in e.lower() or 'audio' in e.lower()]

                    if sync_errors:
                        self.end_test(test_name, False, f"检测到同步错误: {sync_errors[:3]}")
                        return False

                # 验证渲染正常
                screenshot_path = self.screenshot.capture_full_screen()
                is_black, _, _ = ImageAnalyzer.is_black_screen(screenshot_path, BLACK_THRESHOLD)
                if is_black:
                    self.end_test(test_name, False, f"Seek #{i+1} 后黑屏")
                    return False

            self.end_test(test_name, True, "Seek后音视频同步正常")
            return True

        except Exception as e:
            self.end_test(test_name, False, f"同步测试异常: {e}")
            return False

    def test_pause_resume_sync(self) -> bool:
        """测试暂停/恢复后同步"""
        test_name = "暂停/恢复同步"
        self.start_test(test_name)

        try:
            KeyboardInput.focus_app("WZMediaPlayer", delay=0.3)
            KeyboardInput.toggle_playback()
            time.sleep(WAIT_TIMES["short"])

            # 多次暂停/恢复
            for i in range(5):
                # 暂停
                KeyboardInput.toggle_playback()
                time.sleep(1.0)

                # 恢复
                KeyboardInput.toggle_playback()
                time.sleep(1.5)

                # 验证渲染正常
                screenshot_path = self.screenshot.capture_full_screen()
                is_black, _, _ = ImageAnalyzer.is_black_screen(screenshot_path, BLACK_THRESHOLD)
                if is_black:
                    self.end_test(test_name, False, f"暂停/恢复 #{i+1} 后黑屏")
                    return False

            self.end_test(test_name, True, "暂停/恢复同步正常")
            return True

        except Exception as e:
            self.end_test(test_name, False, f"暂停/恢复测试异常: {e}")
            return False

    # ==================== 综合测试运行 ====================

    def run_all_tests(self, video_path: str = None):
        """运行所有稳定性测试"""
        print("\n" + "=" * 80)
        print("WZMediaPlayer 稳定性测试套件")
        print("=" * 80)

        video_path = video_path or TEST_VIDEO_60S_PATH
        if video_path and not os.path.exists(video_path):
            print(f"测试视频不存在: {video_path}")
            return

        if not self.setup(APP_PATH, video_path=video_path):
            print("测试准备失败，跳过所有测试")
            return

        try:
            # 视频切换测试
            print("\n--- 视频切换测试 ---")
            self.test_video_switch_normal()
            self.test_video_switch_after_seek()
            self.test_video_switch_during_playback()

            # 快速操作测试
            print("\n--- 快速操作测试 ---")
            self.test_rapid_seek()
            self.test_rapid_play_pause()
            self.test_seek_to_boundaries()

            # 3D模式稳定性测试
            print("\n--- 3D模式稳定性测试 ---")
            self.test_3d_switch_during_playback()
            self.test_seek_during_3d_mode()
            self.test_rapid_parallax_adjust()

            # 压力测试
            print("\n--- 压力测试 ---")
            self.test_mixed_stress_operations()
            self.test_monkey_test(iterations=50)

            # 音视频同步测试
            print("\n--- 音视频同步测试 ---")
            self.test_av_sync_after_seek()
            self.test_pause_resume_sync()

        finally:
            self.teardown()

        self.generate_report()
        self.print_summary()
        self.save_report()


def main():
    import argparse

    parser = argparse.ArgumentParser(description="WZMediaPlayer 稳定性测试")
    parser.add_argument("--video", type=str, default=None, help="测试视频路径")
    parser.add_argument("--monkey", type=int, default=50, help="Monkey测试迭代次数")
    parser.add_argument("--quick", action="store_true", help="快速测试模式")
    args = parser.parse_args()

    test = StabilityTest()

    if args.quick:
        print("快速测试模式")
        video_path = args.video or TEST_VIDEO_60S_PATH
        if not test.setup(APP_PATH, video_path=video_path):
            print("测试准备失败")
            return 1
        try:
            test.test_rapid_seek()
            test.test_monkey_test(iterations=20)
        finally:
            test.teardown()
        test.generate_report()
        test.print_summary()
    else:
        test.run_all_tests(args.video)

    return 0 if test._report and test._report.failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())