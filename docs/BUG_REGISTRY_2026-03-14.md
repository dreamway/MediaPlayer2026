# BUG Registry - 2026-03-14

## BUG-048: 切换摄像头后视频不继续播放
**状态**: ✅ 已修复
**描述**: 播放视频中切换摄像头，能够切换过去，但再切换回来之后，视频没有继续播放
**预期**: 切换回视频后，视频应该继续播放
**修复**: 在切换到摄像头前保存当前视频路径和播放位置，切换回来时恢复播放

## BUG-049: 双击播放列表视频进度不从0开始
**状态**: ✅ 已修复
**描述**: 播放列表中双击视频，预期应该是重新播放，但进度条起始点并不是从 0 开始，而是从其中的某个时间点，比如 3 秒开始
**预期**: 双击视频应该从头开始播放（进度条从 0 开始）
**修复**: 添加 `isDoubleclickingPlaylist_` 标志防止 `replayCurrentItemChanged` 干扰双击处理；修改 `replayCurrentItemChanged` 使用 `openPath()` 确保进度条正确更新

## BUG-050: 添加视频只能单选
**状态**: ✅ 已修复
**描述**: 播放视频选择添加视频的时候，每次只能加一个，不能多个一起添加（UI 选择的时候哪怕选择了多个，也只会有一个高亮）
**预期**: 应该支持多选添加多个视频文件
**修复**: 将 `QFileDialog::getOpenFileName` 改为 `QFileDialog::getOpenFileNames`

## BUG-051: 频繁播放/停止切换导致状态混乱
**状态**: ✅ 已修复
**描述**: 重复在播放/停止之间切换，大概率会把进度条及视频播放的状态弄乱
**预期**: 频繁切换播放/停止状态应该保持一致
**修复**: 添加 `isStateTransitioning_` 状态保护标志，防止在状态切换过程中接受新的操作

---

# Investigation Notes

## BUG-048 Investigation

相关代码文件：
- MainWindow.cpp: switchToCamera(), switchToVideoFile()

## BUG-049 Investigation

相关代码文件：
- MainWindow.cpp: 播放列表双击处理
- PlayController.cpp: open(), seek()

## BUG-050 Investigation

相关代码文件：
- MainWindow.cpp: 添加视频文件对话框

## BUG-051 Investigation

相关代码文件：
- MainWindow.cpp: on_pushButton_play_clicked(), on_pushButton_stop_clicked()
- PlayController.cpp: play(), stop()