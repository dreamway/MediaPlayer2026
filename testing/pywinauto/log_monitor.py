# 日志监控模块
# 用于在自动化测试过程中实时读取程序日志，检测异常点

import re
import os
import time
import glob
from datetime import datetime
from typing import List, Dict, Optional
import threading


class LogMonitor:
    """日志监控器，实时读取日志文件并检测异常"""

    def __init__(self, config: Dict):
        """
        初始化日志监控器

        Args:
            config: 配置字典，包含以下键：
                - enabled: 是否启用日志监控
                - level_threshold: 日志级别阈值（trace、debug、info、warn、error、critical）
                - patterns: 异常模式列表（正则表达式）
                - log_file_pattern: 日志文件路径模式
        """
        self.enabled = config.get('enabled', True)
        self.level_threshold = config.get('level_threshold', 'warn').lower()
        self.patterns = config.get('patterns', ['error', 'warning', 'critical'])
        self.log_file_pattern = config.get('log_file_pattern', 'logs/MediaPlayer_*.log')

        # 日志级别优先级（数字越大级别越高）
        self.level_priority = {
            'trace': 0,
            'debug': 1,
            'info': 2,
            'warn': 3,
            'warning': 3,
            'error': 4,
            'critical': 5
        }

        # 异常日志存储
        self.anomalies: List[Dict] = []
        self.anomaly_lock = threading.Lock()
        
        # 已检查的异常索引（用于跟踪哪些异常已经被检查过）
        self.checked_indices: set = set()
        self.checked_lock = threading.Lock()

        # 监控线程
        self.monitor_thread: Optional[threading.Thread] = None
        self.is_monitoring = False

        # 文件位置追踪
        self.file_position = 0
        self._log_file_path: Optional[str] = None

    def _parse_timestamp_to_epoch(self, timestamp_str: str) -> Optional[float]:
        """将日志时间戳字符串转换为 Unix 时间戳（秒）"""
        if not timestamp_str:
            return None
        s = timestamp_str.strip()
        for fmt in (
            '%Y-%m-%d %H:%M:%S.%f',
            '%Y-%m-%dT%H:%M:%S.%f',
            '%Y-%m-%d %H:%M:%S',
            '%Y-%m-%dT%H:%M:%S',
        ):
            try:
                part = s[:26] if '.%f' in fmt else s[:19]
                dt = datetime.strptime(part, fmt)
                return dt.timestamp()
            except (ValueError, TypeError):
                continue
        return None

    def get_lines_in_window(
        self, t_start: float, t_end: float, log_file_path: Optional[str] = None
    ) -> List[str]:
        """
        返回时间窗口内的日志行。t_start, t_end 为 time.time() 秒。
        从 anomalies 或直接读取日志文件筛选。
        """
        path = log_file_path or self._log_file_path
        if not path or not os.path.exists(path):
            # 从 anomalies 筛选（anomalies 有 timestamp 字符串，需转换）
            with self.anomaly_lock:
                result = []
                for a in self.anomalies:
                    ts = self._parse_timestamp_to_epoch(a.get('timestamp', ''))
                    if ts and t_start <= ts <= t_end:
                        result.append(a.get('full_line', a.get('message', '')))
                return result

        result = []
        try:
            with open(path, 'r', encoding='utf-8', errors='replace') as f:
                for line in f:
                    line = line.rstrip('\n\r')
                    if not line:
                        continue
                    entry = self._parse_log_line(line)
                    if entry:
                        ts = self._parse_timestamp_to_epoch(entry.get('timestamp', ''))
                        if ts and t_start <= ts <= t_end:
                            result.append(entry.get('full_line', line))
        except Exception as e:
            print(f"[LogMonitor] 读取日志文件失败: {e}")
        return result

    @staticmethod
    def find_latest_log(logs_dir: str, pattern: str = 'MediaPlayer_*.log') -> Optional[str]:
        """查找 logs 目录下最新的日志文件"""
        if not os.path.isdir(logs_dir):
            return None
        files = glob.glob(os.path.join(logs_dir, pattern))
        if not files:
            return None
        return max(files, key=os.path.getmtime)

    def start_monitoring(self, log_file_path: str):
        """
        开始监控日志文件

        Args:
            log_file_path: 日志文件路径
        """
        if not self.enabled:
            return

        if not os.path.exists(log_file_path):
            print(f"[LogMonitor] Warning: Log file not found: {log_file_path}")
            return

        self._log_file_path = log_file_path
        # 初始化文件位置
        self.file_position = os.path.getsize(log_file_path)

        # 启动监控线程
        self.is_monitoring = True
        self.monitor_thread = threading.Thread(
            target=self._monitor_loop,
            args=(log_file_path,),
            daemon=True
        )
        self.monitor_thread.start()

        print(f"[LogMonitor] Started monitoring: {log_file_path}")
        print(f"[LogMonitor] Level threshold: {self.level_threshold}")
        print(f"[LogMonitor] Patterns: {self.patterns}")

    def stop_monitoring(self):
        """停止监控日志文件"""
        self.is_monitoring = False

        if self.monitor_thread and self.monitor_thread.is_alive():
            self.monitor_thread.join(timeout=2.0)

        print("[LogMonitor] Stopped monitoring")

    def _monitor_loop(self, log_file_path: str):
        """
        监控循环，持续读取新增日志

        Args:
            log_file_path: 日志文件路径
        """
        while self.is_monitoring:
            try:
                # 读取新增的日志行
                new_lines = self._read_new_lines(log_file_path)

                if new_lines:
                    for line in new_lines:
                        self._process_line(line)

                # 等待1秒再检查
                time.sleep(1.0)

            except Exception as e:
                print(f"[LogMonitor] Error in monitor loop: {e}")
                time.sleep(1.0)

    def _read_new_lines(self, log_file_path: str) -> List[str]:
        """
        读取新增的日志行

        Args:
            log_file_path: 日志文件路径

        Returns:
            新增的日志行列表
        """
        new_lines = []

        try:
            with open(log_file_path, 'r', encoding='utf-8') as f:
                f.seek(self.file_position)
                new_lines = f.readlines()
                self.file_position = f.tell()

        except Exception as e:
            print(f"[LogMonitor] Error reading log file: {e}")

        return new_lines

    def _process_line(self, line: str):
        """
        处理单行日志

        Args:
            line: 日志行
        """
        try:
            # 解析日志行
            log_entry = self._parse_log_line(line)

            if not log_entry:
                return

            # 检查是否为异常
            if self._is_anomaly(log_entry):
                with self.anomaly_lock:
                    self.anomalies.append(log_entry)

                print(f"[LogMonitor] Anomaly detected: [{log_entry['level']}] {log_entry['message'][:100]}")

        except Exception as e:
            print(f"[LogMonitor] Error processing log line: {e}")

    def _parse_log_line(self, line: str) -> Optional[Dict]:
        """
        解析日志行

        Args:
            line: 日志行

        Returns:
            解析后的日志条目字典，或None（解析失败）
        """
        # 常见日志格式：
        # [2026-01-23 10:30:45.123] [level] message
        # [2026-01-23T10:30:45.123] level message
        # timestamp level message

        line = line.strip()

        # 正则表达式匹配
        patterns = [
            r'\[(\d{4}-\d{2}-\d{2}[T ]\d{2}:\d{2}:\d{2}\.\d{3})\]\s*\[(\w+)\]\s*(.+)',
            r'\[(\d{4}-\d{2}-\d{2}[T ]\d{2}:\d{2}:\d{2}\.\d{3})\]\s*(\w+)\s+(.+)',
            r'(\d{4}-\d{2}-\d{2}[T ]\d{2}:\d{2}:\d{2}\.\d{3})\s*\[(\w+)\]\s*(.+)'
        ]

        for pattern in patterns:
            match = re.match(pattern, line)
            if match:
                timestamp_str, level, message = match.groups()

                return {
                    'timestamp': timestamp_str,
                    'level': level.lower(),
                    'message': message,
                    'full_line': line
                }

        return None

    def _is_anomaly(self, log_entry: Dict) -> bool:
        """
        检查日志条目是否为异常

        Args:
            log_entry: 日志条目字典

        Returns:
            True如果是异常，False否则
        """
        # 检查日志级别
        level = log_entry['level']
        if level in self.level_priority:
            if self.level_priority[level] >= self.level_priority.get(self.level_threshold, 3):
                return True

        # 检查异常模式
        message = log_entry['message'].lower()
        for pattern in self.patterns:
            if pattern.lower() in message:
                return True

        return False

    def get_anomalies(self) -> List[Dict]:
        """
        获取所有检测到的异常

        Returns:
            异常列表
        """
        with self.anomaly_lock:
            return self.anomalies.copy()
    
    def get_new_anomalies(self) -> List[Dict]:
        """
        获取新的异常（自上次检查以来）
        
        Returns:
            新的异常列表
        """
        with self.anomaly_lock:
            with self.checked_lock:
                new_anomalies = []
                for i, anomaly in enumerate(self.anomalies):
                    if i not in self.checked_indices:
                        new_anomalies.append(anomaly)
                        self.checked_indices.add(i)
                return new_anomalies

    def get_anomalies_summary(self) -> Dict:
        """
        获取异常摘要统计

        Returns:
            异常摘要字典
        """
        anomalies = self.get_anomalies()

        summary = {
            'total': len(anomalies),
            'by_level': {
                'critical': 0,
                'error': 0,
                'warning': 0,
                'info': 0,
                'debug': 0
            }
        }

        for anomaly in anomalies:
            level = anomaly['level']
            if level in summary['by_level']:
                summary['by_level'][level] += 1
            elif 'warn' in level:
                summary['by_level']['warning'] += 1

        return summary

    def clear_anomalies(self):
        """清除所有异常记录"""
        with self.anomaly_lock:
            with self.checked_lock:
                self.anomalies.clear()
                self.checked_indices.clear()

    def generate_report(self) -> str:
        """
        生成异常日志报告

        Returns:
            报告文本
        """
        anomalies = self.get_anomalies()
        summary = self.get_anomalies_summary()

        report_lines = []
        report_lines.append("=" * 80)
        report_lines.append("异常日志摘要")
        report_lines.append("=" * 80)
        report_lines.append(f"总计: {summary['total']} 个异常")
        report_lines.append(f"CRITICAL: {summary['by_level']['critical']}")
        report_lines.append(f"ERROR: {summary['by_level']['error']}")
        report_lines.append(f"WARNING: {summary['by_level']['warning']}")
        report_lines.append("")

        # 按级别分组显示
        if summary['by_level']['critical'] > 0:
            report_lines.append("-" * 80)
            report_lines.append(f"CRITICAL ({summary['by_level']['critical']}):")
            for anomaly in [a for a in anomalies if 'crit' in a['level']]:
                report_lines.append(f"  [{anomaly['timestamp']}] {anomaly['message'][:200]}")
            report_lines.append("")

        if summary['by_level']['error'] > 0:
            report_lines.append("-" * 80)
            report_lines.append(f"ERROR ({summary['by_level']['error']}):")
            for anomaly in [a for a in anomalies if 'error' in a['level']]:
                report_lines.append(f"  [{anomaly['timestamp']}] {anomaly['message'][:200]}")
            report_lines.append("")

        if summary['by_level']['warning'] > 0:
            report_lines.append("-" * 80)
            report_lines.append(f"WARNING ({summary['by_level']['warning']}):")
            for anomaly in [a for a in anomalies if 'warn' in a['level']]:
                report_lines.append(f"  [{anomaly['timestamp']}] {anomaly['message'][:200]}")
            report_lines.append("")

        if summary['total'] == 0:
            report_lines.append("无异常")

        report_lines.append("=" * 80)

        return "\n".join(report_lines)


# 使用示例
if __name__ == "__main__":
    # 测试日志监控
    config = {
        'enabled': True,
        'level_threshold': 'warn',
        'patterns': ['error', 'warning', 'critical', 'exception'],
        'log_file_pattern': 'logs/MediaPlayer_*.log'
    }

    monitor = LogMonitor(config)

    # 假设有一个日志文件
    log_file = "test_log.txt"

    # 创建测试日志文件
    with open(log_file, 'w', encoding='utf-8') as f:
        f.write("[2026-01-23 10:30:45.123] [info] Starting application\n")
        f.write("[2026-01-23 10:30:46.456] [debug] Loading configuration\n")
        f.write("[2026-01-23 10:30:47.789] [warn] Configuration file not found, using defaults\n")
        f.write("[2026-01-23 10:30:48.012] [error] Failed to open audio device\n")
        f.write("[2026-01-23 10:30:49.345] [critical] Application crashed\n")

    # 开始监控
    monitor.start_monitoring(log_file)

    # 等待监控
    time.sleep(2)

    # 停止监控
    monitor.stop_monitoring()

    # 生成报告
    report = monitor.generate_report()
    print(report)

    # 清理
    os.remove(log_file)
