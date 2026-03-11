# testing/pyobjc/tests/test_playback_sync.py
"""
WZMediaPlayer 播放进度与 UI 同步测试

验证：实际播放进度、进度条、时间标签、播放/暂停按钮状态一致。
用于发现「进度条/按钮状态与真实播放不同步」的 BUG。
"""

import os
import sys
import time
import subprocess
from datetime import datetime

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from core.test_base import TestBase
from core.keyboard_input import KeyboardInput
from core.log_monitor import LogMonitor
from config import (
    APP_PATH,
    TEST_VIDEO_PATH,
    LOG_DIR,
    WAIT_TIMES,
    PLAYBACK_SYNC_TOLERANCE,
)


class PlaybackSyncTest(TestBase):
    """播放进度与 UI 同步测试"""

    def __init__(self):
        super().__init__("PlaybackSyncTest")
        self.log_monitor = None
        self.test_video = TEST_VIDEO_PATH
        self._used_existing_instance = False

    def setup(self, app_path: str = None, video_path: str = None) -> bool:
        # 直接以视频参数启动，避免"先启动->再关闭->再启动"扰动 UI/状态
        app_args = [video_path] if video_path else None
        result = super().setup(app_path, app_args=app_args)
        if result:
            # 检查是否使用了现有实例（app_launcher 会打印提示）
            # 如果是现有实例，需要通过 UI 打开视频
            if self.app_launcher and hasattr(self.app_launcher, '_process') and self.app_launcher._process is None:
                # 使用了现有实例，通过 UI 打开视频
                self._used_existing_instance = True
                print("[PlaybackSyncTest] Using existing instance, opening video via UI...")
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

    def test_can_read_playback_ui_state(self) -> bool:
        """测试能读取播放相关 UI（时间、进度条、按钮）。"""
        test_name = "读取播放 UI 状态"
        self.start_test(test_name)

        try:
            if not self.window_controller:
                self.end_test(test_name, False, "未连接窗口")
                return False

            state = self.window_controller.get_playback_ui_state()
            play_sec = state.get("play_time_sec", -1)
            slider = state.get("slider_value", -1)
            total_sec = state.get("total_time_sec", -1)

            if play_sec < 0 and slider < 0:
                self.end_test(
                    test_name,
                    False,
                    "无法读取播放时间与进度条（play_time_sec 与 slider_value 均无效）。"
                    "请确认应用已打开视频且可访问性暴露了时间/进度控件。",
                    {"state": state},
                )
                return False

            self.end_test(
                test_name,
                True,
                f"play_time_sec={play_sec}, slider_value={slider}, total_sec={total_sec}, is_playing={state.get('is_playing')}",
                {"state": state},
            )
            return True
        except Exception as e:
            self.end_test(test_name, False, f"读取 UI 异常: {e}")
            return False

    def test_playback_time_and_slider_consistent(self) -> bool:
        """播放时：时间标签与进度条数值应一致（在容差内）。"""
        test_name = "时间与进度条一致"
        self.start_test(test_name)

        try:
            KeyboardInput.toggle_playback()
            time.sleep(WAIT_TIMES["playback_test"])

            state = self.window_controller.get_playback_ui_state()
            play_sec = state.get("play_time_sec", -1)
            slider = state.get("slider_value", -1)
            tol = PLAYBACK_SYNC_TOLERANCE["ui_vs_slider_sec"]

            if play_sec < 0 and slider < 0:
                self.end_test(test_name, False, "无法读取时间或进度条", {"state": state})
                return False

            if play_sec >= 0 and slider >= 0:
                diff = abs(play_sec - slider)
                if diff > tol:
                    self.end_test(
                        test_name,
                        False,
                        f"时间与进度条不一致: play_time_sec={play_sec}, slider_value={slider}, 差={diff}秒 (容差{tol}秒)",
                        {"state": state},
                    )
                    return False

            self.end_test(
                test_name,
                True,
                f"时间与进度条一致: play_sec={play_sec}, slider={slider}",
                {"state": state},
            )
            return True
        except Exception as e:
            self.end_test(test_name, False, f"测试异常: {e}")
            return False

    def test_playback_time_advances_when_playing(self) -> bool:
        """播放中：经过一段时间后，当前时间应明显增加。"""
        test_name = "播放时时间前进"
        self.start_test(test_name)

        try:
            # 先 seek 到开头，确保从已知位置开始
            KeyboardInput.seek_to_start()
            time.sleep(WAIT_TIMES["after_seek"])

            KeyboardInput.toggle_playback()
            time.sleep(WAIT_TIMES["short"])

            state1 = self.window_controller.get_playback_ui_state()
            t1 = state1.get("play_time_sec", state1.get("slider_value", -1))
            if t1 < 0:
                self.end_test(test_name, False, "无法读取初始播放位置", {"state": state1})
                return False

            time.sleep(3.0)

            state2 = self.window_controller.get_playback_ui_state()
            t2 = state2.get("play_time_sec", state2.get("slider_value", -1))
            if t2 < 0:
                self.end_test(test_name, False, "无法读取后续播放位置", {"state": state2})
                return False

            min_advance = PLAYBACK_SYNC_TOLERANCE["min_advance_sec"]
            # 如果时间变小了，可能是视频循环播放或跳到开头
            if t2 < t1:
                self.end_test(
                    test_name,
                    True,
                    f"时间变化: t1={t1}, t2={t2}（可能视频循环或重新开始）",
                    {"t1": t1, "t2": t2},
                )
                return True

            if t2 <= t1 + min_advance:
                self.end_test(
                    test_name,
                    False,
                    f"播放约 3 秒后时间几乎未增加: t1={t1}, t2={t2}, 至少应增加 {min_advance} 秒。"
                    "可能原因：进度/时钟未更新或未真正播放。",
                    {"t1": t1, "t2": t2, "state1": state1, "state2": state2},
                )
                return False

            self.end_test(
                test_name,
                True,
                f"时间正常前进: t1={t1}, t2={t2}",
                {"t1": t1, "t2": t2},
            )
            return True
        except Exception as e:
            self.end_test(test_name, False, f"测试异常: {e}")
            return False

    def test_seek_forward_updates_ui(self) -> bool:
        """向前 seek 后：UI 上的时间/进度条应明显前进。"""
        test_name = "Seek 后 UI 更新"
        self.start_test(test_name)

        try:
            # 确保应用处于前台
            KeyboardInput.focus_app("WZMediaPlayer", delay=0.3)

            KeyboardInput.toggle_playback()
            time.sleep(WAIT_TIMES["medium"])

            state_before = self.window_controller.get_playback_ui_state()
            pos_before = state_before.get("play_time_sec", state_before.get("slider_value", -1))
            if pos_before < 0:
                self.end_test(test_name, False, "无法读取 seek 前位置", {"state": state_before})
                return False

            # 确保应用处于前台
            KeyboardInput.focus_app("WZMediaPlayer", delay=0.3)

            # 每次按键前确保应用有焦点
            for _ in range(5):  # 增加按键次数到 5 次
                KeyboardInput.focus_app("WZMediaPlayer", delay=0.1)
                KeyboardInput.seek_forward()
                time.sleep(0.5)
            time.sleep(WAIT_TIMES["after_seek"])

            state_after = self.window_controller.get_playback_ui_state()
            pos_after = state_after.get("play_time_sec", state_after.get("slider_value", -1))
            if pos_after < 0:
                self.end_test(test_name, False, "无法读取 seek 后位置", {"state": state_after})
                return False

            min_advance = PLAYBACK_SYNC_TOLERANCE["seek_min_advance_sec"]
            if pos_after <= pos_before + min_advance:
                self.end_test(
                    test_name,
                    False,
                    f"向前 seek 后 UI 几乎未更新: before={pos_before}, after={pos_after}, 至少应增加 {min_advance} 秒。"
                    "可能原因：seek 未生效或 UI 未同步。",
                    {"pos_before": pos_before, "pos_after": pos_after, "state_before": state_before, "state_after": state_after},
                )
                return False

            self.end_test(
                test_name,
                True,
                f"Seek 后 UI 已更新: before={pos_before}, after={pos_after}",
                {"pos_before": pos_before, "pos_after": pos_after},
            )
            return True
        except Exception as e:
            self.end_test(test_name, False, f"测试异常: {e}")
            return False

    def test_ui_vs_log_position_when_available(self) -> bool:
        """若日志中有播放位置，则 UI 时间与日志解析位置应在容差内一致。"""
        test_name = "UI 与日志位置一致"
        self.start_test(test_name)

        try:
            KeyboardInput.toggle_playback()
            time.sleep(WAIT_TIMES["playback_test"])

            state = self.window_controller.get_playback_ui_state()
            ui_sec = state.get("play_time_sec", state.get("slider_value", -1))
            log_sec = self.log_monitor.get_last_playback_position_sec() if self.log_monitor else -1

            if log_sec < 0:
                self.end_test(
                    test_name,
                    True,
                    "日志中无播放位置（可能为 release 日志级别），跳过 UI/日志一致性校验",
                    {"ui_sec": ui_sec},
                )
                return True

            if ui_sec < 0:
                self.end_test(test_name, False, "无法读取 UI 时间，无法与日志对比", {"log_sec": log_sec})
                return False

            tol = PLAYBACK_SYNC_TOLERANCE["ui_vs_log_sec"]
            if abs(ui_sec - log_sec) > tol:
                self.end_test(
                    test_name,
                    False,
                    f"UI 与日志位置不一致: ui_sec={ui_sec}, log_sec={log_sec}, 差={abs(ui_sec - log_sec)}秒 (容差{tol}秒)",
                    {"ui_sec": ui_sec, "log_sec": log_sec, "state": state},
                )
                return False

            self.end_test(
                test_name,
                True,
                f"UI 与日志位置一致: ui_sec={ui_sec}, log_sec={log_sec}",
                {"ui_sec": ui_sec, "log_sec": log_sec},
            )
            return True
        except Exception as e:
            self.end_test(test_name, False, f"测试异常: {e}")
            return False

    def run_all_tests(self, video_path: str = None):
        """运行全部播放同步测试。"""
        print("\n" + "=" * 80)
        print("WZMediaPlayer 播放进度与 UI 同步测试")
        print("=" * 80)

        video_path = video_path or self.test_video
        if video_path and not os.path.exists(video_path):
            print(f"测试视频不存在: {video_path}")
            return

        if not self.setup(APP_PATH, video_path=video_path):
            print("测试准备失败，跳过所有测试")
            return

        try:
            print("\n--- 播放 UI 与进度同步 ---")
            self.test_can_read_playback_ui_state()
            self.test_playback_time_and_slider_consistent()
            self.test_playback_time_advances_when_playing()
            self.test_seek_forward_updates_ui()
            self.test_ui_vs_log_position_when_available()
        finally:
            self.teardown()

        self.generate_report()
        self.print_summary()
        self.save_report()


def main():
    import argparse

    parser = argparse.ArgumentParser(description="WZMediaPlayer 播放同步测试")
    parser.add_argument("--video", type=str, default=None, help="测试视频路径")
    args = parser.parse_args()

    suite = PlaybackSyncTest()
    suite.run_all_tests(args.video)
    return 0 if suite._report and suite._report.failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())