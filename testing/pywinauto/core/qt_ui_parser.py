"""
Qt UI解析器 - 从C++源码和UI文件解析控件结构
"""

import re
import os
from typing import Dict, List, Optional, Tuple
from dataclasses import dataclass


@dataclass
class UIControl:
    """UI控件信息"""
    name: str                    # 控件objectName
    control_type: str            # 控件类型 (QPushButton, QSlider等)
    parent_name: Optional[str]   # 父控件名称
    window_title: Optional[str]  # 窗口标题
    automation_id: str           # 自动化ID (用于pywinauto)
    properties: Dict             # 其他属性


class QtUIParser:
    """
    Qt UI解析器
    
    功能：
    1. 解析.ui文件提取控件结构
    2. 解析生成的ui_*.h文件提取控件信息
    3. 解析MainWindow.h/cpp提取控件槽函数
    4. 构建控件层级关系
    """
    
    def __init__(self, project_root: str):
        self.project_root = project_root
        self.controls: Dict[str, UIControl] = {}
        self.control_hierarchy: Dict[str, List[str]] = {}  # parent -> children
        
    def parse_ui_header(self, header_path: str) -> Dict[str, UIControl]:
        """
        解析生成的UI头文件 (如 ui_MainWindow.h)
        
        提取信息：
        - 控件类型和名称
        - 控件层级关系
        - objectName设置
        """
        controls = {}
        
        if not os.path.exists(header_path):
            print(f"[QtUIParser] 警告: 文件不存在 {header_path}")
            return controls
            
        with open(header_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
        
        # 匹配控件声明模式: Type *name;
        # 例如: QPushButton *pushButton_playPause;
        control_pattern = r'(\w+)\s*\*\s*(\w+)\s*;'
        
        # 匹配setObjectName调用
        objectname_pattern = r'(\w+)->setObjectName\("([^"]+)"\)'
        
        # 匹配addWidget调用确定层级关系
        addwidget_pattern = r'(\w+)->addWidget\((\w+)\)'
        
        # 提取所有控件声明
        for match in re.finditer(control_pattern, content):
            control_type = match.group(1)
            var_name = match.group(2)
            
            # 过滤掉非UI控件
            if control_type in ['QAction', 'QMenu', 'QMenuBar', 'QIcon', 'QSizePolicy', 
                               'QSpacerItem', 'QByteArray', 'QString', 'QVariant']:
                continue
                
            controls[var_name] = UIControl(
                name=var_name,
                control_type=control_type,
                parent_name=None,
                window_title=None,
                automation_id=var_name,  # 默认使用变量名作为automation_id
                properties={}
            )
        
        # 提取setObjectName调用，更新automation_id
        for match in re.finditer(objectname_pattern, content):
            var_name = match.group(1)
            object_name = match.group(2)
            if var_name in controls:
                controls[var_name].automation_id = object_name
                controls[var_name].name = object_name
        
        # 提取层级关系
        for match in re.finditer(addwidget_pattern, content):
            parent = match.group(1)
            child = match.group(2)
            if child in controls:
                controls[child].parent_name = parent
                if parent not in self.control_hierarchy:
                    self.control_hierarchy[parent] = []
                self.control_hierarchy[parent].append(child)
        
        self.controls.update(controls)
        return controls
    
    def parse_mainwindow_header(self, header_path: str) -> Dict[str, any]:
        """
        解析MainWindow.h提取额外信息
        
        提取：
        - 槽函数名称 (用于确定控件事件)
        - 快捷键定义
        - 状态枚举
        """
        info = {
            'slots': [],
            'shortcuts': [],
            'enums': {}
        }
        
        if not os.path.exists(header_path):
            return info
            
        with open(header_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
        
        # 提取private slots
        slots_pattern = r'private slots:[\s\S]*?(?=private:|public:|protected:|\Z)'
        slots_match = re.search(slots_pattern, content)
        if slots_match:
            slots_section = slots_match.group(0)
            # 提取槽函数
            slot_pattern = r'void\s+(on_\w+)\s*\('
            for match in re.finditer(slot_pattern, slots_section):
                info['slots'].append(match.group(1))
        
        # 提取快捷键定义 (QShortcut)
        shortcut_pattern = r'QShortcut\s*\*\s*(\w+)\s*='
        for match in re.finditer(shortcut_pattern, content):
            info['shortcuts'].append(match.group(1))
        
        # 提取枚举定义
        enum_pattern = r'typedef\s+enum\s+(\w+)\s*\{([^}]+)\}'
        for match in re.finditer(enum_pattern, content):
            enum_name = match.group(1)
            enum_values = [v.strip() for v in match.group(2).split(',')]
            info['enums'][enum_name] = enum_values
        
        return info
    
    def build_control_map(self) -> Dict[str, Dict]:
        """
        构建完整的控件映射表
        
        返回适合pywinauto使用的控件定位信息
        """
        control_map = {}
        
        for control_id, control in self.controls.items():
            # 映射Qt控件类型到UIA control_type
            uia_type = self._map_to_uia_type(control.control_type)
            
            control_map[control.automation_id] = {
                'qt_type': control.control_type,
                'uia_type': uia_type,
                'automation_id': control.automation_id,
                'parent': control.parent_name,
                'properties': control.properties
            }
        
        return control_map
    
    def _map_to_uia_type(self, qt_type: str) -> str:
        """将Qt控件类型映射到UIA control type"""
        mapping = {
            'QPushButton': 'Button',
            'QSlider': 'Slider',
            'QComboBox': 'ComboBox',
            'QListWidget': 'List',
            'QTabWidget': 'Tab',
            'QLabel': 'Text',
            'QCheckBox': 'CheckBox',
            'QRadioButton': 'RadioButton',
            'QLineEdit': 'Edit',
            'QTextEdit': 'Document',
            'QGroupBox': 'Group',
            'QMenu': 'Menu',
            'QMenuBar': 'MenuBar',
            'QToolBar': 'ToolBar',
            'CustomSlider': 'Slider',
            'UpShowComboBox': 'ComboBox',
            'DropListWidget': 'List',
            'StereoVideoWidget': 'Pane',
            'AdvertisementWidget': 'Pane',
        }
        return mapping.get(qt_type, 'Pane')
    
    def get_control_by_automation_id(self, automation_id: str) -> Optional[UIControl]:
        """通过automation_id获取控件信息"""
        for control in self.controls.values():
            if control.automation_id == automation_id:
                return control
        return None
    
    def get_playback_controls(self) -> Dict[str, str]:
        """获取播放控制相关控件的自动化ID"""
        playback_controls = {
            'play_pause': 'pushButton_playPause',
            'stop': 'pushButton_stop',
            'previous': 'pushButton_previous',
            'next': 'pushButton_next',
            'open': 'pushButton_open',
            'progress_slider': 'horizontalSlider_playProgress',
            'volume_slider': 'horizontalSlider_volume',
            'volume_button': 'pushButton_volume',
            'time_display': 'label_playTime',
            'total_time': 'label_totalTime',
            'format_2d_3d': 'comboBox_src2D_3D2D',
            'input_format': 'comboBox_3D_input',
            'fullscreen': 'pushButton_fullScreen',
            'minimize': 'pushButton_min',
            'maximize': 'pushButton_max',
            'close': 'pushButton_close',
            'playlist': 'listWidget_playlist',
            'add_to_playlist': 'pushButton_add',
            'clear_playlist': 'pushButton_clear',
        }
        return playback_controls
    
    def get_shortcut_map(self) -> Dict[str, str]:
        """获取快捷键映射"""
        return {
            'open': '^o',           # Ctrl+O
            'play_pause': '{SPACE}', # Space
            'stop': '^{c}',         # Ctrl+C
            'previous': '^{p}',     # Ctrl+P
            'next': '^{n}',         # Ctrl+N
            'fullscreen': '{F11}',  # F11
            'fullscreen_plus': '^{f}',  # Ctrl+F
            'mute': 'm',            # M
            'volume_up': '{PGUP}',  # PageUp
            'volume_down': '{PGDN}', # PageDown
            'seek_left': '{LEFT}',   # Left Arrow (5s)
            'seek_right': '{RIGHT}', # Right Arrow (5s)
            'seek_left_large': '{UP}',   # Up Arrow (10s)
            'seek_right_large': '{DOWN}', # Down Arrow (10s)
            '3d_toggle': '^{d}',    # Ctrl+D
            'lr_toggle': '^{l}',    # Ctrl+L
            'rl_toggle': '^{r}',    # Ctrl+R
            'ud_toggle': '^{u}',    # Ctrl+U
            'screenshot': '^{s}',   # Ctrl+S
            'escape': '{ESC}',      # Escape
        }
    
    def print_control_summary(self):
        """打印控件摘要信息"""
        print("=" * 80)
        print("Qt UI控件解析结果")
        print("=" * 80)
        print(f"\n共解析到 {len(self.controls)} 个控件:\n")
        
        # 按类型分组
        by_type = {}
        for control in self.controls.values():
            qt_type = control.control_type
            if qt_type not in by_type:
                by_type[qt_type] = []
            by_type[qt_type].append(control)
        
        for qt_type, controls in sorted(by_type.items()):
            print(f"\n{qt_type} ({len(controls)}个):")
            for ctrl in controls:
                print(f"  - {ctrl.automation_id}")
        
        print("\n" + "=" * 80)
        print("播放控制控件映射:")
        print("=" * 80)
        for name, automation_id in self.get_playback_controls().items():
            status = "[OK]" if self.get_control_by_automation_id(automation_id) else "[X]"
            print(f"  {status} {name}: {automation_id}")
        
        print("\n" + "=" * 80)
