"""
UI自动化控制器 - 基于pywinauto的UIA后端实现精确的控件操作
"""

import sys
import time
import os
from typing import Optional, Dict, List, Callable, Any
from datetime import datetime

try:
    from pywinauto.application import Application
    from pywinauto.keyboard import send_keys
    from pywinauto.timings import TimeoutError as PywinautoTimeoutError
except ImportError:
    raise ImportError("pywinauto未安装，请运行: pip install pywinauto")


class UIAutomationController:
    """
    UI自动化控制器
    
    功能：
    1. 使用UIA后端连接/启动Qt应用
    2. 精确定位控件（通过automation_id, control_type等）
    3. 智能等待控件状态变化
    4. 执行控件操作（点击、输入、获取属性等）
    5. 截图验证
    6. 进程输出监控（捕获 ALSOFT/QMutex 等错误）
    """
    
    def __init__(self, backend: str = "uia"):
        self.backend = backend
        self.app: Optional[Application] = None
        self.main_window = None
        self.process_id: Optional[int] = None
        self._control_cache: Dict[str, Any] = {}  # 控件缓存
        self._output_monitor: Optional[Any] = None  # ProcessOutputMonitor
        
    def start_application(self, exe_path: str, timeout: int = 15,
                         capture_process_output: bool = True) -> bool:
        """
        启动应用程序
        
        Args:
            exe_path: 可执行文件路径
            timeout: 启动超时时间（秒）
            
        Returns:
            是否启动成功
        """
        try:
            if not os.path.exists(exe_path):
                raise FileNotFoundError(f"可执行文件不存在: {exe_path}")

            # 测试时设置环境变量（若已设置则保留）
            # WZ_LOG_MODE=1: 日志输出到 console
            # WZ_TEST_MODE=1: 启用 TestPipeServer 命名管道
            if 'WZ_LOG_MODE' not in os.environ:
                os.environ['WZ_LOG_MODE'] = '1'
            if 'WZ_TEST_MODE' not in os.environ:
                os.environ['WZ_TEST_MODE'] = '1'

            print(f"[UIAuto] 启动应用: {os.path.basename(exe_path)}")
            
            # 使用subprocess启动，可选捕获 stdout/stderr 以检测运行时错误
            import subprocess
            if capture_process_output:
                self._process = subprocess.Popen(
                    [exe_path],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    cwd=os.path.dirname(exe_path),
                )
                from .process_output_monitor import ProcessOutputMonitor
                self._output_monitor = ProcessOutputMonitor(
                    self._process, self._process.stdout, capture_all=False
                )
                self._output_monitor.start()
            else:
                self._process = subprocess.Popen(
                    [exe_path],
                    cwd=os.path.dirname(exe_path),
                )
            
            # 等待应用初始化（Qt6需要更长时间）
            print(f"[UIAuto] 等待应用初始化...")
            time.sleep(5)

            # 检查进程是否仍在运行（可能已崩溃）
            if self._process.poll() is not None:
                err_msg = "进程已退出（可能崩溃）"
                if capture_process_output and self._output_monitor:
                    summary = self._output_monitor.get_errors_summary()
                    if summary:
                        err_msg += f"\n{summary[:500]}"
                raise RuntimeError(err_msg)

            # 尝试通过进程ID连接
            try:
                self.app = Application(backend=self.backend).connect(process=self._process.pid)
            except Exception as conn_err:
                # 进程可能已退出，再次检查
                if self._process.poll() is not None:
                    err_msg = "进程已退出（可能崩溃）"
                    if capture_process_output and self._output_monitor:
                        summary = self._output_monitor.get_errors_summary()
                        if summary:
                            err_msg += f"\n{summary[:500]}"
                    raise RuntimeError(err_msg)
                print(f"[UIAuto] 通过PID连接失败，尝试通过窗口标题连接: {conn_err}")
                time.sleep(3)
                self.app = Application(backend=self.backend).connect(title_re=".*MainWindow.*")
            
            # 获取主窗口
            self.main_window = self.app.window()
            
            # 等待窗口可见
            self.main_window.wait('visible', timeout=timeout)
            
            # 获取进程ID
            self.process_id = self.app.process
            
            print(f"[UIAuto] [OK] 应用已启动，PID: {self.process_id}")
            return True
            
        except Exception as e:
            print(f"[UIAuto] [X] 启动失败: {e}")
            return False
    
    def connect_to_application(self, process_id: int = None, title: str = None) -> bool:
        """
        连接到已运行的应用程序
        
        Args:
            process_id: 进程ID
            title: 窗口标题（正则表达式）
            
        Returns:
            是否连接成功
        """
        try:
            if process_id:
                self.app = Application(backend=self.backend).connect(process=process_id)
            elif title:
                # 处理多个匹配窗口的情况，选择第一个可见的
                from pywinauto import Desktop
                desktop = Desktop(backend=self.backend)
                windows = desktop.windows(title_re=title, visible_only=True)
                
                if not windows:
                    raise Exception(f"未找到匹配窗口: {title}")
                
                # 使用第一个可见窗口
                target_window = windows[0]
                self.app = Application(backend=self.backend).connect(handle=target_window.handle)
            else:
                raise ValueError("必须提供process_id或title")
            
            self.main_window = self.app.window()
            self.process_id = self.app.process
            
            print(f"[UIAuto] [OK] 已连接到应用，PID: {self.process_id}")
            return True
            
        except Exception as e:
            print(f"[UIAuto] [X] 连接失败: {e}")
            return False
    
    def get_control(self, automation_id: str = None, control_type: str = None,
                   title: str = None, parent=None, use_cache: bool = False) -> Optional[Any]:
        """
        获取控件
        
        Args:
            automation_id: 自动化ID（Qt的objectName）
            control_type: 控件类型（Button, Slider等）
            title: 控件标题/文本
            parent: 父控件（默认为主窗口）
            use_cache: 是否使用缓存（默认False，避免UI变化后缓存失效）
            
        Returns:
            控件对象或None
        """
        if parent is None:
            parent = self.main_window
        
        # 构建缓存键
        cache_key = f"{automation_id}_{control_type}_{title}"
        
        if use_cache and cache_key in self._control_cache:
            control = self._control_cache[cache_key]
            try:
                if control.exists() and control.is_visible():
                    return control
            except:
                pass
            # 缓存失效，移除
            del self._control_cache[cache_key]
        
        try:
            # 构建选择器 - 优先使用automation_id
            kwargs = {}
            if automation_id:
                kwargs['auto_id'] = automation_id
            if control_type:
                kwargs['control_type'] = control_type
            if title:
                kwargs['title'] = title
            
            # 首先尝试精确匹配
            control = parent.child_window(**kwargs)
            
            if control.exists() and control.is_visible():
                if use_cache:
                    self._control_cache[cache_key] = control
                return control
            
            # 如果失败，尝试只使用auto_id（最可靠的方式）
            if automation_id and control_type:
                control = parent.child_window(auto_id=automation_id)
                if control.exists() and control.is_visible():
                    if use_cache:
                        self._control_cache[cache_key] = control
                    return control
            
            # 尝试使用部分匹配（Qt可能返回完整路径作为auto_id）
            if automation_id:
                try:
                    # 获取所有匹配的控件类型
                    if control_type:
                        descendants = parent.descendants(control_type=control_type)
                    else:
                        descendants = parent.descendants()
                    
                    for ctrl in descendants:
                        try:
                            ctrl_auto_id = ctrl.automation_id()
                            # 检查是否以目标auto_id结尾
                            if ctrl_auto_id == automation_id or ctrl_auto_id.endswith('.' + automation_id):
                                if ctrl.is_visible():
                                    return ctrl
                        except:
                            continue
                except Exception as e:
                    pass
            
            # 最后尝试遍历直接子控件匹配auto_id
            if automation_id:
                try:
                    for child in parent.children():
                        try:
                            child_auto_id = child.automation_id()
                            if child_auto_id == automation_id or child_auto_id.endswith('.' + automation_id):
                                return child
                        except:
                            continue
                except:
                    pass
            
            return None
                
        except Exception as e:
            return None
    
    def wait_for_control(self, automation_id: str = None, control_type: str = None,
                        title: str = None, timeout: int = 10, 
                        condition: str = 'exists') -> Optional[Any]:
        """
        等待控件满足条件
        
        Args:
            automation_id: 自动化ID
            control_type: 控件类型
            title: 控件标题
            timeout: 超时时间（秒）
            condition: 等待条件 ('exists', 'visible', 'enabled', 'ready')
            
        Returns:
            控件对象或None
        """
        try:
            kwargs = {}
            if automation_id:
                kwargs['auto_id'] = automation_id
            if control_type:
                kwargs['control_type'] = control_type
            if title:
                kwargs['title'] = title
            
            control = self.main_window.child_window(**kwargs)
            control.wait(condition, timeout=timeout)
            
            return control
            
        except PywinautoTimeoutError:
            print(f"[UIAuto] 等待控件超时: {automation_id or title}")
            return None
        except Exception as e:
            print(f"[UIAuto] 等待控件出错: {e}")
            return None
    
    def click_button(self, automation_id: str = None, title: str = None,
                    double_click: bool = False, wait_after: float = 0.5) -> bool:
        """
        点击按钮
        
        Args:
            automation_id: 按钮自动化ID
            title: 按钮标题
            double_click: 是否双击
            wait_after: 点击后等待时间
            
        Returns:
            是否点击成功
        """
        # 首先尝试使用control_type='Button'查找
        control = self.get_control(automation_id=automation_id, 
                                   control_type='Button',
                                   title=title)
        
        # 如果失败，尝试不使用control_type（Qt控件可能不是标准Button类型）
        if not control and automation_id:
            control = self.get_control(automation_id=automation_id, use_cache=False)
        
        if not control:
            print(f"[UIAuto] [X] 未找到按钮: {automation_id or title}")
            return False
        
        try:
            # 等待按钮可用
            try:
                control.wait('enabled', timeout=5)
                control.wait('visible', timeout=5)
            except:
                pass  # 即使等待失败也尝试点击
            
            # 确保控件在视图中
            try:
                control.scroll(direction='set', amount='top')
            except:
                pass
            
            if double_click:
                control.double_click_input()
            else:
                control.click_input()
            
            if wait_after > 0:
                time.sleep(wait_after)
            
            print(f"[UIAuto] [OK] 点击按钮: {automation_id or title}")
            return True
            
        except Exception as e:
            print(f"[UIAuto] [X] 点击按钮失败: {e}")
            return False
    
    def get_slider_value(self, automation_id: str, timeout: float = 2.0) -> Optional[int]:
        """
        获取滑块当前值
        
        Args:
            automation_id: 滑块自动化ID
            timeout: 获取超时时间（秒）
            
        Returns:
            滑块值或None
        """
        import threading
        
        # 首先尝试使用Slider类型
        control = self.get_control(automation_id=automation_id, control_type='Slider')
        
        # 如果失败，尝试不使用control_type
        if not control:
            control = self.get_control(automation_id=automation_id)
        
        if not control:
            return None
        
        result = [None]
        def get_value_thread():
            try:
                result[0] = control.get_value()
            except:
                result[0] = None
        
        # 使用线程避免阻塞
        thread = threading.Thread(target=get_value_thread)
        thread.daemon = True
        thread.start()
        thread.join(timeout=timeout)
        
        return result[0]
    
    def set_slider_value(self, automation_id: str, value: int, timeout: float = 3.0) -> bool:
        """
        设置滑块值
        
        Args:
            automation_id: 滑块自动化ID
            value: 目标值
            timeout: 设置超时时间（秒）
            
        Returns:
            是否设置成功
        """
        import threading
        
        # 首先尝试使用Slider类型
        control = self.get_control(automation_id=automation_id, control_type='Slider')
        
        # 如果失败，尝试不使用control_type
        if not control:
            control = self.get_control(automation_id=automation_id)
        
        if not control:
            return False
        
        result = [False]
        error_msg = [""]
        
        def set_value_thread():
            try:
                control.set_value(value)
                result[0] = True
            except Exception as e:
                error_msg[0] = str(e)
                result[0] = False
        
        # 使用线程避免阻塞
        thread = threading.Thread(target=set_value_thread)
        thread.daemon = True
        thread.start()
        thread.join(timeout=timeout)
        
        if not result[0] and error_msg[0]:
            print(f"[UIAuto] [X] 设置滑块值失败: {error_msg[0]}")
        
        return result[0]
    
    def get_label_text(self, automation_id: str) -> Optional[str]:
        """
        获取标签文本
        
        Args:
            automation_id: 标签自动化ID
            
        Returns:
            文本内容或None
        """
        # 首先尝试使用Text类型
        control = self.get_control(automation_id=automation_id, control_type='Text')
        
        # 如果失败，尝试不使用control_type
        if not control:
            control = self.get_control(automation_id=automation_id)
        
        if not control:
            return None
        
        try:
            # 尝试多种方式获取文本
            try:
                text = control.window_text()
                if text:
                    return text
            except:
                pass
            
            try:
                text = control.texts()
                if text and len(text) > 0:
                    return text[0]
            except:
                pass
            
            try:
                text = control.get_properties().get('text', '')
                if text:
                    return text
            except:
                pass
            
            return None
        except:
            return None
    
    def select_combobox_item(self, automation_id: str, item_text: str = None,
                            item_index: int = None) -> bool:
        """
        选择下拉框项
        
        Args:
            automation_id: 下拉框自动化ID
            item_text: 选项文本
            item_index: 选项索引
            
        Returns:
            是否选择成功
        """
        control = self.get_control(automation_id=automation_id, control_type='ComboBox')
        
        if not control:
            return False
        
        try:
            if item_text:
                control.select(item_text)
            elif item_index is not None:
                control.select(item_index)
            return True
        except Exception as e:
            print(f"[UIAuto] [X] 选择下拉框项失败: {e}")
            return False
    
    def send_keys_shortcut(self, keys: str, wait_after: float = 0.5):
        """
        发送键盘快捷键
        
        Args:
            keys: 按键序列（pywinauto格式）
            wait_after: 发送后等待时间
        """
        try:
            send_keys(keys)
            if wait_after > 0:
                time.sleep(wait_after)
        except Exception as e:
            print(f"[UIAuto] [X] 发送按键失败: {e}")
    
    def capture_window_screenshot(self, save_path: str = None) -> Optional[Any]:
        """
        截取窗口截图
        
        Args:
            save_path: 保存路径（可选）
            
        Returns:
            PIL Image对象或None
        """
        try:
            if not self.main_window:
                return None
            
            image = self.main_window.capture_as_image()
            
            if save_path:
                image.save(save_path)
                print(f"[UIAuto] 截图已保存: {save_path}")
            
            return image
            
        except Exception as e:
            print(f"[UIAuto] [X] 截图失败: {e}")
            return None
    
    def capture_control_screenshot(self, automation_id: str, 
                                   save_path: str = None) -> Optional[Any]:
        """
        截取控件截图
        
        Args:
            automation_id: 控件自动化ID
            save_path: 保存路径（可选）
            
        Returns:
            PIL Image对象或None
        """
        control = self.get_control(automation_id=automation_id)
        
        if not control:
            return None
        
        try:
            image = control.capture_as_image()
            
            if save_path:
                image.save(save_path)
                print(f"[UIAuto] 控件截图已保存: {save_path}")
            
            return image
            
        except Exception as e:
            print(f"[UIAuto] [X] 控件截图失败: {e}")
            return None
    
    def is_control_enabled(self, automation_id: str) -> bool:
        """检查控件是否可用"""
        control = self.get_control(automation_id=automation_id)
        if not control:
            return False
        try:
            return control.is_enabled()
        except:
            return False
    
    def is_control_visible(self, automation_id: str) -> bool:
        """检查控件是否可见"""
        control = self.get_control(automation_id=automation_id)
        if not control:
            return False
        try:
            return control.is_visible()
        except:
            return False
    
    def get_window_state(self) -> Dict[str, Any]:
        """
        获取窗口当前状态
        
        Returns:
            包含窗口状态信息的字典
        """
        if not self.main_window:
            return {}
        
        try:
            rect = self.main_window.rectangle()
            return {
                'title': self.main_window.window_text(),
                'is_visible': self.main_window.is_visible(),
                'is_enabled': self.main_window.is_enabled(),
                'rectangle': {
                    'left': rect.left,
                    'top': rect.top,
                    'right': rect.right,
                    'bottom': rect.bottom,
                    'width': rect.width(),
                    'height': rect.height()
                }
            }
        except Exception as e:
            print(f"[UIAuto] 获取窗口状态失败: {e}")
            return {}
    
    def close_application(self, force: bool = False):
        """
        关闭应用程序
        
        Args:
            force: 是否强制关闭
        """
        try:
            # 停止输出监控
            if self._output_monitor:
                self._output_monitor.stop()
                self._output_monitor = None
            
            if self.app:
                if force:
                    self.app.kill()
                else:
                    self.app.kill()
                print("[UIAuto] 应用已关闭")
        except Exception as e:
            print(f"[UIAuto] 关闭应用出错: {e}")
    
    def get_output_monitor(self):
        """获取进程输出监控器（用于测试框架检查错误）"""
        return getattr(self, '_output_monitor', None)
    
    def has_process_output_errors(self) -> bool:
        """进程输出中是否检测到应判定为失败的错误"""
        monitor = self.get_output_monitor()
        if not monitor:
            return False
        return monitor.has_failures()
    
    def get_process_output_errors_summary(self) -> str:
        """获取进程输出错误摘要"""
        monitor = self.get_output_monitor()
        if not monitor:
            return ""
        return monitor.get_errors_summary()
    
    def is_process_alive(self) -> bool:
        """被测进程是否仍在运行（用于检测崩溃）"""
        if not hasattr(self, '_process') or self._process is None:
            return True  # 未通过本控制器启动
        return self._process.poll() is None
    
    def print_control_tree(self, control=None, indent: int = 0):
        """
        打印控件树结构（用于调试）
        
        Args:
            control: 起始控件（默认为主窗口）
            indent: 缩进级别
        """
        if control is None:
            control = self.main_window
        
        try:
            prefix = "  " * indent
            try:
                auto_id = control.automation_id()
            except:
                auto_id = "N/A"
            
            try:
                ctrl_type = control.control_type()
            except:
                ctrl_type = "Unknown"
            
            try:
                text = control.window_text()[:30]  # 限制长度
            except:
                text = ""
            
            print(f"{prefix}[{ctrl_type}] {auto_id} - '{text}'")
            
            # 递归打印子控件
            children = control.children()
            for child in children:
                self.print_control_tree(child, indent + 1)
                
        except Exception as e:
            print(f"{prefix}[Error] {e}")
