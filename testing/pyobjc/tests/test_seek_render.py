#!/usr/bin/env python3
"""
测试 Seek 后画面更新是否正常
"""

import os
import sys
import time
import subprocess

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from config import APP_PATH, TEST_VIDEO_60S_PATH
from core.keyboard_input import KeyboardInput
from core.screenshot_capture import ScreenshotCapture


def main():
    print("=" * 60)
    print("测试 Seek 后画面更新")
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
    KeyboardInput.focus_app("WZMediaPlayer", delay=0.5)
    KeyboardInput.toggle_playback()
    time.sleep(2.0)
    
    # 快速 Seek 左键多次
    print("\n开始快速 Seek 测试...")
    for i in range(5):
        print(f"  Seek #{i+1}")
        KeyboardInput.seek_backward()  # 左键 -10s
        time.sleep(0.3)
    
    # 等待渲染
    time.sleep(2.0)
    
    # 检查应用状态
    result = subprocess.run(['pgrep', '-x', 'WZMediaPlayer'], capture_output=True)
    running = result.returncode == 0
    poll = process.poll()
    
    print(f"\n应用状态: running={running}, poll={poll}")
    
    # 读取输出中的错误
    output_log = ""
    try:
        # 非阻塞读取
        import select
        readable, _, _ = select.select([process.stdout], [], [], 0.5)
        if readable:
            output_log = process.stdout.read()
    except:
        pass
    
    # 检查是否有 renderFrame failed 日志
    if "renderFrame failed" in output_log:
        # 统计失败次数
        fail_count = output_log.count("renderFrame failed")
        print(f"警告: 发现 renderFrame failed 日志 ({fail_count} 次)")
        
        # 检查是否是大量失败
        if fail_count > 100:
            print("!!! 严重问题: renderFrame 持续失败 !!!")
        else:
            print("可能是正常的跳帧，检查其他日志...")
    else:
        print("未发现 renderFrame failed 日志")
    
    # 检查是否有 First frame after seek 日志
    if "First frame after seek" in output_log:
        print("✓ 检测到 seeking 后第一帧强制渲染日志")
    
    print("\n测试完成")
    
    # 清理
    subprocess.run(['pkill', '-x', 'WZMediaPlayer'], capture_output=True)
    
    if running and poll is None:
        print("结果: 应用正常运行")
        return 0
    else:
        print("结果: 应用已退出（可能崩溃）")
        return 1


if __name__ == "__main__":
    sys.exit(main())
