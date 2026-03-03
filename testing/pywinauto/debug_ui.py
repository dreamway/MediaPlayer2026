"""
UI调试脚本 - 检查视频播放前后的UI结构变化
"""

import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from core import UIAutomationController

def find_control_recursive(parent, automation_id, indent=0):
    """递归查找控件"""
    prefix = "  " * indent
    try:
        # 获取当前控件的automation_id
        try:
            current_id = parent.automation_id()
            if current_id == automation_id:
                print(f"{prefix}[FOUND] {parent.control_type()} - auto_id: {current_id}")
                return parent
        except:
            current_id = "N/A"
        
        # 打印当前控件信息
        try:
            ctrl_type = parent.control_type()
            text = parent.window_text()[:20] if parent.window_text() else ""
            visible = parent.is_visible()
            enabled = parent.is_enabled()
            print(f"{prefix}[{ctrl_type}] auto_id: {current_id}, text: '{text}', visible: {visible}, enabled: {enabled}")
        except Exception as e:
            print(f"{prefix}[Error getting info: {e}]")
        
        # 递归查找子控件
        try:
            children = parent.children()
            for child in children:
                result = find_control_recursive(child, automation_id, indent + 1)
                if result:
                    return result
        except Exception as e:
            print(f"{prefix}[Error getting children: {e}]")
            
    except Exception as e:
        print(f"{prefix}[Error] {e}")
    
    return None

def main():
    exe_path = r"E:\WZMediaPlayer_2025\x64\Debug\WZMediaPlay.exe"
    video_path = r"D:\BaiduNetdiskDownload\test.mp4"
    
    ui = UIAutomationController(backend="uia")
    
    # 启动应用
    print("="*80)
    print("启动应用...")
    print("="*80)
    if not ui.start_application(exe_path):
        print("启动失败")
        return
    
    time.sleep(3)
    
    # 检查初始UI结构
    print("\n" + "="*80)
    print("初始UI结构（查找pushButton_playPause）")
    print("="*80)
    control = find_control_recursive(ui.main_window, "pushButton_playPause")
    if control:
        print(f"\n找到控件: {control}")
    else:
        print("\n未找到控件")
    
    # 打开视频
    print("\n" + "="*80)
    print("打开视频...")
    print("="*80)
    ui.send_keys_shortcut('^o', wait_after=2.0)
    ui.send_keys_shortcut(video_path, wait_after=0.5)
    ui.send_keys_shortcut('{ENTER}', wait_after=8.0)
    
    # 检查视频播放后的UI结构
    print("\n" + "="*80)
    print("视频播放后UI结构（查找pushButton_playPause）")
    print("="*80)
    control = find_control_recursive(ui.main_window, "pushButton_playPause")
    if control:
        print(f"\n找到控件: {control}")
        print(f"控件类型: {control.control_type()}")
        print(f"控件可见: {control.is_visible()}")
        print(f"控件可用: {control.is_enabled()}")
    else:
        print("\n未找到控件")
    
    # 尝试使用不同的方法查找
    print("\n" + "="*80)
    print("尝试不同方法查找控件")
    print("="*80)
    
    # 方法1: 使用child_window
    print("\n方法1: child_window(auto_id='pushButton_playPause')")
    try:
        ctrl = ui.main_window.child_window(auto_id="pushButton_playPause")
        print(f"  exists: {ctrl.exists()}")
        if ctrl.exists():
            print(f"  visible: {ctrl.is_visible()}")
            print(f"  enabled: {ctrl.is_enabled()}")
    except Exception as e:
        print(f"  错误: {e}")
    
    # 方法2: 使用child_window + control_type
    print("\n方法2: child_window(auto_id='pushButton_playPause', control_type='Button')")
    try:
        ctrl = ui.main_window.child_window(auto_id="pushButton_playPause", control_type="Button")
        print(f"  exists: {ctrl.exists()}")
        if ctrl.exists():
            print(f"  visible: {ctrl.is_visible()}")
            print(f"  enabled: {ctrl.is_enabled()}")
    except Exception as e:
        print(f"  错误: {e}")
    
    # 方法3: 遍历所有Button类型控件
    print("\n方法3: 遍历所有Button类型控件")
    try:
        buttons = ui.main_window.descendants(control_type="Button")
        print(f"  找到 {len(buttons)} 个Button控件")
        for btn in buttons[:10]:  # 只显示前10个
            try:
                auto_id = btn.automation_id()
                text = btn.window_text()[:20]
                print(f"    - {auto_id}: '{text}'")
            except:
                pass
    except Exception as e:
        print(f"  错误: {e}")
    
    # 方法4: 遍历所有控件
    print("\n方法4: 遍历所有控件查找auto_id包含'playPause'的")
    try:
        all_controls = ui.main_window.descendants()
        print(f"  总共 {len(all_controls)} 个控件")
        for ctrl in all_controls:
            try:
                auto_id = ctrl.automation_id()
                if 'playPause' in auto_id or 'play' in auto_id.lower():
                    ctrl_type = ctrl.control_type()
                    print(f"    - {auto_id} ({ctrl_type})")
            except:
                pass
    except Exception as e:
        print(f"  错误: {e}")
    
    # 关闭应用
    print("\n" + "="*80)
    print("关闭应用")
    print("="*80)
    ui.close_application()

if __name__ == "__main__":
    main()
