"""
闭环测试基类 - 提供完整的自动化测试框架
"""

import os
import sys
import time
import traceback
from typing import Optional, Dict, List, Callable, Any
from dataclasses import dataclass, field
from datetime import datetime

# 导入核心模块
from .ui_automation import UIAutomationController
from .image_verifier import ImageVerifier, ImageVerificationResult
from .qt_ui_parser import QtUIParser


@dataclass
class TestResult:
    """测试结果"""
    name: str
    passed: bool
    details: str = ""
    timestamp: str = field(default_factory=lambda: datetime.now().strftime('%H:%M:%S'))
    duration_ms: int = 0
    screenshot_path: str = ""
    ui_state: Dict = field(default_factory=dict)
    related_log_lines: List[str] = field(default_factory=list)  # 失败时关联的日志行
    

class ClosedLoopTestBase:
    """
    闭环测试基类
    
    特性：
    1. 自动解析Qt UI结构
    2. 精确的控件操作（基于UIA）
    3. 图像验证（闭环判断）
    4. 智能等待机制
    5. 完整的测试报告
    """
    
    def __init__(self, exe_path: str, test_video_path: str = None,
                 project_root: str = None):
        """
        初始化测试基类
        
        Args:
            exe_path: 播放器可执行文件路径
            test_video_path: 测试视频路径
            project_root: Qt项目根目录（用于解析UI）
        """
        self.exe_path = exe_path
        self.test_video_path = test_video_path
        self.project_root = project_root or os.path.dirname(os.path.dirname(exe_path))
        
        # 初始化核心组件
        self.ui = UIAutomationController(backend="uia")
        self.image_verifier = ImageVerifier()
        self.ui_parser = QtUIParser(self.project_root)
        
        # 测试状态
        self.test_results: List[TestResult] = []
        self.current_test_name: str = ""
        self.test_start_time: float = 0
        
        # 解析UI
        self._parse_ui_structure()
        
    def _parse_ui_structure(self):
        """解析Qt UI结构"""
        # 尝试解析生成的UI头文件
        ui_header_paths = [
            os.path.join(self.project_root, 'WZMediaPlay', 'x64', 'Release', 'qt', 'uic', 'ui_MainWindow.h'),
            os.path.join(self.project_root, 'WZMediaPlay', 'x64', 'Debug', 'qt', 'uic', 'ui_MainWindow.h'),
            os.path.join(self.project_root, 'x64', 'Release', 'qt', 'uic', 'ui_MainWindow.h'),
            os.path.join(self.project_root, 'x64', 'Debug', 'qt', 'uic', 'ui_MainWindow.h'),
            # 直接使用绝对路径
            r'E:\WZMediaPlayer_2025\WZMediaPlay\x64\Release\qt\uic\ui_MainWindow.h',
            r'E:\WZMediaPlayer_2025\WZMediaPlay\x64\Debug\qt\uic\ui_MainWindow.h',
        ]
        
        for header_path in ui_header_paths:
            if os.path.exists(header_path):
                self.ui_parser.parse_ui_header(header_path)
                break
        
        # 解析MainWindow.h
        mainwindow_header = os.path.join(self.project_root, 'WZMediaPlay', 'MainWindow.h')
        if os.path.exists(mainwindow_header):
            self.ui_parser.parse_mainwindow_header(mainwindow_header)
    
    def start_test(self, test_name: str):
        """开始一个测试用例"""
        self.current_test_name = test_name
        self.test_start_time = time.time() * 1000
        self._test_start_wall = time.time()  # 用于日志时间窗口
        print(f"\n[测试] {test_name} ...")
        
    def end_test(self, passed: bool, details: str = "", 
                 screenshot: bool = False) -> TestResult:
        """结束当前测试用例"""
        duration = int(time.time() * 1000 - self.test_start_time)
        
        # 若进程已崩溃或输出中有错误，强制判定为失败
        if passed and hasattr(self, 'ui') and self.ui:
            if not self.ui.is_process_alive() and getattr(self.ui, '_process', None):
                passed = False
                details = ((details + "; ") if details else "") + "被测进程已意外退出（崩溃）"
            elif self.ui.has_process_output_errors():
                passed = False
                err_summary = self.ui.get_process_output_errors_summary()
                details = ((details + "; ") if details else "") + f"进程输出错误: {err_summary[:200]}"
        
        screenshot_path = ""
        if screenshot:
            img = self.ui.capture_window_screenshot()
            if img:
                screenshot_path = self.image_verifier.capture_and_save(
                    img, 
                    f"{self.current_test_name.replace(' ', '_')}_{datetime.now().strftime('%H%M%S')}"
                )
        
        # 获取当前UI状态
        ui_state = self.ui.get_window_state()

        # 失败时获取时间窗口内的关联日志
        related_log_lines: List[str] = []
        if not passed:
            t_start = getattr(self, '_test_start_wall', 0)
            t_end = time.time()
            err_lines: List[str] = []

            # 优先使用 ProcessOutputMonitor
            mon = getattr(self.ui, '_output_monitor', None) if hasattr(self, 'ui') and self.ui else None
            if mon and hasattr(mon, 'get_lines_in_window'):
                lines = mon.get_lines_in_window(t_start, t_end)
                err_lines = [
                    l for l in lines
                    if any(k in l.lower() for k in ['error', 'warn', 'critical', 'failed'])
                ]

            # fallback: ProcessOutputMonitor 无数据时尝试 LogMonitor（文件日志）
            if not err_lines and getattr(self, '_log_monitor', None):
                try:
                    from log_monitor import LogMonitor
                    lm = self._log_monitor
                    if hasattr(lm, 'get_lines_in_window'):
                        log_path = getattr(self, '_log_monitor_path', None)
                        lines = lm.get_lines_in_window(t_start, t_end, log_path)
                        err_lines = [
                            l for l in lines
                            if any(k in l.lower() for k in ['error', 'warn', 'critical', 'failed'])
                        ]
                except Exception as e:
                    pass  # 静默失败，不影响主流程

            if err_lines:
                details += "\n    关联日志:\n" + "\n".join(
                    f"      {l[:120]}" for l in err_lines[-5:]
                )
                related_log_lines = err_lines[-5:]

        result = TestResult(
            name=self.current_test_name,
            passed=passed,
            details=details,
            duration_ms=duration,
            screenshot_path=screenshot_path,
            ui_state=ui_state,
            related_log_lines=related_log_lines
        )
        
        self.test_results.append(result)
        
        symbol = "[PASS]" if passed else "[FAIL]"
        print(f"  [{symbol}] {self.current_test_name} ({duration}ms)")
        if details:
            print(f"      {details}")
        
        return result
    
    def setup(self) -> bool:
        """
        测试准备 - 启动应用
        
        Returns:
            是否准备成功
        """
        print("\n" + "=" * 80)
        print("WZMediaPlayer 闭环自动化测试")
        print("=" * 80)

        # 测试前备份并强制 LogMode=1，使播放器输出到 console
        self._config_backup_logmode = None
        config_path = os.path.join(os.path.dirname(self.exe_path), "config", "SystemConfig.ini")
        if os.path.exists(config_path):
            try:
                import re
                with open(config_path, 'r', encoding='utf-8') as f:
                    content = f.read()
                match = re.search(r'LogMode=(\d+)', content)
                if match:
                    self._config_backup_logmode = match.group(1)
                    content = re.sub(r'LogMode=\d+', 'LogMode=1', content, count=1)
                    with open(config_path, 'w', encoding='utf-8') as f:
                        f.write(content)
            except Exception as e:
                print(f"[警告] 无法修改 LogMode: {e}")

        # 打印UI控件信息
        self.ui_parser.print_control_summary()

        # 初始化 LogMonitor 作为日志关联 fallback（当 ProcessOutputMonitor 无数据时使用）
        self._log_monitor = None
        self._log_monitor_path = None
        try:
            logs_dir = os.path.join(os.path.dirname(self.exe_path), "logs")
            from log_monitor import LogMonitor
            latest_log = LogMonitor.find_latest_log(logs_dir)
            if latest_log:
                self._log_monitor = LogMonitor({
                    'enabled': True,
                    'level_threshold': 'warn',
                    'patterns': ['error', 'warn', 'critical', 'failed'],
                })
                self._log_monitor_path = latest_log
        except Exception as e:
            pass  # 静默，fallback 不可用不影响主流程

        # 确保 exe 同目录有 config（worktree 构建可能未复制）
        exe_dir = os.path.dirname(self.exe_path)
        config_dir = os.path.join(exe_dir, "config")
        config_ini = os.path.join(config_dir, "SystemConfig.ini")
        if not os.path.exists(config_ini):
            for src in [
                os.path.join(exe_dir, "..", "..", "config", "SystemConfig.ini"),
                os.path.join(exe_dir, "..", "..", "..", "WZMediaPlay", "config", "SystemConfig.ini"),
            ]:
                src = os.path.normpath(src)
                if os.path.exists(src):
                    os.makedirs(config_dir, exist_ok=True)
                    import shutil
                    shutil.copy2(src, config_ini)
                    break

        # 启动应用
        if not self.ui.start_application(self.exe_path):
            self.start_test("启动应用")
            self.end_test(False, "应用启动失败")
            return False

        # 等待应用完全初始化
        time.sleep(3)

        # 尝试连接 AudioPipeClient（TestPipeServer 命名管道），用于验证音频
        self.audio_pipe = None
        try:
            from core.audio_pipe_client import AudioPipeClient
            ap = AudioPipeClient()
            if ap.connect():
                self.audio_pipe = ap
        except Exception as e:
            pass  # 管道不可用时跳过音频验证

        return True
    
    def teardown(self):
        """测试清理 - 关闭应用"""
        print("\n" + "=" * 80)
        print("测试完成，关闭应用")
        print("=" * 80)

        # 恢复 LogMode 配置
        if getattr(self, '_config_backup_logmode', None) is not None:
            config_path = os.path.join(os.path.dirname(self.exe_path), "config", "SystemConfig.ini")
            if os.path.exists(config_path):
                try:
                    import re
                    with open(config_path, 'r', encoding='utf-8') as f:
                        content = f.read()
                    content = re.sub(
                        r'LogMode=\d+',
                        f'LogMode={self._config_backup_logmode}',
                        content,
                        count=1
                    )
                    with open(config_path, 'w', encoding='utf-8') as f:
                        f.write(content)
                except Exception as e:
                    print(f"[警告] 无法恢复 LogMode: {e}")

        # 检查进程输出中的错误（ALSOFT、QMutex 等），纳入测试报告
        if self.ui.has_process_output_errors():
            err_summary = self.ui.get_process_output_errors_summary()
            self.start_test("进程输出错误检测")
            self.end_test(False, f"检测到运行时错误:\n{err_summary}", screenshot=False)
            print(f"[警告] 进程输出中检测到错误，已纳入测试失败")
        
        # 检查进程是否意外退出（崩溃）
        if not self.ui.is_process_alive() and hasattr(self.ui, '_process') and self.ui._process:
            self.start_test("进程存活检测")
            self.end_test(False, "被测进程已意外退出（可能崩溃）", screenshot=False)
            print("[警告] 被测进程已退出，可能发生崩溃")
        
        if getattr(self, 'audio_pipe', None):
            try:
                self.audio_pipe.disconnect()
            except Exception:
                pass
            self.audio_pipe = None

        self.ui.close_application()
    
    def run_test_with_retry(self, test_func: Callable, 
                           max_retries: int = 1,
                           retry_delay: float = 1.0) -> bool:
        """
        执行测试函数，支持重试
        
        Args:
            test_func: 测试函数
            max_retries: 最大重试次数
            retry_delay: 重试间隔（秒）
            
        Returns:
            测试是否通过
        """
        for attempt in range(max_retries + 1):
            try:
                result = test_func()
                if result:
                    return True
                    
                if attempt < max_retries:
                    print(f"    重试 ({attempt + 1}/{max_retries})...")
                    time.sleep(retry_delay)
                    
            except Exception as e:
                print(f"    异常: {e}")
                if attempt < max_retries:
                    time.sleep(retry_delay)
                else:
                    traceback.print_exc()
        
        return False
    
    def wait_for_condition(self, condition_func: Callable, 
                          timeout: float = 10.0,
                          interval: float = 0.5,
                          description: str = "") -> bool:
        """
        等待条件满足
        
        Args:
            condition_func: 条件函数（返回bool）
            timeout: 超时时间（秒）
            interval: 检查间隔（秒）
            description: 条件描述
            
        Returns:
            条件是否满足
        """
        start_time = time.time()
        
        while time.time() - start_time < timeout:
            try:
                if condition_func():
                    return True
            except Exception as e:
                pass
            
            time.sleep(interval)
        
        if description:
            print(f"    等待超时: {description}")
        return False
    
    def verify_ui_state(self, expected_state: Dict[str, Any]) -> (bool, str):
        """
        验证UI状态
        
        Args:
            expected_state: 期望的状态字典
            
        Returns:
            (是否通过, 详细信息)
        """
        actual_state = self.ui.get_window_state()
        
        mismatches = []
        for key, expected in expected_state.items():
            actual = actual_state.get(key)
            if actual != expected:
                mismatches.append(f"{key}: 期望={expected}, 实际={actual}")
        
        if mismatches:
            return False, "; ".join(mismatches)
        
        return True, "UI状态验证通过"
    
    def verify_video_playing(self, check_duration: float = 2.0) -> (bool, str):
        """
        验证视频是否正在播放（通过图像比较）
        
        Args:
            check_duration: 检查持续时间（秒）
            
        Returns:
            (是否通过, 详细信息)
        """
        # 截取第一帧
        img1 = self.ui.capture_window_screenshot()
        if not img1:
            return False, "无法截取第一帧"
        
        time.sleep(check_duration)
        
        # 截取第二帧
        img2 = self.ui.capture_window_screenshot()
        if not img2:
            return False, "无法截取第二帧"
        
        # 比较两帧
        result = self.image_verifier.verify_video_playing(img1, img2)
        
        return result.passed, result.message
    
    def verify_control_property(self, automation_id: str, 
                                property_name: str,
                                expected_value: Any) -> (bool, str):
        """
        验证控件属性
        
        Args:
            automation_id: 控件自动化ID
            property_name: 属性名 ('text', 'enabled', 'visible', 'value')
            expected_value: 期望值
            
        Returns:
            (是否通过, 详细信息)
        """
        control = self.ui.get_control(automation_id=automation_id)
        
        if not control:
            return False, f"控件不存在: {automation_id}"
        
        try:
            if property_name == 'text':
                actual = control.window_text()
            elif property_name == 'enabled':
                actual = control.is_enabled()
            elif property_name == 'visible':
                actual = control.is_visible()
            elif property_name == 'value':
                actual = control.get_value()
            else:
                return False, f"未知属性: {property_name}"
            
            if actual == expected_value:
                return True, f"{property_name}={actual}"
            else:
                return False, f"{property_name}: 期望={expected_value}, 实际={actual}"
                
        except Exception as e:
            return False, f"获取属性失败: {e}"
    
    def generate_report(self) -> str:
        """生成测试报告"""
        lines = []
        lines.append("=" * 80)
        lines.append("WZMediaPlayer 闭环自动化测试报告")
        lines.append("=" * 80)
        lines.append(f"测试时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        lines.append(f"测试视频: {self.test_video_path or 'N/A'}")
        lines.append(f"应用路径: {self.exe_path}")
        lines.append("")
        
        # 统计
        total = len(self.test_results)
        passed = sum(1 for r in self.test_results if r.passed)
        failed = total - passed
        total_duration = sum(r.duration_ms for r in self.test_results)
        
        lines.append(f"总计: {total} | 通过: {passed} | 失败: {failed} | 总耗时: {total_duration}ms")
        lines.append("")
        
        # 详细结果
        lines.append("-" * 80)
        lines.append("测试详情:")
        lines.append("-" * 80)
        
        for i, result in enumerate(self.test_results, 1):
            symbol = "PASS" if result.passed else "FAIL"
            lines.append(f"\n{i}. [{symbol}] {result.name} ({result.duration_ms}ms)")
            if result.details:
                lines.append(f"   详情: {result.details}")
            if result.related_log_lines:
                lines.append("   关联日志:")
                for log_line in result.related_log_lines:
                    lines.append(f"     {log_line[:120]}")
            if result.screenshot_path:
                lines.append(f"   截图: {result.screenshot_path}")
        
        lines.append("\n" + "=" * 80)
        
        return "\n".join(lines)
    
    def save_report(self, report: str = None):
        """保存测试报告"""
        if report is None:
            report = self.generate_report()
        
        try:
            timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
            filename = f"closed_loop_test_report_{timestamp}.txt"
            
            script_dir = os.path.dirname(os.path.abspath(__file__))
            report_path = os.path.join(script_dir, '..', 'reports', filename)
            os.makedirs(os.path.dirname(report_path), exist_ok=True)
            
            with open(report_path, 'w', encoding='utf-8') as f:
                f.write(report)
            
            print(f"\n[报告] 测试报告已保存: {report_path}")
            
        except Exception as e:
            print(f"\n[错误] 保存报告失败: {e}")
    
    def get_control_map(self) -> Dict[str, str]:
        """获取控件映射表"""
        return self.ui_parser.get_playback_controls()
    
    def get_shortcuts(self) -> Dict[str, str]:
        """获取快捷键映射"""
        return self.ui_parser.get_shortcut_map()
