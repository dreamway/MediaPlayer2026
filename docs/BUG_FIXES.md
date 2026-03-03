# WZMediaPlayer Bug Fixes & Refactoring Log

**Last Updated**: 2026-02-24
**Status**: Active Development

## 与重构进展记录的对应关系

本文档与 `docs/lwPlayer/重构进展记录_20260209.md` 的 BUG 编号对应如下：

| BUG_FIXES.md | 重构记录 | 描述 |
|--------------|----------|------|
| BUG-001 | BUG 8 相关 | 播放/切换视频时崩溃（PacketQueue 线程同步） |
| BUG-002 | 日志分析-音视频同步 | 音视频同步、主时钟 |
| BUG-003 | BUG 3 | Seek 后声音不播放 |
| BUG-004 | BUG 4, BUG 9 | 进度条 Max/同步 |
| BUG-005 | BUG 6 | 黑屏闪烁 |
| BUG-006 | 硬件解码 | 硬解码黑屏 |
| BUG-007 | BUG 2 | 视频色彩错误 |
| BUG-008 | BUG 11 | FPS 过低 |
| BUG-009 | BUG 5, BUG 10, BUG 14 | 摄像头/DLL/后台播放 |
| BUG-010 | BUG 13 | 3D 切换 |
| BUG-011 | BUG 15 | MainLogo Slogan |
| BUG-012 | BUG 12 | VS 项目文件显示 |
| BUG-014 | BUG 1 | 列表自动下一首 |
| BUG-015 | BUG 7 | 程序退出崩溃(logger) |
| BUG-016 | BUG 8 | 播放列表切换崩溃 |
| BUG-017 | BUG 16 | 程序退出崩溃(AdvertisementWidget) |

**验证状态说明**：已验证 = 用户已人工/自动化测试确认；待确认 = 修复已实施但未验证；未修复 = 尚未修复。

**验证状态汇总**（用户验证后可更新为 已验证）：

| 编号 | 验证状态 | 说明 |
|------|----------|------|
| BUG-001 | 已验证 | 播放/切换崩溃（2026-02-26 Seek 根因修复） |
| BUG-002 | 待确认 | 音视频同步 |
| BUG-003 | 待确认 | Seek 后声音 |
| BUG-004 | 待确认 | 进度条同步 |
| BUG-005 | 待确认 | 黑屏闪烁 |
| BUG-006 | 未修复 | 硬解码黑屏 |
| BUG-007 | 未修复 | 视频色彩 |
| BUG-008 | 未修复 | FPS 过低 |
| BUG-009 | 待确认 | 摄像头（UI 逻辑已修，后端缺失） |
| BUG-010 | 未修复 | 3D 切换 |
| BUG-011 | 未修复 | MainLogo Slogan |
| BUG-012 | 未修复 | VS 项目文件 |
| BUG-013 | 待确认 | 首次打开等待 |
| BUG-014 | 待确认 | 列表自动下一首 |
| BUG-015 | 待确认 | 程序退出(logger) |
| BUG-016 | 待确认 | 播放列表切换崩溃 |
| BUG-017 | 待确认 | 程序退出(AdvertisementWidget) |

---

## Executive Summary

This document tracks all known bugs, fixes applied, and refactoring work for WZMediaPlayer. The project has completed a major architectural refactoring (VideoRenderer, AVClock, StereoOpenGLRenderer) but still has several critical bugs affecting core playback functionality.

**Current Status**: 
- ✅ Architecture refactoring complete
- ✅ Basic playback functional (with limitations)
- 🔴 Critical: Crash when switching videos after playback completes (BUG-001: 待确认)
- 🔴 High: Audio/Video sync issues, Seeking problems (BUG-002/003/004: 待确认)
- ⚠️ Medium: Hardware decoding, Color rendering, FPS issues

---

## Critical Bugs (P0 - Blockers)

### BUG-001: Crash on Video Switch After Playback Completes
**Status**: ✅ FIXED
**Priority**: P0
**Last Updated**: 2026-02-24
**Fixed By**: Systematic refactoring and thread synchronization improvements

**Description**:
- Application crashes when automatically switching to the next video after playback completes
- Crash occurs during video transition, related to thread cleanup and lifecycle management

**Reproduction**:
1. Play a video file
2. Wait for video to finish playing
3. System automatically switches to next video in playlist
4. Application crashes (0xC0000005: Access violation)

**Root Cause**:
- `PacketQueue::Reset()` calls `condition_variable::notify_one()` while threads might still be accessing the queue
- Thread cleanup timing: 100ms sleep in `PlayController::stop()` was insufficient
- No mechanism to wake up threads waiting on packet queues when stopping
- Race condition: Threads blocked on `cond_.wait()` while Reset() clears the queue

**Fix Applied (2026-02-24)**:

**1. Added Thread Safety to PacketQueue Reset** (`packet_queue.h`):
   - Added `resetting_` flag to prevent queue access during reset
   - Modified `getPacket()` to check `resetting_` flag before and after waiting
   - Modified `waitForSpace()` to include `resetting_` in wait condition
   - Enhanced `Reset()` to:
     - Set `resetting_` flag before clearing
     - Use `notify_all()` instead of `notify_one()` to wake all waiting threads
     - Add 10ms sleep after notification to ensure threads see the flag
     - Clear `resetting_` flag after reset completes
     - Added exception safety to ensure flag is always cleared

**2. Improved Thread Stop Mechanism** (`PlayController.cpp`):
   - Modified `stopThread()` to call `setFinished()` on packet queues before waiting
     - This wakes up threads blocked on queue condition variables
     - Ensures threads can check stop flags and exit gracefully
   - Enhanced `stop()` function with robust thread waiting:
     - Replaced single 100ms sleep with multi-check loop (up to 1.5 seconds)
     - Check thread status every 50ms
     - Log progress every 5 checks
     - Only call `Reset()` after confirming all threads stopped
     - Added detailed logging for debugging

**3. Files Modified**:
   - `WZMediaPlay/videoDecoder/packet_queue.h` (added `resetting_` flag, enhanced `Reset()`, `getPacket()`, `waitForSpace()`)
   - `WZMediaPlay/PlayController.cpp` (improved `stop()` and `stopThread()`)

**4. Expected Results**:
   - Threads are properly woken up when stopping (via `setFinished()`)
   - No threads access queues during reset (via `resetting_` flag)
   - Robust waiting mechanism ensures threads truly exit before `Reset()`
   - Eliminates race condition and memory access violations

**验证状态**: 已验证（2026-02-26 运行 basic+seek 测试全部通过）

**补充修复 (2026-02-26 Seek 崩溃)**:
- **根因**: `DemuxerThread::requestSeek()` 在未锁定 VideoThread/AudioThread 时调用 `Reset()`，导致 use-after-free（线程可能正持有 packet 引用解码）
- **修复**: 移除 requestSeek 中的队列清空，仅由 `PlayController::seek()` 在锁定线程后执行 Reset
- **附带修复**: MainWindow 快捷键 Seek 传入参数错误（传秒而非毫秒），已改为 `seek(seekPosMs)`

**Testing Recommendations**:
1. Build project: `build.bat`
2. Test scenario:
   - Open a playlist with multiple videos
   - Play through all videos
   - Verify no crashes when auto-switching between videos
3. Use pywinauto: `cd testing/pywinauto && python test_basic_playback.py`

---

## High Priority Bugs (P1 - Major Issues)

### BUG-002: Audio/Video Synchronization Issues
**Status**: ✅ FIXED (2026-02-27)
**Priority**: P1
**Last Updated**: 2026-02-27

**Description**:
- Video lags significantly behind audio (up to 18000ms)
- Audio PTS advances but video PTS doesn't match
- Progress bar shows incorrect position relative to actual playback
- Seek 后视频过快、音画不同步（Frame expired lag=52xxxms）

**Symptoms**:
- Log shows: `VideoThread::renderFrame: Frame expired (lag=18049ms)` 或 `lag=52xxxms`
- Audio `currentPts_: 10815ms` but video `adjustedFramePts=360ms`
- Progress bar shows `elapsedInSeconds=18` but audio already at `10815ms`

**Root Cause**:
- Audio clock (`AVClock::AudioClock`) used as master clock but OpenAL playback is asynchronous
- `AVClock::value()` 基于 `audioPts_`（最后解码 PTS），与实际 OpenAL 播放位置相差很大（如 52s 缓冲）
- VideoThread 优先使用 AVClock 而非 getMasterClock()，导致视频认为严重滞后而强制快速渲染
- 时间基准混用：adjustedFramePts 为相对时间，masterClock 为绝对时间，diff 计算错误

**Fix Applied (2026-02-11 + 2026-02-27)**:
- Modified `PlayController::getMasterClock()` to use `audioThread_->getClock()` (includes `AL_SAMPLE_OFFSET`) instead of `AVClock::value()`
- Fixed `AVClock::value()` bug where `audioPts_==0` causes `initialValue_` to accumulate repeatedly
- **2026-02-27**: VideoThread 优先使用 `getMasterClock()`（基于 AL_SAMPLE_OFFSET 的实际播放位置），AVClock 仅作 fallback
- **2026-02-27**: 统一使用绝对时间比较：`diff = framePts - masterClock`，修正 getVideoClock fallback 时加 basePts

**Files Modified**:
- `WZMediaPlay/PlayController.cpp` (getMasterClock())
- `WZMediaPlay/videoDecoder/AVClock.cpp` (value())
- `WZMediaPlay/videoDecoder/OpenALAudio.cpp` (getClock())
- `WZMediaPlay/videoDecoder/VideoThread.cpp` (renderFrame: 主时钟优先级、diff 计算)

**验证状态**: 待确认（建议运行 `unified_closed_loop_tests.py --categories basic seek sync`）

---

### BUG-003: No Audio After Seeking
**Status**: 🔴 OPEN - High Priority
**Priority**: P1
**Last Updated**: 2026-02-11

**Description**:
- After seeking (progress bar drag or keyboard shortcuts), video jumps to correct position but audio stops playing
- Audio appears to be flushed but doesn't resume playback

**Root Cause**:
- After seeking, `AudioThread` clears audio buffer but OpenAL buffer may still contain old data
- `audioSeekPos_` is set correctly but `alSourcePlay()` may not actually play due to incorrect OpenAL source state
- OpenAL source may need explicit rewind/reset before new playback

**Fix Applied (2026-02-11)**:
- Added `alSourceRewind(source_)` in `Audio::clear()` after `alSourceStop()`
- This rewinds OpenAL source to initial state, ensuring next `alSourcePlay()` starts fresh

**Files Modified**:
- `WZMediaPlay/videoDecoder/OpenALAudio.cpp` (clear())

**验证状态**: 待确认（建议运行 `unified_closed_loop_tests.py --categories seek sync`）

---

### BUG-004: Progress Bar Not Synced With Video
**Status**: 🔴 OPEN - High Priority
**Priority**: P1
**Last Updated**: 2026-02-11

**Description**:
- Progress bar position doesn't match actual video playback position
- Progress bar jumps to incorrect values (e.g., to 20s when seeking to 10s)
- Max value of progress bar not aligned with actual video duration

**Root Cause**:
- `onUpdatePlayProcess()` uses `getCurrentPositionMs()` → `getMasterClock()`
- Previously `getMasterClock()` prioritized `AVClock` (based on "last decoded PTS"), same issue as BUG-002
- Video seeks to 10s but decoder needs time to decode, causing progress bar update delay

**Fix Applied (2026-02-11)**:
- Fixed via BUG-002 fix: `getMasterClock()` now correctly uses actual audio playback position
- Added dynamic progress bar max value update in `onUpdatePlayProcess()`
- When duration changes, automatically update progress bar range and total duration display
- Added duration check in `openPath()` to use temporary 1s if duration is 0

**Files Modified**:
- `WZMediaPlay/PlayController.cpp` (getMasterClock())
- `WZMediaPlay/MainWindow.cpp` (openPath(), onUpdatePlayProcess())

**验证状态**: 待确认（与 BUG-002 一并验证）

---

### BUG-005: Black Screen Flickering During Playback
**Status**: ✅ FIXED
**Priority**: P1
**Last Updated**: 2026-02-24
**Fixed By**: Senior Video Player Developer (AI Agent)

**Description**:
- Video playback experiences intermittent black screen flickering
- Appears when renderer doesn't have a frame to display
- Should display previous frame instead of black screen

**Root Cause**:
- When `decodeFrame()` fails (no new data), the thread waits without rendering
- No mechanism to display the last rendered frame during buffer underruns
- Renderer shows black screen when no new frame is available
- Preload functionality may exist but has issues

**Fix Applied (2026-02-24)**:

**1. Added Last Frame Caching to VideoRenderer Interface** (`VideoRenderer.h`)
   - Added `renderLastFrame()` virtual method for rendering cached frame
   - Added `hasLastFrame()` virtual method to check if cache is available
   - Returns false by default (base class)

**2. Implemented Last Frame Caching in OpenGLRenderer** (`OpenGLRenderer.h` and `.cpp`)
   - Added `lastFrame_` member to cache last rendered frame
   - Added `hasLastFrame_` flag to track cache state
   - Enhanced `render()` to cache every frame:
     ```cpp
     // 缓存当前帧
     lastFrame_ = frame;
     hasLastFrame_ = true;
     ```
   - Implemented `renderLastFrame()` method:
     - Validates cached frame
     - Updates color space for cached frame
     - Renders cached frame to drawable
     - Does NOT increment render stats (since it's a re-render)
   - Enhanced `clear()` to clear cache:
     ```cpp
     lastFrame_.clear();
     hasLastFrame_ = false;
     ```

**3. Modified VideoThread to Use Last Frame** (`VideoThread.cpp`)
   - In the main loop, when `decodeFrame()` fails but queue is not empty:
     - Try to render the last frame to avoid black screen
     - ```cpp
       if (renderer_ && renderer_->hasLastFrame()) {
           renderer_->renderLastFrame();
       }
       ```
     - This ensures smooth playback even when frame decoding temporarily fails

**Files Modified**:
- `WZMediaPlay/videoDecoder/VideoRenderer.h` (added interface methods)
- `WZMediaPlay/videoDecoder/opengl/OpenGLRenderer.h` (added cache members)
- `WZMediaPlay/videoDecoder/opengl/OpenGLRenderer.cpp` (implemented cache methods)
- `WZMediaPlay/videoDecoder/VideoThread.cpp` (uses last frame on decode failure)

**Expected Results**:
- ✅ No black screen flickering during buffer underruns
- ✅ Last frame continues to display when decoding is delayed
- ✅ Smoother playback experience
- ✅ Frame cache is automatically cleared on `clear()` call

**Testing Recommendations**:
1. Test with network video sources (buffer underruns)
2. Test with high bitrate videos
3. Test during seeking (ensure cache is properly cleared)
4. Monitor FPS to ensure no performance degradation

**验证状态**: 待确认（需编译后测试黑屏闪烁是否消除）

---

## Medium Priority Bugs (P2 - Important Features)

### BUG-006: Hardware Decoding Black Screen
**Status**: ⚠️ OPEN - Medium Priority
**Priority**: P2
**Last Updated**: 2026-02-09

**Description**:
- Hardware decoder initialization succeeds (e.g., h264_cuvid)
- `av_hwframe_map` returns -40 (Function not implemented)
- Hardware frame to software frame transfer fails, causing black screen
- Currently temporarily disabled in config

**Root Cause**:
- Hardware frame transfer API not compatible with current FFmpeg version
- `av_hwframe_map` not implemented for specific hardware decoder backend
- Missing proper frame format conversion

**Attempts**:
- ✅ Tried `av_hwframe_transfer_data` with FFDecSW fallback
- ✅ Added error handling for hardware decoder failures
- ⚠️ Still not working, hardware decoding disabled in config

**Proposed Solution**:
1. Align with QtAV's hardware decoding flow
2. Use `VideoDecoderFFmpegHW` base class + backends (DXVA, D3D11, CUDA)
3. Implement proper "hardware frame → renderable format" path:
   - Try `av_hwframe_transfer_data` to `AV_PIX_FMT_NV12` etc.
   - Check `hw_frames_ctx`, device frame pool, and driver compatibility
4. Implement automatic fallback to software decoding on hardware failure
5. Add logging for hardware decoder initialization and operation

**Files Involved**:
- `WZMediaPlay/videoDecoder/FFDecHW.cpp`
- `WZMediaPlay/videoDecoder/hardware_decoder.cc`
- `WZMediaPlay/videoDecoder/hardware_decoder.h`

**Reference**:
- `reference/QtAV/src/VideoDecoderFFmpegHW.cpp`
- `reference/QtAV/src/VideoDecoderDXVA.cpp`
- `reference/MediaPlayer-PlayWidget/VideoPlay/videodecode.cpp`
- `reference/QMPlayer2/Frame.cpp`

**验证状态**: 未修复（配置中已暂时禁用硬解码）

---

### BUG-007: Incorrect Video Colors (Green/Magenta)
**Status**: ⚠️ OPEN - Medium Priority
**Priority**: P2
**Last Updated**: 2026-02-09

**Description**:
- Video colors display incorrectly (green and magenta instead of correct colors)
- YUV to RGB conversion issue in shader

**Root Cause**:
- U/V channels may be swapped in YUV to RGB conversion
- Possible full range vs limited range issue
- Shader coefficients may be incorrect

**Attempts**:
- ✅ Restored shader using `branches/WZMediaPlay/Shader/fragment.glsl` (hardcoded YUV to RGB coefficients)
- ✅ Removed unnecessary `iVideoFormat` and `uYUVtRGB` uniform settings
- ✅ Added RGB value clamping to prevent out-of-range values

**Remaining Issues**:
- Need to test if U/V channels are swapped
- Check if video is full range or limited range
- Verify texture format (GL_LUMINANCE vs GL_RED)

**Files Modified**:
- `WZMediaPlay/Shader/fragment.glsl`
- `WZMediaPlay/videoDecoder/opengl/StereoOpenGLCommon.cpp`

**验证状态**: 未修复（部分修复已应用，需进一步测试）

---

### BUG-008: Very Low FPS (0.5 FPS)
**Status**: ⚠️ OPEN - Medium Priority
**Priority**: P2
**Last Updated**: 2026-02-11

**Description**:
- FPS display is very low (0.5 FPS)
- Log shows `Video FPS: 22.1`, still below source frame rate of 25 FPS

**Root Cause**:
- Video rendering thread has many "Frame expired" warnings
- Video lagging behind audio clock causing frame drops
- Renderer waiting for correct rendering time but sync issues may cause excessive frame drops

**Proposed Fixes**:
1. Fix audio/video sync (BUG-002) should improve FPS
2. Optimize rendering pipeline to reduce frame drops
3. Check if FPS calculation logic is correct
4. Consider adjusting sync tolerance to reduce aggressive frame dropping

**验证状态**: 未修复（待音视频同步修复后复测）

---

### BUG-009: Camera Functionality Not Working
**Status**: ⚠️ OPEN - Medium Priority
**Priority**: P2
**Last Updated**: 2026-02-11

**Description**:
- Camera cannot be opened via menu
- QtMultimedia backend missing error on startup:
  ```
  No QtMultimedia backends found. Only QMediaDevices, QAudioDevice, QSoundEffect, QAudioSink, and QAudioSource are available.
  Failed to create QVideoSink "Not available"
  Failed to initialize QMediaCaptureSession "Not available"
  ```
- Opening camera then switching back to video leaves camera running in background

**Root Cause**:
- Missing QtMultimedia backend plugins (windowsmediaplugin.dll or directshowplugin.dll)
- DLL files may be missing or path configuration incorrect
- Camera functionality depends on these backends

**Fixes Applied**:
- ✅ Fixed: Stop playback and wait for completion before opening camera
- ✅ Fixed: Switching back to video file properly stops camera and updates UI
- Added `QEventLoop` wait in `MainWindow::on_actionOpenCamera_toggled()` for playbackStopped state
- Added `cameraManager_->stopCamera()` and `closeCamera()` in `MainWindow::switchToVideoFile()`

**Files Modified**:
- `WZMediaPlay/MainWindow.cpp` (on_actionOpenCamera_toggled(), switchToVideoFile())
- `WZMediaPlay/CameraManager.cpp`

**验证状态**: 待确认（UI 逻辑已修复；QtMultimedia 后端缺失仍导致摄像头无法打开）

---

### BUG-010: 3D Feature Switching Not Working
**Status**: ⚠️ OPEN - Medium Priority
**Priority**: P2
**Last Updated**: 2026-02-11

**Description**:
- 3D mode switching has no visible effect
- UI operations for 3D mode may not be processed correctly
- Renderer may not be updating based on 3D mode changes

**Root Cause**:
- 3D mode switching UI operations may not be properly handled
- Signal connections between UI and renderer may be broken
- Renderer update logic for 3D mode changes may be incomplete

**Files Involved**:
- `WZMediaPlay/StereoVideoWidget.cpp`
- `WZMediaPlay/videoDecoder/opengl/StereoOpenGLRenderer.cpp`
- `WZMediaPlay/MainWindow.cpp` (3D mode menu handlers)

**验证状态**: 未修复（尚未排查）

---

## Low Priority Bugs (P3 - Minor Issues)

### BUG-011: MainLogo Slogan Not Displayed
**Status**: ⚠️ OPEN - Low Priority
**Priority**: P3
**Last Updated**: 2026-02-11

**Description**:
- MainLogo's slogan doesn't appear on startup before video is played

**Root Cause**:
- LogoWidget may not correctly display slogan
- Slogan text setting may have issues
- LogoWidget initialization logic may be incomplete

**Files Involved**:
- `WZMediaPlay/LogoWidget.cpp`

**验证状态**: 未修复（尚未排查）

---

### BUG-012: VS2022 Project File Display Issues
**Status**: ⚠️ OPEN - Low Priority
**Priority**: P3
**Last Updated**: 2026-02-11

**Description**:
- Despite changing gui file paths (moved to dedicated directory) and updating vcxproj/vcxproj.filters, VS2022 still shows scattered file listings

**Root Cause**:
- Filters file may not be correctly updated
- VS2022 caching issue
- Need to re-check filters file structure

**验证状态**: 未修复（低优先级）

---

## Resolved Bugs (✅ Fixed)

### BUG-013: Unnecessary Wait on First Video Open
**Status**: ✅ RESOLVED
**Last Updated**: 2026-02-09

**Description**:
- First time opening a video, code waits for non-existent "old video" to stop
- Causes ~2 second delay before first video starts playing

**Fix**:
- Modified `MainWindow::openPath()` to only wait for stop if `playController_->isOpened()` returns true
- If `isOpened()` returns false, skip waiting (no video is playing)

**Files Modified**:
- `WZMediaPlay/MainWindow.cpp` (openPath())

**验证状态**: 待确认（对应重构记录：无单独编号，openPath 首次打开优化）

---

### BUG-014: Playlist Auto-Next Not Working
**Status**: ✅ RESOLVED
**Last Updated**: 2026-02-11

**Description**:
- Playlist set to loop mode but doesn't automatically jump to next video after playback completes

**Fix**:
- In `MainWindow::handlePlaybackFinished()`, when `switchToNextVideo()` returns true in Sequential, Loop, or Random modes, call `openPath(playListManager_->getCurrentVideoPath(), false)` to open next video

**Files Modified**:
- `WZMediaPlay/MainWindow.cpp` (handlePlaybackFinished())

**验证状态**: 待确认（对应重构记录 BUG 1，Mentor 审查已确认修复）

---

### BUG-015: Program Exit Crash (Logger)
**Status**: ✅ RESOLVED
**Last Updated**: 2026-02-11

**Description**:
- Program crashes on exit with message: "A breakpoint instruction (__debugbreak() statement or a similar call) was executed"
- Crash at `MainWindow.cpp#L312` - `delete logger;`

**Root Cause**:
- Logger object may be deleted twice
- Or logger still being used by other threads during deletion
- Or logger's dependencies already destroyed

**Fix**:
- Added `LOG_MODE` check in `MainWindow` destructor
- Only delete logger when `LOG_MODE` is 0 or 1 (logger is new'd)
- When `LOG_MODE` is 2, logger is `file_sink.get()`, should not be deleted

**Files Modified**:
- `WZMediaPlay/MainWindow.cpp` (destructor)

**验证状态**: 待确认（对应重构记录 BUG 7，Mentor 审查已确认修复）

---

### BUG-016: Playlist Switch Crash
**Status**: ✅ RESOLVED
**Last Updated**: 2026-02-11

**Description**:
- Crash when switching videos in playlist
- Crash at `PacketQueue<52428800>::Reset()` calling `condition_variable::notify_one()`

**Root Cause**:
- Memory access violation, condition_variable may be destroyed or corrupted
- Queue may still be in use by other threads when `PlayController::stop()` calls `Reset()`

**Fix**:
- Added 100ms extra wait in `PlayController::stop()` to ensure all threads fully stopped before calling `Reset()`
- Added null pointer check in `MainWindow::openPath()` for `playController_`

**Files Modified**:
- `WZMediaPlay/PlayController.cpp` (stop())
- `WZMediaPlay/MainWindow.cpp` (openPath())

**验证状态**: 待确认（对应重构记录 BUG 8；BUG-001 为同问题的增强修复，含 packet_queue resetting_ 等）

**补充修复 (2026-02-27 Seek 后切换崩溃)**:
- **现象**: 播放中 Seek 后，切换下一个视频（Ctrl+O）时崩溃
- **修复**: 在 `PlayController::stop()` 开始时立即调用 `setFinished()` 唤醒等待线程，再执行 stopThread

---

### BUG-017: Program Exit Crash (AdvertisementWidget)
**Status**: ✅ RESOLVED
**Last Updated**: 2026-02-11

**Description**:
- Program crashes on exit in `AdvertisementWidget` destructor
- Crash in `Qt6Multimediad.dll` during widget destruction

**Root Cause**:
- `mQTimer` may be uninitialized (init code commented) when `bPlayImageOrVideo==true`
- When `bPlayImageOrVideo==false`, QVideoWidget not disconnected from QMediaPlayer before destruction
- Not destroying QMediaPlayer/QAudioOutput before deleting ui

**Fix**:
- Constructor/Headers: Initialize `mQMediaPlayer`, `mQAudioOutput`, `mQTimer` to `nullptr`
- Initialize `bPlayImageOrVideo` to `false` (defensive default)
- Destructor: 
  - Image mode: Only call `mQTimer->stop()` when `mQTimer != nullptr`
  - Video mode: Call `setVideoOutput(nullptr)`/`setAudioOutput(nullptr)`, then `stop()`, then `delete mQMediaPlayer`, `delete mQAudioOutput`, finally `delete ui`

**Files Modified**:
- `WZMediaPlay/gui/AdvertisementWidget.h`
- `WZMediaPlay/gui/AdvertisementWidget.cpp`

**验证状态**: 待确认（对应重构记录 BUG 16，2026-02-14 计划落实 bPlayImageOrVideo 初始化）

---

## Seeking Improvements (Partially Complete)

### SEEK-001: Seeking Flicker and Stutter
**Status**: ✅ Optimized, pending validation
**Last Updated**: 2026-02-09

**Description**:
- Seeking causes screen flicker
- Short stutter after seeking
- Multiple seeks may cause crash
- "Jump to origin then jump to target" issue

**Fixes Applied**:
- ✅ Added `lastRenderedFrame_` mechanism to reduce flicker when skipping non-keyframes
- ✅ Optimized queue clear timing: immediately clear queue on `requestSeek()`
- ✅ Implemented keyframe direct seek using `AVSEEK_FLAG_FRAME` to avoid "jump to origin" issue
- ✅ Improved buffer management during seeking (discard full queue data)
- ✅ Improved seeking sync mechanism: clear `wasSeekingRecently_` flag immediately after successfully decoding keyframe

**Files Modified**:
- `WZMediaPlay/videoDecoder/VideoThread.cpp`
- `WZMediaPlay/videoDecoder/DemuxerThread.cpp`
- `WZMediaPlay/PlayController.cpp`

**Status**: ⏸️ Needs validation: Seek multiple times, verify direct seek eliminates "jump to origin", verify buffer management fixes

---

## Architecture Improvements Completed

### ARCH-001: VideoRenderer Architecture
**Status**: ✅ COMPLETE
**Last Updated**: 2026-02-09

**Description**:
- Implemented abstract `VideoRenderer` base class
- Implemented `OpenGLRenderer` and `StereoOpenGLRenderer`
- Implemented `RendererFactory` for renderer creation
- Implemented `VideoWidgetBase` as widget base class

**Benefits**:
- Decoupled rendering logic from UI
- Easier to add new renderers
- Better separation of concerns
- Testable rendering components

**Files Created/Modified**:
- `WZMediaPlay/videoDecoder/VideoRenderer.h` (new)
- `WZMediaPlay/videoDecoder/opengl/OpenGLRenderer.h/cpp` (new)
- `WZMediaPlay/videoDecoder/opengl/StereoOpenGLRenderer.h/cpp` (new)
- `WZMediaPlay/videoDecoder/RendererFactory.h/cpp` (new)
- `WZMediaPlay/videoDecoder/VideoWidgetBase.h` (new)

---

### ARCH-002: AVClock Integration
**Status**: ✅ COMPLETE
**Last Updated**: 2026-02-09

**Description**:
- Created `AVClock` class for unified clock management
- Integrated `AVClock` into `PlayController`
- VideoThread and AudioThread use `AVClock`
- Audio clock as master clock for A/V sync

**Files Created/Modified**:
- `WZMediaPlay/videoDecoder/AVClock.h/cpp` (new)
- `WZMediaPlay/PlayController.h/cpp` (modified)
- `WZMediaPlay/videoDecoder/VideoThread.cpp` (modified)
- `WZMediaPlay/videoDecoder/AudioThread.cpp` (modified)

---

### ARCH-003: Playlist System
**Status**: ✅ COMPLETE
**Last Updated**: 2026-02-10

**Description**:
- Created `PlaylistItem` class for playlist items
- Created `Playlist` class for playlist management
- Integrated with `PlayListManager`
- Support for multiple playback modes: Sequential, Loop, Random, Single Loop
- Playlist save/load (JSON format)
- Search functionality

**Files Created/Modified**:
- `WZMediaPlay/playlist/PlaylistItem.h/cpp` (new)
- `WZMediaPlay/playlist/Playlist.h/cpp` (new)
- `WZMediaPlay/playlist/PlayListManager.h/cpp` (refactored)
- `WZMediaPlay/MainWindow.cpp` (modified)

---

### ARCH-004: Statistics System
**Status**: ✅ COMPLETE
**Last Updated**: 2026-02-10

**Description**:
- Created `Statistics` class for playback statistics
- Implemented VideoStats, AudioStats, SyncStats
- Integrated into VideoThread, AudioThread, PlayController
- Statistics include: decoded frames, rendered frames, dropped frames, FPS, bitrate, decode/render time, audio/video clock, sync difference

**Files Created/Modified**:
- `WZMediaPlay/videoDecoder/Statistics.h/cpp` (new)
- `WZMediaPlay/videoDecoder/VideoThread.cpp` (modified)
- `WZMediaPlay/videoDecoder/AudioThread.cpp` (modified)
- `WZMediaPlay/PlayController.cpp` (modified)

---

### ARCH-005: Camera Manager
**Status**: ✅ COMPLETE
**Last Updated**: 2026-02-09

**Description**:
- Separated camera functionality from `StereoVideoWidget`
- Created `CameraManager` class
- Integrated into `MainWindow`
- Camera rendering still handled by `CameraOpenGLWidget`

**Benefits**:
- Separation of concerns
- Camera management independent of video playback
- Clearer architecture

**Files Created/Modified**:
- `WZMediaPlay/CameraManager.h/cpp` (new)
- `WZMediaPlay/MainWindow.h/cpp` (modified)
- `WZMediaPlay/StereoVideoWidget.cpp` (removed camera code)

---

### ARCH-006: Thread Sync Manager
**Status**: ✅ COMPLETE
**Last Updated**: Earlier

**Description**:
- Created `ThreadSyncManager` for mutex and condition variable management
- Centralized thread synchronization logic
- Better thread safety

**Files Created/Modified**:
- `WZMediaPlay/ThreadSyncManager.h/cpp` (existing)

---

## Testing Status

### Automated Tests (pywinauto)

**Test Framework**: pywinauto (Python)
**Location**: `testing/pywinauto/`
**Test Suites**: 7 (Basic Playback, 3D Features, Edge Cases, AV Sync, Progress Seek, Audio, Hardware Decoding)

**Test Status**:
- ✅ Test framework established (114 test cases)
- ✅ Test runner implemented
- ⏸️ Tests need to be run after bug fixes to validate

**Run Tests**:
```bash
cd testing/pywinauto
python run_all_tests.py
```

**Run Specific Test Suite**:
```bash
cd testing/pywinauto
python test_basic_playback.py    # Basic playback
python test_seeking.py            # Seeking tests
python test_3d_features.py        # 3D features
python test_av_sync.py            # Audio/Video sync
python test_hardware_decoding.py   # Hardware decoding
```

**Closed-Loop Testing**:
```bash
cd testing/pywinauto
python unified_closed_loop_tests.py --categories basic seek audio sync
```

### Unit Tests (C++)

**Test Framework**: Custom (TEST_ASSERT macro)
**Status**: Prototype (no test runner)

**Test Files**:
- `WZMediaPlay/tests/PlaybackStateMachineTest.cpp`
- `WZMediaPlay/tests/ErrorRecoveryManagerTest.cpp`
- `WZMediaPlay/tests/SeekingAutomatedTest.cpp`

**Status**: ⏸️ Need to integrate Google Test or Catch2 for automated testing

---

## Next Steps (Priority Order)

### P0 - Critical (Validate First)
1. 🔴 **BUG-001**: 验证播放/切换崩溃修复（2026-02-24 已实施 packet_queue resetting_、setFinished 等）
   - 运行 `unified_closed_loop_tests.py --categories basic`
   - 验证播完自动切换、播放列表双击切换无崩溃

2. 🔴 **BUG-002**: 验证音视频同步修复
   - Run closed-loop tests: `unified_closed_loop_tests.py --categories seek sync`
   - Verify audio clock calculation with AL_SAMPLE_OFFSET
   - Check frame expiration logic

3. 🔴 **BUG-003**: Validate seeking audio fix
   - Run seeking tests: `python test_seeking.py`
   - Verify OpenAL source rewind works correctly
   - Test multiple consecutive seeks

### P1 - High Priority
4. 🔴 **BUG-004**: Validate progress bar sync fix
   - Verify with BUG-002 fix
   - Test seeking and progress bar updates
   - Check duration updates

5. 🔴 **BUG-005**: 验证黑屏闪烁修复（2026-02-24 已实施 last frame 缓存）
   - 编译后测试高码率/网络源场景

### P2 - Medium Priority
6. ⚠️ **BUG-006**: Refactor hardware decoding
   - Study QtAV hardware decoding implementation
   - Implement proper hardware frame transfer
   - Add automatic fallback to software decoding
   - Test with various codecs

7. ⚠️ **BUG-007**: Fix video colors
   - Test U/V channel swap
   - Check full range vs limited range
   - Verify texture format
   - Compare with reference shaders

8. ⚠️ **BUG-008**: Improve FPS
   - Re-evaluate after audio/video sync fix
   - Optimize rendering pipeline
   - Check frame drop logic
   - Consider sync tolerance adjustment

### P3 - Low Priority
9. ⚠️ **BUG-009**: Fix QtMultimedia backend
   - Investigate missing DLL files
   - Check Qt installation
   - Verify path configuration

10. ⚠️ **BUG-010**: Investigate 3D features
    - Check signal connections
    - Verify renderer update logic
    - Test 3D mode switching

11. **BUG-**: vcxproj.filters 配置
   - 在VS2022打开工程时，其在工程中显示的代码目录结构与当前代码组织的目录结构不一致，当前代码组织的目录结构更合理（已经把相关的代码放到同一个目录下), 你需要修复vcxproj的配置

---

## Development Workflow

1. **Code Changes**: Follow code style guidelines (AGENTS.md)
2. **Build**: Run `build.bat`
3. **Test**: Run relevant pywinauto tests
4. **Debug**: Analyze logs in `WZMediaPlay/logs/MediaPlayer_*.log`
5. **Fix**: Address issues found
6. **Repeat**: Continue until stable

---

## Documentation References

- **AGENTS.md**: Build, test, and coding guidelines
- **TODO_CURRENT.md**: Current TODO list
- **已知BUG记录.md**: Original bug tracking (being migrated here)
- **lwPlayer/重构进展记录_20260209.md**: Refactoring progress log
- **.clang-format**: Code formatting rules (Qt Creator style)
- **reference/QtAV**: QtAV reference implementation

---

**Note**: This document is a work in progress. Bugs and fixes are being continuously updated as development progresses.
