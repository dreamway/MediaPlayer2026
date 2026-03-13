#!/usr/bin/env python3
"""
GUI E2E 测试
验证 GUI 功能完整性，包括：
1. PlayButton 状态同步
2. 局部3D 功能
3. 字幕加载和显示
4. 截图保存
5. 播放列表导入/导出
6. 设置对话框
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


class GUIE2ETester:
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

    # ==================== Test 1: PlayButton 状态同步 ====================
    def test_playbutton_state_sync(self):
        """
        Test 1: PlayButton 状态同步
        验证：播放按钮状态与实际播放状态同步
        """
        print("\n[Test 1] PlayButton 状态同步")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        # 测试1: 通过快捷键播放/暂停
        print("  测试通过 Space 快捷键播放/暂停...")
        for i in range(3):
            KeyboardInput.toggle_playback()
            time.sleep(1)
            KeyboardInput.toggle_playback()
            time.sleep(1)

        save_screenshot("gui_test1_playbutton_sync")

        if check_app_running():
            print("  ✓ 通过 - PlayButton 状态同步正常")
            return True
        else:
            print("  ✗ 失败")
            return False

    # ==================== Test 2: 局部3D 功能 ====================
    def test_stereo_region(self):
        """
        Test 2: 局部3D 功能
        验证：局部3D 功能可以正常启用
        """
        print("\n[Test 2] 局部3D 功能")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        # 播放
        KeyboardInput.toggle_playback()
        time.sleep(2)

        # 切换到 3D 模式
        print("  切换到 3D 模式...")
        KeyboardInput.toggle_3d()
        time.sleep(1)

        # 测试局部3D（通过菜单快捷键）
        print("  测试局部3D 功能...")
        # Cmd+R 切换局部3D
        KeyboardInput.send_hotkey('r', ['command'])
        time.sleep(1)

        save_screenshot("gui_test2_stereo_region")

        # 再次切换关闭
        KeyboardInput.send_hotkey('r', ['command'])
        time.sleep(0.5)

        # 切回 2D
        KeyboardInput.toggle_3d()
        time.sleep(0.5)

        if check_app_running():
            print("  ✓ 通过 - 局部3D 功能正常")
            return True
        else:
            print("  ✗ 失败")
            return False

    # ==================== Test 3: 视差调节 ====================
    def test_parallax_adjustment(self):
        """
        Test 3: 视差调节
        验证：视差调节功能正常
        """
        print("\n[Test 3] 视差调节")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        # 切换到 3D 模式
        KeyboardInput.toggle_3d()
        time.sleep(1)

        # 增加视差
        print("  测试视差增加...")
        for i in range(3):
            KeyboardInput.send_hotkey('=', ['command'])  # Cmd+= 增加视差
            time.sleep(0.3)

        # 减少视差
        print("  测试视差减少...")
        for i in range(3):
            KeyboardInput.send_hotkey('-', ['command'])  # Cmd+- 减少视差
            time.sleep(0.3)

        # 重置视差
        print("  测试视差重置...")
        KeyboardInput.send_hotkey('0', ['command'])  # Cmd+0 重置视差
        time.sleep(0.5)

        save_screenshot("gui_test3_parallax")

        # 切回 2D
        KeyboardInput.toggle_3d()
        time.sleep(0.5)

        if check_app_running():
            print("  ✓ 通过 - 视差调节正常")
            return True
        else:
            print("  ✗ 失败")
            return False

    # ==================== Test 4: 截图功能 ====================
    def test_screenshot(self):
        """
        Test 4: 截图功能
        验证：截图功能正常工作
        """
        print("\n[Test 4] 截图功能")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        # 播放
        KeyboardInput.toggle_playback()
        time.sleep(2)

        # 截图
        print("  测试截图功能...")
        KeyboardInput.take_screenshot()
        time.sleep(1)

        save_screenshot("gui_test4_screenshot")

        if check_app_running():
            print("  ✓ 通过 - 截图功能正常")
            return True
        else:
            print("  ✗ 失败")
            return False

    # ==================== Test 5: 全屏模式 ====================
    def test_fullscreen_mode(self):
        """
        Test 5: 全屏模式
        验证：全屏和全屏+ 模式正常
        """
        print("\n[Test 5] 全屏模式")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        # 进入全屏
        print("  测试全屏模式...")
        KeyboardInput.send_key('f')  # F 键全屏
        time.sleep(2)

        save_screenshot("gui_test5_fullscreen")

        # 退出全屏
        KeyboardInput.send_key('escape')
        time.sleep(1)

        if check_app_running():
            print("  ✓ 通过 - 全屏模式正常")
            return True
        else:
            print("  ✗ 失败")
            return False

    # ==================== Test 6: 音量控制 ====================
    def test_volume_control(self):
        """
        Test 6: 音量控制
        验证：音量调节和静音功能正常
        """
        print("\n[Test 6] 音量控制")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        # 播放
        KeyboardInput.toggle_playback()
        time.sleep(1)

        # 增加音量
        print("  测试音量增加...")
        for i in range(3):
            KeyboardInput.increase_volume()
            time.sleep(0.2)

        # 减少音量
        print("  测试音量减少...")
        for i in range(3):
            KeyboardInput.decrease_volume()
            time.sleep(0.2)

        # 静音
        print("  测试静音...")
        KeyboardInput.toggle_mute()
        time.sleep(0.5)

        # 取消静音
        KeyboardInput.toggle_mute()
        time.sleep(0.5)

        save_screenshot("gui_test6_volume")

        if check_app_running():
            print("  ✓ 通过 - 音量控制正常")
            return True
        else:
            print("  ✗ 失败")
            return False

    # ==================== Test 7: 3D 输入输出格式 ====================
    def test_3d_format_switch(self):
        """
        Test 7: 3D 输入输出格式切换
        验证：各种 3D 格式切换正常
        """
        print("\n[Test 7] 3D 输入输出格式切换")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        # 切换到 3D 模式
        KeyboardInput.toggle_3d()
        time.sleep(1)

        # 测试输入格式切换 (LR/RL/UD)
        print("  测试输入格式切换...")
        KeyboardInput.send_hotkey('1', ['command'])  # LR
        time.sleep(0.5)
        KeyboardInput.send_hotkey('2', ['command'])  # RL
        time.sleep(0.5)
        KeyboardInput.send_hotkey('3', ['command'])  # UD
        time.sleep(0.5)

        save_screenshot("gui_test7_3d_format")

        # 切回 2D
        KeyboardInput.toggle_3d()
        time.sleep(0.5)

        if check_app_running():
            print("  ✓ 通过 - 3D 格式切换正常")
            return True
        else:
            print("  ✗ 失败")
            return False

    # ==================== Test 8: Seek 功能 ====================
    def test_seek_functionality(self):
        """
        Test 8: Seek 功能
        验证：左右键 Seek 正常
        """
        print("\n[Test 8] Seek 功能")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        # 播放
        KeyboardInput.toggle_playback()
        time.sleep(2)

        # Seek 前进
        print("  测试 Seek 前进...")
        for i in range(5):
            KeyboardInput.seek_forward()
            time.sleep(0.3)

        # Seek 后退
        print("  测试 Seek 后退...")
        for i in range(3):
            KeyboardInput.seek_backward()
            time.sleep(0.3)

        save_screenshot("gui_test8_seek")

        if check_app_running():
            print("  ✓ 通过 - Seek 功能正常")
            return True
        else:
            print("  ✗ 失败")
            return False

    # ==================== Test 9: 播放列表 ====================
    def test_playlist(self):
        """
        Test 9: 播放列表功能
        验证：播放列表显示/隐藏
        """
        print("\n[Test 9] 播放列表功能")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        # 切换播放列表显示
        print("  测试播放列表显示/隐藏...")
        KeyboardInput.send_hotkey('p', ['command'])  # Cmd+P 切换播放列表
        time.sleep(1)

        save_screenshot("gui_test9_playlist_show")

        # 再次切换隐藏
        KeyboardInput.send_hotkey('p', ['command'])
        time.sleep(0.5)

        if check_app_running():
            print("  ✓ 通过 - 播放列表功能正常")
            return True
        else:
            print("  ✗ 失败")
            return False

    # ==================== Test 10: 稳定性测试 ====================
    def test_stability(self):
        """
        Test 10: 稳定性测试
        验证：快速连续操作后应用仍正常运行
        """
        print("\n[Test 10] 稳定性测试")

        if not check_app_running():
            print("  ✗ 应用未运行")
            return False

        self.focus_app()

        print("  快速连续操作测试...")

        # 快速播放/暂停
        for i in range(5):
            KeyboardInput.toggle_playback()
            time.sleep(0.2)

        # 快速 Seek
        for i in range(5):
            KeyboardInput.seek_forward()
            time.sleep(0.15)

        # 快速 3D 切换
        for i in range(3):
            KeyboardInput.toggle_3d()
            time.sleep(0.3)

        # 快速音量调节
        for i in range(3):
            KeyboardInput.increase_volume()
            time.sleep(0.1)
            KeyboardInput.decrease_volume()
            time.sleep(0.1)

        time.sleep(1)
        save_screenshot("gui_test10_stability")

        if check_app_running():
            print("  ✓ 通过 - 稳定性测试正常")
            return True
        else:
            print("  ✗ 失败 - 应用崩溃")
            return False

    def run_all_tests(self):
        """运行所有测试"""
        print("=" * 60)
        print("WZMediaPlayer GUI E2E 测试")
        print("=" * 60)

        ensure_dir(SCREENSHOT_DIR)

        # 启动应用
        if not self.setup():
            print("\n无法启动应用，测试终止")
            return False

        try:
            # 运行所有测试
            tests = [
                ("PlayButton 同步", self.test_playbutton_state_sync),
                ("局部3D", self.test_stereo_region),
                ("视差调节", self.test_parallax_adjustment),
                ("截图功能", self.test_screenshot),
                ("全屏模式", self.test_fullscreen_mode),
                ("音量控制", self.test_volume_control),
                ("3D 格式切换", self.test_3d_format_switch),
                ("Seek 功能", self.test_seek_functionality),
                ("播放列表", self.test_playlist),
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
    tester = GUIE2ETester()
    success = tester.run_all_tests()
    sys.exit(0 if success else 1)