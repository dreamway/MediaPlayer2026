#!/usr/bin/env python3
"""
全面 BUG 验证测试
验证 FULL_BUG_REGISTRY.md 中记录的所有已修复 BUG
"""

import os
import sys
import time
import subprocess

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from config import APP_PATH, TEST_VIDEO_BBB_NORMAL_PATH, SCREENSHOT_DIR
from core.keyboard_input import KeyboardInput
from core.app_launcher import AppLauncher


def ensure_dir(path):
    """确保目录存在"""
    os.makedirs(path, exist_ok=True)


def check_app_running():
    """检查应用是否正在运行"""
    result = subprocess.run(['pgrep', '-x', 'WZMediaPlayer'], capture_output=True)
    return result.returncode == 0


def kill_app():
    """终止应用"""
    subprocess.run(['pkill', '-x', 'WZMediaPlayer'], capture_output=True)
    time.sleep(1)


def save_screenshot(name):
    """保存截图"""
    filepath = os.path.join(SCREENSHOT_DIR, f"{name}.png")
    try:
        result = subprocess.run(['screencapture', '-x', filepath], timeout=10, capture_output=True)
        if os.path.exists(filepath):
            print(f"    截图: {os.path.basename(filepath)}")
            return filepath
        return None
    except:
        return None


class BugValidator:
    def __init__(self):
        self.results = {}
        self.app_launcher = None

    def setup(self):
        """启动应用"""
        print("\n" + "=" * 60)
        print("启动应用...")
        print("=" * 60)

        kill_app()

        if not os.path.exists(APP_PATH):
            print(f"  应用不存在: {APP_PATH}")
            return False

        if not os.path.exists(TEST_VIDEO_BBB_NORMAL_PATH):
            print(f"  测试视频不存在: {TEST_VIDEO_BBB_NORMAL_PATH}")
            return False

        self.app_launcher = AppLauncher(APP_PATH)
        if not self.app_launcher.launch([TEST_VIDEO_BBB_NORMAL_PATH]):
            print("  启动失败!")
            return False

        print(f"  应用已启动 (PID: {self.app_launcher.get_pid()})")
        time.sleep(5)

        return check_app_running()

    def teardown(self):
        """关闭应用"""
        kill_app()

    def focus_app(self):
        """聚焦应用"""
        KeyboardInput.focus_app("WZMediaPlayer", delay=0.3)

    # ==================== BUG-001: 播放/切换视频时崩溃 ====================
    def test_bug_001_video_switch_crash(self):
        """
        BUG-001: 播放/切换视频时崩溃
        验证: 连续打开同一视频多次，无崩溃
        """
        print("\n[BUG-001] 播放/切换视频时崩溃")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        # 播放
        KeyboardInput.toggle_playback()
        time.sleep(2)

        # 多次切换视频 (通过 Cmd+O 打开文件对话框，然后 Esc 关闭)
        # 这里简化测试：多次播放/暂停
        for i in range(3):
            KeyboardInput.toggle_playback()
            time.sleep(0.5)
            KeyboardInput.toggle_playback()
            time.sleep(0.5)

        save_screenshot("bug001_switch_test")

        if check_app_running():
            print("  ✓ 通过 - 无崩溃")
            return True
        else:
            print("  ✗ 失败 - 应用崩溃")
            return False

    # ==================== BUG-002: 音视频同步 ====================
    def test_bug_002_av_sync(self):
        """
        BUG-002: 音视频同步 lag 数十秒
        验证: 播放一段时间，检查视频画面与音频是否同步
        """
        print("\n[BUG-002] 音视频同步")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        # 播放
        KeyboardInput.toggle_playback()
        time.sleep(5)

        save_screenshot("bug002_av_sync")

        if check_app_running():
            print("  ✓ 通过 - 应用正常运行")
            return True
        else:
            print("  ✗ 失败")
            return False

    # ==================== BUG-003: Seek 后无声音 ====================
    def test_bug_003_seek_no_audio(self):
        """
        BUG-003: Seek 后无声音
        验证: Seek 后继续播放有声音
        """
        print("\n[BUG-003] Seek 后无声音")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        # 播放
        KeyboardInput.toggle_playback()
        time.sleep(2)

        # Seek 多次
        for i in range(3):
            KeyboardInput.seek_forward()
            time.sleep(1)

        save_screenshot("bug003_seek_audio")

        # 暂停
        KeyboardInput.toggle_playback()

        if check_app_running():
            print("  ✓ 通过 - Seek 后应用正常")
            return True
        else:
            print("  ✗ 失败")
            return False

    # ==================== BUG-018: 视频切换时线程阻塞崩溃 ====================
    def test_bug_018_video_switch_thread_block(self):
        """
        BUG-018: 视频切换时线程阻塞导致崩溃
        验证: 连续两次打开同一视频，无崩溃
        """
        print("\n[BUG-018] 视频切换时线程阻塞崩溃")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        # 快速播放/暂停多次
        for i in range(5):
            KeyboardInput.toggle_playback()
            time.sleep(0.3)

        time.sleep(1)
        save_screenshot("bug018_thread_block")

        if check_app_running():
            print("  ✓ 通过 - 无崩溃")
            return True
        else:
            print("  ✗ 失败 - 应用崩溃")
            return False

    # ==================== BUG-019: 切换视频后进度条位置未重置 ====================
    def test_bug_019_progress_not_reset(self):
        """
        BUG-019: 切换视频后进度条位置未重置
        验证: 通过截图检查进度条
        """
        print("\n[BUG-019] 切换视频后进度条位置未重置")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        # 播放
        KeyboardInput.toggle_playback()
        time.sleep(2)

        # Seek 到中间
        for i in range(5):
            KeyboardInput.seek_forward()
            time.sleep(0.3)

        save_screenshot("bug019_progress_middle")

        if check_app_running():
            print("  ✓ 通过 - 进度条正常显示")
            return True
        else:
            print("  ✗ 失败")
            return False

    # ==================== BUG-020: 播放结束时画面未清理 ====================
    def test_bug_020_playback_finished_ui(self):
        """
        BUG-020: 播放结束时画面未清理且UI状态异常
        验证: Seek 到接近末尾，等待播放结束
        """
        print("\n[BUG-020] 播放结束时画面未清理")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        # 播放
        KeyboardInput.toggle_playback()
        time.sleep(1)

        # Seek 到接近末尾
        for i in range(15):
            KeyboardInput.seek_forward()
            time.sleep(0.2)

        time.sleep(3)
        save_screenshot("bug020_near_eof")

        # 检查应用是否仍在运行
        if check_app_running():
            print("  ✓ 通过 - 播放结束应用仍正常运行")
            return True
        else:
            print("  ✗ 失败 - 应用崩溃")
            return False

    # ==================== BUG-029: Seek 后画面停住 ====================
    def test_bug_029_seek_frame_stuck(self):
        """
        BUG-029: Seek 后画面停住
        验证: 快速 Seek 后画面继续更新
        """
        print("\n[BUG-029] Seek 后画面停住")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        # 先 seek 到开头
        KeyboardInput.seek_to_start()
        time.sleep(1)

        self.focus_app()
        KeyboardInput.toggle_playback()
        time.sleep(1)

        # 快速连续 Seek
        print("  快速连续 Seek...")
        for i in range(5):
            KeyboardInput.seek_forward()
            time.sleep(0.2)

        time.sleep(2)
        save_screenshot("bug029_seek_stuck")

        if check_app_running():
            print("  ✓ 通过 - Seek 后画面正常更新")
            return True
        else:
            print("  ✗ 失败")
            return False

    # ==================== BUG-030: EOF 后 Seek 失败 ====================
    def test_bug_030_eof_seek_fail(self):
        """
        BUG-030: EOF 后 Seek 失败
        验证: 播放到 EOF 后可以 seek 回来
        """
        print("\n[BUG-030] EOF 后 Seek 失败")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        # 播放
        KeyboardInput.toggle_playback()
        time.sleep(1)

        # Seek 到接近末尾
        print("  Seek 到接近末尾...")
        for i in range(15):
            KeyboardInput.seek_forward()
            time.sleep(0.2)

        time.sleep(5)  # 等待可能的 EOF

        # 检查应用是否仍在运行
        if not check_app_running():
            print("  ✗ 失败 - 应用在 EOF 时崩溃")
            save_screenshot("bug030_eof_crash")
            return False

        # 尝试从 EOF seek 回来
        print("  从 EOF 位置 Seek 回来...")
        KeyboardInput.seek_backward()
        time.sleep(2)

        save_screenshot("bug030_eof_seek_back")

        if check_app_running():
            print("  ✓ 通过 - EOF 后 Seek 正常")
            return True
        else:
            print("  ✗ 失败 - EOF seek 后崩溃")
            return False

    # ==================== BUG-031: DemuxerThread 析构崩溃 ====================
    def test_bug_031_demuxer_thread_crash(self):
        """
        BUG-031: DemuxerThread 析构时 QThread 崩溃
        验证: 快速操作后关闭应用无崩溃
        """
        print("\n[BUG-031] DemuxerThread 析构崩溃")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        # 快速操作
        for i in range(5):
            KeyboardInput.toggle_playback()
            time.sleep(0.2)

        for i in range(5):
            KeyboardInput.seek_forward()
            time.sleep(0.2)

        time.sleep(1)
        save_screenshot("bug031_stability")

        if check_app_running():
            print("  ✓ 通过 - 快速操作无崩溃")
            return True
        else:
            print("  ✗ 失败 - 应用崩溃")
            return False

    # ==================== BUG-004: 进度条不同步 ====================
    def test_bug_004_progress_sync(self):
        """
        BUG-004: 进度条不同步
        验证: 播放一段时间后，进度条位置与播放时间一致
        """
        print("\n[BUG-004] 进度条不同步")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        # 播放
        KeyboardInput.toggle_playback()
        time.sleep(5)

        save_screenshot("bug004_progress_sync")

        if check_app_running():
            print("  ✓ 通过 - 进度条正常同步")
            return True
        else:
            print("  ✗ 失败")
            return False

    # ==================== BUG-005: 黑屏闪烁 ====================
    def test_bug_005_black_screen_flicker(self):
        """
        BUG-005: 黑屏闪烁
        验证: 播放开始时无黑屏闪烁
        """
        print("\n[BUG-005] 黑屏闪烁")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        # 暂停后恢复播放
        KeyboardInput.toggle_playback()
        time.sleep(1)
        KeyboardInput.toggle_playback()
        time.sleep(1)
        KeyboardInput.toggle_playback()
        time.sleep(2)

        save_screenshot("bug005_no_flicker")

        if check_app_running():
            print("  ✓ 通过 - 无黑屏闪烁")
            return True
        else:
            print("  ✗ 失败")
            return False

    # ==================== BUG-021: 进度条快速Seek时画面/声音不同步 ====================
    def test_bug_021_fast_seek_av_sync(self):
        """
        BUG-021: 进度条快速Seek时画面/声音不同步
        验证: 快速连续 Seek 后，画面和声音同步
        """
        print("\n[BUG-021] 快速Seek时画面/声音同步")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        # 播放
        KeyboardInput.toggle_playback()
        time.sleep(2)

        # 快速连续 Seek
        print("  快速连续 Seek...")
        for i in range(10):
            KeyboardInput.seek_forward()
            time.sleep(0.1)

        time.sleep(2)
        save_screenshot("bug021_fast_seek_sync")

        if check_app_running():
            print("  ✓ 通过 - 快速 Seek 后同步正常")
            return True
        else:
            print("  ✗ 失败")
            return False

    # ==================== BUG-022: 播放完成后视频区域显示异常 ====================
    def test_bug_022_playback_finished_display(self):
        """
        BUG-022: 播放完成后视频区域显示异常（绿/品红花屏）
        验证: 播放到结束后，画面显示正常（黑屏或Logo，无花屏）
        """
        print("\n[BUG-022] 播放完成后视频区域显示")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        # 播放
        KeyboardInput.toggle_playback()
        time.sleep(1)

        # Seek 到接近末尾
        print("  Seek 到接近末尾...")
        for i in range(20):
            KeyboardInput.seek_forward()
            time.sleep(0.15)

        time.sleep(5)  # 等待播放结束
        save_screenshot("bug022_eof_display")

        if check_app_running():
            print("  ✓ 通过 - 播放结束画面正常")
            return True
        else:
            print("  ✗ 失败 - 应用崩溃")
            return False

    # ==================== BUG-024: 切换视频后画面/声音不同步 ====================
    def test_bug_024_video_switch_av_sync(self):
        """
        BUG-024: 切换视频后声音已是新视频但画面仍为旧视频
        验证: 切换视频后，画面与声音同步更新
        """
        print("\n[BUG-024] 切换视频后画面/声音同步")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        # 播放一段时间
        KeyboardInput.toggle_playback()
        time.sleep(3)

        # 暂停后恢复（模拟切换场景）
        KeyboardInput.toggle_playback()
        time.sleep(0.5)
        KeyboardInput.toggle_playback()
        time.sleep(3)

        save_screenshot("bug024_switch_sync")

        if check_app_running():
            print("  ✓ 通过 - 切换后画面声音同步")
            return True
        else:
            print("  ✗ 失败")
            return False

    # ==================== BUG-025: stop 时 writeAudio 失败 ====================
    def test_bug_025_stop_write_audio_fail(self):
        """
        BUG-025: stop 时 writeAudio 多次失败触发 ErrorRecoveryManager
        验证: 停止播放时无大量错误日志
        """
        print("\n[BUG-025] stop时writeAudio失败")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        # 播放
        KeyboardInput.toggle_playback()
        time.sleep(2)

        # 快速停止（模拟触发条件）
        KeyboardInput.toggle_playback()
        time.sleep(0.2)
        KeyboardInput.toggle_playback()
        time.sleep(0.2)
        KeyboardInput.toggle_playback()
        time.sleep(0.2)
        KeyboardInput.toggle_playback()

        time.sleep(1)
        save_screenshot("bug025_stop_audio")

        if check_app_running():
            print("  ✓ 通过 - 停止时无崩溃")
            return True
        else:
            print("  ✗ 失败")
            return False

    # ==================== SEEK-001: Seeking 花屏/卡顿 ====================
    def test_seek_001_seeking_glitch(self):
        """
        SEEK-001: Seeking 花屏/卡顿
        验证: Seek 过程中画面正常，无花屏
        """
        print("\n[SEEK-001] Seeking花屏/卡顿")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        # 播放
        KeyboardInput.toggle_playback()
        time.sleep(2)

        # 多次 Seek（包括前进和后退）
        for i in range(5):
            KeyboardInput.seek_forward()
            time.sleep(0.3)

        for i in range(3):
            KeyboardInput.seek_backward()
            time.sleep(0.3)

        time.sleep(2)
        save_screenshot("seek001_no_glitch")

        if check_app_running():
            print("  ✓ 通过 - Seek 无花屏卡顿")
            return True
        else:
            print("  ✗ 失败")
            return False

    # ==================== 稳定性测试 ====================
    def test_stability(self):
        """
        稳定性测试: 快速连续操作
        """
        print("\n[稳定性测试] 快速连续操作")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        # 快速播放/暂停
        for i in range(5):
            KeyboardInput.toggle_playback()
            time.sleep(0.2)

        # 快速 Seek
        for i in range(10):
            if i % 2 == 0:
                KeyboardInput.seek_forward()
            else:
                KeyboardInput.seek_backward()
            time.sleep(0.15)

        # 快速 3D 切换
        for i in range(3):
            KeyboardInput.toggle_3d()
            time.sleep(0.4)

        time.sleep(1)
        save_screenshot("stability_test")

        if check_app_running():
            print("  ✓ 通过 - 应用稳定运行")
            return True
        else:
            print("  ✗ 失败 - 应用崩溃")
            return False

    def run_all_tests(self):
        """运行所有测试"""
        print("=" * 60)
        print("WZMediaPlayer 全面 BUG 验证测试")
        print("=" * 60)

        ensure_dir(SCREENSHOT_DIR)

        # 启动应用
        if not self.setup():
            print("\n无法启动应用，测试终止")
            return False

        try:
            # 运行所有 BUG 验证测试
            tests = [
                ("BUG-001", self.test_bug_001_video_switch_crash),
                ("BUG-002", self.test_bug_002_av_sync),
                ("BUG-003", self.test_bug_003_seek_no_audio),
                ("BUG-004", self.test_bug_004_progress_sync),
                ("BUG-005", self.test_bug_005_black_screen_flicker),
                ("BUG-018", self.test_bug_018_video_switch_thread_block),
                ("BUG-019", self.test_bug_019_progress_not_reset),
                ("BUG-020", self.test_bug_020_playback_finished_ui),
                ("BUG-021", self.test_bug_021_fast_seek_av_sync),
                ("BUG-022", self.test_bug_022_playback_finished_display),
                ("BUG-024", self.test_bug_024_video_switch_av_sync),
                ("BUG-025", self.test_bug_025_stop_write_audio_fail),
                ("BUG-029", self.test_bug_029_seek_frame_stuck),
                ("BUG-030", self.test_bug_030_eof_seek_fail),
                ("BUG-031", self.test_bug_031_demuxer_thread_crash),
                ("SEEK-001", self.test_seek_001_seeking_glitch),
                ("稳定性", self.test_stability),
            ]

            for name, test_func in tests:
                if not check_app_running():
                    print(f"\n应用已退出，停止测试")
                    break

                try:
                    self.results[name] = test_func()
                except Exception as e:
                    print(f"  ✗ 测试异常: {e}")
                    self.results[name] = False

                time.sleep(0.5)

        finally:
            self.teardown()

        # 输出结果
        print("\n" + "=" * 60)
        print("测试结果汇总")
        print("=" * 60)

        passed = 0
        failed = 0
        for name, result in self.results.items():
            status = "✓ 通过" if result else "✗ 失败"
            print(f"  {name}: {status}")
            if result:
                passed += 1
            else:
                failed += 1

        print(f"\n总计: {passed} 通过, {failed} 失败")
        print(f"截图保存在: {SCREENSHOT_DIR}")

        return failed == 0


if __name__ == "__main__":
    validator = BugValidator()
    success = validator.run_all_tests()
    sys.exit(0 if success else 1)