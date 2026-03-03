# WZMediaPlayer 渐进式 BUG 修复计划

**日期**: 2026-02-26  
**基于**: baseline_analysis_20260226.md, BUG_FIXES.md  
**目标**: 提升稳定性，维持现有功能，采用 TDD 逐步完善

---

## 一、当前遗留 BUG 与关联分析

### 1.1 用户反馈（2026-02-26）

| 现象 | 可能 BUG | 关联 |
|------|----------|------|
| 任意跳转后无声音 | BUG-003 | 与 BUG-002 时钟相关 |
| 跳转后图像播放速度过快 | BUG-002 | 音视频同步 |
| 跳转后切换下一个视频崩溃 | BUG-001/016 新场景 | Seek 后 stop/openPath 时序 |
| Frame expired (lag=52s) | BUG-002 | 主时钟/基准 PTS 错误 |

### 1.2 BUG 关联图

```
                    ┌─────────────────┐
                    │   BUG-002       │
                    │ 音视频同步/时钟  │
                    └────────┬────────┘
                             │ 影响
         ┌───────────────────┼───────────────────┐
         ▼                   ▼                   ▼
   ┌──────────┐       ┌──────────┐       ┌──────────┐
   │ BUG-003  │       │ BUG-004  │       │ 视频过快  │
   │ Seek无音 │       │ 进度条   │       │ Frame过期 │
   └──────────┘       └──────────┘       └──────────┘

   ┌─────────────────────────────────────────────────┐
   │ BUG-001/016: Seek 后切换视频崩溃                 │
   │ 根因: stop() 时线程/队列状态异常（可能由 A/V     │
   │       不同步导致 EOF/状态机异常）                │
   └─────────────────────────────────────────────────┘
```

### 1.3 修复优先级（综合考虑关联）

| 阶段 | BUG | 说明 | 测试方式 |
|------|-----|------|----------|
| **Phase 1** | BUG-002 | 音视频同步（根因） | pywinauto sync |
| **Phase 2** | BUG-003 | Seek 后无声音 | pywinauto seek+sync |
| **Phase 3** | BUG-001 扩展 | Seek 后切换视频崩溃 | pywinauto 新增用例 |
| **Phase 4** | BUG-004 | 进度条同步 | pywinauto seek |

---

## 二、测试策略：单元测试 vs 集成测试

### 2.1 结论

| 测试类型 | 适用场景 | 工具 | 覆盖范围 |
|----------|----------|------|----------|
| **单元测试** | 单类/单模块逻辑 | Catch2 | PlaybackStateMachine, PacketQueue, AVClock, Frame, FrameBuffer |
| **集成测试** | 多类协作、端到端 | pywinauto | 视频解码、音视频同步、Seek、切换视频 |

### 2.2 单元测试可覆盖

- **PlaybackStateMachine**: 状态转换合法性
- **PacketQueue**: put/get/Reset 线程安全（需 mock 或单线程测试）
- **AVClock**: value()、setInitialValue、reset 逻辑
- **Frame/FrameBuffer**: 数据结构、PTS 计算

### 2.3 集成测试必须覆盖

- **视频解码 + 渲染**: 依赖 FFmpeg、OpenGL、Qt
- **音视频同步**: VideoThread + AudioThread + AVClock + OpenAL
- **Seek 流程**: DemuxerThread + VideoThread + AudioThread + 状态机
- **切换视频**: openPath + stop + 线程清理

**结论**: 视频解码、音视频同步、Seek 后切换等**必须用 pywinauto** 做集成测试。

---

## 三、C++ 单元测试（WZMediaPlayTests）

### 3.1 当前状态

- 已创建 WZMediaPlayTests.vcxproj
- 包含: PlaybackStateMachineTest, ErrorRecoveryManagerTest, PlaylistItemTest, PacketQueueTest, FrameTest, FrameBufferTest
- 问题: 硬编码路径（D:\github\vcpkg, D:\DevTools\）、可能缺少 packet_queue（模板类）
- 未纳入: AVClock、核心解码逻辑

### 3.2 渐进式改进

1. **修复编译**: 使用相对路径或 vcpkg 集成，确保 Catch2 可找到
2. **优先纳入**: AVClockTest（与 BUG-002 直接相关）
3. **后续纳入**: PacketQueue 实例化测试（需指定模板参数）

---

## 四、渐进式修复计划（TDD）

### Phase 1: BUG-002 音视频同步（根因）

**目标**: 修复 Seek 后视频过快、无声音、Frame expired 的根因

**TDD 步骤**:
1. **RED**: 在 pywinauto 中新增/强化 `test_av_sync`：Seek 后 5 秒内音视频应同步（时间标签与主时钟偏差 < 2s）
2. **GREEN**: 分析 getMasterClock、AVClock、OpenAL getClock，修复 Seek 后基准 PTS 与音频时钟对齐
3. **REFACTOR**: 清理日志，验证无回归

**涉及文件**: PlayController.cpp, AVClock.cpp, OpenALAudio.cpp, VideoThread.cpp

---

### Phase 2: BUG-003 Seek 后无声音

**目标**: Seek 后音频恢复播放

**TDD 步骤**:
1. **RED**: pywinauto 用例：Seek 后检测音频是否播放（若可检测）
2. **GREEN**: 确认 OpenAL clear() 中 alSourceRewind，必要时增强
3. **REFACTOR**: 与 BUG-002 一并验证

**涉及文件**: OpenALAudio.cpp, AudioThread.cpp

---

### Phase 3: BUG-001 扩展 - Seek 后切换视频崩溃

**目标**: 播放中 Seek，然后切换下一个视频不崩溃

**TDD 步骤**:
1. **RED**: 新增 pywinauto 用例 `test_seek_then_switch_video`：
   - 打开视频 A，播放
   - Seek 到中间位置
   - 等待 2 秒
   - 切换到视频 B（播放列表下一首或双击）
   - 验证进程存活、无崩溃
2. **GREEN**: 分析 stop() 在 Seek 完成后的时序，确保 setFinished、线程等待、Reset 顺序正确
3. **REFACTOR**: 与 BUG-001 已有修复对齐

**涉及文件**: PlayController.cpp, packet_queue.h, MainWindow.cpp

---

### Phase 4: BUG-004 进度条同步

**目标**: 进度条与主时钟一致

**TDD 步骤**:
1. **RED**: 强化 seek 测试：Seek 后进度条值应在目标位置 ±2s 内
2. **GREEN**: 依赖 BUG-002 修复，检查 onUpdatePlayProcess
3. **REFACTOR**: 清理

---

### Phase 5: C++ 单元测试完善

**目标**: WZMediaPlayTests 可编译运行，覆盖 AVClock 等核心逻辑

**步骤**:
1. 修复 vcxproj 路径，确保 Catch2 可用
2. 添加 AVClockTest（若 AVClock 可独立测试）
3. 将单元测试纳入 CI/本地验证流程

---

## 五、pywinauto 测试用例增强

### 5.1 新增用例

| 用例名 | 类别 | 目的 |
|--------|------|------|
| test_seek_then_switch_video | seek/playlist | 复现 Seek 后切换崩溃 |
| test_seek_av_sync_within_5s | sync | 验证 Seek 后 5 秒内 A/V 同步 |
| test_seek_audio_resumes | sync | 验证 Seek 后音频恢复（若可检测） |

### 5.2 现有用例强化

- `test_av_sync`: 增加 Seek 后同步判定
- `test_progress_and_seek`: 增加 Seek 后进度条位置验证

---

## 六、执行顺序与检查点

| 步骤 | 内容 | 检查点 |
|------|------|--------|
| 1 | Phase 1: BUG-002 修复 | sync 测试通过，Frame expired 减少 |
| 2 | Phase 2: BUG-003 修复 | Seek 后有声音（用户/测试验证） |
| 3 | Phase 3: Seek 后切换崩溃修复 | test_seek_then_switch_video 通过 |
| 4 | Phase 4: BUG-004 验证 | 进度条与时间一致 |
| 5 | Phase 5: C++ 单元测试 | WZMediaPlayTests 编译通过 |

---

## 七、风险与应对

| 风险 | 应对 |
|------|------|
| BUG-002 修复影响面大 | 小步修改，每步运行 full 测试 |
| Seek 后切换崩溃根因复杂 | 使用 systematic-debugging，添加诊断日志 |
| C++ 测试路径依赖 | 考虑 vcpkg manifest 或 CMake 统一管理 |

---

## 八、已实施修复（2026-02-27）

### Seek 后切换视频崩溃

**修复**: 在 `PlayController::stop()` 开始时立即调用 `vPackets_.setFinished()` 和 `aPackets_.setFinished()`，在 stopThread 之前唤醒所有等待队列的线程。

**新增测试**: `test_seek_then_switch_video`（类别 `seek_switch`）

**验证**: 运行 `python unified_closed_loop_tests.py --categories seek_switch`
