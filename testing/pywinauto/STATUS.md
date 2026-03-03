# Pywinauto 测试脚本实施完成

## 📋 已完成的工作

### Phase 1: 基础播放测试 ✅
- [x] 启动播放器
- [x] 打开视频文件
- [x] 播放/暂停控制
- [x] 停止播放
- [x] 验证窗口标题更新

### Phase 2: Seeking测试 ✅
- [x] 单次seek到不同位置（25%, 50%, 75%）
- [x] 连续多次seek测试（10次）
- [x] seek到开头(0%)和结尾(100%)
- [x] 快速连续seek测试（5次，间隔100ms）

### 测试框架 ✅
- [x] 主测试脚本 (`main.py`)
- [x] 测试配置文件 (`config.py`)
- [x] 使用说明文档 (`README.md`)
- [x] UI检查工具 (`ui_inspector.py`)
- [x] Windows批处理启动脚本 (`run.bat`)
- [x] 依赖包定义 (`requirements.txt`)

## 📁 文件结构

```
testing/pywinauto/
├── main.py              # 主测试脚本
├── config.py            # 测试配置
├── requirements.txt     # Python依赖
├── README.md           # 详细使用说明
├── run.bat             # Windows启动脚本
├── ui_inspector.py     # UI控件检查工具
└── test_report_*.txt   # 测试报告（自动生成）
```

## 🚀 快速开始

### 1. 安装依赖

```bash
cd testing/pywinauto
pip install -r requirements.txt
```

### 2. 配置路径

编辑 `main.py` 中的配置：

```python
exe_path = r"E:\WZMediaPlayer_2025\x64\Debug\WZMediaPlay.exe"
test_video_path = r"D:\videos\test.mp4"
```

### 3. 运行测试

**Windows用户**：
```bash
# 双击运行
run.bat

# 或命令行运行
python main.py
```

**Linux/Mac用户**：
```bash
python main.py
```

### 4. 查看报告

测试完成后，报告会保存在当前目录，文件名格式：
`test_report_YYYYMMDD_HHMMSS.txt`

## 🔧 调试和优化

### 1. 检查UI控件

如果测试脚本找不到控件，运行UI检查工具：

```bash
python ui_inspector.py
```

工具会输出所有可识别的控件，用于更新 `config.py` 中的控件ID。

### 2. 调整超时设置

编辑 `config.py` 中的超时设置：

```python
TIMEOUTS = {
    "app_start": 5000,      # 应用启动超时（毫秒）
    "video_load": 5000,     # 视频加载超时
    "seek_complete": 2000,  # seek完成超时
}
```

### 3. 调整等待时间

编辑 `config.py` 中的等待时间：

```python
WAIT_TIMES = {
    "after_open": 2.0,      # 打开视频后等待（秒）
    "after_seek": 1.0,      # seek后等待
}
```

### 4. 运行特定测试

编辑 `main.py` 的 `main()` 函数：

```python
# 只运行基础播放测试
if tester.run_basic_playback_tests():
    print("✓ 基础播放测试完成")

# 只运行Seeking测试
if tester.run_seeking_tests():
    print("✓ Seeking测试完成")
```

## 📊 测试报告示例

```
================================================================================
WZMediaPlayer 自动化测试报告
================================================================================
测试时间: 2026-01-23 14:30:45
测试视频: D:\videos\test.mp4

总计: 8 | 通过: 8 | 失败: 0 | 错误: 0 | 跳过: 0
总耗时: 15234.56 ms

--------------------------------------------------------------------------------
测试详情:
--------------------------------------------------------------------------------
1. [✓] 启动播放器 (523.45 ms)
   描述: 启动WZMediaPlayer应用程序

2. [✓] 打开视频文件 (2345.67 ms)
   描述: 打开视频: D:\videos\test.mp4
   详情: {"video_path": "D:\\videos\\test.mp4"}

...
```

## 🎯 下一步扩展

根据 `PLAN.md` 中的规划，可以继续实现：

### Phase 3: 3D功能测试
- 3D/2D模式切换
- 输入格式切换(LR/RL/UD)
- 输出格式切换(水平/垂直/棋盘)
- 视差调整
- 局部3D区域设置

### Phase 4: 播放控制测试
- 上一个/下一个视频切换
- 音量控制
- 快进/快退
- 拖拽文件打开

### Phase 5: 播放列表测试
- 添加/删除播放列表项
- 清空播放列表
- 创建新播放列表
- 导入/导出播放列表

### Phase 6: 窗口控制测试
- 最小化/最大化
- 全屏切换
- 窗口调整大小

### Phase 7: 其他功能测试
- 摄像头功能
- 字幕功能
- 截图功能

### Phase 8: 边界和性能测试
- 播放无效文件格式
- 长时间播放稳定性
- 内存泄漏检测
- 性能基准测试

## 🔍 测试用例添加模板

```python
def test_new_feature(self) -> bool:
    """
    测试新功能

    Returns:
        bool: 是否成功
    """
    test_case = TestCase(
        name="新功能测试",
        description="测试新功能的行为"
    )
    start_time = time.time()

    try:
        # 实现测试逻辑
        # 示例：
        button = self.main_window.child_window(auto_id="new_button")
        if button.exists():
            button.click()
            time.sleep(0.5)

            # 验证结果
            test_case.status = TestResult.PASS
            print("✓ 新功能测试成功")
        else:
            raise RuntimeError("未找到新功能按钮")

    except Exception as e:
        test_case.status = TestResult.ERROR
        test_case.error_message = str(e)
        print(f"✗ 新功能测试失败: {e}")

    test_case.duration_ms = (time.time() - start_time) * 1000
    self.test_results.append(test_case)
    return test_case.status == TestResult.PASS
```

## 💡 常见问题

### Q1: 找不到播放器窗口
**A**: 运行 `ui_inspector.py` 检查控件结构，然后更新 `main.py` 中的窗口查找逻辑。

### Q2: 测试速度太快导致失败
**A**: 增加 `config.py` 中的等待时间：
```python
WAIT_TIMES = {
    "after_click": 1.0,  # 增加点击后等待
}
```

### Q3: 视频加载时间较长
**A**: 在 `open_video()` 方法中增加等待时间：
```python
time.sleep(5)  # 增加到5秒
```

### Q4: 测试报告乱码
**A**: 确保终端使用UTF-8编码（Windows已设置 `chcp 65001`）

### Q5: 如何集成到CI/CD
**A**: 创建批处理脚本检查退出码：
```batch
python main.py
if %errorlevel% neq 0 (
    echo 测试失败
    exit 1
)
```

## 📝 注意事项

1. **首次使用前**，运行 `ui_inspector.py` 确认UI控件ID
2. **测试视频准备**，确保测试视频可用且格式正确
3. **不要在开发时运行**，避免干扰正常工作
4. **保存测试报告**，用于追踪问题和验证修复
5. **定期更新脚本**，根据UI变化更新控件查找逻辑

## 📚 参考资源

- [pywinauto官方文档](https://pywinauto.readthedocs.io/)
- [pywinauto GitHub](https://github.com/pywinauto/pywinauto)
- [Windows UI Automation](https://docs.microsoft.com/en-us/windows/win32/winauto/entry-uiauto-win32)

## ✅ 验收标准

当前Phase 1和Phase 2的测试满足以下标准：

1. ✅ 能够自动启动播放器
2. ✅ 能够打开视频文件
3. ✅ 能够控制播放/暂停/停止
4. ✅ 能够进行seek操作
5. ✅ 生成详细的测试报告
6. ✅ 支持配置化（路径、超时等）
7. ✅ 提供UI检查工具
8. ✅ 提供使用说明文档

---

**状态**: Phase 1和Phase 2已完成，可以开始持续测试！
