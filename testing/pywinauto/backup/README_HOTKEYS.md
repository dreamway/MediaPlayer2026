# Qt6 播放器快捷键自动化测试

## 🎯 已完成的工作

### 1. 新增快捷键（需要添加到MainWindow.cpp中）
参考 `docs/SHORTCUTCHANGES.md`，添加以下快捷键支持自动化测试：
- `Left` - Seek backward 5 seconds
- `Right` - Seek forward 5 seconds
- `Ctrl+Left` - Seek backward 10%
- `Ctrl+Right` - Seek forward 10%

### 2. 测试脚本
创建了 `testing/pywinauto/test_qt6_with_hotkeys.py`，使用实际快捷键测试

### 3. 现有快捷键（从GlobalDef.h提取）
```
文件操作:
  打开文件: Ctrl+O
  停止播放: Ctrl+C
  上一个: Page Up
  下一个: Page Down

播放控制:
  播放/暂停: Space
  3D/2D切换: Ctrl+1
  输入LR: Ctrl+2
  输入RL: Ctrl+3
  输入UD: Ctrl+4
  输出垂直: Ctrl+5
  输出水平: Ctrl+6
  输出棋盘: Ctrl+7
  左视图: Ctrl+8
  区域3D: Ctrl+9

图像:
  截图: Ctrl+S

音量控制:
  音量加: Up
  音量减: Down
  静音: M

字幕:
  加载字幕: Ctrl+A
  切换字幕: C

其他:
  播放列表: F3
  全屏+: Ctrl+Return
  全屏: Return
  视差加: Ctrl+E
  视差减: Ctrl+W
  视差复位: Ctrl+R
```

## 🚀 快速开始

### 方案1: 使用现有快捷键测试（推荐）

```bash
cd testing/pywinauto

# 修改 test_qt6_with_hotkeys.py 中的路径
notepad test_qt6_with_hotkeys.py

# 运行测试
python test_qt6_with_hotkeys.py
```

### 方案2: 添加新快捷键后测试

按照 `docs/SHORTCUTCHANGES.md` 中的说明，在 `MainWindow.cpp` 中添加seek相关的快捷键。

然后重新编译并运行测试。

## 📊 测试内容

test_qt6_with_hotkeys.py 包含以下测试：

1. ✅ 启动播放器
2. ✅ 打开视频（Ctrl+O）
3. ✅ 播放/暂停（Space）
4. ✅ 停止播放（Ctrl+C）
5. ✅ 小量seek（左右方向键，各5秒）
6. ✅ 音量控制（上下方向键）
7. ✅ 静音切换（M键）

## ⚠️ 注意事项

1. **等待时间**: 由于是Qt6程序，可能需要更长的等待时间（3-6秒）
2. **路径配置**: 务必修改 `exe_path` 和 `test_video_path`
3. **快捷键冲突**: 关闭其他可能占用快捷键的应用
4. **配置文件**: 如果有配置文件，快捷键可能与默认值不同

## 🔧 调试建议

如果测试失败，可以：

1. 手动测试快捷键是否有效：
   - 启动播放器
   - 手动按快捷键（如Space键）
   - 观察是否响应

2. 查看日志文件确认快捷键被触发

3. 使用UI检查工具：
   ```bash
   python ui_inspector.py
   ```

## 📚 相关文件

- `docs/SHORTCUTCHANGES.md` - 添加新快捷键的说明
- `testing/pywinauto/test_qt6_with_hotkeys.py` - 使用实际快捷键的测试脚本
- `testing/pywinauto/README_QT6.md` - Qt6测试说明
- `GlobalDef.h` - 默认快捷键定义（line 84-120）

## ✅ 验收标准

✓ 能够使用快捷键启动播放器
✓ 能够使用Ctrl+O打开视频
✓ 能够使用Space键播放/暂停
✓ 能够使用Ctrl+C停止播放
✓ 能够使用方向键进行音量控制
✓ 测试报告自动生成

## 🎓 下一步

1. （可选）按照SHORTCUTCHANGES.md添加seek快捷键到程序中
2. 修改test_qt6_with_hotkeys.py中的路径配置
3. 运行测试脚本
4. 查看测试报告
