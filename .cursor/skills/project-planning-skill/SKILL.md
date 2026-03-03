---
name: "project-planning-skill"
description: "Project planning and documentation management. Invoke when user needs to update TODO.md, create development plans, organize project docs, or analyze code structure."
---

# Project Planning Skill - Project Planning & Documentation

## Overview

This skill manages WZMediaPlayer project planning and documentation maintenance, including TODO updates, progress tracking, and code structure analysis.

## Project Documentation Structure

```
WZMediaPlayer_2025/
├── docs/
│   ├── TODO.md              # Task list (core doc)
│   ├── ARCHITECTURE.md      # Architecture doc
│   └── CHANGELOG.md         # Changelog
├── WZMediaPlay/
│   ├── videoDecoder/        # Core decoder module
│   ├── PlayController.cpp   # Play controller
│   ├── MainWindow.cpp       # Main window
│   └── ...
└── testing/
    └── pywinauto/           # Automated testing
```

## TODO.md Management

### Location
`E:\WZMediaPlayer_2025\docs\TODO.md`

### Priority Definitions

| Priority | Icon | Description |
|----------|------|-------------|
| P0 | 🔴 | Critical bug fix, immediate |
| P1 | 🟡 | Important feature, this week |
| P2 | 🟢 | Optimization, next week |
| P3 | 🔵 | Low priority, future plan |

### Status Definitions

| Status | Description |
|--------|-------------|
| Pending | Not started |
| 🔄 In Progress | Working on it |
| ✅ Fixed/Done | Completed and verified |
| ⏸️ Paused | Temporarily on hold |

### Task Template

```markdown
### P{x}.{y} Task Name 🔴
**Priority**: Highest/High/Medium/Low
**Status**: Pending/In Progress/Fixed
**Description**: Brief description

**Root Cause Analysis**:
- Cause 1
- Cause 2

**Fix Plan**:
- [ ] Step 1
- [ ] Step 2
- [ ] Verification

**Related Files**:
- `filepath` (line range)

**Fix Details**:
```cpp
// Before:
code

// After:
code
```
```

## Code Structure Analysis

### Core Modules

```
WZMediaPlay/
├── videoDecoder/           # Decoder module
│   ├── DemuxerThread.cpp   # Demux thread
│   ├── VideoThread.cpp     # Video thread
│   ├── AudioThread.cpp     # Audio thread
│   ├── OpenALAudio.cpp     # Audio output
│   ├── FFDecSW.cpp         # Software decode
│   ├── FFDecHW.cpp         # Hardware decode
│   └── opengl/             # OpenGL render
│       ├── OpenGLWidget.cpp
│       └── OpenGLWriter.cpp
├── PlayController.cpp      # Play controller (core)
├── MainWindow.cpp          # Main window/UI
└── PlaybackStateMachine.cpp # State machine
```

### Class Responsibility Matrix

| Class | Responsibility | Key Methods |
|-------|---------------|-------------|
| PlayController | Playback control, thread coordination, clock management | seek(), play(), pause() |
| DemuxerThread | Demux, packet distribution | run(), handleSeek() |
| VideoThread | Video decode, render | decodeFrame(), renderFrame() |
| AudioThread | Audio decode, output | decodeFrame(), writeAudio() |
| OpenALAudio | OpenAL audio output | writeAudio(), getClock() |

## Development Planning

### Short-term Plan (This Week)

1. **P0 Bug Fixes**
   - [ ] Fix seeking audio silent
   - [ ] Fix progress bar sync
   - [ ] Verify fixes

2. **P1 Features**
   - [ ] Optimize AV sync
   - [ ] Improve seeking response

### Medium-term Plan (This Month)

1. **Hardware Decode Optimization**
   - [ ] Auto fallback mechanism
   - [ ] Optimize hardware decode performance

2. **UI Feature Restoration**
   - [ ] 3D feature migration
   - [ ] Settings dialog completion

### Long-term Plan (This Quarter)

1. **Code Refactoring**
   - [ ] Organize directory structure
   - [ ] Complete unit tests

2. **Performance Optimization**
   - [ ] Memory optimization
   - [ ] CPU usage optimization

## Documentation Update Workflow

### Update TODO.md

1. Read current TODO.md
2. Update completed task status
3. Add newly discovered issues
4. Adjust priorities
5. Update version and timestamp
6. Save file

### Add New Bug Record

```markdown
### P0.{n} Bug Name 🔴
**Priority**: Highest
**Status**: Pending
**Description**:

**Root Cause**:
-

**Fix Plan**:
- [ ]

**Related Files**:
-
```

### Update Changelog

```markdown
## [Version] - Date

### Fixed
- Fixed xxx issue

### Added
- Added xxx feature

### Optimized
- Optimized xxx performance
```

## Code Review Checklist

### Review Points

- [ ] Code follows project conventions
- [ ] No memory leak risks
- [ ] Thread safety properly handled
- [ ] Error handling complete
- [ ] Logging appropriate
- [ ] Performance optimized

### Common Issues

| Issue Type | Example | Solution |
|------------|---------|----------|
| Memory leak | `new` without `delete` | Use smart pointers |
| Race condition | Shared data access | Add locks |
| Signal loop | A→B→A cycle | blockSignals |
| Resource leak | File handle not closed | RAII pattern |

## Usage Examples

### Example 1: Update Task Status

```
User: I've fixed P0.1 audio issue
Actions:
1. Open TODO.md
2. Find P0.1 entry
3. Update status to "✅ Fixed"
4. Add fix details
5. Update timestamp
```

### Example 2: Add New Bug

```
User: Found a new seeking issue
Actions:
1. Analyze bug root cause
2. Create P0.x entry
3. Fill problem description
4. List fix plan
5. Link related files
```

### Example 3: Create Iteration Plan

```
User: Plan next week's development
Actions:
1. Review current task status
2. Determine priorities
3. Allocate time resources
4. Update TODO.md
5. Set milestones
```
