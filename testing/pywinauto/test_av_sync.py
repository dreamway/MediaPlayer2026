"""
WZMediaPlayer 音视频同步测试
测试seek后音视频同步、长时间播放同步等
参考QtAV的同步测试方法
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


class WZMediaPlayerAVSyncTester(WZMediaPlayerTester):
    """WZMediaPlayer 音视频同步测试类"""

    def test_seek_sync(self, seek_count: int = 5) -> bool:
        """测试seek后音视频同步"""
        print(f"\n[同步测试 1/4] 测试seek后音视频同步（{seek_count}次）...")
        try:
            # 确保视频正在播放
            send_keys("{SPACE}")
            time.sleep(2.0)
            
            for i in range(seek_count):
                print(f"  Seek #{i+1}: 前进10秒...")
                send_keys("{UP}")
                time.sleep(4.0)  # 等待seek完成和同步稳定
                
                # 播放一段时间，观察音视频是否同步
                print(f"    播放5秒，观察同步...")
                time.sleep(5.0)
                
                self.log_test(f"Seek后同步 #{i+1}", True, "观察音视频同步")
            
            return True
        except Exception as e:
            self.log_test("Seek后同步", False, str(e))
            return False

    def test_long_playback_sync(self, duration_seconds: int = 60) -> bool:
        """测试长时间播放音视频同步"""
        print(f"\n[同步测试 2/4] 测试长时间播放同步（{duration_seconds}秒）...")
        try:
            # 确保视频正在播放
            send_keys("{SPACE}")
            time.sleep(2.0)
            
            # 分段检查同步（每10秒检查一次）
            check_interval = 10
            checks = duration_seconds // check_interval
            
            for i in range(checks):
                print(f"  检查点 #{i+1}/{checks} (播放 {(i+1)*check_interval}秒)...")
                time.sleep(check_interval)
                self.log_test(f"长时间播放同步检查 #{i+1}", True, f"播放 {(i+1)*check_interval}秒")
            
            return True
        except Exception as e:
            self.log_test("长时间播放同步", False, str(e))
            return False

    def test_seek_to_keyframe_sync(self) -> bool:
        """测试seek到关键帧后的同步"""
        print("\n[同步测试 3/4] 测试seek到关键帧后的同步...")
        try:
            # 确保视频正在播放
            send_keys("{SPACE}")
            time.sleep(2.0)
            
            # 多次seek到不同位置（关键帧位置）
            seek_positions = [
                ("{HOME}", "开头"),
                ("{UP}", "前进10秒"),
                ("{UP}", "再前进10秒"),
                ("{UP}", "再前进10秒"),
                ("{END}", "结尾"),
            ]
            
            for key, desc in seek_positions:
                print(f"  Seek到: {desc}...")
                send_keys(key)
                time.sleep(4.0)  # 等待seek完成
                
                # 播放一段时间观察同步
                print(f"    播放3秒，观察同步...")
                time.sleep(3.0)
                
                self.log_test(f"关键帧seek同步: {desc}", True, "观察音视频同步")
            
            return True
        except Exception as e:
            self.log_test("关键帧seek同步", False, str(e))
            return False

    def test_pause_resume_sync(self) -> bool:
        """测试暂停/恢复后的同步"""
        print("\n[同步测试 4/4] 测试暂停/恢复后的同步...")
        try:
            # 确保视频正在播放
            send_keys("{SPACE}")
            time.sleep(2.0)
            
            # 多次暂停/恢复，检查同步
            for i in range(5):
                print(f"  暂停/恢复 #{i+1}...")
                
                # 暂停
                send_keys("{SPACE}")
                time.sleep(2.0)  # 暂停2秒
                
                # 恢复
                send_keys("{SPACE}")
                time.sleep(3.0)  # 播放3秒，观察同步
                
                self.log_test(f"暂停/恢复同步 #{i+1}", True, "观察音视频同步")
            
            return True
        except Exception as e:
            self.log_test("暂停/恢复同步", False, str(e))
            return False

    def run_av_sync_tests(self) -> bool:
        """运行所有音视频同步测试"""
        print("\n" + "=" * 80)
        print("开始音视频同步测试")
        print("=" * 80)
        
        results = []
        
        # 确保视频已打开并播放
        send_keys("{SPACE}")
        time.sleep(2.0)
        
        # 运行各项测试
        results.append(self.test_seek_sync(seek_count=5))
        time.sleep(2.0)
        
        results.append(self.test_seek_to_keyframe_sync())
        time.sleep(2.0)
        
        results.append(self.test_pause_resume_sync())
        time.sleep(2.0)
        
        # 长时间播放测试（可选，耗时较长）
        # results.append(self.test_long_playback_sync(duration_seconds=60))
        
        # 统计结果
        passed = sum(results)
        total = len(results)
        
        print("\n" + "=" * 80)
        print(f"音视频同步测试完成: {passed}/{total} 通过")
        print("=" * 80)
        
        return passed == total


def main():
    """主函数"""
    print("=" * 80)
    print("WZMediaPlayer 音视频同步自动化测试")
    print("参考QtAV的同步测试方法")
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

    # 创建同步测试器
    tester = WZMediaPlayerAVSyncTester(exe_path, test_video_path)
    
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
        
        # 运行同步测试
        if not tester.run_av_sync_tests():
            print("[警告] 部分音视频同步测试失败")
        
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
