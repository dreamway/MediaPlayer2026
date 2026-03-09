"""
进程输出监控器 - 捕获被测应用的 stdout/stderr，检测错误模式并纳入测试失败判定

用于闭环测试中：
1. 捕获 OpenAL (ALSOFT)、Qt (QMutex) 等运行时错误
2. 检测进程意外退出（崩溃）
3. 将检测到的错误纳入测试报告，避免"实际失败但报告显示通过"的问题
"""

import os
import sys
import re
import threading
import time
from typing import Optional, List, Tuple, Dict
from dataclasses import dataclass, field
from datetime import datetime


@dataclass
class DetectedError:
    """检测到的错误"""
    pattern: str
    line: str
    timestamp: str
    severity: str  # 'critical' | 'warning'


# 关键错误模式（出现即判定测试失败）
CRITICAL_PATTERNS = [
    r'QMutex:\s*destroying\s*locked\s*mutex',
    r'Access\s*violation',
    r'0xC0000005',
    r'Exception\s+thrown',
    r'crashed',
    r'crash\s+dump',
    r'Fatal\s+error',
    r'assert\s+failed',
    r'segmentation\s+fault',
    r'stack\s+overflow',
    r'pure\s+virtual\s+function\s+call',
    r'R6025',  # MSVC runtime: pure virtual function call
    r'R6024',  # MSVC runtime: not enough space for _onexit
    r'__debugbreak',
    # Seeking 相关：应用若将 seek 错误打到 stdout/stderr，可被记录并纳入报告
    r'seek\s+(failed|error|invalid)',
    r'Seeking\s+(failed|error|invalid)',
    r'av_seek_frame\s+failed',
    r'seek.*-?\d+\s+\(.*error',
]

# 警告模式（累积超过阈值时判定失败；ALSOFT 等）
WARNING_PATTERNS = [
    (r'\[ALSOFT\]\s*\(WW\)', 10),   # OpenAL 警告，超过10次视为失败
    (r'\[ALSOFT\]\s*\(EE\)', 1),    # OpenAL 错误，1次即失败
    (r'Error\s+generated\s+on\s+context', 15),  # OpenAL 上下文错误，超过15次
    (r'Modifying\s+storage\s+for\s+in-use\s+buffer', 15),  # OpenAL 缓冲区
]

# 编译正则
CRITICAL_REGEX = [re.compile(p, re.IGNORECASE) for p in CRITICAL_PATTERNS]
WARNING_REGEX = [(re.compile(p, re.IGNORECASE), threshold) for p, threshold in WARNING_PATTERNS]


class ProcessOutputMonitor:
    """
    进程输出监控器
    
    在后台线程中读取进程 stdout/stderr，匹配错误模式，
    供测试框架在 end_test 或 teardown 时检查并判定失败。
    """
    
    def __init__(self, process, pipe, capture_all: bool = False):
        """
        Args:
            process: subprocess.Popen 实例
            pipe: 进程的 stdout 或 stderr 管道（合并时使用 stdout）
            capture_all: 是否保存所有输出行（用于报告）
        """
        self._process = process
        self._pipe = pipe
        self._capture_all = capture_all
        
        self._critical_errors: List[DetectedError] = []
        self._warning_counts: Dict[str, int] = {}  # pattern -> count
        self._all_lines: List[str] = []
        self._timestamped_lines: List[Tuple[float, str]] = []  # (time.time(), line), max 5000
        self._max_timestamped = 5000

        self._lock = threading.Lock()
        self._reader_thread: Optional[threading.Thread] = None
        self._stop_event = threading.Event()
        
    def _match_line(self, line: str) -> Optional[Tuple[str, str]]:
        """匹配行，返回 (severity, pattern) 或 None"""
        for regex in CRITICAL_REGEX:
            if regex.search(line):
                return ('critical', regex.pattern)
        
        for regex, threshold in WARNING_REGEX:
            if regex.search(line):
                return ('warning', regex.pattern)
        
        return None
    
    def _reader_loop(self):
        """后台读取管道"""
        try:
            # 使用 iter 逐行读取，避免阻塞
            for line in iter(self._pipe.readline, ''):
                if self._stop_event.is_set():
                    break
                
                try:
                    line = line.decode('utf-8', errors='replace').strip()
                except Exception:
                    line = str(line)
                
                if not line:
                    continue
                
                with self._lock:
                    if self._capture_all:
                        self._all_lines.append(line)
                    # 始终保存带时间戳的行，用于失败时按时间窗口筛选日志
                    ts = time.time()
                    self._timestamped_lines.append((ts, line))
                    if len(self._timestamped_lines) > self._max_timestamped:
                        self._timestamped_lines.pop(0)

                    match = self._match_line(line)
                    if match:
                        severity, pattern = match
                        err = DetectedError(
                            pattern=pattern,
                            line=line[:200],
                            timestamp=datetime.now().strftime('%H:%M:%S'),
                            severity=severity
                        )
                        if severity == 'critical':
                            self._critical_errors.append(err)
                        else:
                            self._warning_counts[pattern] = self._warning_counts.get(pattern, 0) + 1
        except Exception as e:
            with self._lock:
                self._critical_errors.append(DetectedError(
                    pattern='MonitorError',
                    line=str(e),
                    timestamp=datetime.now().strftime('%H:%M:%S'),
                    severity='critical'
                ))
        finally:
            try:
                self._pipe.close()
            except Exception:
                pass
    
    def start(self):
        """启动监控"""
        if self._pipe is None:
            return
        
        self._reader_thread = threading.Thread(target=self._reader_loop, daemon=True)
        self._reader_thread.start()
    
    def stop(self):
        """停止监控"""
        self._stop_event.set()
        if self._reader_thread:  # 线程会在 readline 返回空时退出
            self._reader_thread.join(timeout=2.0)
    
    def has_failures(self) -> bool:
        """是否存在应判定为失败的错误"""
        with self._lock:
            if self._critical_errors:
                return True
            
            for regex, threshold in WARNING_REGEX:
                pattern = regex.pattern
                count = self._warning_counts.get(pattern, 0)
                if count >= threshold:
                    return True
            return False
    
    def get_errors_summary(self) -> str:
        """获取错误摘要，用于测试报告"""
        with self._lock:
            parts = []
            
            if self._critical_errors:
                parts.append(f"关键错误: {len(self._critical_errors)} 条")
                for e in self._critical_errors[:5]:  # 最多显示5条
                    parts.append(f"  - [{e.timestamp}] {e.pattern}: {e.line[:80]}...")
            
            for regex, threshold in WARNING_REGEX:
                pattern = regex.pattern
                count = self._warning_counts.get(pattern, 0)
                if count >= threshold:
                    parts.append(f"警告超阈值: {pattern[:50]}... x {count} (阈值 {threshold})")
            
            return "\n".join(parts) if parts else ""
    
    def get_all_errors(self) -> List[DetectedError]:
        """获取所有关键错误"""
        with self._lock:
            return list(self._critical_errors)

    def get_lines_in_window(self, t_start: float, t_end: float) -> List[str]:
        """返回时间窗口内的日志行。t_start, t_end 为 time.time() 秒。"""
        with self._lock:
            return [line for ts, line in self._timestamped_lines if t_start <= ts <= t_end]

    def is_process_alive(self) -> bool:
        """进程是否仍在运行"""
        return self._process.poll() is None


