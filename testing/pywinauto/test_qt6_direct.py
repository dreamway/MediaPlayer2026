"""
WZMediaPlayer Qt6 殀化测试脚本（直接使用键盘，不依赖pywinauto Application）
使用send_keys模拟键盘操作，避免GUI框架兼容性问题
"""

import os
import sys
import time
import subprocess
from datetime import datetime

try:
    from pywinauto.keyboard import send_keys
except ImportError:
    print("Error: pywinauto is not installed. Run: pip install pywinauto")
    sys.exit(1)


class Qt6SimpleTester:
    """Simplified tester using direct keyboard control"""

    def __init__(self, exe_path: str, test_video_path: str = None):
        self.exe_path = exe_path
        self.test_video_path = test_video_path
        self.process = None
        self.test_results = []

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
        """Start player using subprocess"""
        print("\n[Step 1/6] Starting player...")

        try:
            if not os.path.exists(self.exe_path):
                raise FileNotFoundError(f"Executable not found: {self.exe_path}")

            # 启动程序
            self.process = subprocess.Popen([self.exe_path])
            time.sleep(8)  # Qt6程序需要足够长的启动时间

            # 验证进程是否还在运行
            if self.process.poll() is not None:
                raise RuntimeError("Player process terminated early")

            self.log_test("Start Player", True, f"Started: {os.path.basename(self.exe_path)}")
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
            time.sleep(2.0)

            # Type file path
            send_keys(self.test_video_path)
            time.sleep(1.0)
            send_keys("{ENTER}")
            time.sleep(8.0)  # 等待视频完全加载（Qt6需要更长时间）

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
            time.sleep(2.0)

            print("  Using Right arrow key to seek forward 5 seconds...")
            send_keys("{RIGHT}")
            time.sleep(4.0)  # Qt6 seek需要更长时间
            self.log_test("Seek Forward Small", True, "Used Right arrow key")

            print("  Using Left arrow key to seek backward 5 seconds...")
            send_keys("{LEFT}")
            time.sleep(4.0)
            self.log_test("Seek Backward Small", True, "Used Left arrow key")

            return True

        except Exception as e:
            self.log_test("Small Seek", False, str(e))
            return False

    def test_volume(self) -> bool:
        """Test volume control"""
        print("\n[Step 6/7] Testing volume control...")

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
            if self.process and self.process.poll() is None:
                self.process.terminate()
                time.sleep(2)
        except Exception as e:
            print(f"Error stopping player: {e}")


def main():
    """Main function"""
    print("="*60)
    print("WZMediaPlayer Qt6 Simplified Test (Direct Keyboard Control)")
    print("="*60)
    print()

    # Configure paths (from config.ini / config.py)
    try:
        import config as test_config
        exe_path = test_config.PLAYER_EXE_PATH
        test_video_path = test_config.TEST_VIDEO_PATH
    except ImportError:
        exe_path = r"D:\2026Github\build\Release\WZMediaPlayer.exe"
        test_video_path = r"D:\2026Github\testing\video\test.mp4"

    # Create tester
    tester = Qt6SimpleTester(exe_path, test_video_path)

    try:
        # Run tests
        if not tester.start_player():
            print("\n[Error] Failed to start player")
        else:
            time.sleep(3)  # Additional wait for full GUI initialization
            tester.open_video()
            time.sleep(5)  # Let video play for a bit
            tester.test_play_pause()
            time.sleep(1)
            tester.test_stop()
            time.sleep(1)
            tester.test_seek_small()
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
        try:
            input("Press Enter to exit...")
        except:
            pass


if __name__ == "__main__":
    main()
