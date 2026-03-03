# Qt6 程序测试说明

## 📋 问题说明

WZMediaPlayer是Qt6编写的程序，pywinauto的控件查找可能遇到困难。为此提供了两个测试脚本：

## 🎯 推荐使用简化版测试

### test_qt6_simple.py（推荐）

这个脚本使用快捷键进行测试，不依赖控件查找，适合Qt6程序。

#### 运行方式：

```bash
cd testing/pywinauto

# 修改脚本中的路径
notepad test_qt6_simple.py
# 修改 exe_path 和 test_video_path

# 运行测试
python test_qt6_simple.py
```

#### 测试内容：

1. ✅ 启动播放器
2. ✅ 打开视频（Ctrl+O）
3. ✅ 播放/暂停（空格键）
4. ✅ 停止播放（Ctrl+S）
5. ✅ 简单seek（左右方向键）
6. ✅ 音量控制（上下方向键）

#### 优点：

- 不依赖控件查找
- 适用于Qt6程序
- 简单可靠
- 自动生成报告

#### 配置：

编辑 `test_qt6_simple.py` 第180行：

```python
exe_path = r"E:\WZMediaPlayer_2025\x64\Debug\WZMediaPlay.exe"
test_video_path = r"D:\videos\test.mp4"  # 修改为实际路径
```

---

## 🔧 完整版测试（可选）

### main.py

这个脚本尝试使用控件查找，可能需要针对Qt6进行调整。

#### 针对Qt6的调整：

1. 运行UI检查工具：
```bash
python ui_inspector.py
```

2. 根据输出更新 `config.py` 中的控件ID

3. 如果控件查找仍然失败，建议使用 `test_qt6_simple.py`

---

## 🚀 快速开始

### Windows用户：

1. **双击运行简化版测试**：
   ```
   testing/pywinauto/test_qt6_simple.py
   ```

2. **或使用命令行**：
   ```bash
   cd testing/pywinauto
   python test_qt6_simple.py
   ```

3. **查看测试报告**：
   - 报告保存在 `testing/pywinauto/` 目录
   - 文件名格式：`test_report_qt6_YYYYMMDD_HHMMSS.txt`

---

## 📊 测试报告示例

```
============================================================
WZMediaPlayer Qt6 简化测试报告
============================================================
测试时间: 2026-01-23 15:30:45
测试视频: D:\videos\test.mp4

总计: 8 | 通过: 8 | 失败: 0

------------------------------------------------------------
测试详情:
------------------------------------------------------------
1. [✓] 启动播放器
   窗口标题: WZMediaPlay - Untitled

2. [✓] 打开视频
   使用快捷键Ctrl+O

3. [✓] 暂停播放
   使用空格键

4. [✓] 恢复播放
   使用空格键

5. [✓] 停止播放
   使用快捷键Ctrl+S

6. [✓] 右方向Seek
   使用右方向键5次

7. [✓] 左方向Seek
   使用左方向键5次

8. [✓] 增大音量
   使用上方向键2次

9. [✓] 减小音量
   使用下方向键2次

============================================================
```

---

## 🛠️ 常见问题

### Q1: test_qt6_simple.py 也失败了

**A**: 检查以下几点：

1. **路径配置**：
   - 确认 `exe_path` 指向正确的播放器路径
   - 确认 `test_video_path` 指向存在的视频文件

2. **播放器权限**：
   - 确保播放器有权限访问视频文件
   - 尝试以管理员身份运行测试

3. **快捷键冲突**：
   - 关闭其他可能占用快捷键的应用程序

### Q2: 没有生成测试报告

**A**: 可能原因：

1. 测试在报告保存前崩溃
2. 路径权限问题
3. Python编码问题

**解决方法**：
- 确保脚本有写入权限
- 手动运行 `python test_qt6_simple.py` 查看完整输出

### Q3: 简化版测试不够全面

**A**: 简化版测试专注于核心功能：

- ✅ 基础播放控制
- ✅ 简单seek（方向键）
- ✅ 音量控制

如果需要更全面的测试（如精确进度条seek、3D功能等），需要：

1. 解决Qt6控件查找问题
2. 使用 `main.py` 完整版
3. 或手动编写更多快捷键测试

### Q4: 如何添加更多测试

**A**: 在 `test_qt6_simple.py` 中添加方法：

```python
def test_custom(self) -> bool:
    """自定义测试"""
    try:
        # 使用快捷键或其他方式
        send_keys("^{f}")  # Ctrl+F (示例）
        time.sleep(1)

        self.log_test("自定义测试", True, "使用快捷键Ctrl+F")
        return True
    except Exception as e:
        self.log_test("自定义测试", False, str(e))
        return False
```

然后在 `main()` 函数中调用：

```python
tester.test_custom()
```

---

## 📝 注意事项

1. **确保视频文件可用**
   - 测试视频路径必须正确
   - 视频格式应被播放器支持

2. **不要在开发时运行**
   - 测试会自动操作窗口
   - 可能干扰正常工作

3. **保存测试报告**
   - 用于追踪问题和验证修复
   - 报告自动保存到脚本所在目录

4. **定期更新**
   - 根据UI变化更新测试
   - 根据快捷键变化调整

---

## 🔗 相关资源

- [pywinauto官方文档](https://pywinauto.readthedocs.io/)
- [Qt6官方文档](https://doc.qt.io/qt-6/)
- [Windows UI Automation](https://docs.microsoft.com/en-us/windows/win32/winauto/)

---

## ✅ 验收标准

✓ 能够自动启动播放器
✓ 能够打开视频文件
✓ 能够控制播放/暂停/停止
✓ 能够进行简单seek操作
✓ 能够控制音量
✓ 生成详细的测试报告
✓ 不依赖控件查找（使用快捷键）

---

**推荐**: 对于Qt6程序，优先使用 `test_qt6_simple.py`
