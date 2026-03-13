# testing/pywinauto/config.py
# Windows 版测试配置文件

import os
import configparser

# 项目根目录
PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

_CONFIG_DIR = os.path.dirname(os.path.abspath(__file__))
_CONFIG_INI = os.path.join(_CONFIG_DIR, "config.ini")

# 默认路径
_DEFAULT_EXE_PATH = os.path.join(PROJECT_ROOT, "build", "Release", "WZMediaPlayer.exe")
_DEFAULT_TEST_VIDEO_PATH = os.path.join(PROJECT_ROOT, "testing", "video", "test.mp4")


def _load_path_config():
    """从 config.ini 读取配置"""
    out = {
        "exe_path": _DEFAULT_EXE_PATH,
        "test_video_path": _DEFAULT_TEST_VIDEO_PATH,
    }
    if not os.path.isfile(_CONFIG_INI):
        return out
    try:
        parser = configparser.ConfigParser()
        parser.read(_CONFIG_INI, encoding="utf-8")
        if parser.has_section("paths"):
            s = parser["paths"]
            if s.get("exe_path"):
                out["exe_path"] = s.get("exe_path").strip()
            if s.get("test_video_path"):
                out["test_video_path"] = s.get("test_video_path").strip()
    except Exception:
        pass
    return out


_paths = _load_path_config()

# 播放器可执行文件路径
APP_PATH = _paths["exe_path"]

# 测试视频目录
TEST_VIDEO_DIR = os.path.join(PROJECT_ROOT, "testing", "video")

# 各测试视频路径
TEST_VIDEO_PATH = _paths["test_video_path"]
TEST_VIDEO_LG_PATH = TEST_VIDEO_PATH

# SMPTE 彩条测试视频
TEST_VIDEO_SMTPTE_PATH = os.path.join(TEST_VIDEO_DIR, "test_smpte_640x480_5s.mp4")

# testsrc 测试视频
TEST_VIDEO_TESTSRC_PATH = os.path.join(TEST_VIDEO_DIR, "test_testsrc_640x480_5s.mp4")

# Big Buck Bunny 测试视频
TEST_VIDEO_BBB_NORMAL_PATH = os.path.join(TEST_VIDEO_DIR, "bbb_sunflower_1080p_30fps_normal.mp4")
TEST_VIDEO_BBB_STEREO_PATH = os.path.join(TEST_VIDEO_DIR, "bbb_sunflower_1080p_30fps_stereo_abl.mp4")

# 60秒测试视频
TEST_VIDEO_60S_PATH = os.path.join(TEST_VIDEO_DIR, "test_60s.mp4")

# 日志目录
LOG_DIR = os.path.join(os.path.dirname(APP_PATH), "logs")

# 截图保存目录
SCREENSHOT_DIR = os.path.join(PROJECT_ROOT, "testing", "pywinauto", "screenshots")

# 参考帧目录
REFERENCE_FRAME_DIR = os.path.join(PROJECT_ROOT, "testing", "pywinauto", "reference_frames")

# 报告保存目录
REPORT_DIR = os.path.join(_CONFIG_DIR, "reports")

# 超时设置（毫秒）
TIMEOUTS = {
    "app_start": 10000,
    "video_load": 10000,
    "seek_complete": 5000,
    "window_ready": 15000,
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

# Pywinauto 后端
PYWINAUTO_BACKEND = "uia"

# 3D测试相关配置
STEREO_TEST_CONFIG = {
    "STEREO_FORMAT_2D": 0,
    "STEREO_FORMAT_3D": 1,
    "STEREO_INPUT_LR": 0,
    "STEREO_INPUT_RL": 1,
    "STEREO_INPUT_UD": 2,
    "STEREO_OUTPUT_VERTICAL": 0,
    "STEREO_OUTPUT_HORIZONTAL": 1,
    "STEREO_OUTPUT_CHESS": 2,
    "STEREO_OUTPUT_ONLY_LEFT": 3,
    "PARALLAX_MIN": -50,
    "PARALLAX_MAX": 50,
    "stereo_mode_switch_delay": 1.0,
    "parallax_adjust_delay": 0.5,
}

# 播放进度/UI 同步测试容差
PLAYBACK_SYNC_TOLERANCE = {
    "ui_vs_slider_sec": 2,
    "ui_vs_log_sec": 2,
    "min_advance_sec": 2,
    "seek_min_advance_sec": 3,
}