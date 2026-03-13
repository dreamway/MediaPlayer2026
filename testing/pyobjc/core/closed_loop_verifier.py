# testing/pyobjc/core/closed_loop_verifier.py
"""
闭环测试验证器

确保每个测试都是闭环的：
1. 发送指令
2. 等待生效
3. 读取 UI 状态验证
4. 截图验证画面
5. 检查日志确认
"""

import time
import os
from typing import Optional, Tuple, Dict, Any
from dataclasses import dataclass


@dataclass
class VerificationResult:
    """验证结果"""
    success: bool
    message: str
    details: Dict[str, Any] = None

    def __post_init__(self):
        if self.details is None:
            self.details = {}


# 参考帧映射（视频名称 -> 参考帧文件名）
REFERENCE_FRAME_MAP = {
    "bbb_sunflower_1080p_30fps_normal.mp4": "reference_bbb_normal_5s.png",
    "bbb_sunflower_1080p_30fps_stereo_abl.mp4": "reference_bbb_stereo_abl_5s.png",
    "wukong_2D3D-40S.mp4": "reference_wukong_2d3d_5s.png",
    "wukong4K-40S.mp4": "reference_wukong_4k_5s.png",
    "Medical3D4k5-2.mp4": "reference_medical_3d_5s.png",
    "test_smpte_640x480_5s.mp4": "reference_smpte_640x480.png",
    "test_testsrc_640x480_5s.mp4": "reference_testsrc_640x480.png",
    "test.mp4": "reference_test_2s.png",
}


class ClosedLoopVerifier:
    """闭环测试验证器"""

    def __init__(self, window_controller, screenshot_capture, log_monitor=None):
        """
        初始化验证器

        Args:
            window_controller: 窗口控制器，用于读取 UI 状态
            screenshot_capture: 截图捕获器，用于画面验证
            log_monitor: 日志监控器，用于日志验证（可选）
        """
        self.window_controller = window_controller
        self.screenshot_capture = screenshot_capture
        self.log_monitor = log_monitor

    # ==================== UI 状态验证 ====================

    def get_current_playback_position(self) -> Tuple[int, int, Dict]:
        """
        获取当前播放位置

        Returns:
            Tuple[int, int, Dict]: (当前秒数, 总秒数, 完整状态)
        """
        if not self.window_controller:
            return -1, -1, {}

        state = self.window_controller.get_playback_ui_state()
        play_sec = state.get("play_time_sec", -1)
        slider_sec = state.get("slider_value", -1)

        # 优先使用时间标签，其次使用进度条
        current_sec = play_sec if play_sec >= 0 else slider_sec
        total_sec = state.get("total_time_sec", -1)

        return current_sec, total_sec, state

    def verify_ui_position_changed(
        self,
        expected_min_change: int = 1,
        timeout_sec: float = 5.0,
        initial_position: int = None
    ) -> VerificationResult:
        """
        验证 UI 位置是否发生变化

        Args:
            expected_min_change: 期望的最小变化量（秒）
            timeout_sec: 超时时间
            initial_position: 初始位置（如果不提供，会自动读取）

        Returns:
            VerificationResult: 验证结果
        """
        # 获取初始位置
        if initial_position is None:
            initial_pos, total, state = self.get_current_playback_position()
        else:
            initial_pos = initial_position

        if initial_pos < 0:
            return VerificationResult(
                False,
                "无法读取初始播放位置",
                {"initial_pos": initial_pos}
            )

        # 等待位置变化
        start_time = time.time()
        while time.time() - start_time < timeout_sec:
            current_pos, total, state = self.get_current_playback_position()

            if current_pos >= 0 and abs(current_pos - initial_pos) >= expected_min_change:
                return VerificationResult(
                    True,
                    f"位置已变化: {initial_pos}s → {current_pos}s",
                    {"initial_pos": initial_pos, "current_pos": current_pos, "total": total}
                )

            time.sleep(0.2)

        # 超时后再次检查
        final_pos, total, state = self.get_current_playback_position()
        return VerificationResult(
            False,
            f"UI 位置未变化或变化不足: 初始={initial_pos}s, 最终={final_pos}s, 期望最小变化={expected_min_change}s",
            {"initial_pos": initial_pos, "final_pos": final_pos, "total": total, "state": state}
        )

    def verify_ui_position_advanced(
        self,
        min_advance: int,
        max_position: int = None,
        timeout_sec: float = 3.0
    ) -> VerificationResult:
        """
        验证 UI 位置是否前进

        Args:
            min_advance: 最小前进量（秒）
            max_position: 最大允许位置（秒），超过则视为异常
            timeout_sec: 超时时间

        Returns:
            VerificationResult: 验证结果
        """
        initial_pos, total, state = self.get_current_playback_position()

        if initial_pos < 0:
            return VerificationResult(
                False,
                "无法读取初始播放位置",
                {"state": state}
            )

        time.sleep(timeout_sec)

        final_pos, _, state_after = self.get_current_playback_position()

        if final_pos < 0:
            return VerificationResult(
                False,
                "无法读取最终播放位置",
                {"initial_pos": initial_pos, "state": state_after}
            )

        # 检查前进量
        advance = final_pos - initial_pos

        if advance < min_advance:
            return VerificationResult(
                False,
                f"播放时间前进不足: {initial_pos}s → {final_pos}s, 前进={advance}s, 期望最小={min_advance}s",
                {"initial_pos": initial_pos, "final_pos": final_pos, "advance": advance}
            )

        # 检查是否超过最大位置
        if max_position is not None and final_pos > max_position:
            return VerificationResult(
                False,
                f"播放位置超过最大值: {final_pos}s > {max_position}s",
                {"initial_pos": initial_pos, "final_pos": final_pos, "max": max_position}
            )

        return VerificationResult(
            True,
            f"播放时间正常前进: {initial_pos}s → {final_pos}s, 前进={advance}s",
            {"initial_pos": initial_pos, "final_pos": final_pos, "advance": advance}
        )

    # ==================== 画面验证 ====================

    def verify_screen_not_black(
        self,
        black_threshold: int = 30,
        black_ratio_threshold: float = 0.95
    ) -> VerificationResult:
        """
        验证画面不是黑屏

        Args:
            black_threshold: 黑色像素阈值
            black_ratio_threshold: 黑色像素比例阈值

        Returns:
            VerificationResult: 验证结果
        """
        from core.screenshot_capture import ImageAnalyzer

        try:
            screenshot_path = self.screenshot_capture.capture_full_screen()

            if not screenshot_path or not os.path.exists(screenshot_path):
                return VerificationResult(
                    False,
                    "截图失败或文件不存在",
                    {}
                )

            is_black, black_ratio, brightness = ImageAnalyzer.is_black_screen(
                screenshot_path, black_threshold
            )

            if is_black:
                return VerificationResult(
                    False,
                    f"检测到黑屏: 黑色像素比例={black_ratio:.2%}, 平均亮度={brightness:.1f}",
                    {"screenshot": screenshot_path, "black_ratio": black_ratio, "brightness": brightness}
                )

            return VerificationResult(
                True,
                f"画面正常: 黑色像素比例={black_ratio:.2%}, 平均亮度={brightness:.1f}",
                {"screenshot": screenshot_path, "black_ratio": black_ratio, "brightness": brightness}
            )

        except Exception as e:
            return VerificationResult(
                False,
                f"画面验证异常: {e}",
                {}
            )

    def verify_rendering_after_action(
        self,
        action_name: str,
        wait_sec: float = 1.0,
        black_threshold: int = 30
    ) -> VerificationResult:
        """
        在执行操作后验证画面

        Args:
            action_name: 操作名称
            wait_sec: 等待时间
            black_threshold: 黑色像素阈值

        Returns:
            VerificationResult: 验证结果
        """
        time.sleep(wait_sec)
        return self.verify_screen_not_black(black_threshold=black_threshold)

    # ==================== 综合验证 ====================

    def verify_seek_result(
        self,
        action_name: str,
        expected_direction: str = "forward",
        min_change_sec: int = 1,
        timeout_sec: float = 5.0
    ) -> VerificationResult:
        """
        验证 Seek 操作结果

        Args:
            action_name: 操作名称
            expected_direction: 期望方向 ("forward" 或 "backward")
            min_change_sec: 最小变化量（秒）
            timeout_sec: 超时时间

        Returns:
            VerificationResult: 验证结果
        """
        # 获取初始位置
        initial_pos, total, initial_state = self.get_current_playback_position()

        if initial_pos < 0:
            return VerificationResult(
                False,
                f"[{action_name}] 无法读取初始播放位置",
                {"initial_state": initial_state}
            )

        # 等待 UI 更新
        time.sleep(timeout_sec)

        # 获取最终位置
        final_pos, _, final_state = self.get_current_playback_position()

        if final_pos < 0:
            return VerificationResult(
                False,
                f"[{action_name}] 无法读取最终播放位置",
                {"initial_pos": initial_pos, "final_state": final_state}
            )

        # 计算变化量
        change = final_pos - initial_pos

        # 验证方向和变化量
        if expected_direction == "forward":
            if change < min_change_sec:
                return VerificationResult(
                    False,
                    f"[{action_name}] 向前 Seek 未生效或变化不足: {initial_pos}s → {final_pos}s, 变化={change}s",
                    {"initial_pos": initial_pos, "final_pos": final_pos, "change": change}
                )
        elif expected_direction == "backward":
            if change > -min_change_sec:
                return VerificationResult(
                    False,
                    f"[{action_name}] 向后 Seek 未生效或变化不足: {initial_pos}s → {final_pos}s, 变化={change}s",
                    {"initial_pos": initial_pos, "final_pos": final_pos, "change": change}
                )

        # 验证画面不是黑屏
        screen_result = self.verify_screen_not_black()
        if not screen_result.success:
            return VerificationResult(
                False,
                f"[{action_name}] Seek 后画面异常: {screen_result.message}",
                {"initial_pos": initial_pos, "final_pos": final_pos, "screen_details": screen_result.details}
            )

        return VerificationResult(
            True,
            f"[{action_name}] Seek 成功: {initial_pos}s → {final_pos}s, 变化={change}s",
            {"initial_pos": initial_pos, "final_pos": final_pos, "change": change, "total": total}
        )

    def verify_playback_progressing(
        self,
        duration_sec: float = 3.0,
        min_advance_sec: int = 1
    ) -> VerificationResult:
        """
        验证播放正在前进

        Args:
            duration_sec: 观察时长
            min_advance_sec: 最小前进量

        Returns:
            VerificationResult: 验证结果
        """
        # 获取初始位置和画面
        initial_pos, total, initial_state = self.get_current_playback_position()

        # 等待一段时间
        time.sleep(duration_sec)

        # 获取最终位置
        final_pos, _, final_state = self.get_current_playback_position()

        if initial_pos < 0 or final_pos < 0:
            return VerificationResult(
                False,
                "无法读取播放位置",
                {"initial_pos": initial_pos, "final_pos": final_pos, "initial_state": initial_state}
            )

        advance = final_pos - initial_pos

        if advance < min_advance_sec:
            return VerificationResult(
                False,
                f"播放未前进或前进不足: {initial_pos}s → {final_pos}s, 前进={advance}s, 期望最小={min_advance_sec}s",
                {"initial_pos": initial_pos, "final_pos": final_pos, "advance": advance}
            )

        # 验证画面
        screen_result = self.verify_screen_not_black()
        if not screen_result.success:
            return VerificationResult(
                False,
                f"播放时画面异常: {screen_result.message}",
                {"initial_pos": initial_pos, "final_pos": final_pos, "screen_details": screen_result.details}
            )

        return VerificationResult(
            True,
            f"播放正常前进: {initial_pos}s → {final_pos}s, 前进={advance}s",
            {"initial_pos": initial_pos, "final_pos": final_pos, "advance": advance}
        )

    def verify_play_pause_state(
        self,
        expected_playing: bool,
        timeout_sec: float = 2.0
    ) -> VerificationResult:
        """
        验证播放/暂停状态

        Args:
            expected_playing: 期望是否正在播放
            timeout_sec: 超时时间

        Returns:
            VerificationResult: 验证结果
        """
        # 读取两次位置来判断是否在播放
        pos1, _, state1 = self.get_current_playback_position()
        time.sleep(timeout_sec)
        pos2, _, state2 = self.get_current_playback_position()

        is_actually_playing = pos2 > pos1

        if is_actually_playing != expected_playing:
            return VerificationResult(
                False,
                f"播放状态不符合预期: 期望={'播放' if expected_playing else '暂停'}, 实际={'播放' if is_actually_playing else '暂停'}",
                {"pos1": pos1, "pos2": pos2, "expected": expected_playing, "actual": is_actually_playing}
            )

        return VerificationResult(
            True,
            f"播放状态正确: {'播放中' if expected_playing else '已暂停'}",
            {"pos1": pos1, "pos2": pos2}
        )

    # ==================== 参考帧验证 ====================

    def verify_with_reference_frame(
        self,
        reference_name: str,
        mse_threshold: float = 500.0
    ) -> VerificationResult:
        """
        使用参考帧验证当前画面

        Args:
            reference_name: 参考帧文件名
            mse_threshold: MSE 阈值

        Returns:
            VerificationResult: 验证结果
        """
        from core.screenshot_capture import ImageAnalyzer
        from config import REFERENCE_FRAME_DIR

        try:
            # 截取当前画面
            screenshot_path = self.screenshot_capture.capture_full_screen()

            if not screenshot_path or not os.path.exists(screenshot_path):
                return VerificationResult(
                    False,
                    "截图失败",
                    {}
                )

            # 查找参考帧
            ref_path = os.path.join(REFERENCE_FRAME_DIR, reference_name)
            if not os.path.exists(ref_path):
                return VerificationResult(
                    False,
                    f"参考帧不存在: {reference_name}",
                    {"available": list(REFERENCE_FRAME_MAP.values())}
                )

            # 比较图像
            result = ImageAnalyzer.compare_to_reference(
                screenshot_path,
                ref_path,
                mse_threshold=mse_threshold
            )

            if result.get("passed", False):
                return VerificationResult(
                    True,
                    f"画面与参考帧匹配: MSE={result.get('mse', 0):.1f}, PSNR={result.get('psnr', 0):.1f}dB",
                    {
                        "screenshot": screenshot_path,
                        "reference": ref_path,
                        "mse": result.get("mse", 0),
                        "psnr": result.get("psnr", 0)
                    }
                )
            else:
                return VerificationResult(
                    False,
                    f"画面与参考帧不匹配: MSE={result.get('mse', 0):.1f} > {mse_threshold}",
                    {
                        "screenshot": screenshot_path,
                        "reference": ref_path,
                        "mse": result.get("mse", 0),
                        "psnr": result.get("psnr", 0)
                    }
                )

        except Exception as e:
            return VerificationResult(
                False,
                f"参考帧验证异常: {e}",
                {}
            )

    def verify_rendering_for_video(
        self,
        video_name: str,
        seek_to_start: bool = True
    ) -> VerificationResult:
        """
        根据视频名称自动选择参考帧进行验证

        Args:
            video_name: 视频文件名
            seek_to_start: 是否先 seek 到开头

        Returns:
            VerificationResult: 验证结果
        """
        from core.keyboard_input import KeyboardInput

        # 查找对应的参考帧
        reference_name = REFERENCE_FRAME_MAP.get(video_name)

        if not reference_name:
            # 尝试模糊匹配
            for key, value in REFERENCE_FRAME_MAP.items():
                if key in video_name or video_name in key:
                    reference_name = value
                    break

        if not reference_name:
            return VerificationResult(
                False,
                f"未找到视频对应的参考帧: {video_name}",
                {"available": list(REFERENCE_FRAME_MAP.keys())}
            )

        # 如果需要，seek 到开头
        if seek_to_start:
            KeyboardInput.focus_app("WZMediaPlayer", delay=0.3)
            KeyboardInput.seek_to_start()
            time.sleep(2.0)

        return self.verify_with_reference_frame(reference_name)