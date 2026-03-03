"""
WZMediaPlayer 闭环自动化测试框架核心模块
"""

from .ui_automation import UIAutomationController
from .image_verifier import ImageVerifier
from .qt_ui_parser import QtUIParser
from .test_base import ClosedLoopTestBase

__all__ = [
    'UIAutomationController',
    'ImageVerifier', 
    'QtUIParser',
    'ClosedLoopTestBase',
]
