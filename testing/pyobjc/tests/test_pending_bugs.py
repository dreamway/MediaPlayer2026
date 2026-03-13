#!/usr/bin/env python3
"""
待验证 BUG 测试脚本
验证以下 BUG 的当前状态：
1. BUG-008: FPS 过低
2. BUG-009: 摄像头功能
3. BUG-010: 3D 切换无效
4. BUG-011: MainLogo Slogan
5. BUG-012: VS 项目文件显示（低优先级，跳过）
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


class PendingBugsTester:
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
        time.sleep(3)

        return check_app_running()

    def teardown(self):
        """关闭应用"""
        kill_app()

    def focus_app(self):
        """聚焦应用"""
        KeyboardInput.focus_app("WZMediaPlayer", delay=0.3)

    # ==================== BUG-008: FPS 过低 ====================
    def test_bug_008_fps(self):
        """
        BUG-008: FPS 过低
        验证：FPS 显示是否接近视频帧率（如 30fps）
        """
        print("\n[BUG-008] FPS 显示测试")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        # 播放视频
        KeyboardInput.toggle_playback()
        time.sleep(3)

        # 获取 FPS 标签内容
        print("  检查 FPS 显示...")
        helper = AXElementHelper()
        fps_value = helper.get_fps_value()

        if fps_value is not None:
            print(f"    FPS 显示值: {fps_value}")

            # FPS 应该接近视频帧率（30fps）
            # 允许范围：20-35 fps
            if 20 <= fps_value <= 35:
                print("  ✓ 通过 - FPS 显示正常")
                result = True
            else:
                print(f"  ⚠ FPS 显示异常（预期 20-35，实际 {fps_value}）")
                result = False
        else:
            print("  ⚠ 无法获取 FPS 值（可能 FPS 显示已关闭）")
            result = None  # 无法确定

        save_screenshot("pending_bug008_fps")
        KeyboardInput.toggle_playback()
        time.sleep(0.5)

        return result

    # ==================== BUG-009: 摄像头功能 ====================
    def test_bug_009_camera(self):
        """
        BUG-009: 摄像头功能
        验证：打开摄像头后是否正常显示图像
        """
        print("\n[BUG-009] 摄像头功能测试")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        # 打开摄像头菜单
        print("  尝试打开摄像头...")
        # Cmd+Shift+C 打开摄像头（假设快捷键）
        # 或者通过菜单访问
        KeyboardInput.send_hotkey('c', ['command', 'shift'])
        time.sleep(3)

        save_screenshot("pending_bug009_camera")

        # 检查应用是否崩溃
        if not check_app_running():
            print("  ✗ 失败 - 打开摄像头后应用崩溃")
            return False

        # 关闭摄像头
        KeyboardInput.send_hotkey('c', ['command', 'shift'])
        time.sleep(1)

        print("  ⚠ 摄像头功能需要手动验证是否有图像显示")
        return None  # 需要人工确认

    # ==================== BUG-010: 3D 切换无效 ====================
    def test_bug_010_3d_switch(self):
        """
        BUG-010: 3D 切换无效
        验证：切换 3D 模式后渲染是否变为立体渲染
        """
        print("\n[BUG-010] 3D 切换有效性测试")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        # 播放视频
        KeyboardInput.toggle_playback()
        time.sleep(2)

        # 切换到 3D 模式
        print("  切换到 3D 模式...")
        KeyboardInput.toggle_3d()
        time.sleep(2)

        save_screenshot("pending_bug010_3d_on")

        # 获取当前 3D 模式状态
        helper = AXElementHelper()
        is_3d = helper.is_3d_mode_enabled()
        print(f"    3D 模式状态: {'启用' if is_3d else '禁用'}")

        # 测试不同的输出格式
        print("  测试 3D 输出格式...")
        # H (水平)
        KeyboardInput.send_hotkey('h', ['command'])
        time.sleep(1)
        save_screenshot("pending_bug010_3d_h")

        # V (垂直)
        KeyboardInput.send_hotkey('v', ['command'])
        time.sleep(1)
        save_screenshot("pending_bug010_3d_v")

        # 切回 2D
        KeyboardInput.toggle_3d()
        time.sleep(1)

        save_screenshot("pending_bug010_3d_off")

        if not check_app_running():
            print("  ✗ 失败 - 3D 切换后应用崩溃")
            return False

        # 需要通过截图对比确认 3D 效果
        print("  ⚠ 3D 效果需要通过截图对比确认")
        return None  # 需要人工确认截图

    # ==================== BUG-011: MainLogo Slogan ====================
    def test_bug_011_logo_slogan(self):
        """
        BUG-011: MainLogo Slogan
        验证：主窗口 Logo 是否正常显示
        """
        print("\n[BUG-011] MainLogo Slogan 测试")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        # 停止播放以显示 Logo
        KeyboardInput.send_hotkey('s', ['command'])  # 假设 Cmd+S 停止
        time.sleep(1)

        save_screenshot("pending_bug011_logo")

        # 检查 Logo 元素
        helper = AXElementHelper()
        logo_visible = helper.is_logo_visible()

        if logo_visible:
            print("  ✓ Logo 正常显示")
            result = True
        else:
            print("  ⚠ 无法确认 Logo 是否显示")
            result = None

        return result

    def run_all_tests(self):
        """运行所有测试"""
        print("=" * 60)
        print("WZMediaPlayer 待验证 BUG 测试")
        print("=" * 60)

        ensure_dir(SCREENSHOT_DIR)

        # 启动应用
        if not self.setup():
            print("\n无法启动应用，测试终止")
            return False

        try:
            # 运行测试
            tests = [
                ("BUG-008: FPS 过低", self.test_bug_008_fps),
                ("BUG-009: 摄像头功能", self.test_bug_009_camera),
                ("BUG-010: 3D 切换无效", self.test_bug_010_3d_switch),
                ("BUG-011: MainLogo Slogan", self.test_bug_011_logo_slogan),
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
        print("\n注意: BUG-012 (VS 项目文件显示) 为低优先级问题，不影响功能，已跳过测试")

        return failed == 0


if __name__ == "__main__":
    tester = PendingBugsTester()
    success = tester.run_all_tests()
    sys.exit(0 if success else 1)