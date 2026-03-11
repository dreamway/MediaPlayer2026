#!/usr/bin/env python3
"""
使用 lldb 调试崩溃
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


def main():
    print("=" * 60)
    print("LLDB 调试崩溃")
    print("=" * 60)

    # 先杀掉现有进程
    subprocess.run(['pkill', '-x', 'WZMediaPlayer'], capture_output=True)
    time.sleep(1)

    # 创建 lldb 命令文件
    lldb_commands = f'''
env DYLD_LIBRARY_PATH=/Users/jingwenlai/leadwit-tech/WeiZheng/MediaPlayer2026/build/WZMediaPlayer.app/Contents/MacOS
run {TEST_VIDEO_60S_PATH}
bt
quit
'''

    lldb_cmd_file = '/tmp/lldb_commands.txt'
    with open(lldb_cmd_file, 'w') as f:
        f.write(lldb_commands)

    print(f"\n启动 LLDB...")
    print("这将在应用启动后自动退出，无法进行交互操作")
    print("")

    # 注意：lldb 交互模式下无法自动进行视频切换
    # 让我们使用另一种方式：运行应用并等待崩溃

    # 使用进程替代方式捕获崩溃
    process = subprocess.Popen(
        [APP_PATH, TEST_VIDEO_60S_PATH],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )

    print(f"应用已启动 (PID: {process.pid})")
    time.sleep(5.0)

    # 播放
    KeyboardInput.focus_app("WZMediaPlayer", delay=0.5)
    KeyboardInput.toggle_playback()
    time.sleep(2.0)

    # Seek
    for i in range(3):
        KeyboardInput.focus_app("WZMediaPlayer", delay=0.1)
        KeyboardInput.seek_forward()
        time.sleep(0.5)

    time.sleep(1.0)

    print("\n切换到第二个视频...")
    KeyboardInput.open_video_file(TEST_VIDEO_BBB_NORMAL_PATH, delay=3.0)

    # 等待进程退出
    print("等待进程退出...")
    try:
        stdout, stderr = process.communicate(timeout=15)
        print(f"\n进程退出，返回码: {process.returncode}")

        if stdout:
            print("\n=== STDOUT ===")
            print(stdout[-5000:] if len(stdout) > 5000 else stdout)

        if stderr:
            print("\n=== STDERR ===")
            print(stderr[-5000:] if len(stderr) > 5000 else stderr)

    except subprocess.TimeoutExpired:
        print("进程未在超时时间内退出，可能仍在运行")
        process.terminate()

    # 检查是否有崩溃报告
    print("\n检查崩溃报告...")
    result = subprocess.run(
        ['ls', '-lt', '~/Library/Logs/DiagnosticReports/'],
        shell=True,
        capture_output=True,
        text=True
    )
    lines = result.stdout.split('\n')
    for line in lines[:10]:
        if 'WZMedia' in line or 'ips' in line:
            print(line)


if __name__ == "__main__":
    main()