"""
UI检查工具 - 用于识别WZMediaPlayer的UI控件

使用方法：
1. 运行此脚本
2. 脚本会启动播放器并输出所有可识别的控件
3. 根据输出更新config.py中的控件ID
"""

import sys
import time

try:
    from pywinauto.application import Application
except ImportError:
    print("错误: pywinauto未安装. 运行: pip install pywinauto")
    sys.exit(1)


def inspect_ui(exe_path: str):
    """检查UI控件结构"""
    print(f"启动播放器: {exe_path}")

    try:
        app = Application(backend="uia").start(exe_path)
        time.sleep(3)  # 等待启动

        # 获取主窗口
        main_window = app.window()

        if main_window.exists():
            print(f"\n主窗口标题: {main_window.window_text()}")
            print("\n" + "="*80)
            print("控件结构:")
            print("="*80)

            # 打印所有控件
            main_window.print_control_identifiers()

            print("\n" + "="*80)
            print("重要控件信息:")
            print("="*80)

            # 尝试查找关键控件
            key_controls = [
                "pushButton_open",
                "pushButton_playPause",
                "pushButton_stop",
                "pushButton_previous",
                "pushButton_next",
                "horizontalSlider_playProgress",
                "horizontalSlider_volume",
                "switchButton_3D2D",
                "pushButton_fullScreen",
                "comboBox_src2D_3D2D",
                "comboBox_3D_input",
                "pushButton_add",
                "pushButton_clear",
            ]

            for control_id in key_controls:
                try:
                    control = main_window.child_window(auto_id=control_id)
                    if control.exists():
                        print(f"✓ 找到: {control_id} - {control.window_text()}")
                    else:
                        print(f"✗ 未找到: {control_id}")
                except Exception as e:
                    print(f"✗ 查找 {control_id} 失败: {e}")

        else:
            print("错误: 无法找到主窗口")

        # 关闭播放器
        input("\n按Enter键关闭播放器...")
        app.kill()

    except Exception as e:
        print(f"错误: {e}")
        import traceback
        traceback.print_exc()


def main():
    """主函数"""
    # 修改此路径为实际的播放器路径
    exe_path = r"E:\WZMediaPlayer_2025\x64\Debug\WZMediaPlay.exe"

    if not len(sys.argv) > 1:
        print(f"使用方法: {sys.argv[0]} [播放器路径]")
        print(f"默认路径: {exe_path}")
        print()
        answer = input("是否使用默认路径? (y/n): ")
        if answer.lower() != 'y':
            print("\n使用方法:")
            print(f"  {sys.argv[0]} <播放器路径>")
            print()
            return 1
    else:
        exe_path = sys.argv[1]

    inspect_ui(exe_path)


if __name__ == "__main__":
    main()
