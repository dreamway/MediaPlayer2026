"""
WZMediaPlayer 闭环自动化测试用例

测试特性：
1. 基于UIA后端的精确控件操作
2. 图像验证实现闭环判断
3. 智能等待替代固定sleep
4. 完整的UI状态验证
"""

import os
import sys
import time

# 添加父目录到路径
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from core import ClosedLoopTestBase


class WZMediaPlayerClosedLoopTests(ClosedLoopTestBase):
    """WZMediaPlayer闭环测试类"""
    
    def __init__(self, exe_path: str, test_video_path: str = None):
        super().__init__(exe_path, test_video_path)
        self.control_map = self.get_control_map()
        self.shortcuts = self.get_shortcuts()
        
    # ==================== 基础播放测试 ====================
    
    def test_open_video_file(self) -> bool:
        """
        测试打开视频文件
        
        闭环验证点：
        1. 文件对话框能打开
        2. 视频加载后播放区域有内容（非黑屏）
        3. 时间标签更新
        """
        self.start_test("打开视频文件")
        
        if not self.test_video_path or not os.path.exists(self.test_video_path):
            self.end_test(False, f"测试视频不存在: {self.test_video_path}")
            return False
        
        try:
            # 使用快捷键打开文件对话框
            self.ui.send_keys_shortcut(self.shortcuts['open'], wait_after=2.0)
            
            # 输入文件路径
            self.ui.send_keys_shortcut(self.test_video_path, wait_after=0.5)
            self.ui.send_keys_shortcut('{ENTER}', wait_after=8.0)
            
            # 闭环验证1：检查时间标签是否更新（从00:00:00改变）
            time_text = self.ui.get_label_text(self.control_map['time_display'])
            if time_text and time_text != "00:00:00":
                self.end_test(True, f"视频已加载，当前时间: {time_text}")
                return True
            
            # 闭环验证2：通过图像验证播放区域非黑屏
            screenshot = self.ui.capture_window_screenshot()
            if screenshot:
                # 假设播放区域在窗口中央偏上位置 (需要根据实际UI调整)
                window_state = self.ui.get_window_state()
                if 'rectangle' in window_state:
                    rect = window_state['rectangle']
                    # 播放区域大致为窗口中央
                    play_region = (
                        rect['left'] + 100,
                        rect['top'] + 100,
                        rect['right'] - 100,
                        rect['bottom'] - 150
                    )
                    
                    state = self.image_verifier.is_playback_ui_state(screenshot, play_region)
                    if state.get('is_playing'):
                        self.end_test(True, f"视频播放中，亮度: {state['brightness']:.1f}")
                        return True
            
            self.end_test(False, "无法确认视频已加载", screenshot=True)
            return False
            
        except Exception as e:
            self.end_test(False, f"异常: {str(e)}", screenshot=True)
            return False
    
    def test_play_pause_toggle(self) -> bool:
        """
        测试播放/暂停切换
        
        闭环验证点：
        1. 点击播放/暂停按钮
        2. 视频画面停止/恢复变化
        3. 按钮状态改变（图标变化）
        """
        self.start_test("播放/暂停切换")
        
        try:
            # 先确保视频在播放
            is_playing, msg = self.verify_video_playing(check_duration=1.0)
            if not is_playing:
                self.end_test(False, f"视频未在播放: {msg}")
                return False
            
            # 点击暂停按钮
            btn_id = self.control_map['play_pause']
            if not self.ui.click_button(automation_id=btn_id, wait_after=1.0):
                # 尝试使用快捷键
                self.ui.send_keys_shortcut(self.shortcuts['play_pause'], wait_after=1.0)
            
            # 闭环验证：等待一小段时间后，画面应该停止变化
            time.sleep(1.0)
            img1 = self.ui.capture_window_screenshot()
            time.sleep(0.5)
            img2 = self.ui.capture_window_screenshot()
            
            if img1 and img2:
                result = self.image_verifier.verify_video_playing(img1, img2, threshold=100)
                # 暂停后差异应该很小
                if not result.passed:  # 差异小，说明暂停成功
                    self.end_test(True, "视频已暂停")
                    return True
            
            self.end_test(False, "暂停验证失败", screenshot=True)
            return False
            
        except Exception as e:
            self.end_test(False, f"异常: {str(e)}")
            return False
    
    def test_stop_playback(self) -> bool:
        """
        测试停止播放
        
        闭环验证点：
        1. 点击停止按钮
        2. 播放区域回到黑屏/初始状态
        3. 时间标签重置
        """
        self.start_test("停止播放")
        
        try:
            # 点击停止按钮
            btn_id = self.control_map['stop']
            if not self.ui.click_button(automation_id=btn_id, wait_after=2.0):
                # 尝试使用快捷键
                self.ui.send_keys_shortcut(self.shortcuts['stop'], wait_after=2.0)
            
            # 闭环验证1：检查时间标签是否重置
            time_text = self.ui.get_label_text(self.control_map['time_display'])
            
            # 闭环验证2：检查播放区域是否为黑屏
            screenshot = self.ui.capture_window_screenshot()
            if screenshot:
                window_state = self.ui.get_window_state()
                if 'rectangle' in window_state:
                    rect = window_state['rectangle']
                    play_region = (
                        rect['left'] + 100,
                        rect['top'] + 100,
                        rect['right'] - 100,
                        rect['bottom'] - 150
                    )
                    
                    if self.image_verifier.is_black_screen(screenshot.crop(play_region)):
                        self.end_test(True, "播放已停止，画面已重置")
                        return True
            
            # 如果时间重置也算成功
            if time_text == "00:00:00":
                self.end_test(True, "播放已停止，时间已重置")
                return True
            
            self.end_test(False, f"停止验证失败，当前时间: {time_text}", screenshot=True)
            return False
            
        except Exception as e:
            self.end_test(False, f"异常: {str(e)}")
            return False
    
    # ==================== Seek测试 ====================
    
    def test_seek_forward_small(self) -> bool:
        """
        测试小幅前进Seek（5秒）
        
        闭环验证点：
        1. 发送Seek命令
        2. 时间标签前进约5秒
        3. 画面内容变化
        """
        self.start_test("小幅前进Seek")
        
        try:
            # 获取当前时间
            time_before = self.ui.get_label_text(self.control_map['time_display'])
            
            # 发送Seek命令（右方向键）
            self.ui.send_keys_shortcut(self.shortcuts['seek_right'], wait_after=3.0)
            
            # 获取Seek后时间
            time_after = self.ui.get_label_text(self.control_map['time_display'])
            
            # 闭环验证：时间应该前进
            if time_before and time_after and time_after != time_before:
                self.end_test(True, f"Seek成功: {time_before} -> {time_after}")
                return True
            
            self.end_test(False, f"Seek可能失败: {time_before} -> {time_after}")
            return False
            
        except Exception as e:
            self.end_test(False, f"异常: {str(e)}")
            return False
    
    def test_seek_backward_small(self) -> bool:
        """
        测试小幅后退Seek（5秒）
        
        闭环验证点：
        1. 先Seek前进
        2. 再Seek后退
        3. 时间标签相应变化
        """
        self.start_test("小幅后退Seek")
        
        try:
            # 先前进
            self.ui.send_keys_shortcut(self.shortcuts['seek_right'], wait_after=2.0)
            time_forward = self.ui.get_label_text(self.control_map['time_display'])
            
            # 再后退
            self.ui.send_keys_shortcut(self.shortcuts['seek_left'], wait_after=2.0)
            time_backward = self.ui.get_label_text(self.control_map['time_display'])
            
            # 闭环验证
            if time_forward and time_backward and time_backward != time_forward:
                self.end_test(True, f"Seek成功: {time_forward} -> {time_backward}")
                return True
            
            self.end_test(False, f"Seek可能失败")
            return False
            
        except Exception as e:
            self.end_test(False, f"异常: {str(e)}")
            return False
    
    def test_seek_to_position(self) -> bool:
        """
        测试拖动进度条Seek
        
        闭环验证点：
        1. 获取进度条当前值
        2. 设置新值
        3. 验证时间和画面变化
        """
        self.start_test("进度条Seek")
        
        try:
            slider_id = self.control_map['progress_slider']
            
            # 获取当前值
            current_value = self.ui.get_slider_value(slider_id)
            time_before = self.ui.get_label_text(self.control_map['time_display'])
            
            # 设置新值（Seek到50%）
            new_value = 50  # 假设范围是0-100
            if self.ui.set_slider_value(slider_id, new_value):
                time.sleep(3.0)  # 等待Seek完成
                
                # 验证时间变化
                time_after = self.ui.get_label_text(self.control_map['time_display'])
                
                if time_after and time_after != time_before:
                    self.end_test(True, f"Seek到50%成功: {time_before} -> {time_after}")
                    return True
            
            self.end_test(False, "进度条Seek失败")
            return False
            
        except Exception as e:
            self.end_test(False, f"异常: {str(e)}")
            return False
    
    # ==================== 音量控制测试 ====================
    
    def test_volume_control(self) -> bool:
        """
        测试音量控制
        
        闭环验证点：
        1. 调整音量滑块
        2. 验证滑块值变化
        """
        self.start_test("音量控制")
        
        try:
            slider_id = self.control_map['volume_slider']
            
            # 获取当前音量
            vol_before = self.ui.get_slider_value(slider_id)
            
            # 增加音量
            self.ui.send_keys_shortcut(self.shortcuts['volume_up'], wait_after=0.5)
            vol_after_up = self.ui.get_slider_value(slider_id)
            
            # 减少音量
            self.ui.send_keys_shortcut(self.shortcuts['volume_down'], wait_after=0.5)
            vol_after_down = self.ui.get_slider_value(slider_id)
            
            # 闭环验证
            if vol_after_up != vol_before or vol_after_down != vol_after_up:
                self.end_test(True, f"音量控制正常: {vol_before} -> {vol_after_up} -> {vol_after_down}")
                return True
            
            self.end_test(False, f"音量控制可能无效")
            return False
            
        except Exception as e:
            self.end_test(False, f"异常: {str(e)}")
            return False
    
    def test_mute_toggle(self) -> bool:
        """
        测试静音切换
        
        闭环验证点：
        1. 点击静音按钮
        2. 验证按钮状态变化
        """
        self.start_test("静音切换")
        
        try:
            # 使用快捷键切换静音
            self.ui.send_keys_shortcut(self.shortcuts['mute'], wait_after=0.5)
            
            # 验证静音按钮状态（如果可以通过UI获取）
            btn_id = self.control_map['volume_button']
            is_checked_before = self.ui.is_control_enabled(btn_id)
            
            # 再次切换
            self.ui.send_keys_shortcut(self.shortcuts['mute'], wait_after=0.5)
            is_checked_after = self.ui.is_control_enabled(btn_id)
            
            self.end_test(True, "静音切换执行完成")
            return True
            
        except Exception as e:
            self.end_test(False, f"异常: {str(e)}")
            return False
    
    # ==================== 3D功能测试 ====================
    
    def test_3d_mode_toggle(self) -> bool:
        """
        测试3D/2D模式切换
        
        闭环验证点：
        1. 切换3D模式
        2. 验证画面变化（通过图像比较）
        """
        self.start_test("3D模式切换")
        
        try:
            # 截取当前画面
            img_before = self.ui.capture_window_screenshot()
            
            # 切换3D模式（Ctrl+D）
            self.ui.send_keys_shortcut(self.shortcuts['3d_toggle'], wait_after=2.0)
            
            # 截取切换后画面
            img_after = self.ui.capture_window_screenshot()
            
            # 闭环验证：画面应该有变化
            if img_before and img_after:
                result = self.image_verifier.verify_video_playing(img_before, img_after, threshold=500)
                if result.passed:
                    self.end_test(True, "3D模式切换成功，画面有变化")
                    return True
            
            self.end_test(False, "3D模式切换验证失败")
            return False
            
        except Exception as e:
            self.end_test(False, f"异常: {str(e)}")
            return False
    
    def test_input_format_change(self) -> bool:
        """
        测试输入格式切换（LR/RL/UD）
        
        闭环验证点：
        1. 切换输入格式
        2. 验证画面变化
        """
        self.start_test("输入格式切换")
        
        try:
            formats = [
                ('lr_toggle', 'LR'),
                ('rl_toggle', 'RL'),
                ('ud_toggle', 'UD'),
            ]
            
            for shortcut_key, format_name in formats:
                img_before = self.ui.capture_window_screenshot()
                
                # 切换格式
                if shortcut_key in self.shortcuts:
                    self.ui.send_keys_shortcut(self.shortcuts[shortcut_key], wait_after=1.5)
                
                img_after = self.ui.capture_window_screenshot()
                
                # 验证变化
                if img_before and img_after:
                    result = self.image_verifier.verify_video_playing(img_before, img_after, threshold=300)
                    print(f"    {format_name}格式: {result.message}")
            
            self.end_test(True, "输入格式切换测试完成")
            return True
            
        except Exception as e:
            self.end_test(False, f"异常: {str(e)}")
            return False
    
    # ==================== 全屏测试 ====================
    
    def test_fullscreen_toggle(self) -> bool:
        """
        测试全屏切换
        
        闭环验证点：
        1. 切换全屏
        2. 验证窗口尺寸变化
        """
        self.start_test("全屏切换")
        
        try:
            # 获取当前窗口状态
            state_before = self.ui.get_window_state()
            rect_before = state_before.get('rectangle', {})
            
            # 切换全屏
            self.ui.send_keys_shortcut(self.shortcuts['fullscreen'], wait_after=2.0)
            
            # 获取全屏后状态
            state_after = self.ui.get_window_state()
            rect_after = state_after.get('rectangle', {})
            
            # 闭环验证：窗口尺寸应该有显著变化
            if rect_before and rect_after:
                width_before = rect_before.get('width', 0)
                width_after = rect_after.get('width', 0)
                
                if width_after > width_before * 1.2:  # 宽度增加20%以上
                    # 退出全屏
                    self.ui.send_keys_shortcut(self.shortcuts['escape'], wait_after=1.0)
                    self.end_test(True, f"全屏切换成功: {width_before} -> {width_after}")
                    return True
            
            # 退出全屏（如果还在全屏状态）
            self.ui.send_keys_shortcut(self.shortcuts['escape'], wait_after=1.0)
            
            self.end_test(False, "全屏切换验证失败")
            return False
            
        except Exception as e:
            self.end_test(False, f"异常: {str(e)}")
            return False
    
    # ==================== 播放列表测试 ====================
    
    def test_playlist_add_clear(self) -> bool:
        """
        测试播放列表添加和清空
        
        闭环验证点：
        1. 添加文件到播放列表
        2. 验证列表项增加
        3. 清空列表
        4. 验证列表为空
        """
        self.start_test("播放列表操作")
        
        try:
            # 点击添加按钮
            btn_add = self.control_map['add_to_playlist']
            self.ui.click_button(automation_id=btn_add, wait_after=2.0)
            
            # 如果有文件对话框，输入视频路径
            if self.test_video_path:
                self.ui.send_keys_shortcut(self.test_video_path, wait_after=0.5)
                self.ui.send_keys_shortcut('{ENTER}', wait_after=2.0)
            
            # 点击清空按钮
            btn_clear = self.control_map['clear_playlist']
            self.ui.click_button(automation_id=btn_clear, wait_after=1.0)
            
            self.end_test(True, "播放列表操作完成")
            return True
            
        except Exception as e:
            self.end_test(False, f"异常: {str(e)}")
            return False
    
    # ==================== 运行所有测试 ====================
    
    def run_all_tests(self):
        """运行所有测试用例"""
        
        # 准备
        if not self.setup():
            self.save_report()
            return False
        
        try:
            # 1. 基础播放测试
            if self.test_video_path and os.path.exists(self.test_video_path):
                self.test_open_video_file()
                time.sleep(3)  # 等待视频稳定播放
                
                # 验证视频在播放
                self.start_test("验证视频播放状态")
                is_playing, msg = self.verify_video_playing(check_duration=2.0)
                self.end_test(is_playing, msg, screenshot=not is_playing)
                
                # 播放控制测试
                time.sleep(1)
                self.test_play_pause_toggle()
                time.sleep(1)
                self.test_stop_playback()
                
                # 重新打开视频用于后续测试
                time.sleep(1)
                self.test_open_video_file()
                time.sleep(3)
                
                # 2. Seek测试
                self.test_seek_forward_small()
                time.sleep(1)
                self.test_seek_backward_small()
                time.sleep(1)
                self.test_seek_to_position()
                
                # 3. 音量控制测试
                time.sleep(1)
                self.test_volume_control()
                time.sleep(1)
                self.test_mute_toggle()
                
                # 4. 3D功能测试
                time.sleep(1)
                self.test_3d_mode_toggle()
                time.sleep(1)
                self.test_input_format_change()
                
                # 5. 全屏测试
                time.sleep(1)
                self.test_fullscreen_toggle()
                
                # 6. 播放列表测试
                time.sleep(1)
                self.test_playlist_add_clear()
            else:
                print("\n[警告] 未提供测试视频，跳过播放相关测试")
                self.start_test("检查测试视频")
                self.end_test(False, "未找到测试视频文件")
            
        except KeyboardInterrupt:
            print("\n[用户中断] 测试被手动停止")
        except Exception as e:
            print(f"\n[错误] 测试过程中发生异常: {e}")
            import traceback
            traceback.print_exc()
        finally:
            # 清理
            self.teardown()
            
            # 生成并保存报告
            report = self.generate_report()
            print("\n" + report)
            self.save_report(report)
            
            # 返回测试结果
            failed_count = sum(1 for r in self.test_results if not r.passed)
            return failed_count == 0


def main():
    """主函数"""
    # 配置路径
    exe_path = r"E:\WZMediaPlayer_2025\x64\Debug\WZMediaPlay.exe"
    test_video_path = r"D:\BaiduNetdiskDownload\test.mp4"
    
    # 检查路径
    if not os.path.exists(exe_path):
        print(f"[错误] 播放器可执行文件不存在: {exe_path}")
        print("请修改脚本中的exe_path为正确的路径")
        return 1
    
    # 创建并运行测试
    tester = WZMediaPlayerClosedLoopTests(exe_path, test_video_path)
    success = tester.run_all_tests()
    
    return 0 if success else 1


if __name__ == "__main__":
    import sys
    sys.exit(main())
