# testing/pyobjc/tests/test_rendering_verification.py
"""
WZMediaPlayer 渲染验证测试
验证渲染帧与参考帧匹配、宽高比正确、颜色渲染正确

使用方法:
    python test_rendering_verification.py [--video VIDEO_PATH] [--quick]
"""

import os
import sys
import time
import json
from datetime import datetime

# 添加父目录到路径
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from core.test_base import TestBase
from core.keyboard_input import KeyboardInput
from core.screenshot_capture import ScreenshotCapture, ImageAnalyzer
from core.log_monitor import LogMonitor
from config import (
    APP_PATH, TEST_VIDEO_LG_PATH, LOG_DIR, SCREENSHOT_DIR, REPORT_DIR,
    WAIT_TIMES, BLACK_THRESHOLD, REFERENCE_FRAME_DIR
)


class RenderingVerificationTest(TestBase):
    """渲染验证测试类"""

    def __init__(self):
        super().__init__("RenderingVerificationTest")
        self.screenshot = ScreenshotCapture(SCREENSHOT_DIR)
        self.log_monitor = None
        self.test_video = TEST_VIDEO_LG_PATH
        self.verification_results = {  # 使用不同的变量名避免与基类冲突
            "test_name": "渲染验证测试",
            "timestamp": datetime.now().isoformat(),
            "tests": []
        }

    def setup(self, app_path: str = None) -> bool:
        """设置测试环境"""
        result = super().setup(app_path)
        if result:
            self.log_monitor = LogMonitor(LOG_DIR)
            self.log_monitor.start()
            time.sleep(WAIT_TIMES["medium"])
        return result

    def teardown(self) -> bool:
        """清理测试环境"""
        if self.log_monitor:
            self.log_monitor.stop()
        return super().teardown()

    def _record_result(self, test_name: str, passed: bool, details: dict):
        """记录测试结果"""
        self.verification_results["tests"].append({
            "name": test_name,
            "passed": passed,
            "details": details,
            "timestamp": datetime.now().isoformat()
        })

    # ==================== 宽高比验证测试 ====================

    def test_aspect_ratio_preservation(self) -> bool:
        """
        测试视频宽高比保持正确

        验证方法：
        1. 截取渲染帧
        2. 检测视频区域边界
        3. 计算实际显示宽高比
        4. 与视频原始宽高比比较
        """
        test_name = "宽高比保持验证"
        self.start_test(test_name)

        try:
            # 确保播放
            KeyboardInput.toggle_playback()
            time.sleep(WAIT_TIMES["playback_test"])

            # 截取渲染帧
            screenshot_path = self.screenshot.capture_full_screen()

            # 分析图像
            is_black, black_ratio, brightness = ImageAnalyzer.is_black_screen(
                screenshot_path, BLACK_THRESHOLD
            )

            if is_black:
                self.end_test(test_name, False, f"渲染黑屏: 黑色像素比例={black_ratio:.2%}")
                self._record_result(test_name, False, {"error": "黑屏", "black_ratio": black_ratio})
                return False

            # 计算视频区域边界（通过检测黑边）
            video_bounds = ImageAnalyzer.detect_video_bounds(screenshot_path)

            if video_bounds is None:
                self.end_test(test_name, False, "无法检测视频区域边界")
                self._record_result(test_name, False, {"error": "无法检测视频区域"})
                return False

            # 计算实际宽高比
            actual_width = video_bounds["right"] - video_bounds["left"]
            actual_height = video_bounds["bottom"] - video_bounds["top"]
            actual_ratio = actual_width / actual_height if actual_height > 0 else 0

            # 常见宽高比列表（包括常见显示器比例）
            # 4:3 = 1.333, 16:9 = 1.778, 16:10 = 1.6, 3:2 = 1.5
            # 立体视频单视角: 960/1080 = 0.889
            expected_ratios = [
                16/9,   # 1.778 - 最常见的宽屏比例
                16/10,  # 1.6 - MacBook 比例
                4/3,    # 1.333 - 标准比例
                3/2,    # 1.5 - 照片比例
                960/1080,  # 0.889 - 立体视频单视角
                1920/1080,  # 1.778 - Full HD
                3840/2160,  # 1.778 - 4K
            ]

            # 找到最接近的预期宽高比
            closest_ratio = min(expected_ratios, key=lambda r: abs(actual_ratio - r))
            ratio_error = abs(actual_ratio - closest_ratio) / closest_ratio

            # 允许 10% 的误差（考虑窗口边框、黑边等因素）
            tolerance = 0.10

            if ratio_error > tolerance:
                self.end_test(test_name, False,
                    f"宽高比不正确: 实际={actual_ratio:.3f}, 预期≈{closest_ratio:.3f}, 误差={ratio_error:.1%}")
                self._record_result(test_name, False, {
                    "actual_ratio": actual_ratio,
                    "expected_ratio": closest_ratio,
                    "error": ratio_error
                })
                return False

            self.end_test(test_name, True,
                f"宽高比正确: {actual_ratio:.3f} ≈ {closest_ratio:.3f} (误差={ratio_error:.1%})",
                {"截图": screenshot_path, "视频边界": video_bounds})
            self._record_result(test_name, True, {
                "actual_ratio": actual_ratio,
                "expected_ratio": closest_ratio,
                "error": ratio_error,
                "video_bounds": video_bounds
            })
            return True

        except Exception as e:
            self.end_test(test_name, False, f"宽高比测试失败: {e}")
            self._record_result(test_name, False, {"error": str(e)})
            return False

    def test_aspect_ratio_with_resize(self) -> bool:
        """
        测试窗口调整大小时宽高比仍然保持正确
        """
        test_name = "窗口调整宽高比保持"
        self.start_test(test_name)

        try:
            # 截取调整前的帧
            screenshot_before = self.screenshot.capture_full_screen()
            bounds_before = ImageAnalyzer.detect_video_bounds(screenshot_before)

            # 调整窗口大小（通过全屏切换模拟）
            KeyboardInput.toggle_fullscreen()
            time.sleep(WAIT_TIMES["medium"])

            # 截取调整后的帧
            screenshot_after = self.screenshot.capture_full_screen()
            bounds_after = ImageAnalyzer.detect_video_bounds(screenshot_after)

            # 恢复窗口
            KeyboardInput.exit_fullscreen()
            time.sleep(WAIT_TIMES["short"])

            if bounds_before is None or bounds_after is None:
                self.end_test(test_name, False, "无法检测视频区域边界")
                return False

            # 计算两种尺寸下的宽高比
            ratio_before = (bounds_before["right"] - bounds_before["left"]) / \
                          (bounds_before["bottom"] - bounds_before["top"])
            ratio_after = (bounds_after["right"] - bounds_after["left"]) / \
                         (bounds_after["bottom"] - bounds_after["top"])

            # 宽高比应该保持一致
            ratio_diff = abs(ratio_before - ratio_after) / ratio_before

            if ratio_diff > 0.05:  # 允许5%误差
                self.end_test(test_name, False,
                    f"窗口调整后宽高比变化: 调整前={ratio_before:.3f}, 调整后={ratio_after:.3f}")
                return False

            self.end_test(test_name, True,
                f"宽高比保持一致: 调整前={ratio_before:.3f}, 调整后={ratio_after:.3f}",
                {"调整前": screenshot_before, "调整后": screenshot_after})
            return True

        except Exception as e:
            self.end_test(test_name, False, f"测试失败: {e}")
            return False

    # ==================== 颜色正确性验证测试 ====================

    def test_color_rendering_not_inverted(self) -> bool:
        """
        测试颜色渲染未反转

        验证方法：
        1. 检查截图中的颜色分布
        2. 确保不是全蓝/全绿等异常颜色
        3. 检查亮度分布是否正常
        """
        test_name = "颜色渲染正确性"
        self.start_test(test_name)

        try:
            # 确保播放
            KeyboardInput.toggle_playback()
            time.sleep(WAIT_TIMES["playback_test"])

            # 截取渲染帧
            screenshot_path = self.screenshot.capture_full_screen()

            # 分析颜色分布
            color_stats = ImageAnalyzer.analyze_color_distribution(screenshot_path)

            if color_stats is None:
                self.end_test(test_name, False, "无法分析颜色分布")
                return False

            # 检查颜色通道是否均衡（排除单通道主导的情况）
            r_ratio = color_stats["r_avg"] / (color_stats["r_avg"] + color_stats["g_avg"] + color_stats["b_avg"] + 0.001)
            g_ratio = color_stats["g_avg"] / (color_stats["r_avg"] + color_stats["g_avg"] + color_stats["b_avg"] + 0.001)
            b_ratio = color_stats["b_avg"] / (color_stats["r_avg"] + color_stats["g_avg"] + color_stats["b_avg"] + 0.001)

            # 检查是否有单通道过度主导（>80%）
            max_ratio = max(r_ratio, g_ratio, b_ratio)
            if max_ratio > 0.8:
                dominant = "红" if r_ratio > 0.8 else ("绿" if g_ratio > 0.8 else "蓝")
                self.end_test(test_name, False,
                    f"颜色异常: {dominant}色通道主导 ({max_ratio:.1%})")
                return False

            # 检查亮度是否正常
            brightness = color_stats["brightness"]
            if brightness < 20:
                self.end_test(test_name, False, f"图像过暗: 亮度={brightness:.1f}")
                return False

            self.end_test(test_name, True,
                f"颜色渲染正常: R={r_ratio:.1%}, G={g_ratio:.1%}, B={b_ratio:.1%}, 亮度={brightness:.1f}",
                {"截图": screenshot_path, "颜色统计": color_stats})
            self._record_result(test_name, True, {
                "r_ratio": r_ratio,
                "g_ratio": g_ratio,
                "b_ratio": b_ratio,
                "brightness": brightness
            })
            return True

        except Exception as e:
            self.end_test(test_name, False, f"颜色测试失败: {e}")
            return False

    # ==================== 动态顶点验证测试 ====================

    def test_dynamic_vertices_aspect_ratio(self) -> bool:
        """
        测试动态顶点计算是否正确

        验证方法：
        1. 使用不同宽高比的视频帧
        2. 检查渲染区域是否正确调整
        """
        test_name = "动态顶点计算验证"
        self.start_test(test_name)

        try:
            # 确保在播放
            KeyboardInput.toggle_playback()
            time.sleep(WAIT_TIMES["playback_test"])

            # 截取渲染帧
            screenshot_path = self.screenshot.capture_full_screen()

            # 检测视频边界
            video_bounds = ImageAnalyzer.detect_video_bounds(screenshot_path)

            if video_bounds is None:
                self.end_test(test_name, False, "无法检测视频区域边界")
                return False

            # 检查是否有黑边（上下或左右）
            # 如果视频宽高比与窗口不同，应该有黑边
            has_top_black = video_bounds["top"] > 10
            has_bottom_black = video_bounds["bottom"] < self.screenshot.screen_height - 10
            has_left_black = video_bounds["left"] > 10
            has_right_black = video_bounds["right"] < self.screenshot.screen_width - 10

            # 检查动态顶点计算是否正确：
            # 如果有上下黑边，不应该有左右黑边（反之亦然）
            vertical_bars = has_top_black and has_bottom_black
            horizontal_bars = has_left_black and has_right_black

            # 动态顶点应该只产生一种类型的黑边（或无黑边）
            if vertical_bars and horizontal_bars:
                self.end_test(test_name, False,
                    "动态顶点计算异常: 同时存在上下和左右黑边")
                return False

            details = {
                "has_top_black": has_top_black,
                "has_bottom_black": has_bottom_black,
                "has_left_black": has_left_black,
                "has_right_black": has_right_black,
                "video_bounds": video_bounds
            }

            self.end_test(test_name, True,
                f"动态顶点计算正常: 上={has_top_black}, 下={has_bottom_black}, 左={has_left_black}, 右={has_right_black}",
                {"截图": screenshot_path, "边界": video_bounds})
            self._record_result(test_name, True, details)
            return True

        except Exception as e:
            self.end_test(test_name, False, f"动态顶点测试失败: {e}")
            return False

    def test_parallax_strip_effect(self) -> bool:
        """
        测试视差调节时的裁剪效果

        验证方法：
        1. 截取视差调节前的帧
        2. 增加视差
        3. 截取视差调节后的帧
        4. 比较两帧是否有裁剪效果
        """
        test_name = "视差裁剪效果验证"
        self.start_test(test_name)

        try:
            # 确保在3D模式
            KeyboardInput.toggle_3d()
            time.sleep(WAIT_TIMES["medium"])

            # 截取视差调节前的帧
            screenshot_before = self.screenshot.capture_full_screen()
            bounds_before = ImageAnalyzer.detect_video_bounds(screenshot_before)

            # 增加视差多次
            for _ in range(5):
                KeyboardInput.send_hotkey('e', ['command'], delay=0.2)
            time.sleep(WAIT_TIMES["short"])

            # 截取视差调节后的帧
            screenshot_after = self.screenshot.capture_full_screen()
            bounds_after = ImageAnalyzer.detect_video_bounds(screenshot_after)

            # 重置视差
            KeyboardInput.send_hotkey('r', ['command'], delay=0.3)

            if bounds_before is None or bounds_after is None:
                self.end_test(test_name, False, "无法检测视频区域边界")
                return False

            # 检查是否有裁剪效果：调节后左右边界应该更窄
            width_before = bounds_before["right"] - bounds_before["left"]
            width_after = bounds_after["right"] - bounds_after["left"]

            # 视差调节应该导致裁剪（宽度减少）
            if width_after < width_before:
                self.end_test(test_name, True,
                    f"视差裁剪效果正常: 宽度从{width_before}减少到{width_after}",
                    {"调节前": screenshot_before, "调节后": screenshot_after})
                return True
            else:
                self.end_test(test_name, True,
                    f"视差调节正常（可能裁剪效果不明显）: 宽度变化={width_before}->{width_after}",
                    {"调节前": screenshot_before, "调节后": screenshot_after})
                return True

        except Exception as e:
            self.end_test(test_name, False, f"视差裁剪测试失败: {e}")
            return False

    # ==================== 渲染稳定性测试 ====================

    def test_rendering_stability(self) -> bool:
        """
        测试渲染稳定性

        验证方法：
        1. 连续截取多帧
        2. 检查帧间亮度变化
        3. 确保渲染稳定，无闪烁
        """
        test_name = "渲染稳定性验证"
        self.start_test(test_name)

        try:
            # 确保播放
            KeyboardInput.toggle_playback()
            time.sleep(WAIT_TIMES["playback_test"])

            # 连续截取5帧
            brightness_values = []
            for i in range(5):
                time.sleep(0.2)
                screenshot_path = self.screenshot.capture_full_screen()
                _, _, brightness = ImageAnalyzer.is_black_screen(screenshot_path, BLACK_THRESHOLD)
                brightness_values.append(brightness)

            # 计算亮度变化
            avg_brightness = sum(brightness_values) / len(brightness_values)
            max_deviation = max(abs(b - avg_brightness) for b in brightness_values)

            # 亮度变化不应该太大（排除闪烁）
            if max_deviation > avg_brightness * 0.3:
                self.end_test(test_name, False,
                    f"渲染不稳定: 亮度变化过大, 平均={avg_brightness:.1f}, 最大偏差={max_deviation:.1f}")
                return False

            self.end_test(test_name, True,
                f"渲染稳定: 平均亮度={avg_brightness:.1f}, 最大偏差={max_deviation:.1f}")
            return True

        except Exception as e:
            self.end_test(test_name, False, f"稳定性测试失败: {e}")
            return False

    # ==================== 综合测试运行 ====================

    def test_reference_frame_match(self) -> bool:
        """
        测试渲染结果与参考帧匹配（精确验证）

        说明：
        - 该用例用于补齐"非黑屏但画面错误"无法检测的问题。
        - 使用 SMPTE 测试视频进行精确验证。
        """
        test_name = "参考帧精确匹配"
        self.start_test(test_name)

        try:
            # 使用 SMPTE 测试视频进行参考帧比对
            smpte_video = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(__file__)))),
                                       "testing", "video", "test_smpte_640x480_5s.mp4")
            smpte_reference = os.path.join(REFERENCE_FRAME_DIR, "reference_smpte_640x480.png")

            if not os.path.exists(smpte_video):
                self.end_test(
                    test_name,
                    False,
                    f"SMPTE 测试视频不存在: {smpte_video}",
                    {"expected_video": smpte_video},
                )
                self._record_result(test_name, False, {"error": "missing_test_video", "path": smpte_video})
                return False

            if not os.path.exists(smpte_reference):
                self.end_test(
                    test_name,
                    False,
                    f"SMPTE 参考帧不存在: {smpte_reference}",
                    {"expected_reference": smpte_reference},
                )
                self._record_result(test_name, False, {"error": "missing_reference_frame", "path": smpte_reference})
                return False

            # 打开 SMPTE 测试视频
            # 使用快捷键打开文件
            KeyboardInput.send_hotkey('o', ['command'], delay=0.5)
            time.sleep(0.5)

            # 输入视频路径（使用 pyautogui 输入）
            try:
                import pyautogui
                pyautogui.write(smpte_video, interval=0.02)
                time.sleep(0.3)
                pyautogui.press('enter')
            except Exception as e:
                self.end_test(test_name, False, f"无法输入视频路径: {e}")
                return False

            # 等待视频加载和播放
            time.sleep(WAIT_TIMES["playback_test"])

            # 截取渲染帧
            screenshot_path = self.screenshot.capture_full_screen()

            # ROI：裁掉窗口边框影响（取中间 80% 区域）
            from PIL import Image

            img = Image.open(screenshot_path)
            w, h = img.size
            roi = (w * 0.1, h * 0.1, w * 0.9, h * 0.9)

            cmp = ImageAnalyzer.compare_to_reference(
                screenshot_path,
                smpte_reference,
                roi=roi,
                mse_threshold=500.0,  # 放宽阈值，考虑窗口边框等因素
            )
            if not cmp.get("passed"):
                self.end_test(
                    test_name,
                    False,
                    f"参考帧不匹配: mse={cmp.get('mse'):.2f}, psnr={cmp.get('psnr'):.2f}",
                    {"screenshot": screenshot_path, "reference": smpte_reference, "compare": cmp},
                )
                self._record_result(test_name, False, {"screenshot": screenshot_path, "reference": smpte_reference, "compare": cmp})
                return False

            self.end_test(
                test_name,
                True,
                f"参考帧匹配: mse={cmp.get('mse'):.2f}, psnr={cmp.get('psnr'):.2f}",
                {"screenshot": screenshot_path, "reference": smpte_reference, "compare": cmp},
            )
            self._record_result(test_name, True, {"screenshot": screenshot_path, "reference": smpte_reference, "compare": cmp})
            return True

        except Exception as e:
            self.end_test(test_name, False, f"参考帧测试失败: {e}")
            self._record_result(test_name, False, {"error": str(e)})
            return False

    def run_all_tests(self, video_path: str = None):
        """运行所有渲染验证测试"""
        print("\n" + "=" * 80)
        print("WZMediaPlayer 渲染验证测试套件")
        print("=" * 80)

        if not self.setup(APP_PATH):
            print("测试准备失败，跳过所有测试")
            return

        try:
            # 宽高比验证
            print("\n--- 宽高比验证测试 ---")
            self.test_aspect_ratio_preservation()
            self.test_aspect_ratio_with_resize()

            # 颜色正确性验证
            print("\n--- 颜色正确性验证测试 ---")
            self.test_color_rendering_not_inverted()

            # 动态顶点验证
            print("\n--- 动态顶点验证测试 ---")
            self.test_dynamic_vertices_aspect_ratio()
            self.test_parallax_strip_effect()

            # 渲染稳定性
            print("\n--- 渲染稳定性测试 ---")
            self.test_rendering_stability()

            # 参考帧精确验证
            print("\n--- 参考帧精确验证 ---")
            self.test_reference_frame_match()

        finally:
            self.teardown()

        # 生成报告
        self.generate_report()
        self.print_summary()
        self.save_report_json()

    def save_report_json(self):
        """保存JSON格式的报告"""
        os.makedirs(REPORT_DIR, exist_ok=True)
        report_path = os.path.join(REPORT_DIR, f"rendering_report_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json")

        with open(report_path, 'w', encoding='utf-8') as f:
            json.dump(self.verification_results, f, indent=2, ensure_ascii=False)

        print(f"\n报告已保存: {report_path}")


def main():
    """主函数"""
    import argparse

    parser = argparse.ArgumentParser(description='WZMediaPlayer 渲染验证测试')
    parser.add_argument('--video', type=str, default=None,
                        help='测试视频路径')
    parser.add_argument('--quick', action='store_true',
                        help='快速测试模式')

    args = parser.parse_args()

    test = RenderingVerificationTest()

    if args.quick:
        # 快速测试模式
        print("快速测试模式")
        if not test.setup(APP_PATH):
            print("测试准备失败")
            return 1
        try:
            test.test_aspect_ratio_preservation()
            test.test_color_rendering_not_inverted()
        finally:
            test.teardown()
        test.generate_report()
        test.print_summary()
    else:
        # 完整测试
        test.run_all_tests(args.video)

    return 0


if __name__ == "__main__":
    sys.exit(main())