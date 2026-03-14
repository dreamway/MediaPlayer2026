/*
    StereoVideoWidget: 视频显示控件
    继承 VideoWidgetBase，支持新的渲染架构
    提供与 FFmpegView 兼容的接口
*/

#pragma once

#include "GlobalDef.h"
#include "PlaybackStateMachine.h"
#include "videoDecoder/VideoWidgetBase.h"
#include <string>
#include <QCamera>
#include <QCameraDevice>
#include <QCameraFormat>
#include <QEnterEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QList>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QMoveEvent>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QVideoFrame>
#include <QVideoSink>

class PlayController;
class FullscreenTipsWidget;
class SubtitleWidget;
// 注意：CameraOpenGLWidget 现在由 MainWindow 管理，不再需要前向声明
// BUG-045: FloatButton 已移除，改用 MainWindow 底部控制栏的播放列表按钮

/**
 * StereoVideoWidget: 视频显示控件
 * 继承 VideoWidgetBase 以支持新的渲染架构
 * 职责：
 * - 提供与 FFmpegView 兼容的接口
 * - 使用 VideoRenderer 架构进行渲染
 * - 管理 UI 组件（FullscreenTipsWidget, SubtitleWidget）
 */
class StereoVideoWidget : public VideoWidgetBase
{
    Q_OBJECT

public:
    explicit StereoVideoWidget(QWidget *parent = nullptr);
    ~StereoVideoWidget() override;

    // VideoWidgetBase 接口实现
    void setRenderer(VideoRendererPtr renderer) override;
    VideoRendererPtr renderer() const override;

    void setStereoFormat(StereoFormat format) override;
    StereoFormat stereoFormat() const override { return currentStereoFormat_; }

    void setStereoInputFormat(StereoInputFormat inputFormat) override;
    StereoInputFormat stereoInputFormat() const override { return currentStereoInputFormat_; }

    void setStereoOutputFormat(StereoOutputFormat outputFormat) override;
    StereoOutputFormat stereoOutputFormat() const override { return currentStereoOutputFormat_; }

    void setParallaxShift(int shift) override;
    int parallaxShift() const override { return parallaxShift_; }

    QImage grabFrame() override;
    void clear() override;
    bool isReady() const override;

signals:
    void updatePlayProcess(int64_t);

public slots:
    void OnRenderTimer();
    void OnUpdateStatusTimer();
    void onPlaybackStateChanged(PlaybackState state);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void enterEvent(QEnterEvent *e) override;
    void leaveEvent(QEvent *e) override;
    void moveEvent(QMoveEvent *event) override;
    void resizeEvent(QResizeEvent *e) override;
    void paintEvent(QPaintEvent *e) override;

public:
    // 与 FFmpegView 兼容的接口
    int StartRendering(StereoFormat stereoFormat, StereoInputFormat stereoInputFormat, StereoOutputFormat stereoOutputFormat, float frameRate);
    bool StopRendering();
    bool PlayPause(bool isPause);
    void SetPlayController(PlayController *controller);
    void SetFullscreenMode(FullscreenMode mode);
    bool IsRendering();
    void DebugPrintStatus();

    // StereoControl 接口（与 FFmpegView 兼容）
    bool SetRenderInputSource(RenderInputSource ris);
    StereoFormat ToggleStereo(bool stereo_checked);
    StereoFormat SetStereoFormat(StereoFormat stereoFormat);
    StereoInputFormat SetStereoInputFormat(StereoInputFormat inputFormat);
    StereoOutputFormat SetStereoOutputFormat(StereoOutputFormat outputFormat);
    StereoOutputFormat GetStereoOutputFormat();
    bool IsStereoRegion();
    bool SetStereoEnableRegion(bool enabled, int blX, int blY, int trX, int trY);
    bool CancelStereoRegion();
    bool SaveImage(QString screenShotRootFolder, QString &savedFilePath);
    void IncreaseParallax();
    void DecreaseParallax();
    void ResetParallax();
    void ToggleViewParallaxSideStrip();

    // 其他接口（与 FFmpegView 兼容）
    bool TakeScreenshot();
    void SetSeeking(const std::string &parentFunc, bool seekFlag);
    bool LoadSubtitle(QString filename);
    void StopSubtitle();
    bool UpdateSubtitlePosition(int64_t timestamp);

    // Shader 切换接口（用于调试对比）
    void setUseDefault2DShader(bool use);
    bool isUsingDefault2DShader() const;

    // 进度条位置跟踪重置（BUG-038 修复）
    void resetPositionTracking();



    // UI 组件访问（与 FFmpegView 兼容）

    // BUG-045: FloatButton 已移除，播放列表按钮已移至 MainWindow 底部控制栏

    /**
     * FullscreenTipsWidget：全屏提示显示组件
     * - 位置：左上角(0, 0)
     * - 功能：显示全屏模式提示和局部3D提示
     * - 提示类型：
     *   - "Fullscreen"：普通全屏模式（保持比例）
     *   - "Fullscreen+"：全屏+拉伸模式（拉伸填充屏幕）
     *   - "局部3D"：局部3D模式激活时显示（持续显示）
     * - 显示时长：1秒后自动消失（局部3D提示除外）
     */
    FullscreenTipsWidget *mFullscreenTipsWidget = nullptr;

    /**
     * SubtitleWidget：字幕显示组件
     * - 位置：窗口底部
     * - 功能：显示字幕文本
     * - 支持的字幕格式：.srt, .ass等
     * - 字幕字体高度：mSubtitleFontHeight（默认50px）
     * - Seeking逻辑：Seeking时暂停更新，Seeking结束后恢复
     */
    SubtitleWidget *mSubtitleWidget = nullptr;

    // Camera 和 Shader 管理（分离的功能）
    class ShaderManager *shaderManager() const { return shaderManager_; }



private:
    void initializeUIComponents();
    void setupConnections();
    void setupUIComponentsLayout();

private:
    // 新的渲染架构
    VideoRendererPtr videoRenderer_;  // 新的渲染器接口
    bool useNewRenderer_ = false;     // 是否使用新的渲染架构

    PlayController *playController_ = nullptr;

    // 分离的功能组件
    class ShaderManager *shaderManager_ = nullptr;

    // 注意：CameraOpenGLWidget 现在由 MainWindow 创建和管理（与 StereoVideoWidget 同级）
    // 不再在这里管理 cameraOpenGLWidget_
    // 保留 onCameraFrameChanged 接口以保持兼容性，但实际处理已迁移到 CameraManager

    // 渲染状态
    bool isRendering_ = false;
    RenderInputSource currentRenderInputSource_ = RenderInputSource::RIS_VIDEO_FILE; // 当前渲染输入源
    StereoFormat currentStereoFormat_ = STEREO_FORMAT_NORMAL_2D;
    StereoInputFormat currentStereoInputFormat_ = STEREO_INPUT_FORMAT_LR;
    StereoOutputFormat currentStereoOutputFormat_ = STEREO_OUTPUT_FORMAT_HORIZONTAL;
    int parallaxShift_ = 0;
    bool stereoRegionEnabled_ = false;
    float stereoRegion_[4] = {0.0f, 0.0f, 1.0f, 1.0f}; // topLeftX, topLeftY, bottomRightX, bottomRightY
    FullscreenMode currentFullscreenMode_ = FULLSCREEN_KEEP_RATIO;

    // 定时器
    QTimer *renderTimer_ = nullptr;
    QTimer *statusUpdateTimer_ = nullptr;

    // 字幕字体高度（与 FFmpegView 兼容）
    int mSubtitleFontHeight = 50;

    // 视频布局（用于管理OpenGL widget）
    QVBoxLayout *videoLayout_ = nullptr;

    // 进度条位置跟踪（BUG-038 修复：避免使用 static 变量）
    int64_t lastPositionSeconds_ = -1;
};
