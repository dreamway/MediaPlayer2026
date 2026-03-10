# testing/pyobjc/config.py
import os

# 项目根目录
PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# 播放器可执行文件路径
APP_PATH = os.path.join(
    PROJECT_ROOT,
    "build", "WZMediaPlayer.app", "Contents", "MacOS", "WZMediaPlayer"
)

# 日志目录
LOG_DIR = os.path.join(
    PROJECT_ROOT,
    "build", "WZMediaPlayer.app", "Contents", "logs"
)

# 测试视频文件路径
TEST_VIDEO_DIR = os.path.join(
    PROJECT_ROOT,
    "testing", "video"
)

TEST_VIDEO_PATH = os.path.join(TEST_VIDEO_DIR, "test.mp4")
TEST_VIDEO_LG_PATH = os.path.join(TEST_VIDEO_DIR, "LG.mp4")

# 截图保存目录
SCREENSHOT_DIR = os.path.join(
    PROJECT_ROOT,
    "testing", "pyobjc", "screenshots"
)

# 超时设置（毫秒）
TIMEOUTS = {
    "app_start": 10000,
    "video_load": 10000,
    "seek_complete": 5000,
    "window_ready": 5000,
    "render_check": 15000,
}

# 等待时间（秒）
WAIT_TIMES = {
    "after_open": 3.0,
    "after_seek": 2.0,
    "playback_test": 5.0,
    "short": 0.5,
    "medium": 1.0,
    "long": 3.0,
}

# 黑屏检测阈值
BLACK_THRESHOLD = 15  # 像素亮度阈值
BLACK_RATIO_THRESHOLD = 0.90  # 黑色像素比例阈值