# 测试配置文件
# 路径等易变项优先从同目录下的 config.ini 读取，未配置时使用下方默认值

import os
import configparser

_CONFIG_DIR = os.path.dirname(os.path.abspath(__file__))
_CONFIG_INI = os.path.join(_CONFIG_DIR, "config.ini")

# 默认路径（config.ini 不存在或未配置时使用）
_DEFAULT_EXE_PATH = r"D:\2026Github\build\Release\WZMediaPlayer.exe"
_DEFAULT_TEST_VIDEO_PATH = r"D:\2026Github\testing\video\test.mp4"


def _load_path_config():
    """从 config.ini 读取 [paths] 配置，缺失则返回默认值"""
    out = {
        "exe_path": _DEFAULT_EXE_PATH,
        "test_video_path": _DEFAULT_TEST_VIDEO_PATH,
        "test_3d_video_path": "",
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
            t3d = s.get("test_3d_video_path", "").strip()
            out["test_3d_video_path"] = t3d if t3d else out["test_video_path"]
    except Exception:
        pass
    return out


_paths = _load_path_config()

# 播放器可执行文件路径（供各脚本统一引用）
PLAYER_EXE_PATH = _paths["exe_path"]

# 测试视频文件路径
TEST_VIDEO_PATH = _paths["test_video_path"]

# 3D 测试视频路径（未配置时与 TEST_VIDEO_PATH 相同）
TEST_3D_VIDEO_PATH = _paths["test_3d_video_path"] or _paths["test_video_path"]

# 额外测试视频（可选）
TEST_VIDEO_LIBRARY = {
    # "normal": r"D:\videos\test_normal.mp4",
    # "3d_lr": r"D:\videos\test_3d_lr.mp4",
    # "3d_rl": r"D:\videos\test_3d_rl.mp4",
    # "3d_ud": r"D:\videos\test_3d_ud.mp4",
    # "long": r"D:\videos\test_long.mp4",
}

# 超时设置（毫秒）
TIMEOUTS = {
    "app_start": 5000,
    "video_load": 5000,
    "seek_complete": 2000,
    "window_ready": 2000,
    "button_click": 1000,
}

# Pywinauto 后端（uia 或 win32）
PYWINAUTO_BACKEND = "uia"

# 等待时间（秒）
WAIT_TIMES = {
    "after_open": 2.0,
    "after_seek": 1.0,
    "after_click": 0.5,
    "playback_test": 5.0,
}

# 测试选项
TEST_OPTIONS = {
    "verbose": True,
    "save_report": True,
    "screenshot_on_fail": True,
    "close_after_test": True,
}

# 控件 ID 映射（根据实际 UI 调整）
CONTROL_IDS = {
    "open_button": "pushButton_open",
    "play_pause_button": "pushButton_playPause",
    "stop_button": "pushButton_stop",
    "previous_button": "pushButton_previous",
    "next_button": "pushButton_next",
    "progress_slider": "horizontalSlider_playProgress",
    "volume_slider": "horizontalSlider_volume",
    "3d_switch": "switchButton_3D2D",
    "fullscreen_button": "pushButton_fullScreen",
}

# 快捷键映射
SHORTCUTS = {
    "open": "^o",
    "play_pause": "{SPACE}",
    "stop": "^{s}",
    "next": "^{n}",
    "previous": "^{p}",
    "fullscreen": "{F11}",
    "mute": "^{m}",
}
