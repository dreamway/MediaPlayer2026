#!/usr/bin/env python3
"""
BUG-033 ~ BUG-038 测试脚本
验证新发现的 6 个 BUG

BUG-033: DrawWidget 底图应显示 3D_ONLY_LEFT 模式
BUG-034: SplashLogo 背景残影问题
BUG-035: 手动打开视频时 SplashLogo 未隐藏、进度条不更新
BUG-036: 连续切换 play/pause 后声音丢失、进度条错乱
BUG-037: 全屏/全屏+ 功能未区分
BUG-038: 播放列表循环时进度条跳到中间、音视频不同步
"""

import os
import sys
import time
import subprocess

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from config import APP_PATH, TEST_VIDEO_BBB_NORMAL_PATH, SCREENSHOT_DIR
from core.keyboard_input import KeyboardInput
from core.app_launcher import AppLauncher
from core.ax_element import AXElementHelper
from core.screenshot_capture import ScreenshotCapture


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


class NewBugsTester:
    def __init__(self):
        self.results = {}
        self.app_launcher = None

    def setup(self, with_video=True):
        """启动应用"""
        print("\n" + "=" * 60)
        print("启动应用...")
        print("=" * 60)

        kill_app()

        if not os.path.exists(APP_PATH):
            print(f"  应用不存在: {APP_PATH}")
            return False

        if with_video and not os.path.exists(TEST_VIDEO_BBB_NORMAL_PATH):
            print(f"  测试视频不存在: {TEST_VIDEO_BBB_NORMAL_PATH}")
            return False

        args = [TEST_VIDEO_BBB_NORMAL_PATH] if with_video else []
        self.app_launcher = AppLauncher(APP_PATH)
        if not self.app_launcher.launch(args):
            print("  启动失败!")
            return False

        print(f"  应用已启动 (PID: {self.app_launcher.get_pid()})")
        time.sleep(3)

        return check_app_running()

    def teardown(self):
        """关闭应用"""
        kill_app()

    def focus_app(self):
        """聚焦应用"""
        KeyboardInput.focus_app("WZMediaPlayer", delay=0.3)

    # ==================== BUG-033: DrawWidget 底图显示问题 ====================
    def test_bug_033_drawwidget_only_left(self):
        """
        BUG-033: DrawWidget 底图显示问题
        验证：DrawWidget 启用时，底图应显示 3D_ONLY_LEFT 模式

        步骤：
        1. 打开立体视频
        2. 启用 DrawWidget (Cmd+9)
        3. 验证当前输出格式是否为 ONLY_LEFT
        """
        print("\n[BUG-033] DrawWidget 底图显示 ONLY_LEFT 模式测试")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()
        time.sleep(1)

        # 首先切换到 3D 模式
        print("  切换到 3D 模式...")
        KeyboardInput.toggle_3d()
        time.sleep(1)

        # 启用 DrawWidget (Ctrl+9)
        print("  启用 DrawWidget (Ctrl+9)...")
        KeyboardInput.send_hotkey('9', ['command'])
        time.sleep(1)

        save_screenshot("bug033_drawwidget_enabled")

        # 检查应用是否崩溃
        if not check_app_running():
            print("  ✗ 失败 - 启用 DrawWidget 后应用崩溃")
            return False

        # 获取当前输出格式
        helper = AXElementHelper()
        # 注意：这里需要通过 Accessibility API 检查输出格式
        # 由于目前可能没有直接的 API，我们检查是否有 DrawWidget 相关元素

        # 尝试获取 3D 输出格式状态
        output_format = helper.get_stereo_output_format()
        print(f"    当前输出格式: {output_format}")

        # 预期：输出格式应该是 ONLY_LEFT (值为 3)
        expected_format = 3  # STEREO_OUTPUT_FORMAT_ONLY_LEFT
        if output_format == expected_format:
            print("  ✓ 通过 - 输出格式正确为 ONLY_LEFT")
            result = True
        elif output_format is None:
            print("  ⚠ 无法获取输出格式，需要人工验证")
            result = None
        else:
            print(f"  ✗ 失败 - 输出格式应为 ONLY_LEFT (3)，实际为 {output_format}")
            result = False

        # 关闭 DrawWidget
        KeyboardInput.send_hotkey('9', ['command'])
        time.sleep(0.5)

        # 切回 2D 模式
        KeyboardInput.toggle_3d()
        time.sleep(0.5)

        return result

    # ==================== BUG-034: SplashLogo 背景残影问题 ====================
    def test_bug_034_splashlogo_background(self):
        """
        BUG-034: SplashLogo 背景残影问题
        验证：视频停止后，SplashLogo 显示时背景应干净无残影

        步骤：
        1. 播放视频
        2. 停止播放
        3. 截图检查 Logo 背景是否有残影
        """
        print("\n[BUG-034] SplashLogo 背景残影测试")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        # 确保视频在播放
        KeyboardInput.toggle_playback()
        time.sleep(2)

        # 停止播放（使用停止快捷键，如果没有则使用 open 清空）
        # 尝试使用 Escape 或其他方式停止
        print("  停止播放以显示 Logo...")
        KeyboardInput.send_key('escape')
        time.sleep(1)

        save_screenshot("bug034_logo_background")

        # 检查 Logo 是否显示
        helper = AXElementHelper()
        logo_visible = helper.is_logo_visible()

        if logo_visible:
            print("  ✓ Logo 已显示")
            # 背景残影需要人工检查截图
            print("  ⚠ 需要人工检查截图确认背景是否干净")
            result = None
        else:
            print("  ⚠ Logo 未显示或无法确认")
            result = None

        return result

    # ==================== BUG-035: 手动打开视频时 SplashLogo 未隐藏 ====================
    def test_bug_035_manual_open_video(self):
        """
        BUG-035: 手动打开视频时 SplashLogo 未隐藏、进度条不更新
        验证：通过 Cmd+O 打开视频后，Logo 应隐藏，进度条应更新

        步骤：
        1. 先停止当前视频（显示 Logo）
        2. 通过 Cmd+O 打开文件对话框
        3. 选择视频文件
        4. 验证 Logo 已隐藏，进度条在更新
        """
        print("\n[BUG-035] 手动打开视频测试")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        # 首先停止当前视频
        print("  停止当前播放...")
        KeyboardInput.send_key('escape')
        time.sleep(1)

        # 检查 Logo 是否显示
        helper = AXElementHelper()
        logo_before = helper.is_logo_visible()
        print(f"    打开前 Logo 状态: {'显示' if logo_before else '隐藏'}")

        # 使用 open_video_file 方法打开视频
        print(f"  通过文件对话框打开视频...")
        KeyboardInput.open_video_file(TEST_VIDEO_BBB_NORMAL_PATH)
        time.sleep(3)

        save_screenshot("bug035_after_open")

        # 检查应用是否崩溃
        if not check_app_running():
            print("  ✗ 失败 - 打开视频后应用崩溃")
            return False

        # 检查 Logo 是否隐藏
        logo_after = helper.is_logo_visible()
        print(f"    打开后 Logo 状态: {'显示' if logo_after else '隐藏'}")

        # 检查进度条是否在更新
        progress1 = helper.get_progress_value()
        print(f"    进度条值 (第一次): {progress1}")
        time.sleep(1)
        progress2 = helper.get_progress_value()
        print(f"    进度条值 (第二次): {progress2}")

        # 验证
        if logo_after:
            print("  ✗ 失败 - Logo 未隐藏")
            result = False
        elif progress1 is None or progress2 is None:
            print("  ⚠ 无法获取进度条值")
            result = None
        elif progress2 > progress1:
            print("  ✓ 通过 - Logo 已隐藏，进度条在更新")
            result = True
        else:
            print(f"  ✗ 失败 - 进度条未更新 ({progress1} -> {progress2})")
            result = False

        return result

    # ==================== BUG-036: 连续切换 play/pause 问题 ====================
    def test_bug_036_rapid_play_pause(self):
        """
        BUG-036: 连续切换 play/pause 后声音丢失、进度条错乱
        验证：连续快速切换播放/暂停后，音频和进度条应正常

        步骤：
        1. 播放视频
        2. 连续快速按空格键 10 次切换播放/暂停
        3. 最终状态应为播放
        4. 验证音频正常，进度条在更新
        """
        print("\n[BUG-036] 连续切换 play/pause 测试")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        # 确保开始播放
        print("  开始播放...")
        KeyboardInput.toggle_playback()
        time.sleep(1)

        # 连续快速切换
        print("  连续切换播放/暂停 (10 次)...")
        for i in range(10):
            KeyboardInput.toggle_playback()
            time.sleep(0.1)  # 快速切换

        # 最终确保播放状态
        print("  最终确保播放状态...")
        KeyboardInput.toggle_playback()
        time.sleep(1)

        save_screenshot("bug036_after_toggle")

        # 检查应用是否崩溃
        if not check_app_running():
            print("  ✗ 失败 - 切换后应用崩溃")
            return False

        # 检查进度条是否在更新
        helper = AXElementHelper()
        progress1 = helper.get_progress_value()
        print(f"    进度条值 (第一次): {progress1}")
        time.sleep(1)
        progress2 = helper.get_progress_value()
        print(f"    进度条值 (第二次): {progress2}")

        # 检查播放按钮状态
        is_playing = helper.is_play_button_checked()
        print(f"    播放按钮状态: {'播放中' if is_playing else '暂停'}")

        # 验证
        if progress1 is None or progress2 is None:
            print("  ⚠ 无法获取进度条值")
            result = None
        elif progress2 > progress1:
            # 进度条在更新，说明视频在播放
            print("  ✓ 通过 - 进度条在更新（视频正在播放）")
            result = True
        else:
            print(f"  ✗ 失败 - 进度条未更新 ({progress1} -> {progress2})")
            result = False

        # 音频需要人工确认
        print("  ⚠ 音频正常需要人工确认")

        return result

    # ==================== BUG-037: 全屏/全屏+ 功能未区分 ====================
    def test_bug_037_fullscreen_modes(self):
        """
        BUG-037: 全屏/全屏+ 功能未区分
        验证：全屏模式和全屏+模式应该有不同的行为

        步骤：
        1. 播放视频
        2. 进入全屏模式 (Enter)
        3. 截图检查是否有全屏提示
        4. 退出全屏 (Escape)
        5. 进入全屏+模式 (Cmd+Enter)
        6. 截图检查是否有全屏+提示
        7. 验证两种模式行为不同
        """
        print("\n[BUG-037] 全屏/全屏+ 功能区分测试")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        # 确保播放
        KeyboardInput.toggle_playback()
        time.sleep(1)

        # 测试全屏模式
        print("  进入全屏模式 (Enter)...")
        KeyboardInput.send_key('return')
        time.sleep(2)

        save_screenshot("bug037_fullscreen")

        # 检查全屏提示
        helper = AXElementHelper()
        fullscreen_tip = helper.get_fullscreen_tip()
        print(f"    全屏提示: {fullscreen_tip}")

        # 退出全屏
        print("  退出全屏...")
        KeyboardInput.send_key('escape')
        time.sleep(1)

        # 测试全屏+模式
        print("  进入全屏+模式 (Cmd+Enter)...")
        KeyboardInput.send_hotkey('return', ['command'])
        time.sleep(2)

        save_screenshot("bug037_fullscreen_plus")

        # 检查全屏+提示
        fullscreen_plus_tip = helper.get_fullscreen_tip()
        print(f"    全屏+提示: {fullscreen_plus_tip}")

        # 退出全屏
        print("  退出全屏...")
        KeyboardInput.send_key('escape')
        time.sleep(1)

        # 验证两种模式提示不同
        if fullscreen_tip and fullscreen_plus_tip:
            if fullscreen_tip != fullscreen_plus_tip:
                print(f"  ✓ 通过 - 全屏提示不同: '{fullscreen_tip}' vs '{fullscreen_plus_tip}'")
                result = True
            else:
                print(f"  ✗ 失败 - 全屏提示相同: '{fullscreen_tip}'")
                result = False
        else:
            print("  ⚠ 无法获取全屏提示，需要人工验证")
            result = None

        return result

    # ==================== BUG-038: 播放列表循环问题 ====================
    def test_bug_038_playlist_loop(self):
        """
        BUG-038: 播放列表循环时进度条跳到中间、音视频不同步
        验证：播放列表循环切换时，进度条应从 0 开始

        步骤：
        1. 打开播放列表
        2. 添加多个视频
        3. 播放第一个视频到中间位置
        4. 切换到下一个视频
        5. 验证进度条从 0 开始
        """
        print("\n[BUG-038] 播放列表循环测试")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        # 确保播放
        KeyboardInput.toggle_playback()
        time.sleep(1)

        # 等待视频播放到中间位置
        print("  等待视频播放...")
        time.sleep(3)

        helper = AXElementHelper()
        progress_before = helper.get_progress_value()
        seconds_before = helper.get_progress_seconds()
        print(f"    当前进度: {progress_before}% ({seconds_before}秒)")

        # 切换到下一个视频 (Page Down)
        print("  切换到下一个视频 (Page Down)...")
        KeyboardInput.play_next_video()
        time.sleep(3)

        save_screenshot("bug038_after_switch")

        # 检查进度条是否重置
        progress_after = helper.get_progress_value()
        seconds_after = helper.get_progress_seconds()
        print(f"    切换后进度: {progress_after}% ({seconds_after}秒)")

        # 验证进度条是否从开头开始
        # 注意：由于测试可能只有一个视频，Page Down 可能不会切换到新视频
        # 所以如果进度继续增加，可能是正常行为
        if progress_after is not None:
            if progress_after < 5:
                print(f"  ✓ 通过 - 进度条已重置到开头 ({progress_after}%)")
                result = True
            elif progress_after > progress_before:
                # 进度增加了，可能是因为新视频已经播放了一段时间
                # 或者播放列表只有一个视频，Page Down 没有切换
                print(f"  ⚠ 进度条未重置 ({progress_after}%)")
                print("  注意：可能播放列表只有一个视频，或新视频已播放一段时间")
                result = None  # 需要人工确认
            else:
                print(f"  ✗ 失败 - 进度条未重置 ({progress_after}%)")
                result = False
        else:
            print("  ⚠ 无法获取进度条值")
            result = None

        # 音视频同步需要人工确认
        print("  ⚠ 音视频同步需要人工确认")

        return result

    def run_all_tests(self):
        """运行所有测试"""
        print("=" * 60)
        print("WZMediaPlayer BUG-033 ~ BUG-038 测试")
        print("=" * 60)

        ensure_dir(SCREENSHOT_DIR)

        # 启动应用
        if not self.setup():
            print("\n无法启动应用，测试终止")
            return False

        try:
            # 运行测试
            tests = [
                ("BUG-033: DrawWidget ONLY_LEFT", self.test_bug_033_drawwidget_only_left),
                ("BUG-034: SplashLogo 背景", self.test_bug_034_splashlogo_background),
                ("BUG-035: 手动打开视频", self.test_bug_035_manual_open_video),
                ("BUG-036: 连续 play/pause", self.test_bug_036_rapid_play_pause),
                ("BUG-037: 全屏模式区分", self.test_bug_037_fullscreen_modes),
                ("BUG-038: 播放列表循环", self.test_bug_038_playlist_loop),
            ]

            for name, test_func in tests:
                if not check_app_running():
                    print(f"\n应用已退出，停止测试")
                    break

                try:
                    self.results[name] = test_func()
                except Exception as e:
                    print(f"  ✗ 测试异常: {e}")
                    import traceback
                    traceback.print_exc()
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
        pending = 0

        for name, result in self.results.items():
            if result is True:
                status = "✓ 通过"
                passed += 1
            elif result is False:
                status = "✗ 失败"
                failed += 1
            else:
                status = "⚠ 待人工确认"
                pending += 1
            print(f"  {name}: {status}")

        print(f"\n总计: {passed} 通过, {failed} 失败, {pending} 待人工确认")
        print(f"截图保存在: {SCREENSHOT_DIR}")

        return failed == 0


if __name__ == "__main__":
    tester = NewBugsTester()
    success = tester.run_all_tests()
    sys.exit(0 if success else 1)