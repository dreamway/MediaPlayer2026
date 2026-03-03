# WZMediaPlayer Qt6 自动化测试 - 快速指南

## 🚀 立即使用（推荐）

### 1. 运行测试
```bash
cd testing/pywinauto
python test_qt6_direct.py
```

### 2. 查看测试报告
测试完成后，报告会自动保存到：
```
testing/pywinauto/test_report_qt6_YYYYMMDD_HHMMSS.txt
```

## 📝 测试覆盖

✅ 启动播放器
✅ 打开视频（Ctrl+O）
✅ 播放/暂停（Space键）
✅ 停止播放（Ctrl+C）
✅ 小量seek（左右方向键）
✅ 音量控制（上下方向键）
✅ 静音切换（M键）

## 🛠️ 问题排查

### 问题1：视频文件不存在
**错误**：Video file not found
**解决**：修改 `test_qt6_direct.py` 第267行，设置正确的视频路径：
```python
test_video_path = r"D:\your\video\path.mp4"
```

### 问题2：播放器启动失败
**错误**：Executable not found
**解决**：修改第266行的exe_path：
```python
exe_path = r"E:\WZMediaPlayer_2025\x64\Debug\WZMediaPlay.exe"
```

### 问题3：快捷键无响应
**可能原因**：
- 窗口焦点不在播放器上
- 快捷键冲突
- 需要先编译程序

**解决**：
- 手动测试快捷键是否有效
- 确保播放器已正确编译

## 📂 可用脚本

- `test_qt6_direct.py` - 推荐：直接键盘控制（最新版本）
- `test_qt6_simple.py` - 原始pywinauto版（Qt6兼容性有问题）
- `main.py` - 完整版测试（控件查找可能失败）

## 🔧 快捷键映射

```
播放控制：
  播放/暂停: Space
  停止: Ctrl+C

文件操作：
  打开文件: Ctrl+O
  上一个: Page Up
  下一个: Page Down

音量控制：
  音量加: Up
  音量减: Down
  静音: M
```

## 📚 相关文档

- `README_HOTKEYS.md` - 快捷键测试详细说明
- `docs/SHORTCUTCHANGES.md` - 添加新快捷键到代码
- `AGENTS.md` - 项目整体配置

## 🎯 下一步

### 如果测试失败：
1. 检查视频文件路径是否正确
2. 手动运行播放器确认可以正常打开
3. 查看日志文件排查问题

### 如果需要更多测试：
1. 参考 `PLAN.md` 规划
2. 参考 `main.py` 完整版测试
3. 添加新的快捷键（见SHORTCUTCHANGES.md）

## 💡 提示

- 使用实际的视频文件进行测试
- 测试前确保程序已编译完成
- 定期运行测试验证功能正常
- 保存测试报告用于问题追踪
