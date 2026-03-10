# testing/pyobjc/core/screenshot_capture.py
"""
macOS 屏幕截图捕获和分析
用于验证渲染是否正常（非黑屏）
"""

import os
import sys
import time
import subprocess
from datetime import datetime

# 添加 pyobjc 目录到路径
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

try:
    from PIL import Image
except ImportError:
    print("Error: Pillow is not installed. Run: pip install Pillow")
    sys.exit(1)

try:
    from Quartz import (
        CGWindowListCopyWindowInfo,
        kCGNullWindowID,
        kCGWindowListOptionOnScreenOnly,
        CGWindowID,
        CGDisplayCreateImageForRect,
        CGRectNull,
        CGMainDisplayID,
        CGDisplayBounds,
        CGImageDestinationCreateWithData,
        CGImageDestinationAddImage,
        CGImageDestinationFinalize,
        kUTTypePNG,
        CFDataCreateMutable,
        kCFAllocatorDefault,
    )
    from CoreGraphics import CGRectMake
    HAS_QUARTZ = True
except ImportError:
    HAS_QUARTZ = False
    print("Warning: Quartz not available, using screencapture command")


class ScreenshotCapture:
    """macOS 屏幕截图捕获"""

    def __init__(self, output_dir: str = None):
        """
        初始化截图捕获器

        Args:
            output_dir: 截图保存目录
        """
        self.output_dir = output_dir or os.path.join(
            os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
            "screenshots"
        )
        os.makedirs(self.output_dir, exist_ok=True)

    def capture_window_by_title(self, window_title: str, save_path: str = None) -> str:
        """
        通过窗口标题捕获窗口截图

        Args:
            window_title: 窗口标题
            save_path: 保存路径

        Returns:
            str: 截图文件路径
        """
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"screenshot_{timestamp}.png"
        save_path = save_path or os.path.join(self.output_dir, filename)

        # 使用 screencapture 命令捕获窗口
        # -l 选项指定窗口ID，-x 不播放声音
        result = subprocess.run(
            ["screencapture", "-x", "-t", "png", save_path],
            capture_output=True,
            timeout=10
        )

        if result.returncode == 0:
            return save_path
        else:
            raise Exception(f"screencapture failed: {result.stderr.decode()}")

    def capture_region(self, x: int, y: int, width: int, height: int, save_path: str = None) -> str:
        """
        捕获指定区域的截图

        Args:
            x, y: 区域左上角坐标
            width, height: 区域宽高
            save_path: 保存路径

        Returns:
            str: 截图文件路径
        """
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"region_{timestamp}.png"
        save_path = save_path or os.path.join(self.output_dir, filename)

        # 使用 screencapture 捕获区域
        region = f"-R{x},{y},{width},{height}"
        result = subprocess.run(
            ["screencapture", "-x", region, "-t", "png", save_path],
            capture_output=True,
            timeout=10
        )

        if result.returncode == 0:
            return save_path
        else:
            raise Exception(f"screencapture failed: {result.stderr.decode()}")

    def capture_full_screen(self, save_path: str = None) -> str:
        """
        捕获全屏截图

        Returns:
            str: 截图文件路径
        """
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"fullscreen_{timestamp}.png"
        save_path = save_path or os.path.join(self.output_dir, filename)

        result = subprocess.run(
            ["screencapture", "-x", "-t", "png", save_path],
            capture_output=True,
            timeout=10
        )

        if result.returncode == 0:
            return save_path
        else:
            raise Exception(f"screencapture failed: {result.stderr.decode()}")


class ImageAnalyzer:
    """图像分析器，用于检测黑屏等状态"""

    # 黑色阈值（低于此值认为是黑色）
    BLACK_THRESHOLD = 10

    # 黑屏像素比例阈值（超过此比例认为是黑屏）
    BLACK_RATIO_THRESHOLD = 0.95

    @staticmethod
    def is_black_screen(image_path: str, black_threshold: int = None) -> tuple:
        """
        检测图像是否为黑屏

        Args:
            image_path: 图像路径
            black_threshold: 黑色阈值 (0-255)

        Returns:
            tuple: (是否黑屏, 黑色像素比例, 平均亮度)
        """
        black_threshold = black_threshold or ImageAnalyzer.BLACK_THRESHOLD

        try:
            img = Image.open(image_path)
            # 转换为灰度图
            gray = img.convert('L')

            # 计算像素统计
            pixels = list(gray.getdata())
            total_pixels = len(pixels)

            # 计算黑色像素数量
            black_pixels = sum(1 for p in pixels if p < black_threshold)
            black_ratio = black_pixels / total_pixels

            # 计算平均亮度
            avg_brightness = sum(pixels) / total_pixels

            # 判断是否为黑屏
            is_black = black_ratio > ImageAnalyzer.BLACK_RATIO_THRESHOLD

            return is_black, black_ratio, avg_brightness

        except Exception as e:
            raise Exception(f"Failed to analyze image: {e}")

    @staticmethod
    def analyze_color_distribution(image_path: str) -> dict:
        """
        分析图像颜色分布

        Args:
            image_path: 图像路径

        Returns:
            dict: 颜色分布统计
        """
        try:
            img = Image.open(image_path)
            # 转换为 RGB
            rgb = img.convert('RGB')

            pixels = list(rgb.getdata())
            total = len(pixels)

            # 计算各通道平均值
            r_sum = g_sum = b_sum = 0
            for r, g, b in pixels:
                r_sum += r
                g_sum += g
                b_sum += b

            return {
                'avg_r': r_sum / total,
                'avg_g': g_sum / total,
                'avg_b': b_sum / total,
                'avg_brightness': (r_sum + g_sum + b_sum) / (3 * total),
            }

        except Exception as e:
            raise Exception(f"Failed to analyze colors: {e}")

    @staticmethod
    def compare_images(image1_path: str, image2_path: str) -> float:
        """
        比较两张图像的相似度

        Args:
            image1_path: 第一张图像路径
            image2_path: 第二张图像路径

        Returns:
            float: 相似度 (0-1)
        """
        try:
            img1 = Image.open(image1_path).convert('L')
            img2 = Image.open(image2_path).convert('L')

            # 确保尺寸相同
            if img1.size != img2.size:
                img2 = img2.resize(img1.size)

            # 计算差异
            import math
            pixels1 = list(img1.getdata())
            pixels2 = list(img2.getdata())

            total_diff = sum(abs(p1 - p2) for p1, p2 in zip(pixels1, pixels2))
            max_diff = 255 * len(pixels1)

            similarity = 1 - (total_diff / max_diff)
            return similarity

        except Exception as e:
            raise Exception(f"Failed to compare images: {e}")


def test_black_screen_detection():
    """测试黑屏检测功能"""
    print("Testing black screen detection...")

    # 创建测试图像
    from PIL import Image

    # 创建一个黑色测试图像
    black_img = Image.new('RGB', (100, 100), color='black')
    black_path = '/tmp/test_black.png'
    black_img.save(black_path)

    # 创建一个白色测试图像
    white_img = Image.new('RGB', (100, 100), color='white')
    white_path = '/tmp/test_white.png'
    white_img.save(white_path)

    # 测试黑屏检测
    is_black, ratio, brightness = ImageAnalyzer.is_black_screen(black_path)
    print(f"Black image: is_black={is_black}, ratio={ratio:.2f}, brightness={brightness:.1f}")

    is_black, ratio, brightness = ImageAnalyzer.is_black_screen(white_path)
    print(f"White image: is_black={is_black}, ratio={ratio:.2f}, brightness={brightness:.1f}")


if __name__ == "__main__":
    test_black_screen_detection()