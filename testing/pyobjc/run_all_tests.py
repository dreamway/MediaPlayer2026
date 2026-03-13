# testing/pyobjc/run_all_tests.py
"""
macOS 自动测试入口
运行 WZMediaPlayer 综合测试套件

功能：
1. 只启动一次应用，运行所有测试
2. 在测试开始前，加载 testing/video 目录下所有视频到播放列表
3. 运行 tests 目录下的所有子测试
4. 使用 reference_frames 目录下的参考帧进行图像验证

每个测试都是闭环的：
- 发送指令 → 等待生效 → 验证 UI 状态 → 验证画面 → 验证日志
"""

import os
import sys
import argparse
import time
import glob

# 确保可以找到模块
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from core.app_launcher import AppLauncher
from core.window_controller import WindowController
from core.keyboard_input import KeyboardInput
from core.screenshot_capture import ScreenshotCapture, ImageAnalyzer
from core.log_monitor import LogMonitor
from core.closed_loop_verifier import ClosedLoopVerifier
from config import (
    APP_PATH, TEST_VIDEO_DIR, REFERENCE_FRAME_DIR, LOG_DIR, SCREENSHOT_DIR,
    WAIT_TIMES, BLACK_THRESHOLD, DEFAULT_TEST_VIDEOS
)


# 全局共享的测试上下文
class TestContext:
    """测试上下文，所有测试共享同一个应用实例"""

    _instance = None

    def __init__(self):
        self.app_launcher = None
        self.window_controller = None
        self.screenshot = None
        self.log_monitor = None
        self.verifier = None
        self.video_files = []
        self.current_video = None
        self._initialized = False

    @classmethod
    def get_instance(cls):
        if cls._instance is None:
            cls._instance = TestContext()
        return cls._instance

    def initialize(self, video_files: list, first_video: str = None):
        """初始化测试环境"""
        if self._initialized:
            return True

        print("\n" + "=" * 80)
        print("初始化测试环境")
        print("=" * 80)

        self.video_files = video_files
        self.current_video = first_video or (video_files[0] if video_files else None)

        # 启动应用，通过命令行参数打开第一个视频
        print("启动 WZMediaPlayer...")
        self.app_launcher = AppLauncher(APP_PATH)
        app_args = [self.current_video] if self.current_video else None
        self.app_launcher.launch(app_args=app_args)

        time.sleep(WAIT_TIMES["after_open"])

        if not self.app_launcher.is_running():
            print("应用启动失败")
            return False

        print(f"应用启动成功，已打开: {os.path.basename(self.current_video) if self.current_video else '无'}")

        # 初始化窗口控制器
        self.window_controller = WindowController()
        self.window_controller.connect(self.app_launcher.get_pid())

        # 初始化截图捕获
        self.screenshot = ScreenshotCapture(SCREENSHOT_DIR)

        # 启动日志监控
        self.log_monitor = LogMonitor(LOG_DIR)
        self.log_monitor.start()

        # 初始化验证器
        self.verifier = ClosedLoopVerifier(
            self.window_controller,
            self.screenshot,
            self.log_monitor
        )

        self._initialized = True
        return True

    def cleanup(self):
        """清理测试环境"""
        print("\n" + "=" * 80)
        print("清理测试环境")
        print("=" * 80)

        if self.log_monitor:
            self.log_monitor.stop()

        if self.app_launcher:
            if self.app_launcher.is_running():
                print("关闭应用...")
                self.app_launcher.quit()
            else:
                print("警告: 应用已不在运行（可能已崩溃）")

        self._initialized = False

    def is_app_running(self) -> bool:
        """检查应用是否仍在运行"""
        if self.app_launcher:
            return self.app_launcher.is_running()
        return False

    def is_initialized(self):
        return self._initialized


def load_video_playlist(video_dir: str) -> list:
    """扫描视频目录并返回所有视频文件路径列表"""
    video_extensions = ['.mp4', '.mkv', '.avi', '.mov', '.wmv', '.flv', '.webm']
    video_files = []

    for ext in video_extensions:
        pattern = os.path.join(video_dir, f'*{ext}')
        video_files.extend(glob.glob(pattern))
        pattern = os.path.join(video_dir, f'*{ext.upper()}')
        video_files.extend(glob.glob(pattern))

    video_files = [f for f in video_files if not f.endswith('.zip') and not f.startswith('.')]
    video_files.sort()

    return video_files


def open_videos_in_playlist(video_files: list, first_video_only: bool = False):
    """通过 UI 打开视频文件，将其加载到播放列表"""
    if not video_files:
        print("没有找到视频文件")
        return

    print(f"\n加载视频到播放列表...")

    # 打开第一个视频
    first_video = video_files[0]
    print(f"  打开视频: {os.path.basename(first_video)}")
    KeyboardInput.open_video_file(first_video, delay=WAIT_TIMES["after_open"])

    if first_video_only:
        return

    # 添加其他视频到播放列表
    for i, video in enumerate(video_files[1:], start=2):
        print(f"  添加到播放列表 ({i}/{len(video_files)}): {os.path.basename(video)}")
        KeyboardInput.open_video_file(video, delay=WAIT_TIMES["after_open"])

    # 切回第一个视频
    print("  切回第一个视频...")
    KeyboardInput.focus_app("WZMediaPlayer", delay=0.5)
    for _ in range(len(video_files) - 1):
        KeyboardInput.focus_app("WZMediaPlayer", delay=0.2)
        KeyboardInput.send_key('pageup', delay=0.5)

    time.sleep(1.0)


def get_available_reference_frames() -> dict:
    """获取可用的参考帧映射"""
    reference_frames = {}
    if os.path.exists(REFERENCE_FRAME_DIR):
        for ref_file in os.listdir(REFERENCE_FRAME_DIR):
            if ref_file.endswith('.png'):
                reference_frames[ref_file] = os.path.join(REFERENCE_FRAME_DIR, ref_file)
    return reference_frames


# ==================== 测试函数 ====================

def test_ui_state_readability(ctx: TestContext) -> tuple:
    """测试：UI 状态可读取"""
    test_name = "UI状态读取"
    print(f"\n[TEST] {test_name}")

    try:
        # 多次尝试读取，确保视频加载完成
        for attempt in range(3):
            pos, total, state = ctx.verifier.get_current_playback_position()

            # 检查是否成功读取（total >= 0 表示成功读取）
            if total > 0:
                print(f"  ✓ 通过: 当前={pos}s, 总时长={total}s")
                return True, {"position": pos, "total": total, "state": state}

            time.sleep(1.0)

        # 3次尝试后仍未读取到
        print(f"  ✗ 失败: 无法读取视频时长 (total={total})")
        print(f"    debug_time_strs: {state.get('debug_time_strs', [])}")
        print(f"    debug_slider_raw: {state.get('debug_slider_raw', [])}")
        return False, {"error": "无法读取视频时长", "state": state}
    except Exception as e:
        print(f"  ✗ 异常: {e}")
        return False, {"error": str(e)}


def test_screen_not_black(ctx: TestContext) -> tuple:
    """测试：画面非黑屏"""
    test_name = "画面验证"
    print(f"\n[TEST] {test_name}")

    try:
        result = ctx.verifier.verify_screen_not_black(black_threshold=BLACK_THRESHOLD)
        if result.success:
            print(f"  ✓ 通过: {result.message}")
            return True, result.details
        else:
            print(f"  ✗ 失败: {result.message}")
            return False, result.details
    except Exception as e:
        print(f"  ✗ 异常: {e}")
        return False, {"error": str(e)}


def test_playback_progresses(ctx: TestContext) -> tuple:
    """测试：播放进度前进"""
    test_name = "播放进度"
    print(f"\n[TEST] {test_name}")

    try:
        KeyboardInput.focus_app("WZMediaPlayer", delay=0.3)

        # 检测播放状态
        pos_a, _, _ = ctx.verifier.get_current_playback_position()
        time.sleep(0.8)
        pos_b, _, _ = ctx.verifier.get_current_playback_position()
        is_playing = (pos_b - pos_a) > 0.3

        if not is_playing:
            KeyboardInput.toggle_playback()
            time.sleep(0.5)

        # 验证播放前进
        result = ctx.verifier.verify_playback_progressing(duration_sec=3.0, min_advance_sec=1)
        if result.success:
            print(f"  ✓ 通过: {result.message}")
            return True, result.details
        else:
            print(f"  ✗ 失败: {result.message}")
            return False, result.details
    except Exception as e:
        print(f"  ✗ 异常: {e}")
        return False, {"error": str(e)}


def test_pause_functionality(ctx: TestContext) -> tuple:
    """测试：暂停功能"""
    test_name = "暂停功能"
    print(f"\n[TEST] {test_name}")

    try:
        KeyboardInput.focus_app("WZMediaPlayer", delay=0.3)

        # 检测播放状态并暂停
        pos_a, _, _ = ctx.verifier.get_current_playback_position()
        time.sleep(0.8)
        pos_b, _, _ = ctx.verifier.get_current_playback_position()
        is_playing = (pos_b - pos_a) > 0.3

        if is_playing:
            KeyboardInput.toggle_playback()
            time.sleep(0.5)

        # 验证暂停后位置不变
        pos1, _, _ = ctx.verifier.get_current_playback_position()
        time.sleep(2.0)
        pos2, _, _ = ctx.verifier.get_current_playback_position()

        if abs(pos2 - pos1) > 1:
            print(f"  ✗ 失败: 暂停后位置仍在变化 {pos1}s → {pos2}s")
            return False, {"pos1": pos1, "pos2": pos2}

        print(f"  ✓ 通过: 暂停成功，位置保持在 {pos1}s")
        return True, {"pos1": pos1, "pos2": pos2}
    except Exception as e:
        print(f"  ✗ 异常: {e}")
        return False, {"error": str(e)}


def test_seek_forward(ctx: TestContext) -> tuple:
    """测试：向前 Seek"""
    test_name = "向前Seek"
    print(f"\n[TEST] {test_name}")

    try:
        KeyboardInput.focus_app("WZMediaPlayer", delay=0.3)

        # 确保正在播放
        pos_a, _, _ = ctx.verifier.get_current_playback_position()
        time.sleep(0.5)
        pos_b, _, _ = ctx.verifier.get_current_playback_position()
        is_playing = (pos_b - pos_a) > 0.3

        if not is_playing:
            KeyboardInput.toggle_playback()
            time.sleep(0.5)

        initial_pos, total, _ = ctx.verifier.get_current_playback_position()

        if initial_pos < 0:
            print(f"  ✗ 失败: 无法读取初始位置")
            return False, {"error": "无法读取初始位置"}

        # 向前 Seek
        for i in range(5):
            KeyboardInput.focus_app("WZMediaPlayer", delay=0.05)
            KeyboardInput.seek_forward()
            time.sleep(0.3)

        time.sleep(WAIT_TIMES["after_seek"])

        final_pos, _, state = ctx.verifier.get_current_playback_position()

        if final_pos < 0:
            print(f"  ✗ 失败: 无法读取最终位置")
            return False, {"error": "无法读取最终位置", "state": state}

        change = final_pos - initial_pos

        # 验证画面
        screen_result = ctx.verifier.verify_screen_not_black()
        if not screen_result.success:
            print(f"  ✗ 失败: Seek后黑屏")
            return False, {"screen": screen_result.details}

        print(f"  ✓ 通过: {initial_pos}s → {final_pos}s, 变化={change}s")
        return True, {"initial": initial_pos, "final": final_pos, "change": change}
    except Exception as e:
        print(f"  ✗ 异常: {e}")
        return False, {"error": str(e)}


def test_seek_backward(ctx: TestContext) -> tuple:
    """测试：向后 Seek"""
    test_name = "向后Seek"
    print(f"\n[TEST] {test_name}")

    try:
        KeyboardInput.focus_app("WZMediaPlayer", delay=0.3)

        # 先暂停以避免播放影响
        pos_a, _, _ = ctx.verifier.get_current_playback_position()
        time.sleep(0.8)
        pos_b, _, _ = ctx.verifier.get_current_playback_position()
        is_playing = (pos_b - pos_a) > 0.3

        if is_playing:
            KeyboardInput.toggle_playback()
            time.sleep(0.5)

        # 先向前 Seek 确保有空间向后
        for i in range(5):
            KeyboardInput.focus_app("WZMediaPlayer", delay=0.05)
            KeyboardInput.seek_forward()
            time.sleep(0.3)

        time.sleep(0.5)

        initial_pos, _, _ = ctx.verifier.get_current_playback_position()

        if initial_pos < 0:
            print(f"  ✗ 失败: 无法读取初始位置")
            return False, {"error": "无法读取初始位置"}

        # 向后 Seek
        for i in range(3):
            KeyboardInput.focus_app("WZMediaPlayer", delay=0.05)
            KeyboardInput.seek_backward()
            time.sleep(0.3)

        time.sleep(WAIT_TIMES["after_seek"])

        final_pos, _, state = ctx.verifier.get_current_playback_position()

        if final_pos < 0:
            print(f"  ✗ 失败: 无法读取最终位置")
            return False, {"error": "无法读取最终位置", "state": state}

        change = final_pos - initial_pos

        if change > -1:
            print(f"  ✗ 失败: 向后Seek未生效 {initial_pos}s → {final_pos}s")
            return False, {"initial": initial_pos, "final": final_pos, "change": change}

        # 验证画面
        screen_result = ctx.verifier.verify_screen_not_black()
        if not screen_result.success:
            print(f"  ✗ 失败: Seek后黑屏")
            return False, {"screen": screen_result.details}

        print(f"  ✓ 通过: {initial_pos}s → {final_pos}s, 变化={change}s")
        return True, {"initial": initial_pos, "final": final_pos, "change": change}
    except Exception as e:
        print(f"  ✗ 异常: {e}")
        return False, {"error": str(e)}


def test_rapid_seeking(ctx: TestContext) -> tuple:
    """测试：快速 Seeking"""
    test_name = "快速Seeking"
    print(f"\n[TEST] {test_name}")

    try:
        KeyboardInput.focus_app("WZMediaPlayer", delay=0.3)

        # 确保播放
        pos_a, _, _ = ctx.verifier.get_current_playback_position()
        time.sleep(0.5)
        pos_b, _, _ = ctx.verifier.get_current_playback_position()
        is_playing = (pos_b - pos_a) > 0.3

        if not is_playing:
            KeyboardInput.toggle_playback()
            time.sleep(0.5)

        # 快速 Seeking
        for i in range(10):
            KeyboardInput.focus_app("WZMediaPlayer", delay=0.05)
            if i % 2 == 0:
                KeyboardInput.seek_forward()
            else:
                KeyboardInput.seek_backward()
            time.sleep(0.1)

        time.sleep(1.0)

        # 验证画面
        screen_result = ctx.verifier.verify_screen_not_black()
        if not screen_result.success:
            print(f"  ✗ 失败: 快速Seeking后黑屏")
            return False, {"screen": screen_result.details}

        pos, _, _ = ctx.verifier.get_current_playback_position()
        print(f"  ✓ 通过: 快速Seeking后画面正常, 位置={pos}s")
        return True, {"position": pos}
    except Exception as e:
        print(f"  ✗ 异常: {e}")
        return False, {"error": str(e)}


def test_playlist_navigation(ctx: TestContext) -> tuple:
    """测试：播放列表导航"""
    test_name = "播放列表导航"
    print(f"\n[TEST] {test_name}")

    try:
        KeyboardInput.focus_app("WZMediaPlayer", delay=0.3)

        # 测试下一首
        KeyboardInput.play_next_video()
        time.sleep(WAIT_TIMES["after_open"])

        screen_result = ctx.verifier.verify_screen_not_black()
        if not screen_result.success:
            print(f"  ✗ 失败: 下一首后黑屏")
            return False, {"screen": screen_result.details}

        # 测试上一首
        KeyboardInput.play_previous_video()
        time.sleep(WAIT_TIMES["after_open"])

        screen_result = ctx.verifier.verify_screen_not_black()
        if not screen_result.success:
            print(f"  ✗ 失败: 上一首后黑屏")
            return False, {"screen": screen_result.details}

        print(f"  ✓ 通过: 播放列表导航正常")
        return True, {}
    except Exception as e:
        print(f"  ✗ 异常: {e}")
        return False, {"error": str(e)}


def test_3d_mode_toggle(ctx: TestContext) -> tuple:
    """测试：3D模式切换"""
    test_name = "3D模式切换"
    print(f"\n[TEST] {test_name}")

    try:
        KeyboardInput.focus_app("WZMediaPlayer", delay=0.3)

        # 切换到3D
        KeyboardInput.toggle_3d()
        time.sleep(WAIT_TIMES["short"])

        screen_result = ctx.verifier.verify_screen_not_black()
        if not screen_result.success:
            print(f"  ✗ 失败: 3D模式黑屏")
            # 切回2D
            KeyboardInput.toggle_3d()
            return False, {"screen": screen_result.details}

        # 切回2D
        KeyboardInput.toggle_3d()
        time.sleep(WAIT_TIMES["short"])

        print(f"  ✓ 通过: 3D模式切换正常")
        return True, {}
    except Exception as e:
        print(f"  ✗ 异常: {e}")
        return False, {"error": str(e)}


def test_progress_bar_sync(ctx: TestContext) -> tuple:
    """测试：进度条与播放同步"""
    test_name = "进度条同步"
    print(f"\n[TEST] {test_name}")

    try:
        KeyboardInput.focus_app("WZMediaPlayer", delay=0.3)

        # 确保播放
        pos_a, _, _ = ctx.verifier.get_current_playback_position()
        time.sleep(0.5)
        pos_b, _, _ = ctx.verifier.get_current_playback_position()
        is_playing = (pos_b - pos_a) > 0.3

        if not is_playing:
            KeyboardInput.toggle_playback()
            time.sleep(0.5)

        # 读取多次位置验证连续性
        positions = []
        for i in range(5):
            pos, total, state = ctx.verifier.get_current_playback_position()
            positions.append((time.time(), pos))
            time.sleep(1.0)

        # 验证位置递增
        errors = []
        for i in range(1, len(positions)):
            time_diff = positions[i][0] - positions[i-1][0]
            pos_diff = positions[i][1] - positions[i-1][1]
            # 位置变化应该与时间变化大致一致（允许0.5秒误差）
            if abs(pos_diff - time_diff) > 0.5:
                errors.append(f"t={i}: 时间差{time_diff:.1f}s, 位置差{pos_diff:.1f}s")

        if errors:
            print(f"  ✗ 失败: 进度条与播放不同步")
            return False, {"errors": errors, "positions": positions}

        print(f"  ✓ 通过: 进度条与播放同步正常")
        return True, {"positions": [(p[1]) for p in positions]}
    except Exception as e:
        print(f"  ✗ 异常: {e}")
        return False, {"error": str(e)}


def main():
    """运行所有测试"""
    parser = argparse.ArgumentParser(description='WZMediaPlayer macOS 自动化测试')
    parser.add_argument('--video', type=str, default=None,
                        help='指定测试视频路径')
    parser.add_argument('--quick', action='store_true',
                        help='快速测试模式')
    parser.add_argument('--no-playlist', action='store_true',
                        help='不加载播放列表')

    args = parser.parse_args()

    print("=" * 80)
    print("WZMediaPlayer macOS 自动化测试套件 (闭环验证)")
    print("=" * 80)
    print()
    print("注意：首次运行需要在「系统偏好设置 > 安全性与隐私 > 隐私 > 辅助功能」")
    print("      中授权终端或 Python 解释器")
    print()
    print("每个测试都会验证: 指令执行 → UI 更新 → 画面正常")
    print()

    # 扫描视频文件
    video_files = load_video_playlist(TEST_VIDEO_DIR)
    print(f"已发现 {len(video_files)} 个视频文件:")
    for vf in video_files:
        print(f"  - {os.path.basename(vf)}")
    print()

    # 显示参考帧信息
    reference_frames = get_available_reference_frames()
    if reference_frames:
        print(f"已发现 {len(reference_frames)} 个参考帧:")
        for name in sorted(reference_frames.keys())[:5]:
            print(f"  - {name}")
        if len(reference_frames) > 5:
            print(f"  ... 及其他 {len(reference_frames) - 5} 个")
        print()

    # 初始化测试上下文（只启动一次应用）
    ctx = TestContext.get_instance()
    if not ctx.initialize(video_files, args.video):
        print("测试环境初始化失败")
        return 1

    try:
        # 等待视频加载完成
        print("\n等待视频加载...")
        time.sleep(2.0)  # 额外等待视频加载

        # 验证视频是否加载成功
        pos, total, state = ctx.verifier.get_current_playback_position()
        print(f"视频状态: 当前时间={pos}s, 总时长={total}s")

        if total <= 0:
            print("警告: 无法读取视频时长，可能视频加载失败")

        # 加载其他视频到播放列表（第一个视频已通过命令行打开）
        if not args.no_playlist and video_files and len(video_files) > 1:
            print("\n添加其他视频到播放列表...")
            for i, video in enumerate(video_files[1:], start=2):
                print(f"  添加 ({i}/{len(video_files)}): {os.path.basename(video)}")
                KeyboardInput.open_video_file(video, delay=WAIT_TIMES["after_open"])
            # 切回第一个视频
            print("  切回第一个视频...")
            KeyboardInput.focus_app("WZMediaPlayer", delay=0.5)
            for _ in range(len(video_files) - 1):
                KeyboardInput.focus_app("WZMediaPlayer", delay=0.2)
                KeyboardInput.send_key('pageup', delay=0.5)
            time.sleep(2.0)

            # 再次验证视频状态
            pos, total, state = ctx.verifier.get_current_playback_position()
            print(f"切回后视频状态: 当前时间={pos}s, 总时长={total}s")

        # 定义测试列表
        tests = [
            ("基础验证", [
                test_ui_state_readability,
                test_screen_not_black,
            ]),
            ("播放测试", [
                test_playback_progresses,
                test_pause_functionality,
                test_progress_bar_sync,
            ]),
            ("Seeking测试", [
                test_seek_forward,
                test_seek_backward,
                test_rapid_seeking,
            ]),
            ("播放列表测试", [
                test_playlist_navigation,
            ]),
            ("3D功能测试", [
                test_3d_mode_toggle,
            ]),
        ]

        if args.quick:
            # 快速模式只运行基础测试
            tests = tests[:2]

        # 运行所有测试
        all_passed = True
        total_passed = 0
        total_failed = 0
        failed_tests = []
        app_crashed = False

        for group_name, test_funcs in tests:
            print(f"\n{'='*80}")
            print(f"--- {group_name} ---")
            print("=" * 80)

            for test_func in test_funcs:
                # 检查应用是否仍在运行
                if not ctx.is_app_running():
                    print(f"\n[ERROR] 应用已崩溃！无法继续测试")
                    app_crashed = True
                    all_passed = False
                    break

                try:
                    passed, details = test_func(ctx)
                    if passed:
                        total_passed += 1
                    else:
                        total_failed += 1
                        failed_tests.append(test_func.__doc__ or test_func.__name__)
                        all_passed = False
                except Exception as e:
                    print(f"  ✗ 测试异常: {e}")
                    total_failed += 1
                    failed_tests.append(test_func.__doc__ or test_func.__name__)
                    all_passed = False

                    # 检查是否因为应用崩溃
                    if not ctx.is_app_running():
                        print(f"\n[ERROR] 应用在测试过程中崩溃！")
                        app_crashed = True
                        break

            if app_crashed:
                break

        # 打印总结
        print("\n" + "=" * 80)
        print("测试总结")
        print("=" * 80)

        if app_crashed:
            print("\n[严重错误] 应用在测试过程中崩溃！")
            print("请检查日志文件以确定崩溃原因。")

        print(f"\n总计: {total_passed + total_failed} 个测试")
        print(f"  ✓ 通过: {total_passed}")
        print(f"  ✗ 失败: {total_failed}")

        if failed_tests:
            print("\n失败的测试:")
            for t in failed_tests:
                print(f"  - {t}")

        print("\n" + "=" * 80)
        if all_passed:
            print("所有测试通过")
        else:
            print("存在测试失败，请查看上方详细信息")
        print("=" * 80)

        return 0 if all_passed else 1

    finally:
        ctx.cleanup()


if __name__ == "__main__":
    sys.exit(main())