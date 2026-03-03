---
name: "wzmediaplayer-skills"
description: "WZMediaPlayer 项目专用技能集合。包含编译、测试、知识记录、文档查询和项目规划五大核心能力。当用户需要处理 WZMediaPlayer 相关任务时，优先引用此技能。"
---

# WZMediaPlayer Skills - 项目技能集合

## 概述

此技能集合为 WZMediaPlayer 项目提供完整的开发支持，包括编译构建、自动化测试、知识管理、文档查询和项目规划五大核心能力。

## 子技能列表

### 1. build-skill - 编译构建
**路径**: `.cursor/skills/build-skill/SKILL.md`

**功能**:
- 运行 `build.bat` 编译项目
- 支持 Debug/Release 配置
- 编译错误诊断

**触发条件**:
- 用户需要编译项目
- 运行 build.bat
- 检查编译错误

---

### 2. test-skill - 自动化测试
**路径**: `.cursor/skills/test-skill/SKILL.md`

**功能**:
- pywinauto GUI 自动化测试
- 测试播放/暂停/seeking 功能
- 日志监控和异常捕获
- 生成测试报告

**触发条件**:
- 用户需要运行测试
- 验证 Bug 修复效果
- 测试 seeking/播放功能

---

### 3. memory-skill - 知识记录
**路径**: `.cursor/skills/memory-skill/SKILL.md`

**功能**:
- 记录 Bug 修复到 graph-memory
- 记录架构决策
- 记录关键配置
- 建立实体关系

**触发条件**:
- 用户需要记录关键信息
- 避免重复 prompt 提示
- 建立项目知识图谱

---

### 4. docs-skill - 文档查询
**路径**: `.cursor/skills/docs-skill/SKILL.md`

**功能**:
- FFmpeg API 查询
- Qt 信号/槽查询
- OpenGL 渲染查询
- 项目代码示例查询

**触发条件**:
- 用户需要查询 API 用法
- 查找函数说明
- 查询技术细节

---

### 5. project-planning-skill - 项目规划
**路径**: `.cursor/skills/project-planning-skill/SKILL.md`

**功能**:
- 更新 TODO.md
- 创建开发计划
- 整理项目文档
- 分析代码结构

**触发条件**:
- 用户需要更新 TODO
- 制定开发计划
- 组织项目文档

---

### 6. git-skill - Git 版本控制
**路径**: `.cursor/skills/git-skill/SKILL.md`

**功能**:
- 执行 git 命令查看代码变更
- 检查文件修改状态
- 查看提交历史
- 比较代码版本差异
- 管理分支状态

**触发条件**:
- 用户需要查看代码变更
- 检查文件修改状态
- 比较代码版本
- 查看提交历史
- 管理 git 分支

---

## 项目信息

- **项目名称**: WZMediaPlayer
- **项目路径**: `E:\WZMediaPlayer_2025`
- **技术栈**: C++, Qt 6.6.3, FFmpeg, OpenGL, OpenAL
- **构建工具**: Visual Studio 2019, MSBuild
- **测试框架**: pywinauto

## 快速参考

### 文件结构

```
WZMediaPlayer_2025/
├── .cursor/skills/          # Cursor 技能目录
│   ├── SKILL.md             # 本文件（技能入口）
│   ├── build-skill/         # 编译技能
│   ├── test-skill/          # 测试技能
│   ├── memory-skill/        # 知识记录技能
│   ├── docs-skill/          # 文档查询技能
│   └── project-planning-skill/  # 项目规划技能
├── WZMediaPlay/             # 源代码
├── testing/pywinauto/       # 自动化测试
├── docs/TODO.md             # 任务列表
└── build.bat                # 编译脚本
```

### 常用命令

```powershell
# 编译项目
cd E:\WZMediaPlayer_2025
.\build.bat

# 运行测试
cd E:\WZMediaPlayer_2025\testing\pywinauto
python main.py

# 查看 TODO
Get-Content E:\WZMediaPlayer_2025\docs\TODO.md

# Git 命令
cd E:\WZMediaPlayer_2025

# 查看文件修改状态
git status

# 查看代码变更
git diff

# 查看提交历史
git log

# 比较版本差异
git diff <commit1> <commit2>

# 查看分支状态
git branch
```

## 使用建议

1. **编译前**: 检查代码保存状态，确认 FFmpeg/Qt 环境配置
2. **测试前**: 确保已编译 Debug 版本，测试视频文件存在
3. **记录知识**: 修复重要 Bug 后立即记录到 memory
4. **查询文档**: 优先使用 Context7 查询官方文档
5. **规划项目**: 定期更新 TODO.md，保持任务状态同步

## 注意事项

- 所有路径使用绝对路径 `E:\WZMediaPlayer_2025`
- 编译需要 Visual Studio 2019 环境
- 测试需要 pywinauto 依赖: `pip install pywinauto`
- 知识记录使用 graph-memory MCP 工具
