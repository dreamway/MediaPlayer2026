# testing/pyobjc/core/log_monitor.py
"""
macOS 日志监控器
用于实时监控应用日志，验证渲染、同步等功能
"""

import os
import sys
import time
import re
import threading
from typing import Callable, Optional, Dict, List
from datetime import datetime

# 添加 pyobjc 目录到路径
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


class LogMonitor:
    """日志监控器"""

    def __init__(self, log_dir: str = None, callback: Callable = None):
        """
        初始化日志监控器

        Args:
            log_dir: 日志目录路径
            callback: 日志回调函数
        """
        self.log_dir = log_dir
        self.callback = callback
        self._running = False
        self._thread = None
        self._last_position = 0
        self._current_log_file = None
        self._patterns = {}

        # 已检测到的事件
        self.events = {
            'render_frames': 0,
            'seek_events': 0,
            'audio_sync': 0,
            'video_sync': 0,
            'errors': [],
            'warnings': [],
        }
        # 播放位置（用于与 UI 同步校验）
        self._last_playback_position_sec = -1   # onUpdatePlayProcess 的 elapsedInSeconds
        self._last_seek_position_ms = -1        # onSeekingFinished 的 positionMs
        self._last_seek_position_sec = -1
        self._playback_position_updates = 0     # 累计收到的进度更新次数

    def find_latest_log(self) -> Optional[str]:
        """查找最新的日志文件"""
        if not self.log_dir or not os.path.exists(self.log_dir):
            return None

        log_files = [f for f in os.listdir(self.log_dir) if f.endswith('.log')]
        if not log_files:
            return None

        # 按修改时间排序
        log_files.sort(key=lambda f: os.path.getmtime(os.path.join(self.log_dir, f)), reverse=True)
        return os.path.join(self.log_dir, log_files[0])

    def set_patterns(self, patterns: Dict[str, str]):
        """
        设置日志匹配模式

        Args:
            patterns: 模式字典 {事件名: 正则表达式}
        """
        self._patterns = patterns

    def parse_line(self, line: str) -> Optional[Dict]:
        """
        解析日志行

        Args:
            line: 日志行

        Returns:
            Dict: 解析结果
        """
        result = {
            'raw': line,
            'timestamp': None,
            'level': None,
            'message': None,
        }

        # 匹配时间戳 [2026-03-10 12:00:00.000]
        ts_match = re.search(r'\[(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d+)\]', line)
        if ts_match:
            result['timestamp'] = ts_match.group(1)

        # 匹配日志级别 [info], [error], [warning], [debug]
        level_match = re.search(r'\[(\w+)\]', line)
        if level_match:
            result['level'] = level_match.group(1).lower()

        # 提取消息内容
        result['message'] = line

        return result

    def analyze_event(self, line: str):
        """
        分析日志行中的事件

        Args:
            line: 日志行
        """
        line_lower = line.lower()

        # 渲染帧计数
        if 'render' in line_lower and 'frame' in line_lower:
            self.events['render_frames'] += 1

        # Seek 事件
        if 'seek' in line_lower:
            self.events['seek_events'] += 1

        # 播放进度：MainWindow::onUpdatePlayProcess 或 onUpdatePlayProcess: elapsedInSeconds=
        m = re.search(r'elapsedInSeconds[=:]?\s*(\d+)', line, re.IGNORECASE)
        if m:
            try:
                self._last_playback_position_sec = int(m.group(1))
                self._playback_position_updates += 1
            except ValueError:
                pass

        # Seek 完成位置：MainWindow::onSeekingFinished: received positionMs=5000ms
        m = re.search(r'positionMs[=:]?\s*(\d+)\s*ms?', line, re.IGNORECASE)
        if not m:
            m = re.search(r'PlayController::seek:\s*(\d+)\s*ms', line)
        if m:
            try:
                self._last_seek_position_ms = int(m.group(1))
                self._last_seek_position_sec = self._last_seek_position_ms // 1000
            except ValueError:
                pass

        # 音视频同步
        if 'audio' in line_lower and 'sync' in line_lower:
            self.events['audio_sync'] += 1
        if 'video' in line_lower and 'sync' in line_lower:
            self.events['video_sync'] += 1

        # 错误和警告
        if '[error]' in line_lower or 'error' in line_lower:
            self.events['errors'].append(line.strip())
        if '[warning]' in line_lower or 'warning' in line_lower:
            self.events['warnings'].append(line.strip())

    def start(self):
        """启动日志监控"""
        self._running = True
        self._thread = threading.Thread(target=self._monitor_loop, daemon=True)
        self._thread.start()

    def stop(self):
        """停止日志监控"""
        self._running = False
        if self._thread:
            self._thread.join(timeout=2)

    def _monitor_loop(self):
        """监控循环"""
        while self._running:
            try:
                log_file = self.find_latest_log()
                if log_file:
                    self._read_new_lines(log_file)
            except Exception as e:
                pass

            time.sleep(0.5)

    def _read_new_lines(self, log_file: str):
        """读取日志文件的新行"""
        try:
            with open(log_file, 'r', encoding='utf-8', errors='ignore') as f:
                # 如果文件变了，从头开始
                if log_file != self._current_log_file:
                    self._current_log_file = log_file
                    self._last_position = 0

                f.seek(self._last_position)
                new_lines = f.readlines()
                self._last_position = f.tell()

                for line in new_lines:
                    self.analyze_event(line)
                    if self.callback:
                        self.callback(line.strip())

        except Exception as e:
            pass

    def get_stats(self) -> Dict:
        """获取统计信息"""
        return self.events.copy()

    def reset_stats(self):
        """重置统计信息"""
        self.events = {
            'render_frames': 0,
            'seek_events': 0,
            'audio_sync': 0,
            'video_sync': 0,
            'errors': [],
            'warnings': [],
        }
        self._last_playback_position_sec = -1
        self._last_seek_position_ms = -1
        self._last_seek_position_sec = -1
        self._playback_position_updates = 0

    def get_last_playback_position_sec(self) -> int:
        """最近一次从日志解析到的播放进度（秒）。无则返回 -1。"""
        return self._last_playback_position_sec

    def get_last_seek_position_ms(self) -> int:
        """最近一次 seek 完成时的目标位置（毫秒）。无则返回 -1。"""
        return self._last_seek_position_ms

    def get_last_seek_position_sec(self) -> int:
        """最近一次 seek 完成时的目标位置（秒）。无则返回 -1。"""
        return self._last_seek_position_sec

    def get_playback_position_update_count(self) -> int:
        """累计收到的播放进度更新次数（onUpdatePlayProcess）。"""
        return self._playback_position_updates


class LogVerifier:
    """日志验证器，用于验证特定功能是否正常"""

    def __init__(self, log_monitor: LogMonitor):
        """
        初始化日志验证器

        Args:
            log_monitor: 日志监控器实例
        """
        self.log_monitor = log_monitor

    def verify_rendering(self, min_frames: int = 10, timeout_sec: float = 10.0) -> bool:
        """
        验证渲染是否正常（检查是否有渲染帧）

        Args:
            min_frames: 最小帧数
            timeout_sec: 超时时间

        Returns:
            bool: 是否正常渲染
        """
        start_time = time.time()
        initial_frames = self.log_monitor.events.get('render_frames', 0)

        while time.time() - start_time < timeout_sec:
            current_frames = self.log_monitor.events.get('render_frames', 0)
            if current_frames - initial_frames >= min_frames:
                return True
            time.sleep(0.5)

        return False

    def verify_seek_completed(self, timeout_sec: float = 5.0) -> bool:
        """
        验证 Seek 是否完成

        Args:
            timeout_sec: 超时时间

        Returns:
            bool: 是否完成
        """
        start_time = time.time()
        initial_seeks = self.log_monitor.events.get('seek_events', 0)

        while time.time() - start_time < timeout_sec:
            current_seeks = self.log_monitor.events.get('seek_events', 0)
            if current_seeks > initial_seeks:
                return True
            time.sleep(0.2)

        return False

    def check_errors(self) -> List[str]:
        """
        检查是否有错误

        Returns:
            List[str]: 错误列表
        """
        return self.log_monitor.events.get('errors', [])

    def check_warnings(self) -> List[str]:
        """
        检查是否有警告

        Returns:
            List[str]: 警告列表
        """
        return self.log_monitor.events.get('warnings', [])

    def verify_no_critical_errors(self, patterns: List[str] = None) -> tuple:
        """
        验证没有严重错误

        Args:
            patterns: 错误模式列表

        Returns:
            tuple: (是否有严重错误, 错误列表)
        """
        patterns = patterns or [
            'crash', 'segfault', 'assertion', 'fatal',
            'null pointer', 'access violation'
        ]

        critical_errors = []
        for error in self.log_monitor.events.get('errors', []):
            error_lower = error.lower()
            for pattern in patterns:
                if pattern in error_lower:
                    critical_errors.append(error)
                    break

        return len(critical_errors) == 0, critical_errors


def main():
    """测试日志监控"""
    import tempfile

    # 创建测试日志目录
    log_dir = tempfile.mkdtemp()
    log_file = os.path.join(log_dir, 'test.log')

    # 写入测试日志
    with open(log_file, 'w') as f:
        f.write('[2026-03-10 12:00:00.000][info] Application started\n')
        f.write('[2026-03-10 12:00:01.000][info] Render frame 1\n')
        f.write('[2026-03-10 12:00:02.000][info] Seek to position 1000\n')
        f.write('[2026-03-10 12:00:03.000][error] Test error\n')

    # 创建监控器
    monitor = LogMonitor(log_dir)
    monitor.start()

    # 等待处理
    time.sleep(2)

    # 获取统计
    stats = monitor.get_stats()
    print(f"Stats: {stats}")

    # 停止监控
    monitor.stop()

    # 清理
    os.remove(log_file)
    os.rmdir(log_dir)


if __name__ == "__main__":
    main()