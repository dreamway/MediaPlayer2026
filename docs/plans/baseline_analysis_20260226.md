# 基线测试分析报告

**日期**: 2026-02-26  
**测试报告**: `testing/pywinauto/reports/closed_loop_test_report_20260226_152439.txt`  
**日志文件**: `x64/Debug/logs/MediaPlayer_20260226151352.log`

---

## 一、测试报告摘要

| 指标 | 数值 |
|------|------|
| 总计 | 8 用例 |
| 通过 | 3 |
| 失败 | 5 |
| 总耗时 | 236279 ms (~4 分钟) |

### 通过用例

1. **打开视频文件** (16.2s) - 视频已加载，时间: 00:00:42
2. **播放/暂停切换** (15.1s) - 视频已暂停
3. **停止播放** (20.8s) - 视频已停止播放，时间已停止在: 00:00:42

### 失败用例

| 用例 | 失败原因 | 对应 BUG |
|------|----------|----------|
| 小幅前进Seek | 00:00:42 -> None; **被测进程已意外退出（崩溃）** | BUG-001 / BUG-003 |
| 小幅后退Seek | 无法获取时间 | 进程已崩溃，后续测试无效 |
| 大幅Seek | 时间未变化且视频未播放 | 同上 |
| 进度条拖动 | 无法找到滑块 | 进程可能已崩溃或窗口状态异常 |
| 进程存活检测 | 被测进程已意外退出（可能崩溃） | **崩溃确认** |

**关键发现**: 在「小幅前进Seek」时进程崩溃，导致后续 Seek 相关测试全部失败。

---

## 二、日志警告分析

### 2.1 警告统计

| 类型 | 数量 | 来源 |
|------|------|------|
| 总 warning 数 | 470 | - |
| VideoThread 相关 | 77 | VideoThread.cpp |
| PlayController 相关 | 401 | PlayController.cpp |

### 2.2 警告类型与根因

#### 类型 A: `VideoThread::run: renderFrame failed 100/200/.../800 times`

- **出现**: 8 次（100 到 800 递增）
- **时间**: 15:15:29 ~ 15:15:33（Seek 执行阶段）
- **含义**: VideoThread 持续 decode 成功，但 `renderFrame()` 返回 false

**renderFrame 返回 false 的可能原因**（见 `VideoThread.cpp:580-794`）:  
1. `controller_->isSeeking()` 为 true → 持续处于 seeking 状态  
2. `renderer_->render()` 失败  
3. `renderer_` 为 null  
4. 帧 PTS 无效或视频超前过多被跳过  

**推断**: Seek 后 `isSeeking()` 可能长时间未清除，或 Seek 流程异常导致 VideoThread 无法正常渲染，最终可能引发崩溃。

#### 类型 B: `VideoThread::renderFrame: Frame expired (lag=40xxxms), forcing render`

- **出现**: 69 次
- **含义**: 视频滞后主时钟约 40 秒，音视频严重不同步
- **对应**: 与 BUG-002 音视频同步问题一致

#### 类型 C: `PlayController::getCurrentPositionMs: Position (42473 ms) exceeds duration (42459 ms), clamping to duration`

- **出现**: 401 次
- **含义**: 主时钟位置超出视频时长（约 14ms）
- **原因**: 可能是 BUG-002 导致的时钟漂移，或播放结束时的边界处理问题

---

## 三、因果链推断

```
停止播放 (00:00:42)
    ↓
小幅前进 Seek
    ↓
Seek 流程触发
    ↓
VideoThread: renderFrame 持续失败（isSeeking? renderer?）
    ↓
进程崩溃（Access violation / 其他）
    ↓
后续 Seek 测试失败（进程已死）
```

---

## 四、与 BUG_FIXES 的映射

| 现象 | BUG 编号 | 说明 |
|------|----------|------|
| Seek 时进程崩溃 | BUG-001, BUG-016 | 播放/切换崩溃，PacketQueue reset 时序 |
| Seek 后无声音/视频停止 | BUG-003 | OpenAL rewind 等 |
| 音视频同步严重偏差 | BUG-002 | 主时钟、AVClock 使用 |
| 播放位置超出时长 | BUG-004 相关 | 进度条/时钟 clamp |

---

## 五、修复优先级建议

1. **P0 - Seek 崩溃**  
   - 定位：`requestSeek` → `PacketQueue` → `VideoThread` → `renderFrame`  
   - 重点：`isSeeking()` 清除时机、seek 完成后的状态恢复、`renderFrame` 失败时的处理

2. **P0 - 音视频同步**  
   - 修复 BUG-002 后，可能改善 Frame expired、Position exceeds duration 等警告

3. **P1 - 进度条/滑块**  
   - 在崩溃修复后，再验证进度条和滑块 UI 逻辑

---

## 六、后续行动

1. ~~使用 **systematic-debugging** 对 Seek 崩溃做根因分析~~ ✅ 已完成
2. ~~按 TDD 流程：先写最小可复现 Seek 崩溃的测试，再实现修复~~ ✅ 已完成
3. ~~验证 BUG-001 修复后，重新运行测试~~ ✅ 2026-02-26 全部通过

## 七、修复结果（2026-02-26）

**修复内容**:
- 移除 `DemuxerThread::requestSeek()` 中的 `Reset()` 调用（根因：未锁线程时清空队列导致 use-after-free）
- 修正 MainWindow Seek 快捷键参数（seekPosMs 而非 seekPosMs/1000）

**测试结果**: 7/7 通过（basic + seek），无崩溃
