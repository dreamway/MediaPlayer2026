#!/usr/bin/env python3
"""
完整稳定性测试 - 带详细日志捕获
"""

import os
import sys
import time
import subprocess
import signal

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from config import (
    APP_PATH, TEST_VIDEO_60S_PATH, TEST_VIDEO_BBB_NORMAL_PATH, TEST_VIDEO_BBB_STEREO_PATH
)
from core.keyboard_input import KeyboardInput
from core.screenshot_capture import ScreenshotCapture, ImageAnalyzer
from config import SCREENSHOT_DIR, BLACK_THRESHOLD


def check_app_running():
    """检查应用是否在运行"""
    result = subprocess.run(
        ['pgrep', '-x', 'WZMediaPlayer'],
        capture_output=True,
        text=True
    )
    return result.returncode == 0


def main():
    print("=" * 60)
    print("完整稳定性测试")
    print("=" * 60)

    # 启动应用
    print(f"\n启动应用: {APP_PATH}")
    process = subprocess.Popen(
        [APP_PATH, TEST_VIDEO_60S_PATH],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1
    )

    print(f"应用已启动 (PID: {process.pid})")
    time.sleep(5.0)

    # 测试视频切换
    videos = [TEST_VIDEO_60S_PATH, TEST_VIDEO_BBB_NORMAL_PATH, TEST_VIDEO_BBB_STEREO_PATH]

    for round_num in range(3):  # 3轮视频切换
        print(f"\n=== 第 {round_num + 1} 轮视频切换测试 ===")

        for i, video_path in enumerate(videos):
            if not os.path.exists(video_path):
                continue

            print(f"\n切换到视频 {i+1}/{len(videos)}: {os.path.basename(video_path)}")

            # 检查进程状态
            poll_result = process.poll()
            if poll_result is not None:
                print(f"\n!!! 应用已退出，退出码: {poll_result} !!!")
                # 读取剩余输出
                remaining_output = process.stdout.read()
                if remaining_output:
                    print("\n--- 应用剩余输出 ---")
                    print(remaining_output[-5000:])  # 最后5000字符
                return False

            # 打开视频
            KeyboardInput.open_video_file(video_path, delay=3.0)

            # 检查应用是否还在运行
            if not check_app_running():
                print(f"\n!!! 应用在切换视频时崩溃 !!!")
                poll_result = process.poll()
                if poll_result is not None:
                    print(f"退出码: {poll_result}")
                remaining_output = process.stdout.read()
                if remaining_output:
                    print("\n--- 崩溃前日志 ---")
                    print(remaining_output[-5000:])
                return False

            # 播放一段时间
            KeyboardInput.focus_app("WZMediaPlayer", delay=0.3)
            KeyboardInput.toggle_playback()
            time.sleep(2.0)

            # 执行seek操作
            for j in range(3):
                KeyboardInput.focus_app("WZMediaPlayer", delay=0.1)
                KeyboardInput.seek_forward()
                time.sleep(0.3)

            time.sleep(1.0)

            # 暂停
            KeyboardInput.focus_app("WZMediaPlayer", delay=0.1)
            KeyboardInput.toggle_playback()
            time.sleep(0.5)

    print("\n" + "=" * 60)
    print("测试完成 - 应用稳定运行")
    print("=" * 60)

    # 关闭应用
    subprocess.run(['osascript', '-e', 'tell application "WZMediaPlayer" to quit'], timeout=5)
    process.wait(timeout=5)
    return True


if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)