# testing/pyobjc/config.py
import os

# 播放器可执行文件路径
APP_PATH = os.path.join(
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
    "dist", "WZMediaPlayer.app", "Contents", "MacOS", "WZMediaPlayer"
)

# 测试视频文件路径
TEST_VIDEO_PATH = os.path.join(
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
    "testing", "video", "test.mp4"
)

# 超时设置（毫秒）
TIMEOUTS = {
    "app_start": 5000,
    "video_load": 5000,
    "seek_complete": 2000,
    "window_ready": 2000,
}

# 等待时间（秒）
WAIT_TIMES = {
    "after_open": 2.0,
    "after_seek": 1.0,
    "playback_test": 5.0,
}