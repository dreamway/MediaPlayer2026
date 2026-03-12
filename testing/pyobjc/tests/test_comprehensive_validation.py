#!/usr/bin/env python3
"""
综合功能验证测试
测试所有主要功能并截图验证
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


def get_app_window_id():
    """获取应用窗口ID用于截图"""
    try:
        # 使用 AppleScript 获取 WZMediaPlayer 的窗口
        script = '''
        tell application "System Events"
            set appList to every process whose name is "WZMediaPlayer"
            if length of appList > 0 then
                set appProcess to item 1 of appList
                set winList to every window of appProcess
                if length of winList > 0 then
                    return name of item 1 of winList
                end if
            end if
            return ""
        end tell
        '''
        result = subprocess.run(['osascript', '-e', script], capture_output=True, text=True, timeout=5)
        return result.stdout.strip()
    except:
        return None


def save_screenshot(name):
    """保存截图 - 使用全屏截图方式"""
    filepath = os.path.join(SCREENSHOT_DIR, f"{name}.png")
    try:
        # 使用全屏截图（不等待窗口选择）
        result = subprocess.run(['screencapture', '-x', filepath], timeout=10, capture_output=True)
        if os.path.exists(filepath):
            print(f"  ✓ 截图保存: {filepath}")
            return filepath
        else:
            print(f"  ✗ 截图失败: {filepath}")
            return None
    except subprocess.TimeoutExpired:
        print(f"  ✗ 截图超时")
        return None


def test_basic_playback():
    """测试1: 基本播放功能"""
    print("\n" + "=" * 60)
    print("测试1: 基本播放功能")
    print("=" * 60)

    if not check_app_running():
        print("  ✗ 应用未运行，跳过测试")
        return False

    # 聚焦应用
    KeyboardInput.focus_app("WZMediaPlayer", delay=0.5)

    # 播放
    print("  按下空格键播放...")
    KeyboardInput.toggle_playback()
    time.sleep(3)

    # 截图
    save_screenshot("test1_playback_playing")

    # 暂停
    print("  按下空格键暂停...")
    KeyboardInput.toggle_playback()
    time.sleep(1)

    save_screenshot("test1_playback_paused")

    return True


def test_seek_functionality():
    """测试2: Seek 功能"""
    print("\n" + "=" * 60)
    print("测试2: Seek 功能")
    print("=" * 60)

    if not check_app_running():
        print("  ✗ 应用未运行，跳过测试")
        return False

    # 聚焦应用
    KeyboardInput.focus_app("WZMediaPlayer", delay=0.5)

    # 先播放
    KeyboardInput.toggle_playback()
    time.sleep(2)

    # Seek 到开头
    print("  Seek 到开头...")
    KeyboardInput.seek_to_start()
    time.sleep(1.5)
    save_screenshot("test2_seek_start")

    # Seek 右 5 秒
    print("  Seek 右 5 秒...")
    KeyboardInput.seek_forward()
    time.sleep(1)
    save_screenshot("test2_seek_forward")

    # 再 Seek 右 5 秒
    print("  再 Seek 右 5 秒...")
    KeyboardInput.seek_forward()
    time.sleep(1)
    save_screenshot("test2_seek_forward2")

    # Seek 左 5 秒
    print("  Seek 左 5 秒...")
    KeyboardInput.seek_backward()
    time.sleep(1)
    save_screenshot("test2_seek_backward")

    # 快速连续 Seek
    print("  快速连续 Seek 右 5 次...")
    for i in range(5):
        KeyboardInput.seek_forward()
        time.sleep(0.2)
    time.sleep(2)
    save_screenshot("test2_seek_rapid")

    # 暂停
    KeyboardInput.toggle_playback()
    time.sleep(0.5)

    return True


def test_eof_seek():
    """测试3: EOF 后 Seek 功能"""
    print("\n" + "=" * 60)
    print("测试3: EOF 后 Seek 功能")
    print("=" * 60)

    if not check_app_running():
        print("  ✗ 应用未运行，跳过测试")
        return False

    # 聚焦应用
    KeyboardInput.focus_app("WZMediaPlayer", delay=0.5)

    # 播放一会儿
    KeyboardInput.toggle_playback()
    time.sleep(2)

    # 多次 seek 右到接近末尾
    print("  Seek 到接近末尾...")
    for i in range(15):
        KeyboardInput.seek_forward()
        time.sleep(0.2)
    time.sleep(3)

    save_screenshot("test3_near_eof")

    # 等待可能的 EOF
    print("  等待 EOF...")
    time.sleep(5)

    # 检查应用是否仍在运行
    if not check_app_running():
        print("  ✗ 应用在到达 EOF 后崩溃!")
        save_screenshot("test3_crashed")
        return False

    save_screenshot("test3_after_eof")

    # 尝试从 EOF seek 回来
    print("  从 EOF 位置 Seek 左...")
    KeyboardInput.seek_backward()
    time.sleep(2)
    save_screenshot("test3_seek_from_eof")

    # 再试一次
    KeyboardInput.seek_backward()
    time.sleep(2)
    save_screenshot("test3_seek_from_eof2")

    # 检查应用是否仍在运行
    if not check_app_running():
        print("  ✗ 应用在 EOF seek 后崩溃!")
        return False

    print("  ✓ EOF seek 测试完成，应用仍正常运行")
    return True


def test_3d_mode_switch():
    """测试4: 3D 模式切换"""
    print("\n" + "=" * 60)
    print("测试4: 3D 模式切换")
    print("=" * 60)

    if not check_app_running():
        print("  ✗ 应用未运行，跳过测试")
        return False

    # 聚焦应用
    KeyboardInput.focus_app("WZMediaPlayer", delay=0.5)

    # 确保在播放
    KeyboardInput.toggle_playback()
    time.sleep(1)

    # 切换到 2D/3D 模式
    print("  切换 2D/3D 模式 (Cmd+1)...")
    KeyboardInput.toggle_3d()
    time.sleep(1.5)
    save_screenshot("test4_3d_mode_toggled")

    # 再切换回来
    print("  切换回 2D 模式...")
    KeyboardInput.toggle_3d()
    time.sleep(1.5)
    save_screenshot("test4_2d_mode_restored")

    # 暂停
    KeyboardInput.toggle_playback()
    time.sleep(0.5)

    return True


def test_volume_control():
    """测试5: 音量控制"""
    print("\n" + "=" * 60)
    print("测试5: 音量控制")
    print("=" * 60)

    if not check_app_running():
        print("  ✗ 应用未运行，跳过测试")
        return False

    # 聚焦应用
    KeyboardInput.focus_app("WZMediaPlayer", delay=0.5)

    # 播放
    KeyboardInput.toggle_playback()
    time.sleep(1)

    # 音量增加
    print("  音量增加...")
    for i in range(3):
        KeyboardInput.increase_volume()
        time.sleep(0.3)
    time.sleep(0.5)
    save_screenshot("test5_volume_up")

    # 音量减少
    print("  音量减少...")
    for i in range(5):
        KeyboardInput.decrease_volume()
        time.sleep(0.3)
    time.sleep(0.5)
    save_screenshot("test5_volume_down")

    # 静音切换
    print("  静音切换...")
    KeyboardInput.toggle_mute()
    time.sleep(0.5)
    save_screenshot("test5_muted")

    # 取消静音
    KeyboardInput.toggle_mute()
    time.sleep(0.5)
    save_screenshot("test5_unmuted")

    # 暂停
    KeyboardInput.toggle_playback()
    time.sleep(0.5)

    return True


def test_stability():
    """测试6: 稳定性测试（快速操作）"""
    print("\n" + "=" * 60)
    print("测试6: 稳定性测试（快速操作）")
    print("=" * 60)

    if not check_app_running():
        print("  ✗ 应用未运行，跳过测试")
        return False

    # 聚焦应用
    KeyboardInput.focus_app("WZMediaPlayer", delay=0.5)

    # 快速播放/暂停
    print("  快速播放/暂停 5 次...")
    for i in range(5):
        KeyboardInput.toggle_playback()
        time.sleep(0.3)
    time.sleep(1)
    save_screenshot("test6_playback_toggle")

    # 快速 seek
    print("  快速 Seek 左右交替...")
    for i in range(10):
        if i % 2 == 0:
            KeyboardInput.seek_forward()
        else:
            KeyboardInput.seek_backward()
        time.sleep(0.2)
    time.sleep(2)
    save_screenshot("test6_rapid_seek")

    # 快速 3D 模式切换
    print("  快速 3D 模式切换...")
    for i in range(3):
        KeyboardInput.toggle_3d()
        time.sleep(0.5)
    time.sleep(1)
    save_screenshot("test6_3d_toggle")

    # 检查应用是否仍然运行
    if check_app_running():
        print("  ✓ 应用仍然运行中")
        return True
    else:
        print("  ✗ 应用已崩溃退出")
        return False


def run_all_tests():
    """运行所有测试"""
    print("=" * 60)
    print("WZMediaPlayer 综合功能验证测试")
    print("=" * 60)

    ensure_dir(SCREENSHOT_DIR)

    # 清除旧进程
    print("\n清除旧进程...")
    kill_app()

    # 启动应用
    print(f"\n启动应用: {APP_PATH}")
    print(f"测试视频: {TEST_VIDEO_BBB_NORMAL_PATH}")

    if not os.path.exists(APP_PATH):
        print(f"✗ 应用不存在: {APP_PATH}")
        return False

    if not os.path.exists(TEST_VIDEO_BBB_NORMAL_PATH):
        print(f"✗ 测试视频不存在: {TEST_VIDEO_BBB_NORMAL_PATH}")
        return False

    launcher = AppLauncher(APP_PATH)
    if not launcher.launch([TEST_VIDEO_BBB_NORMAL_PATH]):
        print("✗ 启动失败!")
        return False

    print(f"应用已启动 (PID: {launcher.get_pid()})")
    time.sleep(5)  # 等待应用完全加载

    # 验证应用正在运行
    if not check_app_running():
        print("✗ 应用启动后立即退出")
        return False

    # 运行测试
    results = {}

    tests = [
        ("基本播放", test_basic_playback),
        ("Seek 功能", test_seek_functionality),
        ("EOF 后 Seek", test_eof_seek),
        ("3D 模式切换", test_3d_mode_switch),
        ("音量控制", test_volume_control),
        ("稳定性测试", test_stability),
    ]

    for name, test_func in tests:
        try:
            if check_app_running():
                results[name] = test_func()
            else:
                print(f"\n✗ 应用已退出，停止测试")
                results[name] = False
                break
        except Exception as e:
            print(f"  ✗ 测试异常: {e}")
            results[name] = False

        # 每个测试后检查应用状态
        if not check_app_running():
            print(f"\n✗ 应用在测试 '{name}' 后崩溃")
            break

    # 最终检查
    print("\n" + "=" * 60)
    print("测试结果汇总")
    print("=" * 60)

    for name, result in results.items():
        status = "✓ 通过" if result else "✗ 失败"
        print(f"  {name}: {status}")

    # 检查应用最终状态
    if check_app_running():
        print("\n✓ 应用仍在运行")
        # 清理
        kill_app()
    else:
        print("\n✗ 应用已退出")

    print("\n" + "=" * 60)
    print(f"截图保存在: {SCREENSHOT_DIR}")
    print("=" * 60)

    return all(results.values()) if results else False


if __name__ == "__main__":
    success = run_all_tests()
    sys.exit(0 if success else 1)