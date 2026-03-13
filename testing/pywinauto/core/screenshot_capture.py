# testing/pywinauto/core/screenshot_capture.py
"""
Windows 截图捕获
用于自动化测试中的截图和图像分析
"""

import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

try:
    from PIL import Image
    import numpy as np
    PIL_AVAILABLE = True
except ImportError:
    PIL_AVAILABLE = False

try:
    import pywinauto
    PYWINAUTO_AVAILABLE = True
except ImportError:
    PYWINAUTO_AVAILABLE = False


class ScreenshotCapture:
    """Windows 截图捕获"""

    def __init__(self, save_dir: str = None):
        """
        初始化截图捕获

        Args:
            save_dir: 截图保存目录
        """
        self.save_dir = save_dir or os.path.join(
            os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
            "screenshots"
        )
        os.makedirs(self.save_dir, exist_ok=True)

    def capture_full_screen(self) -> str:
        """
        截取全屏

        Returns:
            截图文件路径
        """
        timestamp = time.strftime("%Y%m%d_%H%M%S")
        filename = f"fullscreen_{timestamp}.png"
        filepath = os.path.join(self.save_dir, filename)

        try:
            if PIL_AVAILABLE:
                from PIL import ImageGrab
                img = ImageGrab.grab()
                img.save(filepath)
            else:
                # 使用 PowerShell 截图
                import subprocess
                script = f'''
                Add-Type -AssemblyName System.Windows.Forms
                Add-Type -AssemblyName System.Drawing
                $bitmap = New-Object System.Drawing.Bitmap([System.Windows.Forms.Screen]::PrimaryScreen.Bounds.Width, [System.Windows.Forms.Screen]::PrimaryScreen.Bounds.Height)
                $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
                $graphics.CopyFromScreen([System.Drawing.Point]::Empty, [System.Drawing.Point]::Empty, $bitmap.Size)
                $bitmap.Save("{filepath.replace(chr(92), chr(92)*2)}")
                '''
                subprocess.run(['powershell', '-Command', script], timeout=10)
        except Exception as e:
            print(f"Screenshot failed: {e}")
            return None

        return filepath

    def capture_window(self, window_handle=None) -> str:
        """
        截取指定窗口

        Args:
            window_handle: 窗口句柄

        Returns:
            截图文件路径
        """
        timestamp = time.strftime("%Y%m%d_%H%M%S")
        filename = f"window_{timestamp}.png"
        filepath = os.path.join(self.save_dir, filename)

        try:
            if PYWINAUTO_AVAILABLE and window_handle:
                app = pywinauto.Application(backend="uia").connect(handle=window_handle)
                window = app.window(handle=window_handle)
                window.capture_as_image().save(filepath)
            else:
                return self.capture_full_screen()
        except Exception as e:
            print(f"Window capture failed: {e}")
            return self.capture_full_screen()

        return filepath


class ImageAnalyzer:
    """图像分析工具"""

    @staticmethod
    def is_black_screen(image_path: str, threshold: int = 15) -> tuple:
        """
        检测图像是否为黑屏

        Args:
            image_path: 图像路径
            threshold: 亮度阈值

        Returns:
            (是否黑屏, 黑色像素比例, 平均亮度)
        """
        if not PIL_AVAILABLE:
            print("PIL not available, skipping black screen detection")
            return False, 0.0, 0.0

        try:
            img = Image.open(image_path).convert('L')  # 转为灰度图
            arr = np.array(img)

            # 计算平均亮度
            avg_brightness = arr.mean()

            # 计算黑色像素比例
            black_pixels = (arr < threshold).sum()
            total_pixels = arr.size
            black_ratio = black_pixels / total_pixels

            is_black = black_ratio > 0.9  # 90% 以上为黑色像素视为黑屏

            return is_black, black_ratio, avg_brightness

        except Exception as e:
            print(f"Image analysis failed: {e}")
            return False, 0.0, 0.0

    @staticmethod
    def compare_images(img1_path: str, img2_path: str) -> float:
        """
        比较两张图像的相似度

        Args:
            img1_path: 第一张图像路径
            img2_path: 第二张图像路径

        Returns:
            相似度 (0.0 - 1.0)
        """
        if not PIL_AVAILABLE:
            return 0.0

        try:
            img1 = Image.open(img1_path).convert('L')
            img2 = Image.open(img2_path).convert('L')

            # 调整大小以匹配
            if img1.size != img2.size:
                img2 = img2.resize(img1.size)

            arr1 = np.array(img1, dtype=np.float64)
            arr2 = np.array(img2, dtype=np.float64)

            # 计算均方误差
            mse = np.mean((arr1 - arr2) ** 2)

            # 转换为相似度 (MSE 越小越相似)
            similarity = 1.0 / (1.0 + mse / 1000.0)

            return similarity

        except Exception as e:
            print(f"Image comparison failed: {e}")
            return 0.0

    @staticmethod
    def get_average_brightness(image_path: str) -> float:
        """
        获取图像平均亮度

        Args:
            image_path: 图像路径

        Returns:
            平均亮度 (0-255)
        """
        if not PIL_AVAILABLE:
            return 0.0

        try:
            img = Image.open(image_path).convert('L')
            arr = np.array(img)
            return arr.mean()
        except Exception as e:
            print(f"Brightness calculation failed: {e}")
            return 0.0


def main():
    """测试截图功能"""
    print("Testing screenshot capture...")

    capture = ScreenshotCapture()
    filepath = capture.capture_full_screen()

    if filepath:
        print(f"Screenshot saved: {filepath}")

        is_black, black_ratio, brightness = ImageAnalyzer.is_black_screen(filepath)
        print(f"Black screen: {is_black}, Black ratio: {black_ratio:.2%}, Brightness: {brightness:.1f}")


if __name__ == "__main__":
    main()