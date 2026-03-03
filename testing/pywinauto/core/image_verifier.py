"""
图像验证器 - 基于PIL的图像捕获和验证
用于闭环测试中的视觉状态验证
"""

import os
import time
from typing import Optional, Tuple, List, Dict, Any
from dataclasses import dataclass
from datetime import datetime

try:
    from PIL import Image, ImageChops, ImageFilter
except ImportError:
    raise ImportError("PIL未安装，请运行: pip install Pillow")


@dataclass
class ImageVerificationResult:
    """图像验证结果"""
    passed: bool
    message: str
    diff_percentage: float = 0.0
    screenshot_path: str = ""
    reference_path: str = ""


class ImageVerifier:
    """
    图像验证器
    
    功能：
    1. 捕获窗口/控件截图
    2. 与参考图像比较
    3. 像素级验证（颜色、区域等）
    4. 生成差异报告
    """
    
    def __init__(self, output_dir: str = None):
        """
        初始化图像验证器
        
        Args:
            output_dir: 截图保存目录
        """
        if output_dir is None:
            output_dir = os.path.join(os.path.dirname(__file__), '..', 'screenshots')
        
        self.output_dir = output_dir
        self.reference_dir = os.path.join(output_dir, 'references')
        self.diff_dir = os.path.join(output_dir, 'diffs')
        
        # 创建目录
        os.makedirs(self.output_dir, exist_ok=True)
        os.makedirs(self.reference_dir, exist_ok=True)
        os.makedirs(self.diff_dir, exist_ok=True)
        
        self._last_screenshot: Optional[Image.Image] = None
        self._last_screenshot_path: str = ""
    
    def capture_and_save(self, image: Image.Image, name: str = None) -> str:
        """
        保存截图
        
        Args:
            image: PIL Image对象
            name: 截图名称（不含扩展名）
            
        Returns:
            保存的文件路径
        """
        if name is None:
            timestamp = datetime.now().strftime('%Y%m%d_%H%M%S_%f')
            name = f"screenshot_{timestamp}"
        
        filepath = os.path.join(self.output_dir, f"{name}.png")
        image.save(filepath)
        
        self._last_screenshot = image
        self._last_screenshot_path = filepath
        
        return filepath
    
    def compare_with_reference(self, current_image: Image.Image, 
                               reference_name: str,
                               threshold: float = 0.95) -> ImageVerificationResult:
        """
        将当前图像与参考图像比较
        
        Args:
            current_image: 当前图像
            reference_name: 参考图像名称（不含扩展名）
            threshold: 相似度阈值（0-1）
            
        Returns:
            验证结果
        """
        reference_path = os.path.join(self.reference_dir, f"{reference_name}.png")
        
        if not os.path.exists(reference_path):
            return ImageVerificationResult(
                passed=False,
                message=f"参考图像不存在: {reference_path}",
                reference_path=reference_path
            )
        
        try:
            reference_image = Image.open(reference_path)
            
            # 确保尺寸一致
            if current_image.size != reference_image.size:
                current_image = current_image.resize(reference_image.size)
            
            # 转换为RGB模式
            current_rgb = current_image.convert('RGB')
            reference_rgb = reference_image.convert('RGB')
            
            # 计算差异
            diff = ImageChops.difference(current_rgb, reference_rgb)
            
            # 计算差异百分比
            diff_bbox = diff.getbbox()
            if diff_bbox is None:
                # 图像完全相同
                return ImageVerificationResult(
                    passed=True,
                    message="图像完全相同",
                    diff_percentage=0.0,
                    reference_path=reference_path
                )
            
            # 计算差异像素数量
            diff_pixels = sum(1 for pixel in diff.getdata() if pixel != (0, 0, 0))
            total_pixels = diff.size[0] * diff.size[1]
            diff_percentage = (diff_pixels / total_pixels) * 100
            
            # 相似度
            similarity = 1.0 - (diff_pixels / total_pixels)
            
            # 保存差异图像
            timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
            diff_path = os.path.join(self.diff_dir, f"diff_{reference_name}_{timestamp}.png")
            diff.save(diff_path)
            
            passed = similarity >= threshold
            
            return ImageVerificationResult(
                passed=passed,
                message=f"相似度: {similarity:.2%}, 差异像素: {diff_pixels}/{total_pixels}",
                diff_percentage=diff_percentage,
                screenshot_path=diff_path,
                reference_path=reference_path
            )
            
        except Exception as e:
            return ImageVerificationResult(
                passed=False,
                message=f"比较失败: {str(e)}",
                reference_path=reference_path
            )
    
    def verify_pixel_color(self, image: Image.Image, 
                          position: Tuple[int, int],
                          expected_color: Tuple[int, int, int],
                          tolerance: int = 10) -> ImageVerificationResult:
        """
        验证指定位置的像素颜色
        
        Args:
            image: 图像
            position: (x, y) 像素位置
            expected_color: (R, G, B) 期望颜色
            tolerance: 容差值
            
        Returns:
            验证结果
        """
        try:
            rgb_image = image.convert('RGB')
            actual_color = rgb_image.getpixel(position)
            
            # 计算颜色差异
            diff = sum(abs(a - e) for a, e in zip(actual_color, expected_color))
            max_diff = tolerance * 3  # RGB三个通道的总容差
            
            passed = diff <= max_diff
            
            return ImageVerificationResult(
                passed=passed,
                message=f"位置{position}: 期望{expected_color}, 实际{actual_color}, 差异{diff}",
                diff_percentage=diff / 765.0  # 归一化差异
            )
            
        except Exception as e:
            return ImageVerificationResult(
                passed=False,
                message=f"像素验证失败: {str(e)}"
            )
    
    def verify_region_color(self, image: Image.Image,
                           region: Tuple[int, int, int, int],  # (left, top, right, bottom)
                           expected_color: Tuple[int, int, int],
                           tolerance: int = 10,
                           min_match_percentage: float = 0.8) -> ImageVerificationResult:
        """
        验证区域的颜色分布
        
        Args:
            image: 图像
            region: (left, top, right, bottom) 区域坐标
            expected_color: 期望颜色
            tolerance: 容差值
            min_match_percentage: 最小匹配百分比
            
        Returns:
            验证结果
        """
        try:
            rgb_image = image.convert('RGB')
            
            # 裁剪区域
            left, top, right, bottom = region
            region_image = rgb_image.crop((left, top, right, bottom))
            
            # 统计匹配像素
            pixels = list(region_image.getdata())
            total_pixels = len(pixels)
            
            matching_pixels = 0
            for pixel in pixels:
                diff = sum(abs(a - e) for a, e in zip(pixel, expected_color))
                if diff <= tolerance * 3:
                    matching_pixels += 1
            
            match_percentage = matching_pixels / total_pixels
            passed = match_percentage >= min_match_percentage
            
            return ImageVerificationResult(
                passed=passed,
                message=f"区域{region}: {match_percentage:.1%}像素匹配期望颜色{expected_color}",
                diff_percentage=(1 - match_percentage) * 100
            )
            
        except Exception as e:
            return ImageVerificationResult(
                passed=False,
                message=f"区域验证失败: {str(e)}"
            )
    
    def verify_video_playing(self, image1: Image.Image, 
                            image2: Image.Image,
                            region: Tuple[int, int, int, int] = None,
                            threshold: int = 1000) -> ImageVerificationResult:
        """
        验证视频是否在播放（通过比较两帧的差异）
        
        Args:
            image1: 第一帧
            image2: 第二帧（稍后的时间点）
            region: 检测区域（默认为整个图像）
            threshold: 差异阈值（像素变化数量）
            
        Returns:
            验证结果
        """
        try:
            # 裁剪区域
            if region:
                left, top, right, bottom = region
                img1 = image1.crop((left, top, right, bottom))
                img2 = image2.crop((left, top, right, bottom))
            else:
                img1 = image1
                img2 = image2
            
            # 转换为灰度图
            gray1 = img1.convert('L')
            gray2 = img2.convert('L')
            
            # 计算差异
            diff = ImageChops.difference(gray1, gray2)
            
            # 统计差异像素
            diff_pixels = sum(1 for pixel in diff.getdata() if pixel > 10)
            
            # 视频在播放的条件：有足够多的像素发生变化
            passed = diff_pixels > threshold
            
            return ImageVerificationResult(
                passed=passed,
                message=f"两帧差异像素: {diff_pixels} (阈值: {threshold})",
                diff_percentage=0.0
            )
            
        except Exception as e:
            return ImageVerificationResult(
                passed=False,
                message=f"视频播放验证失败: {str(e)}"
            )
    
    def create_reference_image(self, image: Image.Image, name: str):
        """
        创建参考图像
        
        Args:
            image: 图像
            name: 参考图像名称
        """
        reference_path = os.path.join(self.reference_dir, f"{name}.png")
        image.save(reference_path)
        print(f"[ImageVerifier] 参考图像已创建: {reference_path}")
    
    def get_dominant_color(self, image: Image.Image, 
                          region: Tuple[int, int, int, int] = None) -> Tuple[int, int, int]:
        """
        获取图像或区域的主色调
        
        Args:
            image: 图像
            region: 区域坐标
            
        Returns:
            (R, G, B) 主色调
        """
        if region:
            left, top, right, bottom = region
            image = image.crop((left, top, right, bottom))
        
        # 缩小图像以加速计算
        small = image.resize((50, 50))
        rgb = small.convert('RGB')
        
        # 计算平均颜色
        pixels = list(rgb.getdata())
        avg_r = sum(p[0] for p in pixels) // len(pixels)
        avg_g = sum(p[1] for p in pixels) // len(pixels)
        avg_b = sum(p[2] for p in pixels) // len(pixels)
        
        return (avg_r, avg_g, avg_b)
    
    def is_black_screen(self, image: Image.Image, 
                       threshold: int = 30) -> bool:
        """
        检测是否为黑屏
        
        Args:
            image: 图像
            threshold: 亮度阈值
            
        Returns:
            是否为黑屏
        """
        gray = image.convert('L')
        pixels = list(gray.getdata())
        avg_brightness = sum(pixels) / len(pixels)
        
        return avg_brightness < threshold
    
    def is_playback_ui_state(self, image: Image.Image,
                            play_region: Tuple[int, int, int, int]) -> Dict[str, Any]:
        """
        分析播放UI状态
        
        Args:
            image: 窗口截图
            play_region: 播放区域坐标
            
        Returns:
            状态字典
        """
        result = {
            'is_black_screen': False,
            'is_playing': False,
            'dominant_color': (0, 0, 0),
            'brightness': 0
        }
        
        try:
            # 裁剪播放区域
            left, top, right, bottom = play_region
            play_area = image.crop((left, top, right, bottom))
            
            # 检测黑屏
            result['is_black_screen'] = self.is_black_screen(play_area)
            
            # 获取主色调
            result['dominant_color'] = self.get_dominant_color(play_area)
            
            # 计算亮度
            gray = play_area.convert('L')
            result['brightness'] = sum(gray.getdata()) / (gray.size[0] * gray.size[1])
            
            # 判断是否正在播放（非黑屏且有一定亮度）
            result['is_playing'] = not result['is_black_screen'] and result['brightness'] > 30
            
        except Exception as e:
            result['error'] = str(e)
        
        return result
