"""
WZMediaPlayer Windows 自动化测试框架核心模块
"""

# 原有模块
from .ui_automation import UIAutomationController
from .image_verifier import ImageVerifier
from .qt_ui_parser import QtUIParser
from .test_base import ClosedLoopTestBase

# 新增模块（从 macOS pyobjc 迁移）
from .keyboard_input import KeyboardInput
from .screenshot_capture import ScreenshotCapture, ImageAnalyzer
from .window_controller import WindowController, WindowControllerError
from .app_launcher import AppLauncher

__all__ = [
    # 原有
    'UIAutomationController',
    'ImageVerifier',
    'QtUIParser',
    'ClosedLoopTestBase',
    # 新增
    'KeyboardInput',
    'ScreenshotCapture',
    'ImageAnalyzer',
    'WindowController',
    'WindowControllerError',
    'AppLauncher',
]
