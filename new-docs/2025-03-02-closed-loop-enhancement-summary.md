# 闭环测试增强 - 实施总结

> 分支：closed-loop-enhancement → trae（已合并）

## 变更概览

### Phase 1: 日志同步闭环 ✅

| 任务 | 提交 | 说明 |
|------|------|------|
| Task 1 | `feat(test): ProcessOutputMonitor 支持时间窗口筛选日志` | 新增 `_timestamped_lines`、`get_lines_in_window(t_start, t_end)` |
| Task 2 | `feat(test): 失败步骤关联时间窗口内日志` | `start_test` 记录 `_test_start_wall`，`end_test` 失败时输出关联 error/warn 日志 |
| Task 3 | `feat(test): 测试时强制 LogMode=1 输出到 console` | setup 备份并写入 LogMode=1，teardown 恢复 |
| Task 4 | `feat(test): LogMonitor 作为日志关联 fallback` | LogMonitor 增加 `get_lines_in_window`，ProcessOutputMonitor 无数据时回退 |

### Phase 2: 音频命名管道 ✅

| 任务 | 提交 | 说明 |
|------|------|------|
| Task 5 | `feat: 添加 TestPipeServer 命名管道输出音频状态` | C++ TestPipeServer，WZ_TEST_MODE=1 时启用 |
| Task 6 | `feat(test): 添加 audio_pipe_client 连接命名管道验证音频` | Python AudioPipeClient，`verify_audio_playing`、`verify_av_sync` |
| Task 7 | `feat(test): 集成音频管道验证到测试用例` | test_basic_playback、test_av_sync、test_audio_features 集成 |

### 修复与改进

| 提交 | 说明 |
|------|------|
| `fix(test): worktree 下优先使用主仓库 exe，自动复制 config，改进错误提示` | worktree exe 从 subprocess 启动会崩溃时，优先用主仓库 exe；setup 自动复制 config；进程退出时输出错误摘要 |

## 涉及文件

**新增**
- `WZMediaPlay/test_support/TestPipeServer.h`
- `WZMediaPlay/test_support/TestPipeServer.cpp`
- `testing/pywinauto/core/audio_pipe_client.py`

**修改**
- `testing/pywinauto/core/process_output_monitor.py`
- `testing/pywinauto/core/test_base.py`
- `testing/pywinauto/core/ui_automation.py`
- `testing/pywinauto/log_monitor.py`
- `testing/pywinauto/unified_closed_loop_tests.py`
- `testing/pywinauto/requirements.txt`（+pywin32）
- `WZMediaPlay/MainWindow.cpp`、`MainWindow.h`
- `WZMediaPlay/WZMediaPlay.vcxproj`
- `testing/pywinauto/README_CLOSED_LOOP.md`
- `.gitignore`（+ .worktrees）

## 已知问题

- **worktree 构建的 exe 从 pywinauto/subprocess 启动会崩溃**（0xC0000135），手动双击可正常启动。当前通过优先使用主仓库 exe 规避。

## 运行方式

```bash
cd testing/pywinauto
python unified_closed_loop_tests.py --categories basic
```

exe 路径会自动推断；worktree 下会优先使用主仓库 `x64/Debug/WZMediaPlay.exe`。
