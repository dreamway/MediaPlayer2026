# testing/pyobjc/run_all_tests.py
"""
macOS 自动测试入口
"""

import os
import sys

# 确保可以找到 core 模块
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tests.test_basic_playback import BasicPlaybackTest


def main():
    """运行所有测试"""
    print("=" * 80)
    print("WZMediaPlayer macOS 自动化测试套件")
    print("=" * 80)
    print()
    print("注意：首次运行需要在「系统偏好设置 > 安全性与隐私 > 隐私 > 辅助功能」")
    print("      中授权终端或 Python 解释器")
    print()

    test = BasicPlaybackTest()
    test.run_all_tests()


if __name__ == "__main__":
    main()