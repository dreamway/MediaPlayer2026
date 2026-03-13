#!/usr/bin/env python3
"""
BUG 验证测试脚本
验证两个问题：
1. 背景清除 BUG - StopRendering 后 Logo 背景残影
2. 进度条不更新 - 启动后进度条/时间不变化
"""

import os
import sys
import time
import subprocess

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from config import APP_PATH, TEST_VIDEO_BBB_NORMAL_PATH
from core.app_launcher import AppLauncher
from core.keyboard_input import KeyboardInput
from core.ax_element import AXElementHelper


def kill_app():
    """终止应用"""
    subprocess.run(['pkill', '-x', 'WZMediaPlayer'], capture_output=True)
    time.sleep(1)


def test_progress_bar_update():
    """
    测试：启动后进度条是否更新
    """
    print("\n" + "=" * 60)
    print("测试：启动后进度条更新")
    print("=" * 60)

    kill_app()

    # 启动应用（带视频参数）
    print("\n[步骤1] 启动应用（带视频参数）...")
    app_launcher = AppLauncher(APP_PATH)
    if not app_launcher.launch([TEST_VIDEO_BBB_NORMAL_PATH]):
        print("  ✗ 启动失败!")
        return False

    print(f"  应用已启动 (PID: {app_launcher.get_pid()})")
    time.sleep(3)  # 等待应用完全启动

    helper = AXElementHelper()

    # 检查进度条初始值
    print("\n[步骤2] 检查进度条初始值...")
    progress1 = helper.get_progress_value()
    print(f"  进度条值 (初始): {progress1}")

    # 等待视频播放
    print("\n[步骤3] 等待 3 秒...")
    time.sleep(3)

    # 再次检查进度条
    print("\n[步骤4] 检查进度条更新值...")
    progress2 = helper.get_progress_value()
    print(f"  进度条值 (3秒后): {progress2}")

    # 判断结果
    if progress1 is not None and progress2 is not None:
        if progress2 > progress1:
            print("\n  ✓ 测试通过: 进度条在更新")
            result = True
        elif progress2 == progress1:
            print(f"\n  ✗ BUG 复现: 进度条未更新 ({progress1} -> {progress2})")
            result = False
        else:
            print(f"\n  ? 进度条异常: {progress1} -> {progress2}")
            result = None
    else:
        print("\n  ⚠ 无法获取进度条值")
        result = None

    kill_app()
    return result


def test_background_clear():
    """
    测试：背景清除
    停止播放后 Logo 背景是否有残影
    """
    print("\n" + "=" * 60)
    print("测试：背景清除（Logo 背景残影）")
    print("=" * 60)

    kill_app()

    # 启动应用
    print("\n[步骤1] 启动应用...")
    app_launcher = AppLauncher(APP_PATH)
    if not app_launcher.launch([TEST_VIDEO_BBB_NORMAL_PATH]):
        print("  ✗ 启动失败!")
        return False

    print(f"  应用已启动 (PID: {app_launcher.get_pid()})")
    time.sleep(3)

    helper = AXElementHelper()

    # 播放一段时间
    print("\n[步骤2] 播放视频 5 秒...")
    time.sleep(5)

    # 停止播放
    print("\n[步骤3] 停止播放...")
    KeyboardInput.focus_app("WZMediaPlayer")
    KeyboardInput.send_key('space')  # 暂停
    time.sleep(1)
    KeyboardInput.send_key('s')  # 停止 (如果有的话)

    # 尝试按 Escape 或其他方式停止
    KeyboardInput.send_key('escape')
    time.sleep(2)

    # 检查 Logo 是否显示
    print("\n[步骤4] 检查 Logo 状态...")
    logo_visible = helper.is_logo_visible()
    print(f"  Logo 可见: {logo_visible}")

    print("\n  ⚠ 背景残影需要人工验证")
    print("  请检查 Logo 背景是否有旧帧残影")

    kill_app()
    return None  # 需要人工验证


def test_timer_debug():
    """
    测试：定时器调试
    检查 statusUpdateTimer 是否正常工作
    """
    print("\n" + "=" * 60)
    print("测试：定时器调试")
    print("=" * 60)

    kill_app()

    # 启动应用
    print("\n[步骤1] 启动应用...")
    app_launcher = AppLauncher(APP_PATH)
    if not app_launcher.launch([TEST_VIDEO_BBB_NORMAL_PATH]):
        print("  ✗ 启动失败!")
        return False

    print(f"  应用已启动 (PID: {app_launcher.get_pid()})")

    helper = AXElementHelper()

    # 连续监控进度条
    print("\n[步骤2] 连续监控进度条 (10次，每次1秒)...")
    values = []
    for i in range(10):
        time.sleep(1)
        progress = helper.get_progress_value()
        values.append(progress)
        print(f"  [{i+1}/10] 进度: {progress}")

    # 分析变化
    print("\n[分析] 进度变化情况:")
    progress_values = [v for v in values if v is not None]
    if len(progress_values) >= 2:
        changes = [progress_values[i+1] - progress_values[i] for i in range(len(progress_values)-1)]
        positive_changes = sum(1 for c in changes if c > 0)
        print(f"  总采样数: {len(progress_values)}")
        print(f"  正向变化次数: {positive_changes}")
        print(f"  变化序列: {changes}")

        if positive_changes >= len(changes) * 0.8:
            print("\n  ✓ 进度条正常更新")
            result = True
        elif positive_changes == 0:
            print("\n  ✗ 进度条完全不更新")
            result = False
        else:
            print(f"\n  ⚠ 进度条部分更新 ({positive_changes}/{len(changes)})")
            result = None
    else:
        print("\n  ⚠ 采样数据不足")
        result = None

    kill_app()
    return result


if __name__ == "__main__":
    print("=" * 60)
    print("WZMediaPlayer BUG 调试测试")
    print("=" * 60)

    # 测试 1：进度条更新
    result1 = test_progress_bar_update()

    # 测试 2：定时器调试
    result2 = test_timer_debug()

    # 测试 3：背景清除
    result3 = test_background_clear()

    print("\n" + "=" * 60)
    print("测试结果汇总")
    print("=" * 60)
    print(f"  进度条更新测试: {'✓ 通过' if result1 else '✗ 失败' if result1 is False else '⚠ 需人工验证'}")
    print(f"  定时器调试测试: {'✓ 通过' if result2 else '✗ 失败' if result2 is False else '⚠ 需人工验证'}")
    print(f"  背景清除测试: {'✓ 通过' if result3 else '✗ 失败' if result3 is False else '⚠ 需人工验证'}")