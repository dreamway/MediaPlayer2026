#!/usr/bin/env python3
"""
直接运行WZMediaPlayer并捕获输出来诊断崩溃
"""

import os
import sys
import time
import subprocess
import signal

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from config import (
    APP_PATH, TEST_VIDEO_60S_PATH, TEST_VIDEO_BBB_NORMAL_PATH
)
from core.keyboard_input import KeyboardInput


def main():
    print("=" * 60)
    print("WZMediaPlayer 崩溃诊断")
    print("=" * 60)

    # 直接启动应用并捕获输出
    print(f"\n启动应用: {APP_PATH}")
    print(f"视频1: {TEST_VIDEO_60S_PATH}")
    print(f"视频2: {TEST_VIDEO_BBB_NORMAL_PATH}")

    # 启动应用，捕获输出
    process = subprocess.Popen(
        [APP_PATH, TEST_VIDEO_60S_PATH],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1  # 行缓冲
    )

    print(f"应用已启动 (PID: {process.pid})")

    # 等待应用启动
    time.sleep(3.0)

    # 读取已有的输出
    print("\n--- 应用启动输出 ---")
    try:
        import select
        while True:
            readable, _, _ = select.select([process.stdout], [], [], 0.1)
            if not readable:
                break
            line = process.stdout.readline()
            if not line:
                break
            print(line.rstrip())
    except:
        pass

    # 播放第一个视频
    print("\n--- 播放第一个视频 ---")
    KeyboardInput.focus_app("WZMediaPlayer", delay=0.5)
    KeyboardInput.toggle_playback()
    time.sleep(3.0)

    # 执行seek操作
    print("\n--- 执行Seek操作 ---")
    for i in range(3):
        KeyboardInput.focus_app("WZMediaPlayer", delay=0.1)
        KeyboardInput.seek_forward()
        time.sleep(0.5)

    time.sleep(1.0)

    # 检查进程状态
    poll_result = process.poll()
    if poll_result is not None:
        print(f"\n!!! 应用已退出，退出码: {poll_result} !!!")
        # 读取剩余输出
        remaining_output = process.stdout.read()
        if remaining_output:
            print("\n--- 应用输出 ---")
            print(remaining_output)
        return

    # 切换到第二个视频
    print("\n--- 切换到第二个视频 ---")
    print("这是崩溃可能发生的时刻...")

    KeyboardInput.open_video_file(TEST_VIDEO_BBB_NORMAL_PATH, delay=3.0)

    # 持续监控进程状态
    print("\n--- 监控进程状态 ---")
    start_time = time.time()
    while time.time() - start_time < 10:
        poll_result = process.poll()
        if poll_result is not None:
            print(f"\n!!! 应用已退出，退出码: {poll_result} !!!")
            # 读取剩余输出
            remaining_output = process.stdout.read()
            if remaining_output:
                print("\n--- 应用剩余输出 ---")
                print(remaining_output)
            break

        # 读取新输出
        try:
            import select
            readable, _, _ = select.select([process.stdout], [], [], 0.5)
            if readable:
                line = process.stdout.readline()
                if line:
                    print(f"[LOG] {line.rstrip()}")
        except:
            pass

        time.sleep(0.1)

    # 如果还在运行，尝试正常关闭
    poll_result = process.poll()
    if poll_result is None:
        print("\n--- 应用仍在运行，尝试正常关闭 ---")
        KeyboardInput.focus_app("WZMediaPlayer", delay=0.3)
        subprocess.run(['osascript', '-e', 'tell application "WZMediaPlayer" to quit'], timeout=5)

        # 等待进程退出
        try:
            process.wait(timeout=5)
            print(f"应用正常退出，退出码: {process.returncode}")
        except subprocess.TimeoutExpired:
            print("应用未响应，强制终止")
            process.terminate()
            process.wait(timeout=2)

    # 读取最后的输出
    try:
        remaining = process.stdout.read()
        if remaining:
            print("\n--- 最终输出 ---")
            print(remaining)
    except:
        pass


if __name__ == "__main__":
    main()