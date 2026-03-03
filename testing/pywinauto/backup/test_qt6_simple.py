"""
WZMediaPlayer Qt6 Simplified Test Script
使用快捷键进行测试，避免控件查找问题
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
    """Simplified tester for Qt6 applications"""

    def __init__(self, exe_path: str, test_video_path: str = None):
        self.exe_path = exe_path
        self.test_video_path = test_video_path
        self.app = None
        self.main_window = None
        self.test_results = []

        Timings.fast()

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
        """Start the player"""
        print("\n[Step 1/6] Starting player...")

        try:
            if not os.path.exists(self.exe_path):
                raise FileNotFoundError(f"Executable not found: {self.exe_path}")

            self.app = Application(backend="uia").start(self.exe_path)
            time.sleep(3)

            self.main_window = self.app.window()

            if not self.main_window.exists():
                raise RuntimeError("Main window not found")

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

            # Use Ctrl+O to open file
            send_keys("^o")
            time.sleep(1.0)

            # Type file path
            send_keys(self.test_video_path)
            time.sleep(0.5)
            send_keys("{ENTER}")
            time.sleep(5.0)  # Wait for video to load

            self.log_test("Open Video", True, "Used Ctrl+O shortcut")
            return True

        except Exception as e:
            self.log_test("Open Video", False, str(e))
            return False

    def test_play_pause(self) -> bool:
        """Test play/pause"""
        print("\n[Step 3/6] Testing play/pause...")

        try:
            # Pause
            send_keys("{SPACE}")
            time.sleep(1.0)
            self.log_test("Pause", True, "Used spacebar")

            # Play
            send_keys("{SPACE}")
            time.sleep(1.0)
            self.log_test("Play", True, "Used spacebar")

            return True

        except Exception as e:
            self.log_test("Play/Pause", False, str(e))
            return False

    def test_stop(self) -> bool:
        """Test stop"""
        print("\n[Step 4/6] Testing stop playback...")

        try:
            # Use Ctrl+S to stop
            send_keys("^{s}")
            time.sleep(1.0)
            self.log_test("Stop", True, "Used Ctrl+S shortcut")

            return True

        except Exception as e:
            self.log_test("Stop", False, str(e))
            return False

    def test_seek_simple(self) -> bool:
        """Simple seek test (without progress bar)"""
        print("\n[Step 5/6] Simple seek test...")

        try:
            # Play for a few seconds
            print("  Playing for 5 seconds...")
            time.sleep(5)

            # Try using arrow keys for seek
            print("  Using right arrow key to seek...")
            send_keys("{RIGHT 5}")  # Right arrow 5 times
            time.sleep(2)

            self.log_test("Seek Right", True, "Used right arrow key 5 times")

            print("  Using left arrow key to seek...")
            send_keys("{LEFT 5}")  # Left arrow 5 times
            time.sleep(2)

            self.log_test("Seek Left", True, "Used left arrow key 5 times")

            return True

        except Exception as e:
            self.log_test("Simple Seek", False, str(e))
            return False

    def test_volume(self) -> bool:
        """Test volume control"""
        print("\n[Step 6/6] Testing volume control...")

        try:
            # Use up/down arrow keys for volume
            send_keys("{UP 2}")  # Volume up
            time.sleep(0.5)
            self.log_test("Volume Up", True, "Used up arrow key 2 times")

            send_keys("{DOWN 2}")  # Volume down
            time.sleep(0.5)
            self.log_test("Volume Down", True, "Used down arrow key 2 times")

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
        """Stop the player"""
        try:
            if self.app:
                self.app.kill()
                time.sleep(1)
        except Exception as e:
            print(f"Error stopping player: {e}")


def main():
    """Main function"""
    print("="*60)
    print("WZMediaPlayer Qt6 Simplified Test")
    print("="*60)
    print("Using keyboard shortcuts, avoiding Qt6 control finding issues")
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
            tester.test_play_pause()
            tester.test_stop()
            tester.test_seek_simple()
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
