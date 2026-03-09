"""
WZMediaPlayer 完整测试套件
整合所有测试：基础播放、Seeking、3D功能、边界条件、音视频同步
"""

import os
import sys
import time
from datetime import datetime

try:
    from main import WZMediaPlayerTester
    from test_3d_features import WZMediaPlayer3DTester
    from test_edge_cases import WZMediaPlayerEdgeCaseTester
    from test_av_sync import WZMediaPlayerAVSyncTester
    from test_progress_seek import WZMediaPlayerProgressSeekTester
    from test_audio import WZMediaPlayerAudioTester
    from test_hardware_decoding import WZMediaPlayerHardwareDecodingTester
except ImportError as e:
    print(f"Error: 无法导入测试模块: {e}")
    print("请确保所有测试文件都在同一目录下")
    sys.exit(1)


class WZMediaPlayerFullTestSuite:
    """完整测试套件"""

    def __init__(self, exe_path: str, test_video_path: str = None, test_3d_video_path: str = None):
        """
        初始化测试套件
        
        Args:
            exe_path: 播放器可执行文件路径
            test_video_path: 普通测试视频路径
            test_3d_video_path: 3D测试视频路径
        """
        self.exe_path = exe_path
        self.test_video_path = test_video_path
        self.test_3d_video_path = test_3d_video_path or test_video_path
        
        self.test_results = []
        self.start_time = None
        self.end_time = None

    def run_basic_tests(self) -> bool:
        """运行基础播放测试"""
        print("\n" + "=" * 80)
        print("阶段 1/7: 基础播放测试")
        print("=" * 80)
        
        tester = WZMediaPlayerTester(self.exe_path, self.test_video_path)
        
        try:
            if not tester.start_player():
                print("[错误] 无法启动播放器")
                return False
            
            time.sleep(3)
            
            if not tester.open_video():
                print("[错误] 打开视频失败")
                return False
            
            time.sleep(3)
            
            # 运行基础测试
            tester.test_play_pause()
            time.sleep(2)
            tester.test_stop()
            time.sleep(2)
            tester.test_seek_small()
            time.sleep(2)
            tester.test_seek_large()
            time.sleep(2)
            tester.test_multiple_seeks()
            time.sleep(2)
            tester.test_volume()
            time.sleep(2)
            tester.test_video_switch_twice()

            # 保存结果（含进程输出摘要，与 run_closed_loop_tests 一致）
            po = tester.get_process_output_summary() if hasattr(tester, 'get_process_output_summary') else ""
            self.test_results.append({
                "name": "基础播放测试",
                "results": tester.test_results,
                "passed": sum(1 for r in tester.test_results if r["passed"]),
                "process_output": po or ""
            })
            tester.stop_player()
            return True
        except Exception as e:
            print(f"[错误] 基础测试异常: {e}")
            tester.stop_player()
            return False

    def run_3d_tests(self) -> bool:
        """运行3D功能测试"""
        print("\n" + "=" * 80)
        print("阶段 2/7: 3D功能测试")
        print("=" * 80)
        
        tester = WZMediaPlayer3DTester(self.exe_path, self.test_3d_video_path)
        
        try:
            if not tester.start_player():
                print("[错误] 无法启动播放器")
                return False
            
            time.sleep(3)
            
            if not tester.open_video():
                print("[错误] 打开3D视频失败")
                return False
            
            time.sleep(3)
            
            result = tester.run_3d_tests()
            
            po = tester.get_process_output_summary() if hasattr(tester, 'get_process_output_summary') else ""
            self.test_results.append({
                "name": "3D功能测试",
                "results": tester.test_results,
                "passed": sum(1 for r in tester.test_results if r["passed"]),
                "process_output": po or ""
            })
            tester.stop_player()
            return result
        except Exception as e:
            print(f"[错误] 3D测试异常: {e}")
            tester.stop_player()
            return False

    def run_edge_case_tests(self) -> bool:
        """运行边界条件测试"""
        print("\n" + "=" * 80)
        print("阶段 3/7: 边界条件测试")
        print("=" * 80)
        
        tester = WZMediaPlayerEdgeCaseTester(self.exe_path, self.test_video_path)
        
        try:
            if not tester.start_player():
                print("[错误] 无法启动播放器")
                return False
            
            time.sleep(3)
            
            if not tester.open_video():
                print("[错误] 打开视频失败")
                return False
            
            time.sleep(3)
            
            result = tester.run_edge_case_tests()
            po = tester.get_process_output_summary() if hasattr(tester, 'get_process_output_summary') else ""
            self.test_results.append({
                "name": "边界条件测试",
                "results": tester.test_results,
                "passed": sum(1 for r in tester.test_results if r["passed"]),
                "process_output": po or ""
            })
            tester.stop_player()
            return result
        except Exception as e:
            print(f"[错误] 边界条件测试异常: {e}")
            tester.stop_player()
            return False

    def run_av_sync_tests(self) -> bool:
        """运行音视频同步测试"""
        print("\n" + "=" * 80)
        print("阶段 4/7: 音视频同步测试")
        print("=" * 80)
        
        tester = WZMediaPlayerAVSyncTester(self.exe_path, self.test_video_path)
        
        try:
            if not tester.start_player():
                print("[错误] 无法启动播放器")
                return False
            
            time.sleep(3)
            
            if not tester.open_video():
                print("[错误] 打开视频失败")
                return False
            
            time.sleep(3)
            
            result = tester.run_av_sync_tests()
            po = tester.get_process_output_summary() if hasattr(tester, 'get_process_output_summary') else ""
            self.test_results.append({
                "name": "音视频同步测试",
                "results": tester.test_results,
                "passed": sum(1 for r in tester.test_results if r["passed"]),
                "process_output": po or ""
            })
            tester.stop_player()
            return result
        except Exception as e:
            print(f"[错误] 同步测试异常: {e}")
            tester.stop_player()
            return False

    def run_progress_seek_tests(self) -> bool:
        """运行进度条和Seeking测试"""
        print("\n" + "=" * 80)
        print("阶段 5/7: 进度条和Seeking测试")
        print("=" * 80)
        
        tester = WZMediaPlayerProgressSeekTester(self.exe_path, self.test_video_path)
        
        try:
            if not tester.start_player():
                print("[错误] 无法启动播放器")
                return False
            
            time.sleep(3)
            
            if not tester.open_video():
                print("[错误] 打开视频失败")
                return False
            
            time.sleep(3)
            
            result = tester.run_progress_seek_tests()
            po = tester.get_process_output_summary() if hasattr(tester, 'get_process_output_summary') else ""
            self.test_results.append({
                "name": "进度条和Seeking测试",
                "results": tester.test_results,
                "passed": sum(1 for r in tester.test_results if r["passed"]),
                "process_output": po or ""
            })
            tester.stop_player()
            return result
        except Exception as e:
            print(f"[错误] 进度条测试异常: {e}")
            tester.stop_player()
            return False

    def run_audio_tests(self) -> bool:
        """运行音频测试"""
        print("\n" + "=" * 80)
        print("阶段 6/7: 音频测试")
        print("=" * 80)
        
        tester = WZMediaPlayerAudioTester(self.exe_path, self.test_video_path)
        
        try:
            if not tester.start_player():
                print("[错误] 无法启动播放器")
                return False
            
            time.sleep(3)
            
            if not tester.open_video():
                print("[错误] 打开视频失败")
                return False
            
            time.sleep(3)
            
            result = tester.run_audio_tests()
            po = tester.get_process_output_summary() if hasattr(tester, 'get_process_output_summary') else ""
            self.test_results.append({
                "name": "音频测试",
                "results": tester.test_results,
                "passed": sum(1 for r in tester.test_results if r["passed"]),
                "process_output": po or ""
            })
            tester.stop_player()
            return result
        except Exception as e:
            print(f"[错误] 音频测试异常: {e}")
            tester.stop_player()
            return False

    def run_hardware_decoding_tests(self) -> bool:
        """运行硬件解码测试"""
        print("\n" + "=" * 80)
        print("阶段 7/8: 硬件解码测试")
        print("=" * 80)
        
        tester = WZMediaPlayerHardwareDecodingTester(self.exe_path, self.test_video_path)
        
        try:
            if not tester.start_player():
                print("[错误] 无法启动播放器")
                return False
            
            time.sleep(3)
            
            if not tester.open_video():
                print("[错误] 打开视频失败")
                return False
            
            time.sleep(3)
            
            result = tester.run_hardware_decoding_tests()
            po = tester.get_process_output_summary() if hasattr(tester, 'get_process_output_summary') else ""
            self.test_results.append({
                "name": "硬件解码测试",
                "results": tester.test_results,
                "passed": sum(1 for r in tester.test_results if r["passed"]),
                "process_output": po or ""
            })
            tester.stop_player()
            return result
        except Exception as e:
            print(f"[错误] 硬件解码测试异常: {e}")
            tester.stop_player()
            return False

    def generate_full_report(self) -> str:
        """生成完整测试报告"""
        report_lines = []
        report_lines.append("=" * 80)
        report_lines.append("WZMediaPlayer 完整测试套件报告")
        report_lines.append("=" * 80)
        report_lines.append(f"测试开始时间: {self.start_time.strftime('%Y-%m-%d %H:%M:%S')}")
        report_lines.append(f"测试结束时间: {self.end_time.strftime('%Y-%m-%d %H:%M:%S')}")
        
        if self.start_time and self.end_time:
            duration = self.end_time - self.start_time
            report_lines.append(f"测试耗时: {duration.total_seconds():.2f} 秒")
        
        report_lines.append("")
        
        # 统计总体结果
        total_tests = 0
        total_passed = 0
        
        for suite_result in self.test_results:
            total_tests += len(suite_result["results"])
            total_passed += suite_result["passed"]
        
        report_lines.append(f"总计: {total_tests} | 通过: {total_passed} | 失败: {total_tests - total_passed}")
        report_lines.append("")
        
        # 各阶段结果
        report_lines.append("-" * 80)
        report_lines.append("各阶段测试结果:")
        report_lines.append("-" * 80)
        
        for suite_result in self.test_results:
            suite_total = len(suite_result["results"])
            suite_passed = suite_result["passed"]
            suite_failed = suite_total - suite_passed
            
            report_lines.append(f"\n{suite_result['name']}:")
            report_lines.append(f"  总计: {suite_total} | 通过: {suite_passed} | 失败: {suite_failed}")
            
            # 显示失败的测试
            if suite_failed > 0:
                report_lines.append("  失败的测试:")
                for result in suite_result["results"]:
                    if not result["passed"]:
                        details = result.get('details', 'N/A')
                        if len(details) > 100:
                            details = details[:100] + "..."
                        report_lines.append(f"    - {result['name']}: {details}")
            # 进程输出错误（ProcessOutputMonitor，与 run_closed_loop_tests 一致）
            po = suite_result.get("process_output", "").strip()
            if po:
                report_lines.append("  进程输出错误:")
                for line in po.splitlines():
                    report_lines.append(f"    {line[:200]}")
        
        # 添加日志文件路径信息
        report_lines.append("")
        report_lines.append("-" * 80)
        report_lines.append("日志文件:")
        report_lines.append("-" * 80)
        try:
            import glob
            log_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "WZMediaPlay", "logs")
            log_pattern = os.path.join(log_dir, "MediaPlayer_*.log")
            log_files = glob.glob(log_pattern)
            if log_files:
                latest_log = max(log_files, key=os.path.getmtime)
                report_lines.append(f"最新日志文件: {latest_log}")
            else:
                report_lines.append("未找到日志文件")
        except Exception as e:
            report_lines.append(f"获取日志文件失败: {e}")
        
        report_lines.append("")
        report_lines.append("=" * 80)
        
        return "\n".join(report_lines)

    def save_full_report(self, report: str):
        """保存完整测试报告（先写文件再打印，确保终端仅输出时也有有效报告文件）"""
        report_path = None
        try:
            timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
            filename = f"test_report_full_{timestamp}.txt"
            script_dir = os.path.dirname(os.path.abspath(__file__))
            report_path = os.path.join(script_dir, filename)
            with open(report_path, 'w', encoding='utf-8') as f:
                f.write(report)
                f.flush()
        except Exception as e:
            report_path = None
            try:
                print(f"\n[FAIL] 保存测试报告失败: {e}")
            except Exception:
                pass
        if report_path:
            try:
                print(f"\n[OK] 完整测试报告已保存: {report_path}")
            except UnicodeEncodeError:
                print(f"\n[OK] Report saved: {report_path}")

    def run_all(self) -> int:
        """运行所有测试"""
        self.start_time = datetime.now()
        
        print("=" * 80)
        print("WZMediaPlayer 完整测试套件")
        print("包含：基础播放、3D功能、边界条件、音视频同步、进度条Seeking、音频测试、硬件解码测试")
        print("=" * 80)
        print()
        
        results = []
        
        # 运行各阶段测试
        results.append(self.run_basic_tests())
        time.sleep(2)
        
        results.append(self.run_3d_tests())
        time.sleep(2)
        
        results.append(self.run_edge_case_tests())
        time.sleep(2)
        
        results.append(self.run_av_sync_tests())
        time.sleep(2)
        
        results.append(self.run_progress_seek_tests())
        time.sleep(2)
        
        results.append(self.run_audio_tests())
        time.sleep(2)
        
        results.append(self.run_hardware_decoding_tests())
        
        self.end_time = datetime.now()
        
        # 生成报告并先写入文件（避免仅输出到终端时无有效报告）
        report = self.generate_full_report()
        self.save_full_report(report)
        try:
            print("\n" + report)
        except UnicodeEncodeError:
            print("\n[Report printed with encoding fallback; see file for full content]")
        
        # 返回结果（0=全部通过，1=有失败）
        all_passed = all(results)
        return 0 if all_passed else 1


def main():
    """主函数（路径从 config.ini / config.py 读取）"""
    try:
        import config as test_config
        exe_path = test_config.PLAYER_EXE_PATH
        test_video_path = test_config.TEST_VIDEO_PATH
        test_3d_video_path = getattr(test_config, "TEST_3D_VIDEO_PATH", None) or test_video_path
    except ImportError:
        exe_path = r"D:\2026Github\build\Release\WZMediaPlayer.exe"
        test_video_path = r"D:\2026Github\testing\video\test.mp4"
        test_3d_video_path = test_video_path

    suite = WZMediaPlayerFullTestSuite(exe_path, test_video_path, test_3d_video_path)
    
    try:
        return suite.run_all()
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
