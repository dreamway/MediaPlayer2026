"""
WZMediaPlayer 3D Player 自动化测试（增强版）
使用快捷键直接控制，集成日志监控功能

Phase 1: 基础播放和Seeking测试
"""

import os
import sys
import time
import subprocess
import glob
from datetime import datetime
from typing import Optional, Dict, List

try:
    from pywinauto.keyboard import send_keys
except ImportError:
    print("Error: pywinauto is not installed. Run: pip install pywinauto")
    sys.exit(1)

# 导入日志监控模块
try:
    from log_monitor import LogMonitor
except ImportError:
    print("Error: log_monitor module not found. Ensure log_monitor.py is in same directory.")
    LogMonitor = None

# 导入进程输出监控（与 run_closed_loop_tests / core 一致，用于捕获 stdout/stderr 错误）
try:
    from core.process_output_monitor import ProcessOutputMonitor
except ImportError:
    ProcessOutputMonitor = None


class TestResult:
    """测试结果"""
    PASS = "PASS"
    FAIL = "FAIL"
    SKIP = "SKIP"
    ERROR = "ERROR"


class WZMediaPlayerTester:
    """WZMediaPlayer 3D播放器自动化测试类（增强版）"""

    def __init__(self, exe_path: str, test_video_path: str = None):
        """
        初始化测试器

        Args:
            exe_path: 播放器可执行文件路径
            test_video_path: 测试视频文件路径
        """
        self.exe_path = exe_path
        self.test_video_path = test_video_path
        self.process = None
        self.test_results = []

        # 进程输出监控（stdout/stderr，与 core/ui_automation 一致）
        self._output_monitor = None
        self._capture_process_output = True  # 是否捕获进程输出以检测崩溃/OpenAL 等错误

        # 日志监控器配置
        self.log_monitor = None
        self.log_file_path = None
        self.last_checked_anomaly_count = 0  # 跟踪已检查的异常数量

        # 配置日志监控
        if LogMonitor:
            self._setup_log_monitor()
        else:
            print("[LogMonitor] 日志监控模块未找到")

    def _setup_log_monitor(self):
        """设置日志监控器"""
        exe_dir = os.path.dirname(self.exe_path)
        # 查找日志目录（多个候选：CMake 构建可能在 build/Release/logs，或仓库根 WZMediaPlay/logs）
        candidates = [
            os.path.join(exe_dir, "logs"),  # build/Release/logs
            os.path.join(exe_dir, "..", "..", "WZMediaPlay", "logs"),
            os.path.join(exe_dir, "..", "logs"),
        ]
        log_dir = exe_dir
        for d in candidates:
            if os.path.exists(d):
                log_dir = d
                break
        log_pattern = os.path.join(log_dir, "MediaPlayer_*.log")
        
        # 记录启动前的日志文件列表，用于识别exe启动后新创建的日志文件
        self.log_files_before_start = set()
        if os.path.exists(log_dir):
            self.log_files_before_start = set(glob.glob(log_pattern))
        
        # 创建日志监控器配置
        log_monitor_config = {
            'enabled': True,
            'level_threshold': 'warn',
            'patterns': ['error', 'warning', 'critical', 'exception', 'exception while', 'failed', 'fail'],
            'log_file_pattern': log_pattern,
            'log_dir': log_dir
        }

        self.log_monitor = LogMonitor(log_monitor_config)
        self.log_dir = log_dir
        print(f"[LogMonitor] 日志监控器已初始化，日志目录: {log_dir}")

    def start_log_monitoring(self):
        """启动日志监控（在exe启动后调用，自动找到最新创建的日志文件）"""
        if not self.log_monitor:
            print("[LogMonitor] 日志监控未初始化")
            return
        
        # 等待一下，确保exe已经创建日志文件
        time.sleep(2)
        
        # 查找最新的日志文件（exe启动后创建的）
        log_pattern = os.path.join(self.log_dir, "MediaPlayer_*.log")
        log_files_after_start = set(glob.glob(log_pattern))
        
        # 找出新创建的日志文件（启动后创建的）
        new_log_files = log_files_after_start - self.log_files_before_start
        
        if new_log_files:
            # 如果有新文件，使用最新的新文件
            latest_log = max(new_log_files, key=os.path.getmtime)
            self.log_file_path = latest_log
            print(f"[LogMonitor] 找到exe启动后创建的日志文件: {os.path.basename(latest_log)}")
        else:
            # 如果没有新文件，使用最新的日志文件（可能是之前就存在的）
            log_files = list(log_files_after_start)
            if log_files:
                latest_log = max(log_files, key=os.path.getmtime)
                self.log_file_path = latest_log
                print(f"[LogMonitor] 使用最新的日志文件: {os.path.basename(latest_log)}")
            else:
                # 未找到日志文件时：若已启用进程输出监控，以之为准，不报错
                if getattr(self, '_output_monitor', None):
                    print("[LogMonitor] 未找到日志文件，将仅使用进程输出监控（stdout/stderr）")
                else:
                    print("[LogMonitor] 警告: 未找到日志文件")
                return
        
        # 清除之前的异常记录，开始新的监控周期
        self.log_monitor.clear_anomalies()
        self.last_checked_anomaly_count = 0  # 重置计数器
        
        # 启动监控（从文件末尾开始，只监控新内容）
        self.log_monitor.start_monitoring(self.log_file_path)
        print(f"[LogMonitor] [OK] 实时日志监控已启动: {os.path.basename(self.log_file_path)}")
        print(f"[LogMonitor]   监控模式: 实时读取exe日志输出，检测错误和警告")

    def stop_log_monitoring(self):
        """停止日志监控"""
        if self.log_monitor:
            self.log_monitor.stop_monitoring()
            print("[LogMonitor] [OK] 日志监控已停止")

    def check_log_errors(self, context: str = "", wait_for_log: float = 0.5) -> List[Dict]:
        """
        检查日志中的错误（实时检查）
        
        Args:
            context: 上下文信息，用于标识检查时的操作
            wait_for_log: 等待日志写入的时间（秒）
            
        Returns:
            错误日志列表
        """
        errors = []
        if not self.log_monitor:
            return errors
        
        # 等待日志写入
        if wait_for_log > 0:
            time.sleep(wait_for_log)
        
        # 获取所有异常
        all_anomalies = self.log_monitor.get_anomalies()
        
        # 只获取新的异常（自上次检查以来的）
        new_anomalies = all_anomalies[self.last_checked_anomaly_count:]
        self.last_checked_anomaly_count = len(all_anomalies)
        
        # 过滤出错误和严重错误
        for anomaly in new_anomalies:
            level = anomaly.get('level', '').lower()
            if level in ['error', 'critical']:
                errors.append({
                    'level': level,
                    'message': anomaly.get('message', ''),
                    'timestamp': anomaly.get('timestamp', ''),
                    'context': context,
                    'full_line': anomaly.get('full_line', '')
                })

        return errors

    def log_test(self, name: str, passed: bool, details: str = "", check_log: bool = True, wait_for_log: float = 1.5):
        """
        记录测试结果，并实时检查exe日志输出
        
        Args:
            name: 测试名称
            passed: 是否通过（初始状态，可能被日志检查覆盖）
            details: 详细信息
            check_log: 是否检查日志错误
            wait_for_log: 等待日志写入的时间（秒）
        """
        # 如果启用日志检查，在记录测试结果前检查日志错误
        log_errors = []
        log_success_hints = []
        
        if check_log and self.log_monitor:
            # 等待日志写入
            log_errors = self.check_log_errors(context=name, wait_for_log=wait_for_log)
            
            # 检查是否有成功提示（可选，用于验证操作是否真的成功）
            if self.log_file_path and os.path.exists(self.log_file_path):
                try:
                    # 读取最后几行，查找成功相关的日志
                    with open(self.log_file_path, 'r', encoding='utf-8', errors='ignore') as f:
                        lines = f.readlines()
                        # 检查最后50行
                        recent_lines = lines[-50:] if len(lines) > 50 else lines
                        for line in recent_lines:
                            line_lower = line.lower()
                            # 查找成功相关的日志（根据实际日志格式调整）
                            if 'success' in line_lower or 'succeed' in line_lower:
                                if 'error' not in line_lower and 'fail' not in line_lower:
                                    log_success_hints.append(line.strip()[:100])
                except Exception as e:
                    pass  # 忽略读取错误
            
            if log_errors:
                # 如果发现日志错误，标记测试为失败
                if passed:
                    passed = False
                    error_messages = [f"[{e['level']}] {e['message'][:80]}" for e in log_errors[:3]]  # 最多显示3个错误
                    if len(log_errors) > 3:
                        error_messages.append(f"... 还有 {len(log_errors) - 3} 个错误")
                    details = f"{details}\n  [EXE日志错误] {'; '.join(error_messages)}" if details else f"[EXE日志错误] {'; '.join(error_messages)}"
                
                # 实时输出错误信息
                print(f"  ⚠️  [EXE日志] 在 '{name}' 操作中检测到错误:")
                for error in log_errors[:3]:  # 最多显示3个错误
                    level_icon = "🔴" if error['level'] == 'critical' else "❌"
                    print(f"    {level_icon} [{error['level'].upper()}] {error['message'][:120]}")
                if len(log_errors) > 3:
                    print(f"    ... 还有 {len(log_errors) - 3} 个错误（查看完整日志文件获取详情）")

        # 进程输出监控（stdout/stderr 关键错误，与 run_closed_loop_tests / core 一致）
        if passed and getattr(self, '_output_monitor', None) and self._output_monitor.has_failures():
            passed = False
            summary = self._output_monitor.get_errors_summary()
            details = (details + "\n  [进程输出错误] " if details else "[进程输出错误] ") + (summary[:300] or "检测到关键错误")
            log_errors.append({
                'level': 'critical',
                'message': summary.splitlines()[0][:150] if summary else 'ProcessOutputMonitor',
                'timestamp': '',
                'context': name,
                'full_line': summary
            })

        result = {
            "name": name,
            "passed": passed,
            "details": details,
            "timestamp": datetime.now().strftime('%H:%M:%S'),
            "log_errors": log_errors,  # 保存关联的日志错误
            "log_success_hints": log_success_hints  # 保存成功提示
        }
        self.test_results.append(result)

        symbol = "[PASS]" if passed else "[FAIL]"
        print(f"  [{symbol}] {name}")
        if details:
            # 分行显示details，每行缩进
            for detail_line in details.split('\n'):
                print(f"      {detail_line}")

    def start_player(self) -> bool:
        """启动播放器"""
        print("\n[步骤 1/7] 启动播放器...")

        try:
            if not os.path.exists(self.exe_path):
                raise FileNotFoundError(f"可执行文件不存在: {self.exe_path}")

            # 与 core/ui_automation 一致：测试模式下让 exe 可写日志等
            env = os.environ.copy()
            if 'WZ_LOG_MODE' not in env:
                env['WZ_LOG_MODE'] = '1'
            if 'WZ_TEST_MODE' not in env:
                env['WZ_TEST_MODE'] = '1'

            # 启动程序（可选捕获 stdout/stderr，用于 ProcessOutputMonitor）
            if self._capture_process_output and ProcessOutputMonitor:
                self.process = subprocess.Popen(
                    [self.exe_path],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    cwd=os.path.dirname(self.exe_path),
                    env=env,
                )
                self._output_monitor = ProcessOutputMonitor(
                    self.process, self.process.stdout, capture_all=False
                )
                self._output_monitor.start()
            else:
                self.process = subprocess.Popen(
                    [self.exe_path],
                    cwd=os.path.dirname(self.exe_path),
                    env=env,
                )
            time.sleep(8)  # Qt6程序需要足够长的启动时间

            # 验证进程是否还在运行
            if self.process.poll() is not None:
                err_msg = "播放器进程启动后立即退出"
                if self._output_monitor:
                    summary = self._output_monitor.get_errors_summary()
                    if summary:
                        err_msg += "\n" + summary[:500]
                raise RuntimeError(err_msg)

            # 启动日志监控
            self.start_log_monitoring()
            
            # 等待日志初始化
            time.sleep(1.0)
            
            # 重置异常计数（从启动后开始监控）
            if self.log_monitor:
                self.last_checked_anomaly_count = len(self.log_monitor.get_anomalies())

            self.log_test("启动播放器", True, f"已启动: {os.path.basename(self.exe_path)}", check_log=True)
            return True

        except Exception as e:
            self.log_test("启动播放器", False, str(e))
            return False

    def open_video(self) -> bool:
        """打开视频文件"""
        print("\n[步骤 2/7] 打开视频文件...")

        if not self.test_video_path or not os.path.exists(self.test_video_path):
            self.log_test("打开视频文件", False, f"视频文件不存在: {self.test_video_path}")
            return False

        try:
            print(f"  正在打开视频: {os.path.basename(self.test_video_path)}")

            # 使用 Ctrl+O 打开文件（实际快捷键）
            send_keys("^o")
            time.sleep(2.0)

            # 输入文件路径
            send_keys(self.test_video_path)
            time.sleep(1.0)
            send_keys("{ENTER}")
            time.sleep(8.0)  # 等待视频完全加载（Qt6需要更长时间）

            self.log_test("打开视频文件", True, "使用 Ctrl+O 快捷键", check_log=True)
            return True

        except Exception as e:
            self.log_test("打开视频文件", False, str(e))
            return False

    def test_play_pause(self) -> bool:
        """测试播放/暂停"""
        print("\n[步骤 3/7] 测试播放/暂停...")
        try:
            # 使用空格键暂停
            send_keys("{SPACE}")
            self.log_test("暂停", True, "使用空格键", check_log=True, wait_for_log=1.5)

            # 使用空格键播放
            send_keys("{SPACE}")
            self.log_test("播放", True, "使用空格键", check_log=True, wait_for_log=1.5)

            return True

        except Exception as e:
            self.log_test("播放/暂停", False, str(e), check_log=True)
            return False

    def test_stop(self) -> bool:
        """测试停止播放"""
        print("\n[步骤 4/7] 测试停止播放...")
        try:
            # 使用 Ctrl+C 停止（实际快捷键）
            send_keys("^{c}")
            time.sleep(1.0)
            self.log_test("停止播放", True, "使用 Ctrl+C 快捷键", check_log=True)

            return True

        except Exception as e:
            self.log_test("停止播放", False, str(e))
            return False

    def test_seek_small(self) -> bool:
        """测试小幅seek（5秒）使用方向键"""
        print("\n[步骤 5/7] 测试小幅seek...")
        try:
            # 先播放
            send_keys("{SPACE}")
            time.sleep(2.0)

            print("  使用右方向键seek前进5秒...")
            send_keys("{RIGHT}")
            time.sleep(4.0)  # Qt6 seek需要更长时间
            self.log_test("小幅seek前进", True, "使用右方向键", check_log=True)

            print("  使用左方向键seek后退5秒...")
            send_keys("{LEFT}")
            time.sleep(4.0)
            self.log_test("小幅seek后退", True, "使用左方向键", check_log=True)

            return True

        except Exception as e:
            self.log_test("小幅seek", False, str(e), check_log=True)
            return False

    def test_seek_large(self) -> bool:
        """测试大幅seek（10秒）使用方向键"""
        print("\n[步骤 6/7] 测试大幅seek...")
        try:
            print("  使用上方向键seek前进10秒...")
            send_keys("{UP}")
            time.sleep(4.0)  # Qt6 seek需要更长时间
            self.log_test("大幅seek前进", True, "使用上方向键", check_log=True)

            print("  使用下方向键seek后退10秒...")
            send_keys("{DOWN}")
            time.sleep(4.0)
            self.log_test("大幅seek后退", True, "使用下方向键", check_log=True)

            return True

        except Exception as e:
            self.log_test("大幅seek", False, str(e), check_log=True)
            return False

    def test_multiple_seeks(self) -> bool:
        """测试多次连续seek"""
        print("\n[步骤 7/7] 测试多次连续seek...")
        try:
            print("  执行10次连续seek（每次10%）...")
            for i in range(10):
                # 使用右方向键进行连续seek
                send_keys("{RIGHT}")
                time.sleep(2.0)  # 每次间隔2秒
                self.log_test(f"连续seek #{i+1}", True, "使用右方向键", check_log=True)

            print("  执行10次快速连续seek（每次间隔100ms）...")
            for i in range(10):
                # 使用左方向键进行快速连续seek
                send_keys("{LEFT}")
                time.sleep(0.1)  # 非常短的间隔
                # 快速seek时只在最后检查一次日志，避免频繁检查
                check_log = (i == 9)
                self.log_test(f"快速seek #{i+1}", True, "使用左方向键", check_log=check_log)

            return True

        except Exception as e:
            self.log_test("多次连续seek", False, str(e), check_log=True)
            return False

    def test_volume(self) -> bool:
        """测试音量控制"""
        print("\n[步骤 8/7] 测试音量控制...")
        try:
            # 使用上方向键增加音量（实际快捷键）
            send_keys("{PGUP}")
            time.sleep(0.5)
            self.log_test("音量增加", True, "使用 PageUp 键", check_log=True)

            # 使用下方向键减少音量（实际快捷键）
            send_keys("{PGDN}")
            time.sleep(0.5)
            self.log_test("音量减少", True, "使用 PageDown 键", check_log=True)

            # 测试静音使用 M 键
            send_keys("m")
            time.sleep(0.5)
            send_keys("m")  # 再次切换静音
            time.sleep(0.5)
            # 等待日志写入
            time.sleep(0.5)
            self.log_test("静音切换", True, "使用 M 键", check_log=True)

            return True

        except Exception as e:
            self.log_test("音量控制", False, str(e), check_log=True)
            return False

    def test_video_switch_twice(self) -> bool:
        """BUG-018 回归：连续切换两次视频应不崩溃（stop 不调用 DemuxerThread::requestStop）"""
        print("\n[步骤] BUG-018: 视频切换两次...")
        try:
            send_keys("^o")
            time.sleep(2.0)
            send_keys(self.test_video_path)
            time.sleep(1.0)
            send_keys("{ENTER}")
            time.sleep(6.0)
            send_keys("^o")
            time.sleep(2.0)
            send_keys(self.test_video_path)
            time.sleep(1.0)
            send_keys("{ENTER}")
            time.sleep(6.0)
            self.log_test("BUG-018 视频切换两次", True, "两次打开同一视频，无崩溃")
            return True
        except Exception as e:
            self.log_test("BUG-018 视频切换两次", False, str(e))
            return False

    def generate_report(self) -> str:
        """生成测试报告（包含异常日志摘要）"""
        report_lines = []
        report_lines.append("=" * 80)
        report_lines.append("WZMediaPlayer 自动化测试报告（增强版）")
        report_lines.append("=" * 80)
        report_lines.append(f"测试时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        report_lines.append(f"测试视频: {self.test_video_path or 'N/A'}")
        report_lines.append("")

        # 统计
        total = len(self.test_results)
        passed = sum(1 for r in self.test_results if r["passed"])
        failed = total - passed
        
        # 统计日志错误
        total_log_errors = 0
        for r in self.test_results:
            if 'log_errors' in r and r['log_errors']:
                total_log_errors += len(r['log_errors'])

        report_lines.append(f"总计: {total} | 通过: {passed} | 失败: {failed}")
        if total_log_errors > 0:
            report_lines.append(f"检测到日志错误: {total_log_errors} 个")
        if getattr(self, '_output_monitor', None) and self._output_monitor.has_failures():
            report_lines.append("检测到进程输出错误（stdout/stderr）")
        report_lines.append("")

        # 进程输出错误摘要（与 run_closed_loop_tests 一致）
        if getattr(self, '_output_monitor', None) and self._output_monitor.has_failures():
            report_lines.append("-" * 80)
            report_lines.append("进程输出错误（ProcessOutputMonitor）:")
            report_lines.append("-" * 80)
            report_lines.append(self._output_monitor.get_errors_summary())
            report_lines.append("")

        # 异常日志摘要
        if self.log_monitor:
            report_lines.append("-" * 80)
            report_lines.append("异常日志摘要:")
            report_lines.append("-" * 80)
            anomaly_report = self.log_monitor.generate_report()
            report_lines.append(anomaly_report)
            report_lines.append("")

        # 详细结果
        report_lines.append("-" * 80)
        report_lines.append("测试详情:")
        report_lines.append("-" * 80)

        for i, result in enumerate(self.test_results, 1):
            symbol = "PASS" if result["passed"] else "FAIL"
            report_lines.append(f"{i}. [{symbol}] {result['name']} (时间: {result['timestamp']})")
            if result["details"]:
                report_lines.append(f"   {result['details']}")
            
            # 显示关联的日志错误
            if 'log_errors' in result and result['log_errors']:
                report_lines.append(f"   关联的日志错误 ({len(result['log_errors'])} 个):")
                for log_error in result['log_errors']:
                    error_msg = log_error.get('message', '')[:150]
                    error_level = log_error.get('level', 'unknown').upper()
                    report_lines.append(f"     [{error_level}] {error_msg}")
            
            report_lines.append("")

        report_lines.append("=" * 80)

        return "\n".join(report_lines)

    def save_report(self, report: str):
        """保存测试报告到文件"""
        try:
            timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
            filename = f"test_report_enhanced_{timestamp}.txt"

            script_dir = os.path.dirname(os.path.abspath(__file__))
            report_path = os.path.join(script_dir, filename)

            with open(report_path, 'w', encoding='utf-8') as f:
                f.write(report)

            print(f"\n[OK] 测试报告已保存: {report_path}")
        except Exception as e:
            print(f"\n[FAIL] 保存测试报告失败: {e}")

    def stop_player(self):
        """停止播放器"""
        try:
            if getattr(self, '_output_monitor', None):
                self._output_monitor.stop()
                self._output_monitor = None
            self.stop_log_monitoring()
            if self.process and self.process.poll() is None:
                self.process.terminate()
                time.sleep(2)
        except Exception as e:
            print(f"停止播放器错误: {e}")

    def get_process_output_summary(self) -> str:
        """返回进程输出监控错误摘要（供 run_all_tests 写入报告）"""
        if getattr(self, '_output_monitor', None):
            return self._output_monitor.get_errors_summary()
        return ""


def main():
    """主函数"""
    print("=" * 80)
    print("WZMediaPlayer 3D Player 自动化测试（增强版）")
    print("使用快捷键控制 + 实时日志监控")
    print("=" * 80)
    print()

    # 配置路径（根据实际情况修改）
    try:
        import config as test_config
        exe_path = test_config.PLAYER_EXE_PATH
        test_video_path = test_config.TEST_VIDEO_PATH
    except ImportError:
        exe_path = r"D:\2026Github\build\Release\WZMediaPlayer.exe"
        test_video_path = r"D:\2026Github\testing\video\test.mp4"

    # 创建测试器
    tester = WZMediaPlayerTester(exe_path, test_video_path)

    try:
        # 启动播放器
        if not tester.start_player():
            print("\n[错误] 无法启动播放器")
            # 即使启动失败也保存报告
            report = tester.generate_report()
            print("\n" + report)
            tester.save_report(report)
            return 1

        # 运行测试
        time.sleep(3)  # 额外等待，确保GUI完全初始化

        # 1. 打开视频
        if not tester.open_video():
            print("[错误] 打开视频失败")

        # 2. 测试播放/暂停
        time.sleep(5)  # 让视频播放一会
        if not tester.test_play_pause():
            print("[错误] 播放/暂停测试失败")

        # 3. 测试停止
        time.sleep(1)
        if not tester.test_stop():
            print("[错误] 停止测试失败")

        # 4. 测试小幅seek
        time.sleep(2)
        if not tester.test_seek_small():
            print("[错误] 小幅seek测试失败")

        # 5. 测试大幅seek
        time.sleep(2)
        if not tester.test_seek_large():
            print("[错误] 大幅seek测试失败")

        # 6. 测试多次连续seek
        time.sleep(2)
        if not tester.test_multiple_seeks():
            print("[错误] 多次连续seek测试失败")

        # 7. 测试音量控制
        time.sleep(2)
        if not tester.test_volume():
            print("[错误] 音量控制测试失败")

        # 生成并保存报告
        report = tester.generate_report()
        print("\n" + report)
        tester.save_report(report)

        # 检查是否有失败
        has_failures = any(not r["passed"] for r in tester.test_results)
        return 1 if has_failures else 0

    except KeyboardInterrupt:
        print("\n测试被用户中断")
        # 即使中断也保存报告
        try:
            report = tester.generate_report()
            print("\n" + report)
            tester.save_report(report)
        except Exception as e:
            print(f"保存报告失败: {e}")
        tester.stop_player()
        return 1

    except Exception as e:
        print(f"\n测试过程中发生异常: {e}")
        import traceback
        traceback.print_exc()
        # 即使有异常也保存报告
        try:
            report = tester.generate_report()
            print("\n" + report)
            tester.save_report(report)
        except Exception as report_error:
            print(f"保存报告失败: {report_error}")
        tester.stop_player()
        return 1


if __name__ == "__main__":
    sys.exit(main())
