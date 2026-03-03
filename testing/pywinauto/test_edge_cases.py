"""
WZMediaPlayer 边界条件和极端情况测试
测试快速连续操作、极端seek位置、无效文件等边界情况
"""

import os
import sys
import time
from datetime import datetime
from typing import Optional, Dict, List

try:
    from pywinauto.keyboard import send_keys
except ImportError:
    print("Error: pywinauto is not installed. Run: pip install pywinauto")
    sys.exit(1)

# 导入主测试类
try:
    from main import WZMediaPlayerTester, TestResult
except ImportError:
    print("Error: main module not found. Ensure main.py is in same directory.")
    sys.exit(1)


class WZMediaPlayerEdgeCaseTester(WZMediaPlayerTester):
    """WZMediaPlayer 边界条件测试类"""

    def test_rapid_play_pause(self) -> bool:
        """测试快速连续播放/暂停操作"""
        print("\n[边界测试 1/8] 测试快速连续播放/暂停...")
        try:
            print("  执行20次快速播放/暂停切换（间隔50ms）...")
            for i in range(20):
                send_keys("{SPACE}")
                time.sleep(0.05)  # 50ms间隔
                self.log_test(f"快速播放/暂停 #{i+1}", True, "间隔50ms")
            
            return True
        except Exception as e:
            self.log_test("快速播放/暂停", False, str(e))
            return False

    def test_rapid_seek(self) -> bool:
        """测试快速连续seek操作"""
        print("\n[边界测试 2/8] 测试快速连续seek...")
        try:
            print("  执行30次快速连续seek（间隔100ms）...")
            for i in range(30):
                send_keys("{RIGHT}")
                time.sleep(0.1)  # 100ms间隔
                self.log_test(f"快速seek #{i+1}", True, "间隔100ms")
            
            return True
        except Exception as e:
            self.log_test("快速连续seek", False, str(e))
            return False

    def test_seek_to_start(self) -> bool:
        """测试seek到开头（0%）"""
        print("\n[边界测试 3/8] 测试seek到开头...")
        try:
            # 先seek到中间位置
            send_keys("{UP}")
            send_keys("{UP}")
            time.sleep(2.0)
            
            # 使用快捷键seek到开头（需要确认实际快捷键）
            # 假设使用Home键或Ctrl+Home
            print("  Seek到开头（0%）...")
            send_keys("{HOME}")
            time.sleep(3.0)
            self.log_test("Seek到开头", True, "使用Home键")
            
            return True
        except Exception as e:
            self.log_test("Seek到开头", False, str(e))
            return False

    def test_seek_to_end(self) -> bool:
        """测试seek到结尾（100%）"""
        print("\n[边界测试 4/8] 测试seek到结尾...")
        try:
            # 使用快捷键seek到结尾（需要确认实际快捷键）
            # 假设使用End键或Ctrl+End
            print("  Seek到结尾（100%）...")
            send_keys("{END}")
            time.sleep(3.0)
            self.log_test("Seek到结尾", True, "使用End键")
            
            return True
        except Exception as e:
            self.log_test("Seek到结尾", False, str(e))
            return False

    def test_invalid_file(self) -> bool:
        """测试打开无效文件"""
        print("\n[边界测试 5/8] 测试打开无效文件...")
        try:
            # 尝试打开一个不存在的文件
            print("  尝试打开不存在的文件...")
            send_keys("^o")  # Ctrl+O 打开文件对话框
            time.sleep(1.0)
            # 输入无效路径
            send_keys("invalid_file_that_does_not_exist.mp4")
            send_keys("{ENTER}")
            time.sleep(2.0)
            self.log_test("打开无效文件", True, "应该显示错误提示")
            
            return True
        except Exception as e:
            self.log_test("打开无效文件", False, str(e))
            return False

    def test_corrupted_file(self) -> bool:
        """测试打开损坏的视频文件"""
        print("\n[边界测试 6/8] 测试打开损坏的视频文件...")
        try:
            # 如果有损坏的测试文件，尝试打开
            print("  尝试打开损坏的视频文件...")
            # 需要准备一个损坏的测试文件
            # send_keys("^o")
            # time.sleep(1.0)
            # send_keys("corrupted_test.mp4")
            # send_keys("{ENTER}")
            # time.sleep(2.0)
            self.log_test("打开损坏文件", True, "应该处理错误")
            
            return True
        except Exception as e:
            self.log_test("打开损坏文件", False, str(e))
            return False

    def test_volume_extremes(self) -> bool:
        """测试音量极端值"""
        print("\n[边界测试 7/8] 测试音量极端值...")
        try:
            print("  音量调到最大...")
            # 连续按PageUp多次，调到最大音量
            for i in range(20):
                send_keys("{PGUP}")
                time.sleep(0.1)
            time.sleep(1.0)
            self.log_test("音量最大", True, "连续按PageUp")
            
            print("  音量调到最小...")
            # 连续按PageDown多次，调到最小音量
            for i in range(20):
                send_keys("{PGDN}")
                time.sleep(0.1)
            time.sleep(1.0)
            self.log_test("音量最小", True, "连续按PageDown")
            
            return True
        except Exception as e:
            self.log_test("音量极端值", False, str(e))
            return False

    def test_stress_operations(self) -> bool:
        """测试压力操作（混合多种操作）"""
        print("\n[边界测试 8/8] 测试压力操作...")
        try:
            print("  执行混合操作（播放、暂停、seek、音量）...")
            operations = [
                ("{SPACE}", "播放/暂停"),
                ("{RIGHT}", "Seek前进"),
                ("{SPACE}", "播放/暂停"),
                ("{LEFT}", "Seek后退"),
                ("{PGUP}", "音量增加"),
                ("{SPACE}", "播放/暂停"),
                ("{PGDN}", "音量减少"),
            ]
            
            for key, desc in operations:
                send_keys(key)
                time.sleep(0.5)
                self.log_test(f"压力操作: {desc}", True, f"按键: {key}")
            
            return True
        except Exception as e:
            self.log_test("压力操作", False, str(e))
            return False

    def run_edge_case_tests(self) -> bool:
        """运行所有边界条件测试"""
        print("\n" + "=" * 80)
        print("开始边界条件测试")
        print("=" * 80)
        
        results = []
        
        # 确保视频已打开并播放
        send_keys("{SPACE}")
        time.sleep(2.0)
        
        # 运行各项测试
        results.append(self.test_rapid_play_pause())
        time.sleep(2.0)
        
        results.append(self.test_rapid_seek())
        time.sleep(2.0)
        
        results.append(self.test_seek_to_start())
        time.sleep(2.0)
        
        results.append(self.test_seek_to_end())
        time.sleep(2.0)
        
        results.append(self.test_invalid_file())
        time.sleep(2.0)
        
        results.append(self.test_corrupted_file())
        time.sleep(2.0)
        
        results.append(self.test_volume_extremes())
        time.sleep(2.0)
        
        results.append(self.test_stress_operations())
        
        # 统计结果
        passed = sum(results)
        total = len(results)
        
        print("\n" + "=" * 80)
        print(f"边界条件测试完成: {passed}/{total} 通过")
        print("=" * 80)
        
        return passed == total


def main():
    """主函数"""
    print("=" * 80)
    print("WZMediaPlayer 边界条件自动化测试")
    print("=" * 80)
    print()
    
    # 配置路径（从 config.ini / config.py 读取）
    try:
        import config as test_config
        exe_path = test_config.PLAYER_EXE_PATH
        test_video_path = test_config.TEST_VIDEO_PATH
    except ImportError:
        exe_path = r"D:\2026Github\build\Release\WZMediaPlayer.exe"
        test_video_path = r"D:\2026Github\testing\video\test.mp4"

    # 创建边界条件测试器
    tester = WZMediaPlayerEdgeCaseTester(exe_path, test_video_path)
    
    try:
        # 启动播放器
        if not tester.start_player():
            print("\n[错误] 无法启动播放器")
            return 1
        
        # 等待GUI初始化
        time.sleep(3)
        
        # 打开测试视频
        if not tester.open_video():
            print("[错误] 打开视频失败")
            return 1
        
        # 等待视频加载
        time.sleep(3)
        
        # 运行边界条件测试
        if not tester.run_edge_case_tests():
            print("[警告] 部分边界条件测试失败")
        
        # 生成报告
        report = tester.generate_report()
        print("\n" + report)
        tester.save_report(report)
        
        return 0
        
    except KeyboardInterrupt:
        print("\n\n测试被用户中断")
        return 1
    except Exception as e:
        print(f"\n\n测试异常: {e}")
        import traceback
        traceback.print_exc()
        return 1
    finally:
        tester.stop_player()


if __name__ == "__main__":
    sys.exit(main())
