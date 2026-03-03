# WZMediaPlayer 完整自动化测试文档

## 概述

这是WZMediaPlayer 3D播放器的完整自动化测试套件，使用pywinauto实现UI自动化测试。测试覆盖了播放器的核心功能、3D特性、边界条件和音视频同步。

## 测试架构

```
testing/pywinauto/
├── main.py                  # 基础播放和Seeking测试
├── test_3d_features.py      # 3D功能测试
├── test_edge_cases.py       # 边界条件测试
├── test_av_sync.py         # 音视频同步测试
├── run_all_tests.py         # 完整测试套件（整合所有测试）
├── log_monitor.py           # 日志监控模块
├── config.py                # 测试配置
├── ui_inspector.py          # UI控件检查工具
└── README.md                # 使用说明
```

## 测试覆盖范围

### 1. 基础播放测试 (`main.py`)
- ✅ 启动播放器
- ✅ 打开视频文件
- ✅ 播放/暂停控制
- ✅ 停止播放
- ✅ 小幅seek（±5秒）
- ✅ 大幅seek（±10秒）
- ✅ 多次连续seek
- ✅ 音量控制

### 2. 3D功能测试 (`test_3d_features.py`)
- ✅ 3D/2D模式切换
- ✅ 输入格式切换（LR/RL/UD）
- ✅ 输出格式切换（水平/垂直/棋盘）
- ✅ 视差调整（增加/减少/重置）
- ✅ 局部3D区域设置

### 3. 边界条件测试 (`test_edge_cases.py`)
- ✅ 快速连续播放/暂停（20次，间隔50ms）
- ✅ 快速连续seek（30次，间隔100ms）
- ✅ Seek到开头/结尾
- ✅ 无效文件处理
- ✅ 损坏文件处理
- ✅ 音量极端值测试
- ✅ 压力操作测试（混合多种操作）

### 4. 音视频同步测试 (`test_av_sync.py`)
- ✅ Seek后音视频同步验证
- ✅ 长时间播放同步测试（60秒）
- ✅ 关键帧seek同步
- ✅ 暂停/恢复后同步

## 快速开始

### 1. 安装依赖

```bash
cd testing/pywinauto
pip install -r requirements.txt
```

### 2. 配置路径

编辑测试文件中的路径配置：

```python
# 播放器可执行文件路径
exe_path = r"E:\WZMediaPlayer_2025\x64\Debug\WZMediaPlay.exe"

# 普通测试视频路径
test_video_path = r"D:\videos\test.mp4"

# 3D测试视频路径
test_3d_video_path = r"D:\videos\test_3d.mp4"
```

### 3. 运行测试

**运行完整测试套件（推荐）**：
```bash
python run_all_tests.py
```

**运行特定测试模块**：
```bash
# 基础测试
python main.py

# 3D功能测试
python test_3d_features.py

# 边界条件测试
python test_edge_cases.py

# 音视频同步测试
python test_av_sync.py
```

## 测试报告

测试完成后，会生成详细的测试报告：

- **基础测试报告**: `test_report_enhanced_YYYYMMDD_HHMMSS.txt`
- **完整测试报告**: `test_report_full_YYYYMMDD_HHMMSS.txt`

报告包含：
- 测试时间统计
- 通过/失败统计
- 异常日志摘要
- 详细测试结果

## 日志监控

测试过程中会自动监控播放器日志文件，捕获：
- ERROR级别日志
- WARNING级别日志
- CRITICAL级别日志
- 异常信息

日志监控结果会包含在测试报告中。

## 参考QtAV实现

测试方法参考了QtAV的测试策略：
- 音视频同步测试方法
- 边界条件处理
- 长时间播放稳定性测试

## 注意事项

1. **测试视频准备**：
   - 准备普通视频（mp4格式）
   - 准备3D视频（左右格式）
   - 建议使用较长的视频（>5分钟）用于长时间播放测试

2. **快捷键确认**：
   - 部分测试需要确认实际的快捷键
   - 3D功能测试需要根据实际UI调整菜单操作

3. **测试环境**：
   - 确保播放器可以正常启动
   - 确保测试视频文件存在
   - 建议在干净的测试环境中运行

4. **测试时间**：
   - 完整测试套件可能需要较长时间（10-30分钟）
   - 可以根据需要调整测试参数

## 扩展测试

### 添加新的测试用例

1. 继承相应的测试类：
```python
from main import WZMediaPlayerTester

class MyCustomTester(WZMediaPlayerTester):
    def test_my_feature(self):
        # 实现测试逻辑
        pass
```

2. 添加到测试套件：
在 `run_all_tests.py` 中添加新的测试方法。

## 故障排除

### 测试失败

1. 检查日志文件中的错误信息
2. 确认播放器可以手动正常启动
3. 检查测试视频文件是否存在
4. 确认快捷键配置正确

### UI控件找不到

运行UI检查工具：
```bash
python ui_inspector.py
```

更新 `config.py` 中的控件ID。

## 贡献

欢迎添加新的测试用例和改进测试方法。在添加新测试时，请：
1. 遵循现有的代码风格
2. 添加详细的注释
3. 更新本文档
4. 确保测试可以独立运行
