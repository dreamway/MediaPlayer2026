"""
音频管道客户端 - 连接 WZMediaPlayer 的 TestPipeServer 命名管道，验证音频状态

用于闭环测试中验证：
1. 音频是否正在播放（PTS 递增）
2. 音视频同步（audio_pts 与 video_pts 差值）
3. 静音/音量状态
"""

import json
import threading
import time
from typing import Optional, Dict, List, Tuple
from collections import deque


class AudioPipeClient:
    """连接 WZMediaPlayer 命名管道，读取 JSON 格式的音频状态"""

    def __init__(self, pipe_name: str = r"\\.\pipe\WZMediaPlayer_Test"):
        self.pipe_name = pipe_name
        self._queue: deque = deque(maxlen=500)
        self._thread: Optional[threading.Thread] = None
        self._stop = threading.Event()
        self._buffer = ""  # 用于拼接不完整的 JSON 行

    def connect(self, timeout: float = 5.0) -> bool:
        """连接命名管道"""
        try:
            import win32file

            self._handle = win32file.CreateFile(
                self.pipe_name,
                win32file.GENERIC_READ,
                0,
                None,
                win32file.OPEN_EXISTING,
                0,
                None,
            )
        except Exception as e:
            print(f"[AudioPipe] 连接失败: {e}")
            return False
        self._stop.clear()
        self._buffer = ""
        self._thread = threading.Thread(target=self._read_loop, daemon=True)
        self._thread.start()
        return True

    def _read_loop(self):
        """后台读取管道数据"""
        import win32file

        while not self._stop.is_set():
            try:
                _, data = win32file.ReadFile(self._handle, 4096)
                if not data:
                    continue
                text = data.decode("utf-8", errors="ignore")
                self._buffer += text
                lines = self._buffer.split("\n")
                self._buffer = lines[-1]  # 保留可能不完整的最后一行
                for line in lines[:-1]:
                    line = line.strip()
                    if line:
                        try:
                            self._queue.append((time.time(), json.loads(line)))
                        except json.JSONDecodeError:
                            pass
            except Exception:
                break

    def disconnect(self):
        """断开连接"""
        self._stop.set()
        if hasattr(self, "_handle"):
            try:
                import win32file

                win32file.CloseHandle(self._handle)
            except Exception:
                pass

    def get_latest_status(self) -> Optional[Dict]:
        """获取最新状态"""
        if not self._queue:
            return None
        return self._queue[-1][1]

    def verify_audio_playing(self, duration_sec: float = 2.0) -> Tuple[bool, str]:
        """验证音频是否正在播放（PTS 递增）"""
        start = time.time()
        prev_pts = None
        while time.time() - start < duration_sec:
            s = self.get_latest_status()
            if s and s.get("vol", 0) > 0 and s.get("playing"):
                pts = s.get("audio_pts", 0)
                if prev_pts is not None and pts > prev_pts:
                    return True, "音频 PTS 递增，有声音"
                prev_pts = pts
            time.sleep(0.2)
        return False, "未检测到持续音频输出"

    def verify_av_sync(
        self, duration_sec: float, max_diff_sec: float = 0.5
    ) -> Tuple[bool, str]:
        """验证音视频同步"""
        start = time.time()
        max_seen_diff = 0.0
        while time.time() - start < duration_sec:
            s = self.get_latest_status()
            if s and s.get("playing"):
                diff = abs(s.get("audio_pts", 0) - s.get("video_pts", 0))
                max_seen_diff = max(max_seen_diff, diff)
            time.sleep(0.2)
        ok = max_seen_diff <= max_diff_sec
        return ok, f"音视频 PTS 最大差: {max_seen_diff:.3f}s (阈值 {max_diff_sec}s)"
