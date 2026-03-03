# 测试配置文件

# 播放器可执行文件路径
PLAYER_EXE_PATH = r"E:\WZMediaPlayer_2025\x64\Debug\WZMediaPlay.exe"

# 测试视频文件路径
TEST_VIDEO_PATH = r"D:\BaiduNetdiskDownload\3D片源\d67932db75e03691431d4427564d5303.mp4"

# 额外测试视频（可选）
TEST_VIDEO_LIBRARY = {
    # "normal": r"D:\videos\test_normal.mp4",  # 普通视频
    # "3d_lr": r"D:\videos\test_3d_lr.mp4",    # 3D左右格式
    # "3d_rl": r"D:\videos\test_3d_rl.mp4",    # 3D右左格式
    # "3d_ud": r"D:\videos\test_3d_ud.mp4",    # 3D上下格式
    # "long": r"D:\videos\test_long.mp4",      # 长视频（>5分钟）
}

# 超时设置（毫秒）
TIMEOUTS = {
    "app_start": 5000,       # 应用启动超时
    "video_load": 5000,      # 视频加载超时
    "seek_complete": 2000,   # seek完成超时
    "window_ready": 2000,     # 窗口就绪超时
    "button_click": 1000,    # 按钮点击超时
}

# Pywinauto后端（uia或win32）
PYWINAUTO_BACKEND = "uia"

# 等待时间（秒）
WAIT_TIMES = {
    "after_open": 2.0,       # 打开视频后等待
    "after_seek": 1.0,       # seek后等待
    "after_click": 0.5,      # 点击后等待
    "playback_test": 5.0,    # 播放测试时间
}

# 测试选项
TEST_OPTIONS = {
    "verbose": True,         # 详细输出
    "save_report": True,     # 保存测试报告
    "screenshot_on_fail": True,  # 失败时截图
    "close_after_test": True,   # 测试后关闭播放器
}

# 控件ID映射（根据实际UI调整）
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
    "open": "^o",           # Ctrl+O
    "play_pause": "{SPACE}",    # Space
    "stop": "^{s}",         # Ctrl+S
    "next": "^{n}",         # Ctrl+N
    "previous": "^{p}",    # Ctrl+P
    "fullscreen": "{F11}",     # F11
    "mute": "^{m}",         # Ctrl+M
}
