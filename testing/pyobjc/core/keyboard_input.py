# testing/pyobjc/core/keyboard_input.py
"""
macOS 键盘输入模拟
用于自动化测试中的键盘操作
"""

import os
import sys
import subprocess
import time

# 添加 pyobjc 目录到路径
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


class KeyboardInput:
    """macOS 键盘输入模拟"""

    # 特殊键映射
    SPECIAL_KEYS = {
        'enter': 'return',
        'space': 'space',
        'tab': 'tab',
        'escape': 'escape',
        'esc': 'escape',
        'backspace': 'delete',
        'delete': 'forwarddelete',
        'up': 'up arrow',
        'down': 'down arrow',
        'left': 'left arrow',
        'right': 'right arrow',
        'home': 'home',
        'end': 'end',
        'pageup': 'page up',
        'pagedown': 'page down',
        'f1': 'f1',
        'f2': 'f2',
        'f3': 'f3',
        'f4': 'f4',
        'f5': 'f5',
        'f6': 'f6',
        'f7': 'f7',
        'f8': 'f8',
        'f9': 'f9',
        'f10': 'f10',
        'f11': 'f11',
        'f12': 'f12',
    }

    @staticmethod
    def focus_app(app_name: str = "WZMediaPlayer", delay: float = 0.2):
        """
        将目标应用置前，避免按键发送到错误窗口。
        """
        script = f'''
        tell application "System Events"
            try
                set frontmost of process "{app_name}" to true
            end try
        end tell
        '''
        try:
            subprocess.run(['osascript', '-e', script], check=True, timeout=5)
            time.sleep(delay)
        except Exception:
            pass

    @staticmethod
    def send_key(key: str, modifiers: list = None, delay: float = 0.1):
        """
        发送按键

        Args:
            key: 按键名称或字符
            modifiers: 修饰键列表 ['command', 'shift', 'control', 'option']
            delay: 按键后延迟
        """
        modifiers = modifiers or []

        # 构建按键脚本
        key_str = KeyboardInput.SPECIAL_KEYS.get(key.lower(), key)

        # 构建修饰键
        mod_str = ''
        if modifiers:
            mod_str = ' using {' + ', '.join(f'{m} down' for m in modifiers) + '}'

        script = f'''
        tell application "System Events"
            keystroke "{key_str}"{mod_str}
        end tell
        '''

        try:
            KeyboardInput.focus_app()
            subprocess.run(['osascript', '-e', script], check=True, timeout=5)
            time.sleep(delay)
        except subprocess.TimeoutExpired:
            print(f"Warning: Key press timed out: {key}")
        except subprocess.CalledProcessError as e:
            print(f"Warning: Key press failed: {key}, error: {e}")

    @staticmethod
    def send_keys(keys: str, delay: float = 0.05):
        """
        发送一系列按键

        Args:
            keys: 按键字符串
            delay: 按键间延迟
        """
        for char in keys:
            KeyboardInput.send_key(char, delay=delay)

    @staticmethod
    def send_hotkey(key: str, modifiers: list, delay: float = 0.1):
        """
        发送快捷键

        Args:
            key: 主键
            modifiers: 修饰键列表
            delay: 按键后延迟
        """
        KeyboardInput.send_key(key, modifiers=modifiers, delay=delay)

    @staticmethod
    def open_file_dialog():
        """打开文件对话框 (Cmd+O)"""
        KeyboardInput.send_hotkey('o', ['command'], delay=0.5)

    @staticmethod
    def toggle_playback():
        """切换播放/暂停 (Space)"""
        KeyboardInput.send_key('space', delay=0.2)

    @staticmethod
    def seek_forward(seconds: int = 10):
        """向前 seek（右箭头，与 MainWindow shortcut_SeekRight 一致）"""
        KeyboardInput.send_key('right', delay=0.3)

    @staticmethod
    def seek_backward(seconds: int = 10):
        """向后 seek（左箭头，与 MainWindow shortcut_SeekLeft 一致）"""
        KeyboardInput.send_key('left', delay=0.3)

    @staticmethod
    def seek_to_start():
        """Seek到开始 (Home)"""
        KeyboardInput.send_key('home', delay=0.3)

    @staticmethod
    def seek_to_end():
        """Seek到结束 (End)"""
        KeyboardInput.send_key('end', delay=0.3)

    @staticmethod
    def toggle_fullscreen():
        """切换全屏 (Cmd+Enter 或 Enter)"""
        KeyboardInput.send_hotkey('return', ['command'], delay=0.3)

    @staticmethod
    def exit_fullscreen():
        """退出全屏 (Escape)"""
        KeyboardInput.send_key('escape', delay=0.3)

    @staticmethod
    def toggle_3d():
        """切换2D/3D模式 (Cmd+1)"""
        KeyboardInput.send_hotkey('1', ['command'], delay=0.3)

    @staticmethod
    def increase_volume():
        """增加音量 (上箭头)"""
        KeyboardInput.send_key('up', delay=0.1)

    @staticmethod
    def decrease_volume():
        """降低音量 (下箭头)"""
        KeyboardInput.send_key('down', delay=0.1)

    @staticmethod
    def toggle_mute():
        """切换静音 (M)"""
        KeyboardInput.send_key('m', delay=0.2)

    @staticmethod
    def take_screenshot():
        """截屏 (Cmd+S)"""
        KeyboardInput.send_hotkey('s', ['command'], delay=0.3)

    @staticmethod
    def open_video_file(video_path: str, delay: float = 2.0):
        """
        通过文件对话框打开视频文件。

        Args:
            video_path: 视频文件的绝对路径
            delay: 打开后等待时间
        """
        import os

        # 确保路径是绝对路径
        video_path = os.path.abspath(video_path)
        directory = os.path.dirname(video_path)
        filename = os.path.basename(video_path)

        # 使用 AppleScript 打开文件对话框并选择文件
        script = f'''
        tell application "System Events"
            -- 激活 WZMediaPlayer
            set frontmost of process "WZMediaPlayer" to true
            delay 0.3

            -- 打开文件对话框 (Cmd+O)
            keystroke "o" using command down
            delay 1.0

            -- 输入路径 (Cmd+Shift+G 进入"前往文件夹")
            keystroke "g" using {{command down, shift down}}
            delay 0.5

            -- 输入完整路径
            keystroke "{video_path}"
            delay 0.3

            -- 按 Enter 确认路径
            keystroke return
            delay 0.5

            -- 再按 Enter 打开文件
            keystroke return
        end tell
        '''

        try:
            subprocess.run(['osascript', '-e', script], check=True, timeout=15)
            time.sleep(delay)
        except subprocess.TimeoutExpired:
            print(f"Warning: Open video file timed out: {video_path}")
        except subprocess.CalledProcessError as e:
            print(f"Warning: Open video file failed: {video_path}, error: {e}")


def main():
    """测试键盘输入"""
    print("Testing keyboard input (will send keys to active window)...")
    print("Press Ctrl+C to stop")

    try:
        # 测试空格键
        print("Sending space key...")
        KeyboardInput.toggle_playback()

        # 测试方向键
        print("Sending up arrow...")
        KeyboardInput.seek_forward()

        # 测试快捷键
        print("Sending Cmd+O...")
        KeyboardInput.open_file_dialog()

    except KeyboardInterrupt:
        print("\nStopped")


if __name__ == "__main__":
    main()