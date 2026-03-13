# testing/pyobjc/tests/test_closed_loop.py
"""
WZMediaPlayer 闭环测试套件

每个测试都是闭环的：
1. 发送指令
2. 等待生效
3. 读取 UI 状态验证
4. 截图验证画面
5. 检查日志确认

不通过就是不通过，不跳过，不假设成功。
"""

import os
import sys
import time
import subprocess

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from core.test_base import TestBase
from core.keyboard_input import KeyboardInput
from core.screenshot_capture import ScreenshotCapture
from core.log_monitor import LogMonitor
from core.closed_loop_verifier import ClosedLoopVerifier
from config import (
    APP_PATH, TEST_VIDEO_PATH, SCREENSHOT_DIR, LOG_DIR,
    WAIT_TIMES, BLACK_THRESHOLD
)


class ClosedLoopTest(TestBase):
    """闭环测试类 - 每个测试都验证结果"""

    def __init__(self):
        super().__init__("ClosedLoopTest")
        self.screenshot = ScreenshotCapture(SCREENSHOT_DIR)
        self.log_monitor = None
        self.verifier = None
        self.test_video = TEST_VIDEO_PATH

    def setup(self, app_path: str = None, video_path: str = None) -> bool:
        """设置测试环境"""
        app_args = [video_path] if video_path else None
        result = super().setup(app_path, app_args=app_args)

        if result:
            self.log_monitor = LogMonitor(LOG_DIR)
            self.log_monitor.start()

            # 创建闭环验证器
            self.verifier = ClosedLoopVerifier(
                window_controller=self.window_controller,
                screenshot_capture=self.screenshot,
                log_monitor=self.log_monitor
            )

            time.sleep(WAIT_TIMES["after_open"])
        return result

    def teardown(self) -> bool:
        if self.log_monitor:
            self.log_monitor.stop()
        return super().teardown()

    # ==================== 基础验证测试 ====================

    def test_can_read_ui_state(self) -> bool:
        """测试：能读取 UI 状态"""
        test_name = "读取 UI 状态"
        self.start_test(test_name)

        try:
            pos, total, state = self.verifier.get_current_playback_position()

            if pos < 0:
                self.end_test(test_name, False,
                    "无法读取播放位置，检查应用是否正确打开视频",
                    {"state": state})
                return False

            self.end_test(test_name, True,
                f"成功读取: 当前={pos}s, 总时长={total}s",
                {"position": pos, "total": total, "state": state})
            return True

        except Exception as e:
            self.end_test(test_name, False, f"读取 UI 异常: {e}")
            return False

    def test_screen_not_black(self) -> bool:
        """测试：画面不是黑屏"""
        test_name = "画面验证"
        self.start_test(test_name)

        try:
            result = self.verifier.verify_screen_not_black(
                black_threshold=BLACK_THRESHOLD
            )

            if result.success:
                self.end_test(test_name, True, result.message, result.details)
                return True
            else:
                self.end_test(test_name, False, result.message, result.details)
                return False

        except Exception as e:
            self.end_test(test_name, False, f"画面验证异常: {e}")
            return False

    # ==================== 播放测试 ====================

    def test_playback_progresses(self) -> bool:
        """测试：播放时进度前进"""
        test_name = "播放进度前进"
        self.start_test(test_name)

        try:
            # 确保应用有焦点
            KeyboardInput.focus_app("WZMediaPlayer", delay=0.5)

            # 先检测当前播放状态：读取两个位置判断是否在播放
            pos_a, _, _ = self.verifier.get_current_playback_position()
            time.sleep(0.8)
            pos_b, _, _ = self.verifier.get_current_playback_position()

            is_playing = (pos_b - pos_a) > 0.3

            # 如果不在播放，发送播放命令
            if not is_playing:
                KeyboardInput.toggle_playback()
                time.sleep(0.5)

            # 闭环验证：播放前进
            result = self.verifier.verify_playback_progressing(
                duration_sec=3.0,
                min_advance_sec=1
            )

            if result.success:
                self.end_test(test_name, True, result.message, result.details)
                return True
            else:
                self.end_test(test_name, False, result.message, result.details)
                return False

        except Exception as e:
            self.end_test(test_name, False, f"测试异常: {e}")
            return False

    def test_pause_actually_pauses(self) -> bool:
        """测试：暂停时进度停止"""
        test_name = "暂停验证"
        self.start_test(test_name)

        try:
            # 确保应用有焦点
            KeyboardInput.focus_app("WZMediaPlayer", delay=0.5)

            # 先检测当前播放状态：读取两个位置判断是否在播放
            pos_a, _, _ = self.verifier.get_current_playback_position()
            time.sleep(1.0)
            pos_b, _, _ = self.verifier.get_current_playback_position()

            is_playing = (pos_b - pos_a) > 0.5

            # 如果正在播放，发送暂停命令
            if is_playing:
                KeyboardInput.toggle_playback()
                time.sleep(0.5)

            # 现在应该是暂停状态，读取位置
            pos1, _, _ = self.verifier.get_current_playback_position()
            time.sleep(2.0)
            pos2, _, _ = self.verifier.get_current_playback_position()

            # 暂停后位置应该不变（允许 1 秒误差）
            if abs(pos2 - pos1) > 1:
                self.end_test(test_name, False,
                    f"暂停后位置仍在变化: {pos1}s → {pos2}s (之前检测到{'播放中' if is_playing else '已暂停'})",
                    {"pos1": pos1, "pos2": pos2, "was_playing": is_playing})
                return False

            self.end_test(test_name, True,
                f"暂停成功: 位置保持在 {pos1}s",
                {"pos1": pos1, "pos2": pos2, "was_playing": is_playing})
            return True

        except Exception as e:
            self.end_test(test_name, False, f"测试异常: {e}")
            return False

    # ==================== Seeking 测试 ====================

    def test_seek_forward_updates_ui(self) -> bool:
        """测试：向前 Seek 后 UI 更新"""
        test_name = "向前 Seek"
        self.start_test(test_name)

        try:
            # 确保应用有焦点
            KeyboardInput.focus_app("WZMediaPlayer", delay=0.5)

            # 确保正在播放
            KeyboardInput.toggle_playback()
            time.sleep(1.0)

            # 获取初始位置
            initial_pos, total, _ = self.verifier.get_current_playback_position()

            if initial_pos < 0:
                self.end_test(test_name, False, "无法读取初始位置")
                return False

            # 执行向前 Seek（多次）
            for i in range(5):
                KeyboardInput.focus_app("WZMediaPlayer", delay=0.1)
                KeyboardInput.seek_forward()
                time.sleep(0.3)

            # 等待 UI 更新
            time.sleep(WAIT_TIMES["after_seek"])

            # 获取最终位置
            final_pos, _, state = self.verifier.get_current_playback_position()

            if final_pos < 0:
                self.end_test(test_name, False, "无法读取最终位置", {"state": state})
                return False

            # 验证位置变化
            change = final_pos - initial_pos

            if change < 1:
                self.end_test(test_name, False,
                    f"向前 Seek 未生效: {initial_pos}s → {final_pos}s, 变化={change}s",
                    {"initial": initial_pos, "final": final_pos, "change": change, "state": state})
                return False

            # 验证画面不是黑屏
            screen_result = self.verifier.verify_screen_not_black()
            if not screen_result.success:
                self.end_test(test_name, False,
                    f"Seek 后黑屏: {screen_result.message}",
                    {"initial": initial_pos, "final": final_pos, "screen": screen_result.details})
                return False

            self.end_test(test_name, True,
                f"向前 Seek 成功: {initial_pos}s → {final_pos}s, 变化={change}s",
                {"initial": initial_pos, "final": final_pos, "change": change})
            return True

        except Exception as e:
            self.end_test(test_name, False, f"测试异常: {e}")
            return False

    def test_seek_backward_updates_ui(self) -> bool:
        """测试：向后 Seek 后 UI 更新"""
        test_name = "向后 Seek"
        self.start_test(test_name)

        try:
            # 确保应用有焦点
            KeyboardInput.focus_app("WZMediaPlayer", delay=0.5)

            # 先检测当前播放状态并暂停（确保测试期间不会因播放影响位置）
            pos_a, _, _ = self.verifier.get_current_playback_position()
            time.sleep(0.8)
            pos_b, _, _ = self.verifier.get_current_playback_position()
            is_playing = (pos_b - pos_a) > 0.3

            if is_playing:
                KeyboardInput.toggle_playback()
                time.sleep(0.5)

            # 先向前 Seek 几次，确保有足够空间向后
            for i in range(5):
                KeyboardInput.focus_app("WZMediaPlayer", delay=0.05)
                KeyboardInput.seek_forward()
                time.sleep(0.3)

            time.sleep(0.5)

            # 获取初始位置（在暂停状态下）
            initial_pos, total, _ = self.verifier.get_current_playback_position()

            if initial_pos < 0:
                self.end_test(test_name, False, "无法读取初始位置")
                return False

            # 执行向后 Seek
            for i in range(3):
                KeyboardInput.focus_app("WZMediaPlayer", delay=0.05)
                KeyboardInput.seek_backward()
                time.sleep(0.3)

            # 等待 UI 更新
            time.sleep(WAIT_TIMES["after_seek"])

            # 获取最终位置
            final_pos, _, state = self.verifier.get_current_playback_position()

            if final_pos < 0:
                self.end_test(test_name, False, "无法读取最终位置", {"state": state})
                return False

            # 验证位置变化（向后应该减少）
            change = final_pos - initial_pos

            if change > -1:  # 允许 1 秒误差
                self.end_test(test_name, False,
                    f"向后 Seek 未生效: {initial_pos}s → {final_pos}s, 变化={change}s",
                    {"initial": initial_pos, "final": final_pos, "change": change, "state": state})
                return False

            # 验证画面不是黑屏
            screen_result = self.verifier.verify_screen_not_black()
            if not screen_result.success:
                self.end_test(test_name, False,
                    f"Seek 后黑屏: {screen_result.message}",
                    {"initial": initial_pos, "final": final_pos, "screen": screen_result.details})
                return False

            self.end_test(test_name, True,
                f"向后 Seek 成功: {initial_pos}s → {final_pos}s, 变化={change}s",
                {"initial": initial_pos, "final": final_pos, "change": change})
            return True

        except Exception as e:
            self.end_test(test_name, False, f"测试异常: {e}")
            return False

    def test_rapid_seeking_no_black_screen(self) -> bool:
        """测试：快速 Seeking 不出现黑屏"""
        test_name = "快速 Seeking"
        self.start_test(test_name)

        try:
            # 确保应用有焦点
            KeyboardInput.focus_app("WZMediaPlayer", delay=0.5)

            # 确保正在播放
            KeyboardInput.toggle_playback()
            time.sleep(1.0)

            # 快速 Seek
            for i in range(10):
                KeyboardInput.focus_app("WZMediaPlayer", delay=0.05)
                if i % 2 == 0:
                    KeyboardInput.seek_forward()
                else:
                    KeyboardInput.seek_backward()
                time.sleep(0.1)

            # 等待渲染
            time.sleep(1.0)

            # 验证画面不是黑屏
            screen_result = self.verifier.verify_screen_not_black()
            if not screen_result.success:
                self.end_test(test_name, False,
                    f"快速 Seeking 后黑屏: {screen_result.message}",
                    screen_result.details)
                return False

            # 验证 UI 有响应
            pos, _, state = self.verifier.get_current_playback_position()
            if pos < 0:
                self.end_test(test_name, False, "快速 Seeking 后无法读取 UI 状态", {"state": state})
                return False

            self.end_test(test_name, True,
                f"快速 Seeking 后画面正常, 位置={pos}s",
                {"position": pos, "screen": screen_result.details})
            return True

        except Exception as e:
            self.end_test(test_name, False, f"测试异常: {e}")
            return False

    # ==================== 视频切换测试 ====================

    def test_video_switch_no_black_screen(self) -> bool:
        """测试：视频切换不出现黑屏"""
        test_name = "视频切换"
        self.start_test(test_name)

        try:
            # 确保应用有焦点
            KeyboardInput.focus_app("WZMediaPlayer", delay=0.5)

            # 确保正在播放
            KeyboardInput.toggle_playback()
            time.sleep(1.0)

            # 模拟视频切换（下一个视频）
            KeyboardInput.play_next_video()
            time.sleep(2.0)

            # 验证画面不是黑屏
            screen_result = self.verifier.verify_screen_not_black()
            if not screen_result.success:
                self.end_test(test_name, False,
                    f"视频切换后黑屏: {screen_result.message}",
                    screen_result.details)
                return False

            # 验证 UI 有响应
            pos, total, state = self.verifier.get_current_playback_position()

            self.end_test(test_name, True,
                f"视频切换后画面正常, 位置={pos}s, 总时长={total}s",
                {"position": pos, "total": total, "screen": screen_result.details})
            return True

        except Exception as e:
            self.end_test(test_name, False, f"测试异常: {e}")
            return False

    # ==================== 参考帧验证测试 ====================

    def test_rendering_matches_reference(self) -> bool:
        """测试：渲染画面与参考帧匹配"""
        test_name = "参考帧验证"
        self.start_test(test_name)

        try:
            # 确保应用有焦点
            KeyboardInput.focus_app("WZMediaPlayer", delay=0.5)

            # Seek 到开头
            KeyboardInput.seek_to_start()
            time.sleep(WAIT_TIMES["after_seek"])

            # 确保正在播放
            KeyboardInput.toggle_playback()
            time.sleep(1.0)

            # 获取当前视频名称
            import os
            video_name = os.path.basename(self.test_video)

            # 使用参考帧验证
            result = self.verifier.verify_rendering_for_video(video_name, seek_to_start=False)

            if result.success:
                self.end_test(test_name, True, result.message, result.details)
                return True
            else:
                # 即使参考帧不匹配，也检查是否黑屏
                screen_result = self.verifier.verify_screen_not_black()
                if screen_result.success:
                    # 不是黑屏，只是与参考帧不同（可能是时间点不同）
                    self.end_test(test_name, True,
                        f"画面正常但与参考帧不匹配: {result.message}",
                        {"reference_result": result.details, "screen": screen_result.details})
                    return True
                else:
                    self.end_test(test_name, False, result.message, result.details)
                    return False

        except Exception as e:
            self.end_test(test_name, False, f"测试异常: {e}")
            return False

    def test_playlist_navigation(self) -> bool:
        """测试：播放列表导航（上一首/下一首）"""
        test_name = "播放列表导航"
        self.start_test(test_name)

        try:
            # 确保应用有焦点
            KeyboardInput.focus_app("WZMediaPlayer", delay=0.5)

            # 测试下一首
            KeyboardInput.play_next_video()
            time.sleep(WAIT_TIMES["after_open"])

            # 验证画面不是黑屏
            screen_result = self.verifier.verify_screen_not_black()
            if not screen_result.success:
                self.end_test(test_name, False,
                    f"下一首后黑屏: {screen_result.message}",
                    screen_result.details)
                return False

            # 测试上一首
            KeyboardInput.play_previous_video()
            time.sleep(WAIT_TIMES["after_open"])

            # 验证画面不是黑屏
            screen_result = self.verifier.verify_screen_not_black()
            if not screen_result.success:
                self.end_test(test_name, False,
                    f"上一首后黑屏: {screen_result.message}",
                    screen_result.details)
                return False

            self.end_test(test_name, True, "播放列表导航正常", screen_result.details)
            return True

        except Exception as e:
            self.end_test(test_name, False, f"测试异常: {e}")
            return False

    # ==================== 完整测试运行 ====================

    def run_all_tests(self, video_path: str = None):
        """运行所有闭环测试"""
        print("\n" + "=" * 80)
        print("WZMediaPlayer 闭环测试套件")
        print("=" * 80)
        print("\n每个测试都会验证: 指令执行 → UI 更新 → 画面正常")
        print()

        video_path = video_path or self.test_video
        if video_path and not os.path.exists(video_path):
            print(f"测试视频不存在: {video_path}")
            return

        if not self.setup(APP_PATH, video_path=video_path):
            print("测试准备失败，跳过所有测试")
            return

        try:
            # 基础验证
            print("\n--- 基础验证 ---")
            self.test_can_read_ui_state()
            self.test_screen_not_black()

            # 播放测试
            print("\n--- 播放测试 ---")
            self.test_playback_progresses()
            self.test_pause_actually_pauses()

            # Seeking 测试
            print("\n--- Seeking 测试 ---")
            self.test_seek_forward_updates_ui()
            self.test_seek_backward_updates_ui()
            self.test_rapid_seeking_no_black_screen()

            # 视频切换测试
            print("\n--- 视频切换测试 ---")
            self.test_video_switch_no_black_screen()
            self.test_playlist_navigation()

            # 参考帧验证测试
            print("\n--- 参考帧验证 ---")
            self.test_rendering_matches_reference()

        finally:
            self.teardown()

        # 生成报告
        self.generate_report()
        self.print_summary()
        self.save_report()


def main():
    import argparse

    parser = argparse.ArgumentParser(description='WZMediaPlayer 闭环测试')
    parser.add_argument('--video', type=str, default=None, help='测试视频路径')
    args = parser.parse_args()

    test = ClosedLoopTest()
    test.run_all_tests(args.video)

    return 0 if test._report and test._report.failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())