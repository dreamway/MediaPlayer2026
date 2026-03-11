#!/usr/bin/env python3
"""
压力测试 - 快速连续切换视频
"""

import os
import sys
import time
import subprocess

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from config import (
    APP_PATH, TEST_VIDEO_60S_PATH, TEST_VIDEO_BBB_NORMAL_PATH
)


def check_app_running():
    result = subprocess.run(['pgrep', '-x', 'WZMediaPlayer'], capture_output=True, text=True)
    return result.returncode == 0, result.stdout.strip()


def switch_video(video_path):
    """切换视频"""
    script = f'''
    tell application "System Events"
        set frontmost of process "WZMediaPlayer" to true
        delay 0.3
        keystroke "o" using command down
        delay 0.8
        keystroke "g" using {{command down, shift down}}
        delay 0.5
        keystroke "{video_path}"
        delay 0.3
        keystroke return
        delay 0.5
        keystroke return
    end tell
    '''
    try:
        subprocess.run(['osascript', '-e', script], timeout=15, capture_output=True)
        return True
    except:
        return False


def main():
    print("=" * 60)
    print("压力测试 - 快速连续切换视频")
    print("=" * 60)

    # 清除旧进程
    subprocess.run(['pkill', '-x', 'WZMediaPlayer'], capture_output=True)
    time.sleep(1)

    # 启动应用
    process = subprocess.Popen(
        [APP_PATH, TEST_VIDEO_60S_PATH],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True
    )

    print(f"应用已启动 (PID: {process.pid})")
    time.sleep(4.0)

    # 播放
    subprocess.run(['osascript', '-e', '''
    tell application "System Events"
        set frontmost of process "WZMediaPlayer" to true
        delay 0.3
        keystroke space
    end tell
    '''], timeout=5)

    time.sleep(2.0)

    # 快速切换视频
    videos = [TEST_VIDEO_60S_PATH, TEST_VIDEO_BBB_NORMAL_PATH]
    switch_count = 0

    print("\n开始快速切换视频...")
    for round_num in range(5):  # 5轮
        for i, video in enumerate(videos):
            switch_count += 1
            print(f"\n切换 #{switch_count} (轮 {round_num+1}, 视频 {i+1})")

            # 切换视频
            if not switch_video(video):
                print("  切换失败!")
                continue

            # 短暂等待
            time.sleep(1.0)

            # 检查状态
            running, pid = check_app_running()
            poll = process.poll()

            if not running or poll is not None:
                print(f"!!! 崩溃发生在切换 #{switch_count} !!!")
                print(f"  running={running}, poll={poll}")

                # 读取输出
                try:
                    output, _ = process.communicate(timeout=2)
                    print("\n--- 应用输出 (最后50行) ---")
                    lines = output.split('\n')
                    for line in lines[-50:]:
                        print(line)
                except:
                    pass

                # 检查崩溃日志
                result = subprocess.run(
                    ['find', '~/Library/Logs/DiagnosticReports', '-name', 'WZMediaPlayer*.ips', '-mmin', '-5'],
                    shell=True, capture_output=True, text=True
                )
                if result.stdout.strip():
                    print(f"\n崩溃日志: {result.stdout}")

                return

            print(f"  状态: running={running}")

            # 快速seek
            for _ in range(2):
                subprocess.run(['osascript', '-e', '''
                tell application "System Events"
                    set frontmost of process "WZMediaPlayer" to true
                    keystroke (ASCII character 30)
                end tell
                '''], timeout=3)
                time.sleep(0.3)

    print("\n" + "=" * 60)
    print(f"测试完成: {switch_count} 次切换，无崩溃")
    print("=" * 60)

    # 清理
    subprocess.run(['pkill', '-x', 'WZMediaPlayer'], capture_output=True)


if __name__ == "__main__":
    main()