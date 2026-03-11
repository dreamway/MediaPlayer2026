#!/usr/bin/env python3
"""
详细崩溃诊断 - 捕获完整应用输出
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
    print("详细崩溃诊断")
    print("=" * 60)

    # 直接启动应用并捕获所有输出
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

    # 收集所有输出的列表
    all_output = []

    # 等待应用启动
    time.sleep(3.0)

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
            print(remaining_output[-10000:])  # 最后10000字符
        return

    # 切换到第二个视频
    print("\n--- 切换到第二个视频 (关键点) ---")

    # 在切换前记录时间
    switch_start = time.time()

    KeyboardInput.open_video_file(TEST_VIDEO_BBB_NORMAL_PATH, delay=3.0)

    # 持续监控进程状态
    print("\n--- 监控进程状态 ---")
    crash_detected = False
    start_time = time.time()

    while time.time() - start_time < 15:
        poll_result = process.poll()
        if poll_result is not None:
            print(f"\n!!! 应用已退出，退出码: {poll_result} !!!")
            crash_detected = True
            break

        # 读取新输出
        try:
            import select
            readable, _, _ = select.select([process.stdout], [], [], 0.5)
            if readable:
                line = process.stdout.readline()
                if line:
                    all_output.append(line)
                    # 打印关键日志
                    if any(keyword in line.lower() for keyword in ['error', 'warn', 'crash', 'exception', 'fatal', 'segfault', 'abort']):
                        print(f"[CRITICAL] {line.rstrip()}")
        except:
            pass

        time.sleep(0.1)

    if crash_detected:
        # 读取剩余输出
        remaining_output = process.stdout.read()
        if remaining_output:
            all_output.append(remaining_output)

        print("\n" + "=" * 60)
        print("崩溃时的完整日志 (最后200行):")
        print("=" * 60)

        # 合并所有输出
        full_output = ''.join(all_output)
        lines = full_output.split('\n')
        for line in lines[-200:]:
            print(line)
    else:
        print("\n应用在监控期间未崩溃")

        # 尝试正常关闭
        print("\n--- 尝试正常关闭 ---")
        subprocess.run(['osascript', '-e', 'tell application "WZMediaPlayer" to quit'], timeout=5)

        try:
            process.wait(timeout=5)
            print(f"应用正常退出，退出码: {process.returncode}")
        except subprocess.TimeoutExpired:
            print("应用未响应，强制终止")
            process.terminate()
            process.wait(timeout=2)


if __name__ == "__main__":
    main()