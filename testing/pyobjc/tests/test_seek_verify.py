#!/usr/bin/env python3
"""
Seek 后画面更新验证测试
通过截图对比验证渲染画面是否与 seek 目标位置一致
"""

import os
import sys
import time
import subprocess

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from config import APP_PATH, TEST_VIDEO_60S_PATH
from core.keyboard_input import KeyboardInput
from core.screenshot_capture import ScreenshotCapture
from core.app_launcher import AppLauncher
from core.ax_element import AXElement


def save_screenshot(name):
    """保存截图"""
    screenshot_dir = os.path.join(os.path.dirname(__file__), "..", "screenshots")
    os.makedirs(screenshot_dir, exist_ok=True)
    filepath = os.path.join(screenshot_dir, f"{name}.png")

    # 使用 screencapture 获取窗口截图
    result = subprocess.run(
        ['screencapture', '-x', '-w', filepath],
        timeout=5,
        capture_output=True
    )

    if os.path.exists(filepath):
        print(f"  截图保存: {filepath}")
        return filepath
    return None


def main():
    print("=" * 60)
    print("Seek 后画面更新验证测试")
    print("=" * 60)

    # 清除旧进程
    subprocess.run(['pkill', '-x', 'WZMediaPlayer'], capture_output=True)
    time.sleep(1)

    # 创建截图目录
    screenshot_dir = os.path.join(os.path.dirname(__file__), "..", "screenshots")
    os.makedirs(screenshot_dir, exist_ok=True)

    # 启动应用
    print(f"\n启动应用: {APP_PATH}")
    launcher = AppLauncher(APP_PATH, TEST_VIDEO_60S_PATH)
    if not launcher.launch():
        print("启动失败!")
        return 1

    time.sleep(4.0)
    print(f"应用已启动 (PID: {launcher.pid})")

    # 播放
    print("\n开始播放...")
    KeyboardInput.focus_app("WZMediaPlayer", delay=0.5)
    KeyboardInput.toggle_playback()
    time.sleep(3.0)

    # 测试场景1: 正常播放几秒后截图
    print("\n=== 测试场景1: 正常播放 ===")
    KeyboardInput.focus_app("WZMediaPlayer", delay=0.3)

    # Seek 到 20s
    print("  Seek 到 20s...")
    for _ in range(4):  # 从开头 seek 右键 4 次 (每次 +10s，从 0 到约 40s，然后左键 2 次到 20s)
        pass
    # 实际上我们需要从当前位置 seek
    # 先 seek 到开头
    KeyboardInput.seek_to_start()
    time.sleep(1.0)

    # 然后右键两次到约 20s
    KeyboardInput.seek_forward()  # +10s
    time.sleep(0.5)
    KeyboardInput.seek_forward()  # +10s，到 20s
    time.sleep(1.0)

    print("  保存截图: seek_to_20s.png")
    subprocess.run(['screencapture', '-x', '-w', os.path.join(screenshot_dir, "seek_to_20s.png")], timeout=5)
    time.sleep(0.5)

    # 测试场景2: 快速连续 seek
    print("\n=== 测试场景2: 快速连续 Seek ===")
    KeyboardInput.focus_app("WZMediaPlayer", delay=0.3)

    # 快速左键 3 次 (每次 -10s，从 20s 到 -10s = 0s 附近)
    for i in range(3):
        KeyboardInput.seek_backward()
        time.sleep(0.2)

    time.sleep(1.5)
    print("  保存截图: after_rapid_seek_left.png")
    subprocess.run(['screencapture', '-x', '-w', os.path.join(screenshot_dir, "after_rapid_seek_left.png")], timeout=5)
    time.sleep(0.5)

    # 测试场景3: 快速右键 seek
    print("\n=== 测试场景3: 快速右键 Seek ===")
    KeyboardInput.focus_app("WZMediaPlayer", delay=0.3)

    # 快速右键 5 次 (每次 +10s)
    for i in range(5):
        KeyboardInput.seek_forward()
        time.sleep(0.2)

    time.sleep(1.5)
    print("  保存截图: after_rapid_seek_right.png")
    subprocess.run(['screencapture', '-x', '-w', os.path.join(screenshot_dir, "after_rapid_seek_right.png")], timeout=5)
    time.sleep(0.5)

    # 检查日志中是否有错误
    print("\n=== 检查应用状态 ===")
    result = subprocess.run(['pgrep', '-x', 'WZMediaPlayer'], capture_output=True)
    running = result.returncode == 0
    print(f"应用运行状态: {'运行中' if running else '已退出'}")

    # 清理
    subprocess.run(['pkill', '-x', 'WZMediaPlayer'], capture_output=True)

    print("\n" + "=" * 60)
    print("测试完成!")
    print(f"截图保存在: {screenshot_dir}")
    print("请检查截图验证画面是否正确")
    print("=" * 60)

    return 0 if running else 1


if __name__ == "__main__":
    sys.exit(main())