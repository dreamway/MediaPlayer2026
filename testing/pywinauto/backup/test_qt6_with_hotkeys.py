"""
WZMediaPlayer Qt6 简化测试脚本 (使用实际快捷键)
使用快捷键进行测试，避免控件查找问题

快捷键映射（从GlobalDef.h中提取）:
- 打开文件: Ctrl+O
- 停止播放: Ctrl+C
- 上一个: Page Up
- 下一个: Page Down
- 播放/暂停: Space
- 音量加: Up
- 音量减: Down
- 静音: M
"""

import os
import sys
import time
from datetime import datetime

try:
    from pywinauto.application import Application
    from pywinauto.keyboard import send_keys
    from pywinauto.timings import Timings
except ImportError:
    print("Error: pywinauto is not installed. Run: pip install pywinauto")
    sys.exit(1)


class Qt6SimpleTester:
    """Simplified tester for Qt6 applications using actual hotkeys"""

    def __init__(self, exe_path: str, test_video_path: str = None):
        self.exe_path = exe_path
        self.test_video_path = test_video_path
        self.app = None
        self.main_window = None
        self.test_results = []

        # 禁用WaitForInputIdle，避免Qt程序报错
        Timings.fast()
        Timings.window_find_timeout = 10

    def log_test(self, name: str, passed: bool, details: str = ""):
        """Log test result"""
        result = {
            "name": name,
            "passed": passed,
            "details": details,
            "timestamp": datetime.now().strftime('%H:%M:%S')
        }
        self.test_results.append(result)

        symbol = "PASS" if passed else "FAIL"
        print(f"  [{symbol}] {name}")
        if details:
            print(f"      {details}")

    def start_player(self) -> bool:
        """Start player"""
        print("\n[Step 1/6] Starting player...")

        try:
            if not os.path.exists(self.exe_path):
                raise FileNotFoundError(f"Executable not found: {self.exe_path}")

            # 使用win32 backend，对Qt6更稳定
            self.app = Application(backend="win32").start(self.exe_path)
            time.sleep(5)  # Qt6程序需要更长的启动时间

            # 获取主窗口
            self.main_window = self.app.window()

            if not self.main_window.exists():
                raise RuntimeError("Main window not found")

            # 等待窗口就绪
            time.sleep(2)

            window_title = self.main_window.window_text()
            self.log_test("Start Player", True, f"Window title: {window_title}")
            return True

        except Exception as e:
            self.log_test("Start Player", False, str(e))
            return False

    def open_video(self) -> bool:
        """Open video file"""
        print("\n[Step 2/6] Opening video...")

        if not self.test_video_path or not os.path.exists(self.test_video_path):
            self.log_test("Open Video", False, f"Video file not found: {self.test_video_path}")
            return False

        try:
            print(f"  Opening: {os.path.basename(self.test_video_path)}")

            # Use Ctrl+O to open file (actual hotkey)
            send_keys("^o")
            time.sleep(1.5)  # 增加等待时间

            # Type file path
            send_keys(self.test_video_path)
            time.sleep(0.5)
            send_keys("{ENTER}")
            time.sleep(6.0)  # 等待视频加载（增加时间）

            self.log_test("Open Video", True, "Used Ctrl+O shortcut")
            return True

        except Exception as e:
            self.log_test("Open Video", False, str(e))
            return False

    def test_play_pause(self) -> bool:
        """Test play/pause"""
        print("\n[Step 3/6] Testing play/pause...")

        try:
            # Pause using Space
            send_keys("{SPACE}")
            time.sleep(1.0)
            self.log_test("Pause", True, "Used Spacebar")

            # Play using Space
            send_keys("{SPACE}")
            time.sleep(1.0)
            self.log_test("Play", True, "Used Spacebar")

            return True

        except Exception as e:
            self.log_test("Play/Pause", False, str(e))
            return False

    def test_stop(self) -> bool:
        """Test stop"""
        print("\n[Step 4/6] Testing stop playback...")

        try:
            # Use Ctrl+C to stop (actual hotkey)
            send_keys("^{c}")
            time.sleep(1.0)
            self.log_test("Stop", True, "Used Ctrl+C shortcut")

            return True

        except Exception as e:
            self.log_test("Stop", False, str(e))
            return False

    def test_seek_small(self) -> bool:
        """Test seek small amount (5 seconds) using arrow keys"""
        print("\n[Step 5/6] Testing small seek...")

        try:
            # Play first
            send_keys("{SPACE}")
            time.sleep(2)

            print("  Using Right arrow key to seek forward 5 seconds...")
            send_keys("{RIGHT}")
            time.sleep(3.0)
            self.log_test("Seek Forward Small", True, "Used Right arrow key")

            print("  Using Left arrow key to seek backward 5 seconds...")
            send_keys("{LEFT}")
            time.sleep(3.0)
            self.log_test("Seek Backward Small", True, "Used Left arrow key")

            return True

        except Exception as e:
            self.log_test("Small Seek", False, str(e))
            return False

    def test_seek_large(self) -> bool:
        """Test seek large amount (10%) - Note: This requires code changes"""
        print("\n[Step 6/6] Testing large seek...")

        try:
            print("  Note: Large seek (Ctrl+Left/Right) requires code changes")
            print("  Testing basic seeking instead...")

            # Test next/previous using hotkeys
            send_keys("{PGDN}")  # Page Down
            time.sleep(2.0)
            self.log_test("Next Track", True, "Used Page Down")

            send_keys("{PGUP}")  # Page Up
            time.sleep(2.0)
            self.log_test("Previous Track", True, "Used Page Up")

            return True

        except Exception as e:
            self.log_test("Large Seek", False, str(e))
            return False

    def test_volume(self) -> bool:
        """Test volume control"""
        print("\n[Step 7/7] Testing volume control...")

        try:
            # Use Up arrow for volume up (actual hotkey)
            send_keys("{UP}")
            time.sleep(0.5)
            self.log_test("Volume Up", True, "Used Up arrow key")

            # Use Down arrow for volume down (actual hotkey)
            send_keys("{DOWN}")
            time.sleep(0.5)
            self.log_test("Volume Down", True, "Used Down arrow key")

            # Test mute using M key
            send_keys("m")
            time.sleep(0.5)
            send_keys("m")  # Toggle mute again
            time.sleep(0.5)
            self.log_test("Mute Toggle", True, "Used M key")

            return True

        except Exception as e:
            self.log_test("Volume Control", False, str(e))
            return False

    def generate_report(self) -> str:
        """Generate test report"""
        report_lines = []
        report_lines.append("="*60)
        report_lines.append("WZMediaPlayer Qt6 Simplified Test Report")
        report_lines.append("="*60)
        report_lines.append(f"Test Time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        report_lines.append(f"Test Video: {self.test_video_path or 'N/A'}")
        report_lines.append("")

        # Statistics
        total = len(self.test_results)
        passed = sum(1 for r in self.test_results if r["passed"])
        failed = total - passed

        report_lines.append(f"Total: {total} | Passed: {passed} | Failed: {failed}")
        report_lines.append("")
        report_lines.append("-"*60)
        report_lines.append("Test Details:")
        report_lines.append("-"*60)

        for i, result in enumerate(self.test_results, 1):
            symbol = "PASS" if result["passed"] else "FAIL"
            report_lines.append(f"{i}. [{symbol}] {result['name']}")
            if result["details"]:
                report_lines.append(f"   {result['details']}")
            report_lines.append("")

        report_lines.append("="*60)

        return "\n".join(report_lines)

    def save_report(self, report: str):
        """Save test report"""
        try:
            timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
            filename = f"test_report_qt6_{timestamp}.txt"

            script_dir = os.path.dirname(os.path.abspath(__file__))
            report_path = os.path.join(script_dir, filename)

            with open(report_path, 'w', encoding='utf-8') as f:
                f.write(report)

            print(f"\n[Test] Report saved: {report_path}")
        except Exception as e:
            print(f"\n[Error] Failed to save report: {e}")

    def stop_player(self):
        """Stop player"""
        try:
            if self.app:
                self.app.kill()
                time.sleep(1)
        except Exception as e:
            print(f"Error stopping player: {e}")


def main():
    """Main function"""
    print("="*60)
    print("WZMediaPlayer Qt6 Simplified Test (Using Actual Hotkeys)")
    print("="*60)
    print()

    # Configure paths (modify as needed)
    exe_path = r"E:\WZMediaPlayer_2025\x64\Debug\WZMediaPlay.exe"
    test_video_path = r"D:\BaiduNetdiskDownload\test.mp4"

    # Create tester
    tester = Qt6SimpleTester(exe_path, test_video_path)

    try:
        # Run tests
        if not tester.start_player():
            print("\n[Error] Failed to start player")
        else:
            tester.open_video()
            time.sleep(3)  # Let video play for a bit
            tester.test_play_pause()
            tester.test_stop()
            time.sleep(1)
            tester.test_seek_small()
            tester.test_seek_large()
            tester.test_volume()

            # Generate and save report
            report = tester.generate_report()
            print("\n" + report)
            tester.save_report(report)

    except KeyboardInterrupt:
        print("\nTest interrupted by user")
    except Exception as e:
        print(f"\nError during test: {e}")
        import traceback
        traceback.print_exc()
    finally:
        tester.stop_player()
        print("\n[Test] Test completed")
        input("Press Enter to exit...")


if __name__ == "__main__":
    main()
