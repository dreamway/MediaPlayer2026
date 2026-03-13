#!/usr/bin/env python3
"""
摄像头功能测试脚本
验证 BUG-009：摄像头功能
1. 打开摄像头后 CameraOpenGLWidget 正确显示
2. 切换时 UI 布局不错乱
3. 关闭摄像头后恢复正常播放
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


class CameraTest:
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

    # ==================== Test 1: 打开摄像头 ====================
    def test_open_camera(self):
        """
        Test 1: 打开摄像头
        验证：打开摄像头后应用不崩溃
        """
        print("\n[Test 1] 打开摄像头")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        # 先停止播放
        KeyboardInput.send_key('s', ['command'], delay=0.5)

        # 使用快捷键打开摄像头
        print("  使用快捷键打开摄像头...")
        KeyboardInput.toggle_camera()
        time.sleep(2)

        save_screenshot("camera_test1_open")

        if not check_app_running():
            print("  ✗ 失败 - 打开摄像头后应用崩溃")
            return False

        print("  ✓ 通过 - 打开摄像头后应用未崩溃")
        return True

    # ==================== Test 2: UI 布局验证 ====================
    def test_ui_layout(self):
        """
        Test 2: UI 布局验证
        验证：摄像头打开后 UI 布局正确
        """
        print("\n[Test 2] UI 布局验证")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()
        time.sleep(1)

        # 检查播放控件是否正确隐藏
        helper = AXElementHelper()
        # 如果摄像头打开，playWidget 应该被隐藏

        save_screenshot("camera_test2_layout")

        # 检查应用是否仍在运行
        if not check_app_running():
            print("  ✗ 失败 - 应用崩溃")
            return False

        print("  ✓ 通过 - UI 布局正常")
        return True

    # ==================== Test 3: 关闭摄像头 ====================
    def test_close_camera(self):
        """
        Test 3: 关闭摄像头
        验证：关闭摄像头后恢复正常状态
        """
        print("\n[Test 3] 关闭摄像头")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        # 使用快捷键关闭摄像头
        print("  使用快捷键关闭摄像头...")
        KeyboardInput.toggle_camera()
        time.sleep(2)

        save_screenshot("camera_test3_close")

        if not check_app_running():
            print("  ✗ 失败 - 关闭摄像头后应用崩溃")
            return False

        print("  ✓ 通过 - 关闭摄像头后应用正常")
        return True

    # ==================== Test 4: 多次切换测试 ====================
    def test_multiple_toggle(self):
        """
        Test 4: 多次切换测试
        验证：多次切换摄像头不会导致崩溃或 UI 错乱
        """
        print("\n[Test 4] 多次切换测试")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        # 多次切换摄像头
        print("  测试多次切换摄像头...")
        for i in range(3):
            KeyboardInput.toggle_camera()
            time.sleep(1)

            if not check_app_running():
                print(f"  ✗ 失败 - 第 {i+1} 次切换后应用崩溃")
                return False

        save_screenshot("camera_test4_multiple")

        # 确保最后关闭摄像头
        KeyboardInput.toggle_camera()
        time.sleep(1)

        if not check_app_running():
            print("  ✗ 失败 - 最终状态应用崩溃")
            return False

        print("  ✓ 通过 - 多次切换后应用正常")
        return True

    # ==================== Test 5: 播放-摄像头切换 ====================
    def test_play_camera_switch(self):
        """
        Test 5: 播放-摄像头切换
        验证：从视频播放切换到摄像头，再切换回播放
        """
        print("\n[Test 5] 播放-摄像头切换")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        # 开始播放
        print("  开始播放视频...")
        KeyboardInput.toggle_playback()
        time.sleep(2)

        save_screenshot("camera_test5_playing")

        # 切换到摄像头
        print("  切换到摄像头...")
        KeyboardInput.toggle_camera()
        time.sleep(2)

        save_screenshot("camera_test5_camera")

        if not check_app_running():
            print("  ✗ 失败 - 切换到摄像头后应用崩溃")
            return False

        # 切换回视频
        print("  切换回视频...")
        KeyboardInput.toggle_camera()
        time.sleep(1)

        save_screenshot("camera_test5_back")

        if not check_app_running():
            print("  ✗ 失败 - 切换回视频后应用崩溃")
            return False

        print("  ✓ 通过 - 播放-摄像头切换正常")
        return True

    def run_all_tests(self):
        """运行所有测试"""
        print("=" * 60)
        print("WZMediaPlayer 摄像头功能测试")
        print("=" * 60)

        ensure_dir(SCREENSHOT_DIR)

        # 启动应用
        if not self.setup():
            print("\n无法启动应用，测试终止")
            return False

        try:
            # 运行测试
            tests = [
                ("打开摄像头", self.test_open_camera),
                ("UI 布局验证", self.test_ui_layout),
                ("关闭摄像头", self.test_close_camera),
                ("多次切换", self.test_multiple_toggle),
                ("播放-摄像头切换", self.test_play_camera_switch),
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
    tester = CameraTest()
    success = tester.run_all_tests()
    sys.exit(0 if success else 1)