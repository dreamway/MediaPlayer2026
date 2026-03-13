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

# 各测试视频路径
TEST_VIDEO_PATH = os.path.join(TEST_VIDEO_DIR, "test.mp4")
TEST_VIDEO_LG_PATH = os.path.join(TEST_VIDEO_DIR, "test.mp4")  # 使用test.mp4作为默认测试视频

# SMPTE 彩条测试视频 (640x480)
TEST_VIDEO_SMTPTE_PATH = os.path.join(TEST_VIDEO_DIR, "test_smpte_640x480_5s.mp4")

# testsrc 测试视频 (640x480)
TEST_VIDEO_TESTSRC_PATH = os.path.join(TEST_VIDEO_DIR, "test_testsrc_640x480_5s.mp4")

# Big Buck Bunny 1080p 测试视频
TEST_VIDEO_BBB_NORMAL_PATH = os.path.join(TEST_VIDEO_DIR, "bbb_sunflower_1080p_30fps_normal.mp4")
TEST_VIDEO_BBB_STEREO_PATH = os.path.join(TEST_VIDEO_DIR, "bbb_sunflower_1080p_30fps_stereo_abl.mp4")

# 黑神话悟空测试视频
TEST_VIDEO_WUKONG_2D3D_PATH = os.path.join(TEST_VIDEO_DIR, "wukong_2D3D-40S.mp4")
TEST_VIDEO_WUKONG_4K_PATH = os.path.join(TEST_VIDEO_DIR, "wukong4K-40S.mp4")

# 医疗3D演示视频
TEST_VIDEO_MEDICAL_3D_PATH = os.path.join(TEST_VIDEO_DIR, "Medical3D4k5-2.mp4")

# 默认测试视频列表
DEFAULT_TEST_VIDEOS = [
    ("SMPTE彩条", TEST_VIDEO_SMTPTE_PATH, "reference_smpte_640x480.png"),
    ("testsrc", TEST_VIDEO_TESTSRC_PATH, "reference_testsrc_640x480.png"),
    ("BBB 1080p", TEST_VIDEO_BBB_NORMAL_PATH, "reference_bbb_normal_5s.png"),
    ("BBB Stereo", TEST_VIDEO_BBB_STEREO_PATH, "reference_bbb_stereo_abl_5s.png"),
    ("黑神话悟空2D3D", TEST_VIDEO_WUKONG_2D3D_PATH, "reference_wukong_2d3d_5s.png"),
    ("黑神话悟空4K", TEST_VIDEO_WUKONG_4K_PATH, "reference_wukong_4k_5s.png"),
    ("医疗3D演示", TEST_VIDEO_MEDICAL_3D_PATH, "reference_medical_3d_5s.png"),
]

# 60秒测试视频（用于同步测试）
TEST_VIDEO_60S_PATH = os.path.join(TEST_VIDEO_DIR, "test_60s.mp4")

# 截图保存目录
SCREENSHOT_DIR = os.path.join(
    PROJECT_ROOT,
    "testing", "pyobjc", "screenshots"
)

# 参考帧目录（渲染精确验证用）
REFERENCE_FRAME_DIR = os.path.join(
    PROJECT_ROOT,
    "testing", "pyobjc", "reference_frames"
)

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

# 3D测试相关配置
STEREO_TEST_CONFIG = {
    # 3D格式常量 (对应 GlobalDef.h)
    "STEREO_FORMAT_2D": 0,
    "STEREO_FORMAT_3D": 1,
    "STEREO_INPUT_LR": 0,
    "STEREO_INPUT_RL": 1,
    "STEREO_INPUT_UD": 2,
    "STEREO_OUTPUT_VERTICAL": 0,
    "STEREO_OUTPUT_HORIZONTAL": 1,
    "STEREO_OUTPUT_CHESS": 2,
    "STEREO_OUTPUT_ONLY_LEFT": 3,

    # 视差调节范围
    "PARALLAX_MIN": -50,
    "PARALLAX_MAX": 50,

    # 3D模式切换等待时间
    "stereo_mode_switch_delay": 1.0,
    "parallax_adjust_delay": 0.5,
}

# 报告保存目录
REPORT_DIR = os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "reports"
)

# 播放进度/UI 同步测试容差
PLAYBACK_SYNC_TOLERANCE = {
    "ui_vs_slider_sec": 2,       # 当前时间与进度条值允许相差秒数
    "ui_vs_log_sec": 2,          # UI 时间与日志解析位置允许相差秒数
    "min_advance_sec": 2,        # 播放一段时间后至少前进的秒数
    "seek_min_advance_sec": 3,   # 单次向前 seek 后至少前进的秒数
}