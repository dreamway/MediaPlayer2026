"""
WZMediaPlayer Windows 自动化测试套件
"""

from tests.test_comprehensive import ComprehensiveTest
from tests.test_playback_sync import PlaybackSyncTest
from tests.test_3d_rendering import Stereo3DTest
from tests.test_camera import CameraTest

# 保留原有的闭环测试
try:
    from tests.closed_loop_tests import WZMediaPlayerClosedLoopTests
except ImportError:
    WZMediaPlayerClosedLoopTests = None

__all__ = [
    'ComprehensiveTest',
    'PlaybackSyncTest',
    'Stereo3DTest',
    'CameraTest',
    'WZMediaPlayerClosedLoopTests',
]
