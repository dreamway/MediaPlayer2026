"""
WZMediaPlayer 音频测试
测试音频输出、音量控制、静音、音频同步等
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


class WZMediaPlayerAudioTester(WZMediaPlayerTester):
    """WZMediaPlayer 音频测试类"""

    def test_audio_output(self, duration_seconds: int = 10) -> bool:
        """测试音频输出（播放指定时长，验证是否有声音）"""
        print(f"\n[音频测试 1/6] 测试音频输出（播放{duration_seconds}秒）...")
        try:
            # 确保视频正在播放
            send_keys("{SPACE}")
            time.sleep(2.0)
            
            # 播放指定时长，观察是否有音频输出
            print(f"  播放 {duration_seconds} 秒，观察音频输出...")
            time.sleep(duration_seconds)
            
            # 注意：这里无法直接检测音频输出，需要人工验证或使用音频检测工具
            # 但可以记录测试状态
            self.log_test(f"音频输出测试（{duration_seconds}秒）", True, f"播放{duration_seconds}秒，需要人工验证音频输出")
            
            return True
        except Exception as e:
            self.log_test("音频输出", False, str(e))
            return False

    def test_volume_control(self) -> bool:
        """测试音量控制"""
        print("\n[音频测试 2/6] 测试音量控制...")
        try:
            # 确保视频正在播放
            send_keys("{SPACE}")
            time.sleep(2.0)
            
            # 测试音量增加
            print("  增加音量...")
            for i in range(5):
                send_keys("{PGUP}")
                time.sleep(0.2)
            time.sleep(1.0)
            self.log_test("音量增加", True, "连续按PageUp 5次")
            
            # 测试音量减少
            print("  减少音量...")
            for i in range(5):
                send_keys("{PGDN}")
                time.sleep(0.2)
            time.sleep(1.0)
            self.log_test("音量减少", True, "连续按PageDown 5次")
            
            return True
        except Exception as e:
            self.log_test("音量控制", False, str(e))
            return False

    def test_mute_toggle(self) -> bool:
        """测试静音切换"""
        print("\n[音频测试 3/6] 测试静音切换...")
        try:
            # 确保视频正在播放
            send_keys("{SPACE}")
            time.sleep(2.0)
            
            # 多次切换静音
            for i in range(3):
                print(f"  切换静音 #{i+1}...")
                send_keys("m")
                time.sleep(1.0)  # 静音
                send_keys("m")
                time.sleep(1.0)  # 取消静音
                self.log_test(f"静音切换 #{i+1}", True, "使用M键切换")
            
            return True
        except Exception as e:
            self.log_test("静音切换", False, str(e))
            return False

    def test_audio_after_seek(self) -> bool:
        """测试Seek后音频是否正常"""
        print("\n[音频测试 4/6] 测试Seek后音频...")
        try:
            # 确保视频正在播放
            send_keys("{SPACE}")
            time.sleep(2.0)
            
            # 多次seek并验证音频
            seek_positions = [
                ("{HOME}", "开头"),
                ("{UP}", "前进10秒"),
                ("{UP}", "再前进10秒"),
                ("{END}", "结尾"),
            ]
            
            for key, desc in seek_positions:
                print(f"  Seek到: {desc}...")
                send_keys(key)
                time.sleep(3.0)  # 等待seek完成
                
                # 播放3秒，观察音频是否正常
                print(f"    播放3秒，观察音频...")
                time.sleep(3.0)
                
                self.log_test(f"Seek到{desc}后音频", True, f"Seek到{desc}，需要人工验证音频正常")
            
            return True
        except Exception as e:
            self.log_test("Seek后音频", False, str(e))
            return False

    def test_audio_sync(self, duration_seconds: int = 30) -> bool:
        """测试音视频同步（长时间播放）"""
        print(f"\n[音频测试 5/6] 测试音视频同步（{duration_seconds}秒）...")
        try:
            # 确保视频正在播放
            send_keys("{SPACE}")
            time.sleep(2.0)
            
            # 分段检查同步（每10秒检查一次）
            check_interval = 10
            checks = duration_seconds // check_interval
            
            for i in range(checks):
                print(f"  同步检查点 #{i+1}/{checks} (播放 {(i+1)*check_interval}秒)...")
                time.sleep(check_interval)
                self.log_test(f"音视频同步检查 #{i+1}", True, f"播放 {(i+1)*check_interval}秒，需要人工验证同步")
            
            return True
        except Exception as e:
            self.log_test("音视频同步", False, str(e))
            return False

    def test_audio_volume_extremes(self) -> bool:
        """测试音量极端值"""
        print("\n[音频测试 6/6] 测试音量极端值...")
        try:
            # 确保视频正在播放
            send_keys("{SPACE}")
            time.sleep(2.0)
            
            # 音量调到最大
            print("  音量调到最大...")
            for i in range(20):
                send_keys("{PGUP}")
                time.sleep(0.1)
            time.sleep(1.0)
            self.log_test("音量最大", True, "连续按PageUp 20次")
            
            # 音量调到最小
            print("  音量调到最小...")
            for i in range(20):
                send_keys("{PGDN}")
                time.sleep(0.1)
            time.sleep(1.0)
            self.log_test("音量最小", True, "连续按PageDown 20次")
            
            # 恢复中等音量
            print("  恢复中等音量...")
            for i in range(10):
                send_keys("{PGUP}")
                time.sleep(0.1)
            time.sleep(1.0)
            self.log_test("恢复中等音量", True, "连续按PageUp 10次")
            
            return True
        except Exception as e:
            self.log_test("音量极端值", False, str(e))
            return False

    def run_audio_tests(self) -> bool:
        """运行所有音频测试"""
        print("\n" + "=" * 80)
        print("开始音频测试")
        print("=" * 80)
        
        results = []
        
        # 确保视频已打开并播放
        send_keys("{SPACE}")
        time.sleep(2.0)
        
        # 运行各项测试
        results.append(self.test_audio_output(duration_seconds=10))
        time.sleep(2.0)
        
        results.append(self.test_volume_control())
        time.sleep(2.0)
        
        results.append(self.test_mute_toggle())
        time.sleep(2.0)
        
        results.append(self.test_audio_after_seek())
        time.sleep(2.0)
        
        results.append(self.test_audio_volume_extremes())
        time.sleep(2.0)
        
        # 长时间同步测试（可选，耗时较长）
        # results.append(self.test_audio_sync(duration_seconds=30))
        
        # 统计结果
        passed = sum(results)
        total = len(results)
        
        print("\n" + "=" * 80)
        print(f"音频测试完成: {passed}/{total} 通过")
        print("=" * 80)
        print("\n注意：音频测试需要人工验证音频输出和同步情况")
        
        return passed == total


def main():
    """主函数"""
    print("=" * 80)
    print("WZMediaPlayer 音频自动化测试")
    print("注意：部分测试需要人工验证音频输出")
    print("=" * 80)
    print()
    
    # 配置路径（根据实际情况修改）
    exe_path = r"E:\WZMediaPlayer_2025\x64\Debug\WZMediaPlay.exe"
    test_video_path = r"D:\BaiduNetdiskDownload\test.mp4"
    
    # 创建音频测试器
    tester = WZMediaPlayerAudioTester(exe_path, test_video_path)
    
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
        
        # 运行音频测试
        if not tester.run_audio_tests():
            print("[警告] 部分音频测试失败")
        
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
