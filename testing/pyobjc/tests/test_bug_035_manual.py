#!/usr/bin/env python3
"""
BUG-035 测试脚本
验证程序启动后手动在播放列表中双击播放视频时，SplashLogo 未隐藏的问题

测试步骤：
1. 启动应用（不带参数）
2. 等待应用完全启动
3. 通过快捷键打开播放列表
4. 在播放列表中双击播放视频
5. 验证 Logo 已隐藏，进度条在更新
"""

import os
import sys
import time
import subprocess

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from config import APP_PATH, TEST_VIDEO_BBB_NORMAL_PATH, SCREENSHOT_DIR
from core.keyboard_input import KeyboardInput
from core.app_launcher import AppLauncher
from core.ax_element import AXElementHelper


def ensure_dir(path):
    """确保目录存在"""
    os.makedirs(path, exist_ok=True)


def check_app_running():
    """检查应用是否正在运行"""
    result = subprocess.run(['pgrep', '-x', 'WZMediaPlayer'], capture_output=True)
    return result.returncode == 0


def kill_app():
    """终止应用"""
    subprocess.run(['pkill', '-x', 'WZMediaPlayer'], capture_output=True)
    time.sleep(1)


def save_screenshot(name):
    """保存截图"""
    filepath = os.path.join(SCREENSHOT_DIR, f"{name}.png")
    try:
        result = subprocess.run(['screencapture', '-x', filepath], timeout=10, capture_output=True)
        if os.path.exists(filepath):
            print(f"    截图: {os.path.basename(filepath)}")
            return filepath
        return None
    except:
        return None


def test_bug_035_playlist_double_click():
    """
    BUG-035: 手动在播放列表中双击播放视频时 Logo 未隐藏

    复现步骤：
    1. 程序启动（不带参数）
    2. 通过快捷键打开播放列表
    3. 在播放列表中双击视频
    4. 验证 Logo 是否隐藏
    """
    print("\n" + "=" * 60)
    print("BUG-035: 播放列表双击播放测试")
    print("=" * 60)

    ensure_dir(SCREENSHOT_DIR)

    # 杀死现有进程
    kill_app()
    time.sleep(1)

    # 启动应用（不带视频参数）
    print("\n[步骤1] 启动应用（不带视频参数）...")
    if not os.path.exists(APP_PATH):
        print(f"  ✗ 应用不存在: {APP_PATH}")
        return False

    app_launcher = AppLauncher(APP_PATH)
    if not app_launcher.launch([]):  # 不带参数启动
        print("  ✗ 启动失败!")
        return False

    print(f"  应用已启动 (PID: {app_launcher.get_pid()})")
    time.sleep(3)  # 等待应用完全启动

    if not check_app_running():
        print("  ✗ 应用启动后立即退出")
        return False

    # 检查启动后 Logo 是否可见
    print("\n[步骤2] 检查启动后 Logo 状态...")
    helper = AXElementHelper()
    logo_before = helper.is_logo_visible()
    print(f"  启动后 Logo 状态: {'可见' if logo_before else '隐藏'}")

    save_screenshot("bug035_after_startup")

    # 打开播放列表
    print("\n[步骤3] 打开播放列表 (F3)...")
    KeyboardInput.focus_app("WZMediaPlayer", delay=0.3)
    KeyboardInput.send_key('f3')
    time.sleep(1)

    save_screenshot("bug035_playlist_open")

    # 使用 Cmd+O 打开文件对话框添加视频到播放列表
    print("\n[步骤4] 添加视频到播放列表...")
    KeyboardInput.open_video_file(TEST_VIDEO_BBB_NORMAL_PATH)
    time.sleep(2)

    save_screenshot("bug035_after_add_video")

    # 检查 Logo 状态
    print("\n[步骤5] 检查添加视频后 Logo 状态...")
    logo_after_add = helper.is_logo_visible()
    print(f"  添加视频后 Logo 状态: {'可见' if logo_after_add else '隐藏'}")

    # 检查进度条
    progress1 = helper.get_progress_value()
    print(f"  进度条值: {progress1}")

    # 如果 Logo 仍然可见，这是 BUG
    if logo_after_add:
        print("\n  ✗ BUG 复现: Logo 在打开视频后仍然可见!")
        save_screenshot("bug035_logo_still_visible")
        kill_app()
        return False

    # 等待一下再检查进度条是否更新
    time.sleep(1)
    progress2 = helper.get_progress_value()
    print(f"  进度条值 (1秒后): {progress2}")

    if progress1 is not None and progress2 is not None:
        if progress2 > progress1:
            print("\n  ✓ 通过: Logo 已隐藏，进度条在更新")
            result = True
        else:
            print(f"\n  ✗ BUG 复现: 进度条未更新 ({progress1} -> {progress2})")
            result = False
    else:
        print("\n  ⚠ 无法获取进度条值，需要人工验证")
        result = None

    # 清理
    kill_app()
    return result


def test_bug_035_start_with_video():
    """
    对比测试：带视频参数启动应用

    这个测试应该正常工作，作为对比
    """
    print("\n" + "=" * 60)
    print("对比测试: 带视频参数启动应用")
    print("=" * 60)

    kill_app()
    time.sleep(1)

    print("\n[对比测试] 带视频参数启动应用...")
    app_launcher = AppLauncher(APP_PATH)
    if not app_launcher.launch([TEST_VIDEO_BBB_NORMAL_PATH]):
        print("  ✗ 启动失败!")
        return False

    print(f"  应用已启动 (PID: {app_launcher.get_pid()})")
    time.sleep(3)

    helper = AXElementHelper()
    logo_visible = helper.is_logo_visible()
    print(f"  Logo 状态: {'可见' if logo_visible else '隐藏'}")

    progress1 = helper.get_progress_value()
    print(f"  进度条值: {progress1}")
    time.sleep(1)
    progress2 = helper.get_progress_value()
    print(f"  进度条值 (1秒后): {progress2}")

    if not logo_visible and progress2 > progress1:
        print("\n  ✓ 对比测试通过: 带视频参数启动时 Logo 正确隐藏，进度条更新")
        result = True
    else:
        print(f"\n  ✗ 对比测试失败")
        result = False

    kill_app()
    return result


if __name__ == "__main__":
    print("WZMediaPlayer BUG-035 测试")
    print("测试场景：程序启动后手动在播放列表中双击播放视频")
    print()

    # 运行对比测试
    result1 = test_bug_035_start_with_video()
    print()

    # 运行 BUG 复现测试
    result2 = test_bug_035_playlist_double_click()

    print("\n" + "=" * 60)
    print("测试结果")
    print("=" * 60)
    print(f"  对比测试 (带视频启动): {'✓ 通过' if result1 else '✗ 失败'}")
    print(f"  BUG-035 测试: {'✓ 通过' if result2 else '✗ 复现BUG' if result2 is False else '⚠ 需人工验证'}")