#!/usr/bin/env python3
"""
视频切换崩溃诊断 - 捕获详细日志
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
    print("视频切换崩溃诊断")
    print("=" * 60)

    # 清除旧进程
    subprocess.run(['pkill', '-x', 'WZMediaPlayer'], capture_output=True)
    time.sleep(1)

    # 启动应用并捕获输出
    print(f"\n启动应用: {APP_PATH}")
    print(f"视频1: {TEST_VIDEO_60S_PATH}")

    process = subprocess.Popen(
        [APP_PATH, TEST_VIDEO_60S_PATH],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1
    )

    print(f"应用已启动 (PID: {process.pid})")

    # 收集输出
    all_output = []

    # 等待启动
    time.sleep(4.0)

    running, pid = check_app_running()
    print(f"启动后状态: running={running}, pid={pid}")

    if not running:
        print("应用启动后立即退出!")
        output, _ = process.communicate(timeout=5)
        print(output[-5000:] if len(output) > 5000 else output)
        return

    # 播放
    print("\n--- 播放第一个视频 ---")
    subprocess.run(['osascript', '-e', '''
    tell application "System Events"
        set frontmost of process "WZMediaPlayer" to true
        delay 0.3
        keystroke space
    end tell
    '''], timeout=5)
    time.sleep(2.0)

    # Seek
    print("\n--- Seek操作 ---")
    for i in range(3):
        subprocess.run(['osascript', '-e', '''
        tell application "System Events"
            set frontmost of process "WZMediaPlayer" to true
            delay 0.1
            keystroke (ASCII character 30)  -- right arrow
        end tell
        '''], timeout=5)
        time.sleep(0.5)

    time.sleep(1.0)

    # 切换视频
    print("\n--- 切换视频 ---")
    video_path = TEST_VIDEO_BBB_NORMAL_PATH
    print(f"目标视频: {video_path}")

    # 详细步骤
    switch_script = f'''
    tell application "System Events"
        -- 1. 激活应用
        set frontmost of process "WZMediaPlayer" to true
        delay 0.5

        -- 2. 打开文件对话框 (Cmd+O)
        keystroke "o" using command down
        delay 1.5

        -- 3. 前往文件夹 (Cmd+Shift+G)
        keystroke "g" using {{command down, shift down}}
        delay 1.0

        -- 4. 输入完整路径
        keystroke "{video_path}"
        delay 0.5

        -- 5. 确认路径
        keystroke return
        delay 1.0

        -- 6. 选择并打开文件
        keystroke return
        delay 1.0
    end tell
    '''

    print("执行视频切换脚本...")
    try:
        result = subprocess.run(
            ['osascript', '-e', switch_script],
            timeout=30,
            capture_output=True,
            text=True
        )
        print(f"AppleScript 返回码: {result.returncode}")
        if result.stderr:
            print(f"AppleScript 错误: {result.stderr}")
    except subprocess.TimeoutExpired:
        print("AppleScript 超时")
    except Exception as e:
        print(f"AppleScript 异常: {e}")

    # 持续监控
    print("\n--- 监控应用状态 ---")
    crash_time = None
    for i in range(20):
        running, pid = check_app_running()
        poll = process.poll()

        if not running or poll is not None:
            crash_time = time.time()
            print(f"!!! 应用已退出 (检查 {i+1}) !!!")
            print(f"  running={running}, pid={pid}, poll={poll}")
            break

        # 尝试读取输出
        try:
            import select
            readable, _, _ = select.select([process.stdout], [], [], 0.1)
            if readable:
                line = process.stdout.readline()
                if line:
                    all_output.append(line)
                    if any(kw in line.lower() for kw in ['error', 'warn', 'crash', 'exception', 'fatal', 'segfault', 'abort']):
                        print(f"  [LOG] {line.rstrip()}")
        except:
            pass

        print(f"  检查 {i+1}: running={running}, poll={poll}")
        time.sleep(1.0)

    # 读取剩余输出
    try:
        remaining, _ = process.communicate(timeout=2)
        if remaining:
            all_output.append(remaining)
    except:
        pass

    # 输出结果
    if crash_time or process.poll() is not None:
        print("\n" + "=" * 60)
        print("崩溃/退出时的日志:")
        print("=" * 60)
        full_output = ''.join(all_output)
        lines = full_output.split('\n')
        for line in lines[-100:]:
            print(line)
    else:
        print("\n应用仍在运行")
        # 关闭应用
        subprocess.run(['pkill', '-x', 'WZMediaPlayer'], capture_output=True)

    # 检查崩溃日志
    print("\n--- 检查崩溃日志 ---")
    result = subprocess.run(
        ['find', '~/Library/Logs/DiagnosticReports', '-name', 'WZMediaPlayer*.ips', '-mmin', '-5'],
        shell=True,
        capture_output=True,
        text=True
    )
    if result.stdout.strip():
        print("找到崩溃日志:")
        print(result.stdout)
    else:
        print("未找到最近的崩溃日志")


if __name__ == "__main__":
    main()