# testing/pyobjc/tests/__init__.py
import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from tests.test_basic_playback import BasicPlaybackTest

__all__ = ['BasicPlaybackTest']