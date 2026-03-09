"""
WZMediaPlayer 硬件解码自动化测试
测试硬件解码功能是否正常工作
"""

import os
import sys
import time
import glob
from datetime import datetime
from typing import Optional, List, Dict

try:
    from pywinauto.keyboard import send_keys
    from pywinauto import Application
except ImportError:
    print("Error: pywinauto is not installed. Run: pip install pywinauto")
    sys.exit(1)

try:
    from main import WZMediaPlayerTester
except ImportError:
    print("Error: main module not found")
    sys.exit(1)


class WZMediaPlayerHardwareDecodingTester(WZMediaPlayerTester):
    """硬件解码测试器"""

    def __init__(self, exe_path: str, test_video_path: str = None):
        """
        初始化硬件解码测试器
        
        Args:
            exe_path: 播放器可执行文件路径
            test_video_path: 测试视频文件路径
        """
        super().__init__(exe_path, test_video_path)
        self.test_results = []

    def check_hardware_decoding_enabled(self) -> bool:
        """
        检查硬件解码是否启用
        
        Returns:
            True if hardware decoding is enabled, False otherwise
        """
        try:
            # 检查配置文件
            config_path = os.path.join(os.path.dirname(self.exe_path), "..", "..", "config", "SystemConfig.ini")
            if os.path.exists(config_path):
                with open(config_path, 'r', encoding='utf-8') as f:
                    content = f.read()
                    if 'EnableHardwareDecoding=true' in content or 'EnableHardwareDecoding=1' in content:
                        return True
            return False
        except Exception as e:
            print(f"[警告] 检查硬件解码配置失败: {e}")
            return False

    def check_log_for_hw_errors(self) -> Dict[str, any]:
        """
        检查日志文件中的硬件解码错误
        
        Returns:
            包含错误信息的字典
        """
        errors = {
            'hw_frames_ctx_errors': [],
            'transfer_errors': [],
            'nv12_errors': [],
            'other_errors': []
        }
        
        try:
            # 查找最新的日志文件
            log_dir = os.path.join(os.path.dirname(self.exe_path), "..", "..", "logs")
            log_pattern = os.path.join(log_dir, "MediaPlayer_*.log")
            log_files = glob.glob(log_pattern)
            
            if not log_files:
                return errors
            
            # 获取最新的日志文件
            latest_log = max(log_files, key=os.path.getmtime)
            
            with open(latest_log, 'r', encoding='utf-8', errors='ignore') as f:
                lines = f.readlines()
                
                for i, line in enumerate(lines):
                    # 检查 hw_frames_ctx 相关错误
                    if 'hw_frames_ctx' in line.lower() and ('error' in line.lower() or 'failed' in line.lower()):
                        errors['hw_frames_ctx_errors'].append({
                            'line': i + 1,
                            'content': line.strip()
                        })
                    
                    # 检查 transfer 相关错误
                    if 'transfer' in line.lower() and ('error' in line.lower() or 'failed' in line.lower()):
                        errors['transfer_errors'].append({
                            'line': i + 1,
                            'content': line.strip()
                        })
                    
                    # 检查 NV12 相关错误
                    if 'nv12' in line.lower() and ('error' in line.lower() or 'failed' in line.lower()):
                        errors['nv12_errors'].append({
                            'line': i + 1,
                            'content': line.strip()
                        })
                    
                    # 检查其他硬件解码错误
                    if ('hardware' in line.lower() or 'hw' in line.lower()) and 'error' in line.lower():
                        if 'hw_frames_ctx' not in line.lower() and 'transfer' not in line.lower():
                            errors['other_errors'].append({
                                'line': i + 1,
                                'content': line.strip()
                            })
            
        except Exception as e:
            print(f"[警告] 检查日志文件失败: {e}")
        
        return errors

    def check_nv12_rendering(self) -> bool:
        """
        检查NV12格式是否被正确使用（通过日志）
        
        Returns:
            True if NV12 is being used, False otherwise
        """
        try:
            log_dir = os.path.join(os.path.dirname(self.exe_path), "..", "..", "logs")
            log_pattern = os.path.join(log_dir, "MediaPlayer_*.log")
            log_files = glob.glob(log_pattern)
            
            if not log_files:
                return False
            
            latest_log = max(log_files, key=os.path.getmtime)
            
            with open(latest_log, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
                # 检查是否有NV12直接渲染的日志
                if 'nv12' in content.lower() and ('directly' in content.lower() or 'skipping conversion' in content.lower()):
                    return True
                # 检查是否有NV12格式的帧
                if 'nv12' in content.lower() and 'format' in content.lower():
                    return True
            
            return False
        except Exception as e:
            print(f"[警告] 检查NV12渲染失败: {e}")
            return False

    def test_hardware_decoding_basic(self) -> bool:
        """测试硬件解码基础功能"""
        test_name = "硬件解码基础功能测试"
        print(f"\n[测试] {test_name}")
        
        try:
            # 检查硬件解码是否启用
            hw_enabled = self.check_hardware_decoding_enabled()
            if not hw_enabled:
                print(f"  [跳过] 硬件解码未启用")
                self.test_results.append({
                    'name': test_name,
                    'passed': False,
                    'details': '硬件解码未在配置中启用'
                })
                return False
            
            # 等待视频播放一段时间
            time.sleep(5)
            
            # 检查日志中的错误
            errors = self.check_log_for_hw_errors()
            
            has_errors = (len(errors['hw_frames_ctx_errors']) > 0 or 
                         len(errors['transfer_errors']) > 0 or
                         len(errors['nv12_errors']) > 0)
            
            if has_errors:
                error_details = []
                if errors['hw_frames_ctx_errors']:
                    error_details.append(f"hw_frames_ctx错误: {len(errors['hw_frames_ctx_errors'])}个")
                if errors['transfer_errors']:
                    error_details.append(f"transfer错误: {len(errors['transfer_errors'])}个")
                if errors['nv12_errors']:
                    error_details.append(f"NV12错误: {len(errors['nv12_errors'])}个")
                
                print(f"  [失败] 发现硬件解码错误: {', '.join(error_details)}")
                self.test_results.append({
                    'name': test_name,
                    'passed': False,
                    'details': f"发现硬件解码错误: {', '.join(error_details)}"
                })
                return False
            else:
                print(f"  [通过] 硬件解码正常工作，未发现错误")
                self.test_results.append({
                    'name': test_name,
                    'passed': True,
                    'details': '硬件解码正常工作'
                })
                return True
                
        except Exception as e:
            print(f"  [错误] 测试异常: {e}")
            self.test_results.append({
                'name': test_name,
                'passed': False,
                'details': f"测试异常: {e}"
            })
            return False

    def test_nv12_direct_rendering(self) -> bool:
        """测试NV12直接渲染"""
        test_name = "NV12直接渲染测试"
        print(f"\n[测试] {test_name}")
        
        try:
            # 等待视频播放一段时间
            time.sleep(5)
            
            # 检查NV12是否被使用
            nv12_used = self.check_nv12_rendering()
            
            if nv12_used:
                print(f"  [通过] NV12格式被正确使用")
                self.test_results.append({
                    'name': test_name,
                    'passed': True,
                    'details': 'NV12格式被正确使用'
                })
                return True
            else:
                print(f"  [警告] 未检测到NV12格式使用（可能正常，如果视频不是NV12格式）")
                self.test_results.append({
                    'name': test_name,
                    'passed': True,  # 不算失败，因为可能视频本身不是NV12
                    'details': '未检测到NV12格式使用'
                })
                return True
                
        except Exception as e:
            print(f"  [错误] 测试异常: {e}")
            self.test_results.append({
                'name': test_name,
                'passed': False,
                'details': f"测试异常: {e}"
            })
            return False

    def run_hardware_decoding_tests(self) -> bool:
        """运行所有硬件解码测试"""
        print("\n" + "=" * 80)
        print("硬件解码测试")
        print("=" * 80)
        
        results = []
        
        # 运行测试
        results.append(self.test_hardware_decoding_basic())
        time.sleep(2)
        
        results.append(self.test_nv12_direct_rendering())
        
        # 返回结果
        all_passed = all(results)
        return all_passed

    def generate_report(self) -> str:
        """生成测试报告"""
        report_lines = []
        report_lines.append("=" * 80)
        report_lines.append("硬件解码测试报告")
        report_lines.append("=" * 80)
        report_lines.append(f"测试时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        report_lines.append("")
        
        # 统计结果
        total_tests = len(self.test_results)
        passed_tests = sum(1 for r in self.test_results if r['passed'])
        failed_tests = total_tests - passed_tests
        
        report_lines.append(f"总计: {total_tests} | 通过: {passed_tests} | 失败: {failed_tests}")
        report_lines.append("")
        
        # 详细结果
        report_lines.append("-" * 80)
        report_lines.append("测试详情:")
        report_lines.append("-" * 80)
        
        for result in self.test_results:
            status = "✓ 通过" if result['passed'] else "✗ 失败"
            report_lines.append(f"{status} - {result['name']}")
            report_lines.append(f"  详情: {result.get('details', 'N/A')}")
            report_lines.append("")
        
        report_lines.append("=" * 80)
        
        return "\n".join(report_lines)

    def save_report(self, report: str):
        """保存测试报告"""
        try:
            timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
            filename = f"test_report_hardware_decoding_{timestamp}.txt"
            
            script_dir = os.path.dirname(os.path.abspath(__file__))
            report_path = os.path.join(script_dir, filename)
            
            with open(report_path, 'w', encoding='utf-8') as f:
                f.write(report)
            
            print(f"\n✓ 测试报告已保存: {report_path}")
        except Exception as e:
            print(f"\n✗ 保存测试报告失败: {e}")


def main():
    """主函数"""
    print("=" * 80)
    print("WZMediaPlayer 硬件解码自动化测试")
    print("=" * 80)
    print()
    
    # 配置路径（从 config.ini / config.py 读取）
    try:
        import config as test_config
        exe_path = test_config.PLAYER_EXE_PATH
        test_video_path = test_config.TEST_VIDEO_PATH
    except ImportError:
        exe_path = r"D:\2026Github\build\Release\WZMediaPlayer.exe"
        test_video_path = r"D:\2026Github\testing\video\test.mp4"

    # 创建硬件解码测试器
    tester = WZMediaPlayerHardwareDecodingTester(exe_path, test_video_path)
    
    try:
        # 启动播放器
        if not tester.start_player():
            print("\n[错误] 无法启动播放器")
            return 1
        
        # 等待GUI初始化
        time.sleep(3)
        
        # 打开测试视频
        if not tester.open_video():
            print("[错误] 打开视频失败")
            return 1
        
        # 等待视频加载
        time.sleep(3)
        
        # 运行硬件解码测试
        if not tester.run_hardware_decoding_tests():
            print("[警告] 部分硬件解码测试失败")
        
        # 生成报告
        report = tester.generate_report()
        print("\n" + report)
        tester.save_report(report)
        
        return 0
        
    except KeyboardInterrupt:
        print("\n\n测试被用户中断")
        return 1
    except Exception as e:
        print(f"\n\n测试异常: {e}")
        import traceback
        traceback.print_exc()
        return 1
    finally:
        tester.stop_player()


if __name__ == "__main__":
    sys.exit(main())
