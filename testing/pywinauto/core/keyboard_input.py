# testing/pywinauto/core/keyboard_input.py
"""
Windows 键盘输入模拟
用于自动化测试中的键盘操作
"""

import time
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

try:
    import pywinauto.keyboard as keyboard
    PYWINAUTO_AVAILABLE = True
except ImportError:
    PYWINAUTO_AVAILABLE = False


class KeyboardInput:
    """Windows 键盘输入模拟"""

    # 特殊键映射 (pywinauto 格式)
    SPECIAL_KEYS = {
        'enter': '{ENTER}',
        'space': '{SPACE}',
        'tab': '{TAB}',
        'escape': '{ESC}',
        'esc': '{ESC}',
        'backspace': '{BACKSPACE}',
        'delete': '{DELETE}',
        'up': '{UP}',
        'down': '{DOWN}',
        'left': '{LEFT}',
        'right': '{RIGHT}',
        'home': '{HOME}',
        'end': '{END}',
        'pageup': '{PGUP}',
        'pagedown': '{PGDN}',
        'f1': '{F1}',
        'f2': '{F2}',
        'f3': '{F3}',
        'f4': '{F4}',
        'f5': '{F5}',
        'f6': '{F6}',
        'f7': '{F7}',
        'f8': '{F8}',
        'f9': '{F9}',
        'f10': '{F10}',
        'f11': '{F11}',
        'f12': '{F12}',
    }

    @staticmethod
    def focus_app(app_name: str = "WZMediaPlayer", delay: float = 0.2):
        """
        将目标应用置前

        Args:
            app_name: 应用进程名
            delay: 延迟时间
        """
        try:
            import pywinauto
            app = pywinauto.Application(backend="uia").connect(process=app_name, timeout=5)
            if app.windows():
                app.top_window().set_focus()
            time.sleep(delay)
        except Exception:
            pass

    @staticmethod
    def send_key(key: str, modifiers: list = None, delay: float = 0.1):
        """
        发送按键

        Args:
            key: 按键名称或字符
            modifiers: 修饰键列表 ['ctrl', 'shift', 'alt']
            delay: 按键后延迟
        """
        modifiers = modifiers or []

        # 构建按键字符串
        key_str = KeyboardInput.SPECIAL_KEYS.get(key.lower(), key)

        # 构建修饰键
        mod_map = {
            'ctrl': '^',
            'shift': '+',
            'alt': '%',
            'command': '^',  # Windows 上 command 映射为 ctrl
        }

        key_combination = ''
        for mod in modifiers:
            if mod.lower() in mod_map:
                key_combination += mod_map[mod.lower()]

        key_combination += key_str

        try:
            if PYWINAUTO_AVAILABLE:
                keyboard.send_keys(key_combination)
            time.sleep(delay)
        except Exception as e:
            print(f"Warning: Key press failed: {key}, error: {e}")

    @staticmethod
    def send_keys(keys: str, delay: float = 0.05):
        """
        发送一系列按键

        Args:
            keys: 按键字符串
            delay: 按键间延迟
        """
        try:
            if PYWINAUTO_AVAILABLE:
                keyboard.send_keys(keys, pause=delay)
        except Exception as e:
            print(f"Warning: Send keys failed: {e}")

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
        """打开文件对话框 (Ctrl+O)"""
        KeyboardInput.send_hotkey('o', ['ctrl'], delay=0.5)

    @staticmethod
    def toggle_playback():
        """切换播放/暂停 (Space)"""
        KeyboardInput.send_key('space', delay=0.2)

    @staticmethod
    def seek_forward(seconds: int = 10):
        """向前 seek (右箭头)"""
        KeyboardInput.send_key('right', delay=0.3)

    @staticmethod
    def seek_backward(seconds: int = 10):
        """向后 seek (左箭头)"""
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
        """切换全屏 (Ctrl+Enter)"""
        KeyboardInput.send_hotkey('enter', ['ctrl'], delay=0.3)

    @staticmethod
    def exit_fullscreen():
        """退出全屏 (Escape)"""
        KeyboardInput.send_key('escape', delay=0.3)

    @staticmethod
    def toggle_3d():
        """切换2D/3D模式 (Ctrl+1)"""
        KeyboardInput.send_hotkey('1', ['ctrl'], delay=0.3)

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
    def toggle_camera():
        """切换摄像头 (Ctrl+Shift+C)"""
        KeyboardInput.send_hotkey('c', ['ctrl', 'shift'], delay=0.5)

    @staticmethod
    def take_screenshot():
        """截屏 (Ctrl+S)"""
        KeyboardInput.send_hotkey('s', ['ctrl'], delay=0.3)

    @staticmethod
    def open_video_file(video_path: str, delay: float = 2.0):
        """
        通过文件对话框打开视频文件

        Args:
            video_path: 视频文件的绝对路径
            delay: 打开后等待时间
        """
        import subprocess

        video_path = os.path.abspath(video_path)

        # 使用 PowerShell 打开文件对话框
        script = f'''
        Add-Type -AssemblyName System.Windows.Forms
        $file = "{video_path.replace("\\", "\\\\")}"

        # 激活 WZMediaPlayer 窗口
        $proc = Get-Process -Name "WZMediaPlayer" -ErrorAction SilentlyContinue
        if ($proc) {{
            (New-Object -ComObject WScript.Shell).AppActivate($proc.MainWindowTitle)
            Start-Sleep -Milliseconds 300

            # 发送 Ctrl+O
            [System.Windows.Forms.SendKeys]::SendWait("^o")
            Start-Sleep -Milliseconds 1000

            # 输入路径
            [System.Windows.Forms.SendKeys]::SendWait("$file")
            Start-Sleep -Milliseconds 300

            # 按 Enter 确认
            [System.Windows.Forms.SendKeys]::SendWait("{{ENTER}}")
            Start-Sleep -Milliseconds 500

            # 再按 Enter 打开
            [System.Windows.Forms.SendKeys]::SendWait("{{ENTER}}")
        }}
        '''

        try:
            subprocess.run(['powershell', '-Command', script], timeout=15)
            time.sleep(delay)
        except subprocess.TimeoutExpired:
            print(f"Warning: Open video file timed out: {video_path}")
        except Exception as e:
            print(f"Warning: Open video file failed: {video_path}, error: {e}")


def main():
    """测试键盘输入"""
    print("Testing keyboard input (will send keys to active window)...")
    print("Press Ctrl+C to stop")

    try:
        print("Sending space key...")
        KeyboardInput.toggle_playback()

        print("Sending right arrow...")
        KeyboardInput.seek_forward()

        print("Sending Ctrl+O...")
        KeyboardInput.open_file_dialog()

    except KeyboardInterrupt:
        print("\nStopped")


if __name__ == "__main__":
    main()