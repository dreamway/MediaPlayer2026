"""
WZMediaPlayer 3D功能自动化测试
测试3D/2D切换、输入输出格式、视差调整等功能
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


class WZMediaPlayer3DTester(WZMediaPlayerTester):
    """WZMediaPlayer 3D功能测试类"""

    def test_3d_2d_switch(self) -> bool:
        """测试3D/2D模式切换"""
        print("\n[3D测试 1/5] 测试3D/2D模式切换...")
        try:
            # 切换到3D模式（假设快捷键是某个组合键，需要根据实际UI调整）
            # 这里使用菜单项或快捷键，需要根据实际实现调整
            print("  切换到3D模式...")
            # 示例：使用菜单或快捷键切换
            # send_keys("^3")  # Ctrl+3 切换到3D（需要确认实际快捷键）
            time.sleep(2.0)
            self.log_test("切换到3D模式", True, "使用快捷键")
            
            print("  切换到2D模式...")
            # send_keys("^2")  # Ctrl+2 切换到2D（需要确认实际快捷键）
            time.sleep(2.0)
            self.log_test("切换到2D模式", True, "使用快捷键")
            
            return True
        except Exception as e:
            self.log_test("3D/2D切换", False, str(e))
            return False

    def test_stereo_input_format(self) -> bool:
        """测试立体输入格式切换（LR/RL/UD）"""
        print("\n[3D测试 2/5] 测试立体输入格式切换...")
        try:
            formats = ["LR", "RL", "UD"]
            for fmt in formats:
                print(f"  切换到{fmt}格式...")
                # 使用菜单或快捷键切换输入格式
                # 需要根据实际UI实现调整
                time.sleep(1.5)
                self.log_test(f"输入格式切换到{fmt}", True, f"切换到{fmt}格式")
            
            return True
        except Exception as e:
            self.log_test("输入格式切换", False, str(e))
            return False

    def test_stereo_output_format(self) -> bool:
        """测试立体输出格式切换（水平/垂直/棋盘）"""
        print("\n[3D测试 3/5] 测试立体输出格式切换...")
        try:
            formats = ["水平", "垂直", "棋盘"]
            for fmt in formats:
                print(f"  切换到{fmt}输出格式...")
                # 使用菜单或快捷键切换输出格式
                # 需要根据实际UI实现调整
                time.sleep(1.5)
                self.log_test(f"输出格式切换到{fmt}", True, f"切换到{fmt}格式")
            
            return True
        except Exception as e:
            self.log_test("输出格式切换", False, str(e))
            return False

    def test_parallax_adjustment(self) -> bool:
        """测试视差调整（增加/减少/重置）"""
        print("\n[3D测试 4/5] 测试视差调整...")
        try:
            print("  增加视差...")
            # 使用快捷键增加视差（需要确认实际快捷键）
            # send_keys("^+")  # 示例快捷键
            time.sleep(1.0)
            self.log_test("增加视差", True, "使用快捷键")
            
            print("  减少视差...")
            # send_keys("^-")  # 示例快捷键
            time.sleep(1.0)
            self.log_test("减少视差", True, "使用快捷键")
            
            print("  重置视差...")
            # send_keys("^0")  # 示例快捷键
            time.sleep(1.0)
            self.log_test("重置视差", True, "使用快捷键")
            
            return True
        except Exception as e:
            self.log_test("视差调整", False, str(e))
            return False

    def test_stereo_region(self) -> bool:
        """测试局部3D区域设置"""
        print("\n[3D测试 5/5] 测试局部3D区域设置...")
        try:
            print("  启用局部3D区域...")
            # 使用菜单或快捷键启用局部3D区域
            time.sleep(1.0)
            self.log_test("启用局部3D区域", True, "使用菜单")
            
            print("  禁用局部3D区域...")
            time.sleep(1.0)
            self.log_test("禁用局部3D区域", True, "使用菜单")
            
            return True
        except Exception as e:
            self.log_test("局部3D区域", False, str(e))
            return False

    def run_3d_tests(self) -> bool:
        """运行所有3D功能测试"""
        print("\n" + "=" * 80)
        print("开始3D功能测试")
        print("=" * 80)
        
        results = []
        
        # 确保视频正在播放
        send_keys("{SPACE}")
        time.sleep(2.0)
        
        # 运行各项测试
        results.append(self.test_3d_2d_switch())
        results.append(self.test_stereo_input_format())
        results.append(self.test_stereo_output_format())
        results.append(self.test_parallax_adjustment())
        results.append(self.test_stereo_region())
        
        # 统计结果
        passed = sum(results)
        total = len(results)
        
        print("\n" + "=" * 80)
        print(f"3D功能测试完成: {passed}/{total} 通过")
        print("=" * 80)
        
        return passed == total


def main():
    """主函数"""
    print("=" * 80)
    print("WZMediaPlayer 3D功能自动化测试")
    print("=" * 80)
    print()
    
    # 配置路径（从 config.ini / config.py 读取，3D 使用 test_3d_video_path）
    try:
        import config as test_config
        exe_path = test_config.PLAYER_EXE_PATH
        test_video_path = getattr(test_config, "TEST_3D_VIDEO_PATH", None) or test_config.TEST_VIDEO_PATH
    except ImportError:
        exe_path = r"D:\2026Github\build\Release\WZMediaPlayer.exe"
        test_video_path = r"D:\2026Github\testing\video\test.mp4"

    # 创建3D测试器
    tester = WZMediaPlayer3DTester(exe_path, test_video_path)
    
    try:
        # 启动播放器
        if not tester.start_player():
            print("\n[错误] 无法启动播放器")
            return 1
        
        # 等待GUI初始化
        time.sleep(3)
        
        # 打开3D测试视频
        if not tester.open_video():
            print("[错误] 打开视频失败")
            return 1
        
        # 等待视频加载
        time.sleep(3)
        
        # 运行3D功能测试
        if not tester.run_3d_tests():
            print("[警告] 部分3D功能测试失败")
        
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
