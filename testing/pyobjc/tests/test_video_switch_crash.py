#!/usr/bin/env python3
"""
视频切换崩溃测试
尝试复现视频切换时的崩溃问题
"""

import os
import sys
import time
import subprocess

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from config import (
    APP_PATH, TEST_VIDEO_60S_PATH, TEST_VIDEO_BBB_NORMAL_PATH
)
from core.app_launcher import AppLauncher
from core.keyboard_input import KeyboardInput
from core.screenshot_capture import ScreenshotCapture, ImageAnalyzer
from config import SCREENSHOT_DIR, BLACK_THRESHOLD


def check_app_running():
    """检查应用是否在运行"""
    result = subprocess.run(
        ['pgrep', '-x', 'WZMediaPlayer'],  # 使用 -x 精确匹配
        capture_output=True,
        text=True
    )
    return result.returncode == 0


def wait_for_app_exit(timeout=10):
    """等待应用退出"""
    start = time.time()
    while time.time() - start < timeout:
        if not check_app_running():
            return True
        time.sleep(0.5)
    return False


def test_video_switch_crash():
    """测试视频切换崩溃"""
    print("=" * 60)
    print("视频切换崩溃测试")
    print("=" * 60)

    # 检查测试视频是否存在
    videos = [TEST_VIDEO_60S_PATH, TEST_VIDEO_BBB_NORMAL_PATH]
    for v in videos:
        if not os.path.exists(v):
            print(f"错误: 测试视频不存在: {v}")
            return False

    # 启动应用
    print(f"\n1. 启动应用: {APP_PATH}")
    launcher = AppLauncher()
    if not launcher.launch(app_args=[TEST_VIDEO_60S_PATH]):
        print("错误: 无法启动应用")
        return False

    # 等待应用启动
    print("等待应用初始化...")
    time.sleep(5.0)  # 增加等待时间

    if not check_app_running():
        print("错误: 应用启动后立即退出")
        return False

    print("应用启动成功")

    # 播放第一个视频一段时间
    print(f"\n2. 播放第一个视频: {os.path.basename(TEST_VIDEO_60S_PATH)}")
    KeyboardInput.focus_app("WZMediaPlayer", delay=0.5)
    KeyboardInput.toggle_playback()
    time.sleep(3.0)

    if not check_app_running():
        print("错误: 播放第一个视频时应用崩溃")
        return False

    print("第一个视频播放正常")

    # 执行一些seek操作
    print("\n3. 执行Seek操作...")
    for i in range(3):
        KeyboardInput.focus_app("WZMediaPlayer", delay=0.1)
        KeyboardInput.seek_forward()
        time.sleep(0.5)

    time.sleep(1.0)

    if not check_app_running():
        print("错误: Seek操作后应用崩溃")
        return False

    print("Seek操作正常")

    # 切换到第二个视频
    print(f"\n4. 切换到第二个视频: {os.path.basename(TEST_VIDEO_BBB_NORMAL_PATH)}")
    print("   这是崩溃可能发生的时刻...")

    # 记录切换前时间
    switch_start = time.time()

    KeyboardInput.open_video_file(TEST_VIDEO_BBB_NORMAL_PATH, delay=3.0)

    switch_time = time.time() - switch_start
    print(f"   视频切换耗时: {switch_time:.1f}秒")

    # 检查应用是否还在运行
    if not check_app_running():
        print("\n!!! 应用在视频切换时崩溃 !!!")
        print("   正在检查崩溃报告...")

        # 检查最新的崩溃报告
        crash_reports = subprocess.run(
            ['ls', '-lt', '~/Library/Logs/DiagnosticReports/'],
            capture_output=True,
            text=True,
            shell=True
        )
        print(f"   最近的崩溃报告:\n{crash_reports.stdout[:500]}")

        return False

    print("视频切换成功，应用仍在运行")

    # 验证第二个视频播放正常
    print("\n5. 验证第二个视频播放...")
    KeyboardInput.focus_app("WZMediaPlayer", delay=0.5)
    KeyboardInput.toggle_playback()
    time.sleep(2.0)

    # 截图验证
    screenshot = ScreenshotCapture(SCREENSHOT_DIR)
    screenshot_path = screenshot.capture_full_screen()
    is_black, _, brightness = ImageAnalyzer.is_black_screen(screenshot_path, BLACK_THRESHOLD)

    if is_black:
        print(f"警告: 第二个视频显示黑屏 (亮度={brightness:.1f})")
    else:
        print(f"第二个视频渲染正常 (亮度={brightness:.1f})")

    # 清理
    print("\n6. 关闭应用...")
    launcher.quit()

    if wait_for_app_exit():
        print("应用正常关闭")
    else:
        print("应用未能正常关闭，强制终止")
        launcher.kill()

    print("\n" + "=" * 60)
    print("测试完成 - 未检测到崩溃")
    print("=" * 60)
    return True


if __name__ == "__main__":
    success = test_video_switch_crash()
    sys.exit(0 if success else 1)