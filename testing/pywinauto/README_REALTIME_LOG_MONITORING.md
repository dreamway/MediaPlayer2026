# 实时日志监控功能说明

## 概述

测试框架现在支持**实时监控exe程序的日志输出**，在每个测试操作后立即检查日志，快速发现和定位问题。

## 工作原理

1. **启动前准备**：
   - 在启动exe前，记录当前logs目录下的所有日志文件
   - 初始化日志监控器，配置监控模式和错误模式

2. **启动exe后**：
   - 等待exe创建日志文件（通常2秒内）
   - 自动识别exe启动后新创建的日志文件（通过对比启动前后的文件列表）
   - 启动后台线程实时读取日志文件的新内容（类似 `tail -f`）

3. **测试操作中**：
   - 每次执行操作（如播放、暂停、seek等）
   - 等待日志写入（默认0.5-1.5秒）
   - 实时检查日志中是否有错误或警告
   - 如果发现错误，立即标记测试为失败并输出错误信息

## 关键特性

### 1. 自动识别日志文件
```python
# 启动前记录现有日志文件
self.log_files_before_start = set(glob.glob(log_pattern))

# 启动后找出新文件
new_log_files = log_files_after_start - self.log_files_before_start
```

### 2. 实时监控（后台线程）
- 使用独立线程持续读取日志文件
- 每0.5秒检查一次新内容（可配置）
- 自动解析日志格式，提取错误和警告

### 3. 操作后立即检查
```python
# 执行操作
send_keys("{SPACE}")  # 暂停
time.sleep(1.0)

# 立即检查日志
self.log_test("暂停", True, check_log=True, wait_after=0.8)
```

### 4. 错误关联
- 每个测试结果都关联了对应的日志错误
- 测试报告包含详细的日志错误信息
- 可以快速定位是哪个操作导致的问题

## 使用示例

### 基础使用
```python
# 启动播放器（自动启动日志监控）
tester.start_player()

# 执行操作并检查日志
tester.log_test("播放", True, check_log=True, wait_after=0.8)
```

### 自定义等待时间
```python
# 对于需要较长时间的操作，增加等待时间
send_keys("{ENTER}")  # 打开文件
time.sleep(8.0)  # 等待视频加载
tester.log_test("打开视频", True, check_log=True, wait_after=1.5)
```

### 禁用日志检查
```python
# 某些操作不需要检查日志
tester.log_test("简单操作", True, check_log=False)
```

## 日志监控配置

### 监控的错误模式
- `error` - 错误级别日志
- `critical` - 严重错误
- `warning` - 警告（仅记录，不标记为失败）
- `failed` / `fail` - 失败相关
- `exception` - 异常

### 日志格式支持
支持以下日志格式：
```
[2026-02-09 21:30:45.123] [error] message
[2026-02-09T21:30:45.123] error message
2026-02-09 21:30:45.123 [error] message
```

## 输出示例

### 正常情况
```
  [✓ PASS] 播放
```

### 检测到错误
```
  ⚠️  [EXE日志] 在 '播放' 操作中检测到错误:
    ❌ [ERROR] Failed to transfer hardware frame to software frame: -22
      完整日志: [2026-02-09 21:31:47.163][thread 43804][:][error] : Failed to transfer hardware frame...
  [✗ FAIL] 播放
      [EXE日志错误] [error] Failed to transfer hardware frame to software frame: -22
```

## 优势

1. **快速发现问题**：操作后立即检查，不需要等到测试结束
2. **精确定位**：每个错误都关联到具体的操作
3. **实时反馈**：测试过程中就能看到问题，提高调试效率
4. **自动化**：无需手动查看日志文件

## 注意事项

1. **日志写入延迟**：exe写入日志可能有延迟，需要适当的等待时间
2. **日志文件路径**：确保logs目录路径正确
3. **日志格式**：如果exe日志格式变化，可能需要调整解析逻辑
4. **性能影响**：实时监控会消耗一定资源，但影响很小

## run_all_tests 与闭环测试的适配关系

- **run_closed_loop_tests.py / unified_closed_loop_tests.py**：使用 `core` 中的 `UIAutomationController` 启动应用时已开启 `capture_process_output=True`，即使用 **ProcessOutputMonitor** 捕获 stdout/stderr；同时使用 **ImageVerifier** 做图像闭环验证，部分流程使用 **AudioPipeClient**。
- **run_all_tests.py**：已适配 **ProcessOutputMonitor**。`main.WZMediaPlayerTester` 在 `start_player()` 时若存在 `core.process_output_monitor`，会以 `stdout=PIPE, stderr=STDOUT` 启动进程并启动 ProcessOutputMonitor；`log_test()` 会据此将“进程输出关键错误”纳入失败判定；各阶段结果中会带上 `process_output` 摘要并写入完整报告。报告会**先写入文件再打印**，避免仅输出到终端时未生成有效报告文件。
- **图像/音频验证**：`run_all_tests` 当前仍以快捷键+日志监控为主，未接入 ImageVerifier / AudioPipeClient；需要图像或音频闭环时请使用 `run_closed_loop_tests.py` 或 `unified_closed_loop_tests.py`。

## 未来改进

- [ ] 支持更多日志格式
- [ ] 可配置的错误模式
- [ ] 日志统计和分析
- [ ] 自动生成错误报告
- [ ] run_all_tests 可选接入 ImageVerifier / AudioPipeClient（与闭环入口一致）
