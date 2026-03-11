#!/usr/bin/env python3
"""
精确崩溃诊断 - 一步步跟踪
"""

import os
import sys
import time
import subprocess

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from config import (
    APP_PATH, TEST_VIDEO_60S_PATH, TEST_VIDEO_BBB_NORMAL_PATH
)
from core.keyboard_input import KeyboardInput


def check_app_running():
    """检查应用是否在运行"""
    result = subprocess.run(
        ['pgrep', '-x', 'WZMediaPlayer'],
        capture_output=True,
        text=True
    )
    pid = result.stdout.strip()
    return result.returncode == 0, pid


def main():
    print("=" * 60)
    print("精确崩溃诊断")
    print("=" * 60)

    # 确保没有其他实例
    subprocess.run(['pkill', '-x', 'WZMediaPlayer'], capture_output=True)
    time.sleep(1)

    # 启动应用
    print(f"\n启动应用: {APP_PATH}")
    process = subprocess.Popen(
        [APP_PATH, TEST_VIDEO_60S_PATH],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL
    )

    print(f"应用已启动 (PID: {process.pid})")
    time.sleep(5.0)

    # 检查应用状态
    running, pid = check_app_running()
    print(f"检查应用状态: running={running}, pid={pid}")

    if not running:
        print("错误: 应用启动后立即退出")
        return

    # 播放第一个视频
    print("\n--- 播放第一个视频 ---")
    KeyboardInput.focus_app("WZMediaPlayer", delay=0.5)
    KeyboardInput.toggle_playback()
    time.sleep(2.0)

    running, pid = check_app_running()
    print(f"播放后状态: running={running}, pid={pid}")

    # 执行seek操作
    print("\n--- 执行Seek操作 ---")
    for i in range(3):
        KeyboardInput.focus_app("WZMediaPlayer", delay=0.1)
        KeyboardInput.seek_forward()
        time.sleep(0.5)

    time.sleep(1.0)

    running, pid = check_app_running()
    print(f"Seek后状态: running={running}, pid={pid}")

    # 切换到第二个视频
    print("\n--- 切换到第二个视频 ---")

    # 在切换前检查
    running, pid = check_app_running()
    print(f"切换前状态: running={running}, pid={pid}")

    # 使用 AppleScript 直接打开文件
    video_path = TEST_VIDEO_BBB_NORMAL_PATH

    script = f'''
    tell application "System Events"
        set frontmost of process "WZMediaPlayer" to true
        delay 0.3
        keystroke "o" using command down
        delay 1.0
        keystroke "g" using {{command down, shift down}}
        delay 0.5
        keystroke "{video_path}"
        delay 0.3
        keystroke return
        delay 0.5
        keystroke return
    end tell
    '''

    print("执行 AppleScript 切换视频...")
    try:
        result = subprocess.run(['osascript', '-e', script], timeout=15, capture_output=True, text=True)
        print(f"AppleScript 退出码: {result.returncode}")
        if result.returncode != 0:
            print(f"AppleScript 错误: {result.stderr}")
    except subprocess.TimeoutExpired:
        print("AppleScript 超时")
    except Exception as e:
        print(f"AppleScript 异常: {e}")

    # 等待
    time.sleep(3.0)

    # 关键检查
    print("\n--- 关键检查 ---")
    for i in range(5):
        running, pid = check_app_running()
        poll = process.poll()
        print(f"  检查 {i+1}: running={running}, pid={pid}, poll={poll}")
        if not running:
            print("  !!! 应用已退出 !!!")
            break
        time.sleep(1.0)

    # 最终状态
    running, pid = check_app_running()
    poll = process.poll()

    print("\n" + "=" * 60)
    print(f"最终状态: running={running}, pid={pid}, poll={poll}")
    print("=" * 60)

    # 清理
    if running:
        subprocess.run(['pkill', '-x', 'WZMediaPlayer'], capture_output=True)


if __name__ == "__main__":
    main()