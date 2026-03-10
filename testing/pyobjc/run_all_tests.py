# testing/pyobjc/run_all_tests.py
"""
macOS 自动测试入口
运行 WZMediaPlayer 综合测试套件
"""

import os
import sys
import argparse

# 确保可以找到模块
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tests.test_comprehensive import ComprehensiveTest
from config import APP_PATH, TEST_VIDEO_PATH, TEST_VIDEO_LG_PATH


def main():
    """运行所有测试"""
    parser = argparse.ArgumentParser(description='WZMediaPlayer macOS 自动化测试')
    parser.add_argument('--video', type=str, default=None,
                        choices=['test', 'lg', 'both'],
                        help='测试视频选择: test=test.mp4, lg=LG.mp4, both=两个都测')
    parser.add_argument('--quick', action='store_true',
                        help='快速测试模式（只测试基础功能）')
    parser.add_argument('--no-screenshot', action='store_true',
                        help='禁用截图验证')

    args = parser.parse_args()

    print("=" * 80)
    print("WZMediaPlayer macOS 自动化测试套件")
    print("=" * 80)
    print()
    print("注意：首次运行需要在「系统偏好设置 > 安全性与隐私 > 隐私 > 辅助功能」")
    print("      中授权终端或 Python 解释器")
    print()

    # 确定测试视频
    test_videos = []
    if args.video == 'lg':
        test_videos = [TEST_VIDEO_LG_PATH]
    elif args.video == 'both':
        test_videos = [TEST_VIDEO_PATH, TEST_VIDEO_LG_PATH]
    else:
        test_videos = [TEST_VIDEO_PATH]

    # 运行测试
    for i, video in enumerate(test_videos):
        if len(test_videos) > 1:
            print(f"\n{'='*80}")
            print(f"测试视频 {i+1}/{len(test_videos)}: {os.path.basename(video)}")
            print("=" * 80)

        test = ComprehensiveTest()

        if args.quick:
            print("快速测试模式")
            if not test.setup(APP_PATH):
                print("测试准备失败")
                continue
            try:
                test.test_app_launch()
                test.test_open_video(video)
                test.test_rendering_not_black()
            finally:
                test.teardown()
            test.generate_report()
            test.print_summary()
        else:
            test.run_all_tests(video)


if __name__ == "__main__":
    main()