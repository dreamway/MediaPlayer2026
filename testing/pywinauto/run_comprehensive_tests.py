# testing/pywinauto/run_comprehensive_tests.py
"""
WZMediaPlayer Windows 综合测试运行脚本
整合所有从 macOS pyobjc 迁移的测试用例
"""

import os
import sys
import time
from datetime import datetime

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from config import APP_PATH, TEST_VIDEO_60S_PATH, REPORT_DIR
from tests.test_comprehensive import ComprehensiveTest
from tests.test_playback_sync import PlaybackSyncTest
from tests.test_3d_rendering import Stereo3DTest
from tests.test_camera import CameraTest


def print_header(title: str):
    """打印标题"""
    print("\n" + "=" * 80)
    print(title)
    print("=" * 80)


def run_all_tests():
    """运行所有测试"""
    print_header("WZMediaPlayer Windows 综合测试套件")

    print("""
注意：首次运行需要在「系统偏好设置 > 安全性与隐私 > 隐私 > 辅助功能」
      中授权终端或 Python 解释器
""")

    start_time = datetime.now()
    all_results = []

    # 1. 综合测试
    print_header("综合测试")
    comprehensive = ComprehensiveTest()
    comprehensive.run_all_tests(TEST_VIDEO_60S_PATH)
    all_results.append(("ComprehensiveTest", comprehensive._report))

    # 2. 播放同步测试
    print_header("播放进度与 UI 同步测试")
    sync_test = PlaybackSyncTest()
    sync_test.run_all_tests(TEST_VIDEO_60S_PATH)
    all_results.append(("PlaybackSyncTest", sync_test._report))

    # 3. 3D 渲染测试
    print_header("3D 渲染测试")
    stereo_test = Stereo3DTest()
    stereo_test.run_all_tests(TEST_VIDEO_60S_PATH)
    all_results.append(("Stereo3DTest", stereo_test._report))

    # 4. 摄像头测试
    print_header("摄像头功能测试")
    camera_test = CameraTest()
    camera_test.run_all_tests()
    all_results.append(("CameraTest", camera_test._report))

    # 生成总结
    end_time = datetime.now()
    duration = (end_time - start_time).total_seconds()

    print_header("测试总结")

    total_tests = 0
    total_passed = 0
    total_failed = 0

    for suite_name, report in all_results:
        if report:
            t = report.get("total_tests", 0)
            p = report.get("passed", 0)
            f = report.get("failed", 0)
            total_tests += t
            total_passed += p
            total_failed += f

            status = "✓" if f == 0 else "✗"
            print(f"  {status} {suite_name}: {p}/{t} 通过")

    print(f"\n总计: {total_tests} 测试")
    print(f"通过: {total_passed}")
    print(f"失败: {total_failed}")
    print(f"耗时: {duration:.1f} 秒")

    # 保存汇总报告
    os.makedirs(REPORT_DIR, exist_ok=True)
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    report_path = os.path.join(REPORT_DIR, f"comprehensive_summary_{timestamp}.json")

    import json
    summary = {
        "timestamp": start_time.isoformat(),
        "duration_sec": duration,
        "total_tests": total_tests,
        "passed": total_passed,
        "failed": total_failed,
        "suites": {name: report for name, report in all_results if report}
    }

    with open(report_path, 'w', encoding='utf-8') as f:
        json.dump(summary, f, indent=2, ensure_ascii=False)

    print(f"\n[REPORT] 汇总报告已保存: {report_path}")

    return 0 if total_failed == 0 else 1


def main():
    """主函数"""
    try:
        return run_all_tests()
    except KeyboardInterrupt:
        print("\n\n测试被用户中断")
        return 1
    except Exception as e:
        print(f"\n\n测试异常: {e}")
        import traceback
        traceback.print_exc()
        return 1


if __name__ == "__main__":
    sys.exit(main())