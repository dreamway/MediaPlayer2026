"""
WZMediaPlayer 闭环自动化测试入口

使用方法:
    python run_closed_loop_tests.py [选项]

选项:
    --exe-path PATH       播放器可执行文件路径
    --video-path PATH     测试视频路径
    --list-controls       列出所有识别的UI控件
    --inspect-ui          启动UI检查工具
    --test-case CASE      运行指定测试用例
    --all                 运行所有测试（默认）
    --help                显示帮助信息

示例:
    python run_closed_loop_tests.py
    python run_closed_loop_tests.py --exe-path "C:\\Path\\To\\Player.exe"
    python run_closed_loop_tests.py --list-controls
    python run_closed_loop_tests.py --test-case test_open_video_file
"""

import os
import sys
import argparse

# 确保可以导入core模块
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from core import QtUIParser, UIAutomationController


def get_default_paths():
    """获取默认路径"""
    # 尝试从config.py加载
    try:
        import config
        return config.PLAYER_EXE_PATH, config.TEST_VIDEO_PATH
    except ImportError:
        pass
    
    # 默认路径（与 run_all_tests.py 一致：build/Release）
    base_dir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    exe_path = os.path.join(base_dir, 'build', 'Release', 'WZMediaPlayer.exe')
    video_path = r"D:\BaiduNetdiskDownload\test.mp4"
    
    return exe_path, video_path


def list_ui_controls(project_root: str):
    """列出所有UI控件"""
    print("=" * 80)
    print("UI控件识别工具")
    print("=" * 80)
    
    parser = QtUIParser(project_root)
    
    # 尝试解析UI头文件
    ui_header_paths = [
        os.path.join(project_root, 'WZMediaPlay', 'x64', 'Release', 'qt', 'uic', 'ui_MainWindow.h'),
        os.path.join(project_root, 'WZMediaPlay', 'x64', 'Debug', 'qt', 'uic', 'ui_MainWindow.h'),
        os.path.join(project_root, 'x64', 'Release', 'qt', 'uic', 'ui_MainWindow.h'),
        os.path.join(project_root, 'x64', 'Debug', 'qt', 'uic', 'ui_MainWindow.h'),
        # 直接使用绝对路径
        r'E:\WZMediaPlayer_2025\WZMediaPlay\x64\Release\qt\uic\ui_MainWindow.h',
        r'E:\WZMediaPlayer_2025\WZMediaPlay\x64\Debug\qt\uic\ui_MainWindow.h',
    ]
    
    for header_path in ui_header_paths:
        if os.path.exists(header_path):
            print(f"\n解析UI文件: {header_path}")
            parser.parse_ui_header(header_path)
            break
    else:
        print("\n[警告] 未找到UI头文件")
        return
    
    # 打印控件摘要
    parser.print_control_summary()
    
    # 打印控件映射表
    print("\n" + "=" * 80)
    print("控件映射表（用于自动化测试）:")
    print("=" * 80)
    control_map = parser.get_playback_controls()
    for name, automation_id in control_map.items():
        print(f"  {name}: {automation_id}")
    
    print("\n" + "=" * 80)
    print("快捷键映射:")
    print("=" * 80)
    shortcut_map = parser.get_shortcut_map()
    for name, keys in shortcut_map.items():
        print(f"  {name}: {keys}")


def inspect_running_ui(exe_path: str):
    """检查运行中的应用UI"""
    print("=" * 80)
    print("UI检查工具 - 连接到运行中的应用")
    print("=" * 80)
    
    controller = UIAutomationController(backend="uia")
    
    # 尝试连接
    print(f"\n尝试连接到应用...")
    
    # 先尝试通过窗口标题连接（使用更通用的标题）
    connected = False
    for title_pattern in [".*MainWindow.*", ".*WZMediaPlay.*", ".*MediaPlayer.*"]:
        if controller.connect_to_application(title=title_pattern):
            print(f"✓ 已连接到应用 (匹配: {title_pattern})")
            connected = True
            break
    
    if not connected:
        # 尝试启动
        print(f"尝试启动应用: {exe_path}")
        if not controller.start_application(exe_path):
            print("✗ 无法启动或连接应用")
            print("\n提示: 如果应用已经在运行，请关闭后重试")
            return
    
    print("\n" + "=" * 80)
    print("控件树结构:")
    print("=" * 80)
    controller.print_control_tree()
    
    print("\n" + "=" * 80)
    print("尝试查找关键控件:")
    print("=" * 80)
    
    key_controls = [
        ('pushButton_playPause', 'Button', '播放/暂停按钮'),
        ('pushButton_stop', 'Button', '停止按钮'),
        ('pushButton_open', 'Button', '打开按钮'),
        ('horizontalSlider_playProgress', 'Slider', '进度条'),
        ('horizontalSlider_volume', 'Slider', '音量滑块'),
        ('label_playTime', 'Text', '当前时间标签'),
        ('comboBox_src2D_3D2D', 'ComboBox', '2D/3D切换'),
    ]
    
    for auto_id, ctrl_type, description in key_controls:
        control = controller.get_control(automation_id=auto_id, control_type=ctrl_type)
        if control and control.exists():
            try:
                text = control.window_text()
                enabled = control.is_enabled()
                print(f"  ✓ {description}: {auto_id}")
                print(f"    文本: '{text}', 可用: {enabled}")
            except Exception as e:
                print(f"  ✓ {description}: {auto_id} (获取属性失败: {e})")
        else:
            print(f"  ✗ {description}: {auto_id} (未找到)")
    
    input("\n按Enter键关闭应用...")
    controller.close_application()


def run_specific_test(exe_path: str, video_path: str, test_case: str):
    """运行指定测试用例"""
    from tests.closed_loop_tests import WZMediaPlayerClosedLoopTests
    
    tester = WZMediaPlayerClosedLoopTests(exe_path, video_path)
    
    # 准备
    if not tester.setup():
        print("[错误] 测试准备失败")
        return 1
    
    try:
        # 运行指定测试
        if hasattr(tester, test_case):
            result = getattr(tester, test_case)()
            print(f"\n测试结果: {'通过' if result else '失败'}")
        else:
            print(f"[错误] 未知测试用例: {test_case}")
            print(f"可用测试用例:")
            for name in dir(tester):
                if name.startswith('test_'):
                    print(f"  - {name}")
    finally:
        tester.teardown()
        report = tester.generate_report()
        print("\n" + report)
        tester.save_report(report)
    
    return 0


def run_all_tests(exe_path: str, video_path: str):
    """运行所有测试"""
    from tests.closed_loop_tests import WZMediaPlayerClosedLoopTests
    
    tester = WZMediaPlayerClosedLoopTests(exe_path, video_path)
    success = tester.run_all_tests()
    
    return 0 if success else 1


def main():
    """主函数"""
    parser = argparse.ArgumentParser(
        description='WZMediaPlayer 闭环自动化测试',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    
    parser.add_argument('--exe-path', type=str, help='播放器可执行文件路径')
    parser.add_argument('--video-path', type=str, help='测试视频路径')
    parser.add_argument('--list-controls', action='store_true', help='列出所有UI控件')
    parser.add_argument('--inspect-ui', action='store_true', help='启动UI检查工具')
    parser.add_argument('--test-case', type=str, help='运行指定测试用例')
    parser.add_argument('--all', action='store_true', help='运行所有测试（默认）')
    
    args = parser.parse_args()
    
    # 获取默认路径
    default_exe, default_video = get_default_paths()
    exe_path = args.exe_path or default_exe
    video_path = args.video_path or default_video
    
    # 获取项目根目录
    project_root = os.path.dirname(os.path.dirname(exe_path))
    
    # 处理命令
    if args.list_controls:
        list_ui_controls(project_root)
        return 0
    
    if args.inspect_ui:
        inspect_running_ui(exe_path)
        return 0
    
    if args.test_case:
        return run_specific_test(exe_path, video_path, args.test_case)
    
    # 默认运行所有测试
    return run_all_tests(exe_path, video_path)


if __name__ == "__main__":
    sys.exit(main())
