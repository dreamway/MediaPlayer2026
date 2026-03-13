/*
    StereoVideoWidget: 视频显示控件实现
    支持新的 VideoRenderer 架构
*/

#include "StereoVideoWidget.h"
#include "gui/FloatButton.h"
#include "gui/FullscreenTipsWidget.h"
#include "PlayController.h"
#include "ShaderManager.h"
#include "gui/SubtitleWidget.h"
// 注意：CameraOpenGLWidget 现在由 MainWindow 管理，不再需要包含
#include "videoDecoder/opengl/OpenGLRenderer.h"
#include "videoDecoder/opengl/OpenGLCommon.hpp"
#include "videoDecoder/opengl/StereoOpenGLRenderer.h"
#include "videoDecoder/Statistics.h"

#include <chrono>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QLabel>
#include <QLayout>
#include <QMediaCaptureSession>
#include <QPixmap>
#include <QSettings>
#include <QSizePolicy>
#include <QVideoFrame>
#include <QVideoSink>

#include <spdlog/spdlog.h>
#include <QEnterEvent>
#include <QKeyEvent>
#include <QMoveEvent>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QTimer>

extern spdlog::logger *logger;

StereoVideoWidget::StereoVideoWidget(QWidget *parent)
    : VideoWidgetBase(parent)
    , isRendering_(false)
    , currentStereoFormat_(STEREO_FORMAT_NORMAL_2D)
    , currentStereoInputFormat_(STEREO_INPUT_FORMAT_LR)
    , currentStereoOutputFormat_(STEREO_OUTPUT_FORMAT_HORIZONTAL)
    , parallaxShift_(0)
    , stereoRegionEnabled_(false)
{
    renderTimer_ = new QTimer(this);
    statusUpdateTimer_ = new QTimer(this);

    // 创建分离的功能组件
    shaderManager_ = new ShaderManager(this);

    // 加载 Shader（如果需要）
    if (shaderManager_) {
        bool loadInternalOk = shaderManager_->loadInternalShaders();
        bool loadExternalOk = shaderManager_->loadExternalShaders();
        if (!loadInternalOk && !loadExternalOk) {
            if (logger)
                logger->warn("StereoVideoWidget: Failed to load shaders");
        }
    }

    // 设置布局
    videoLayout_ = new QVBoxLayout(this);
    videoLayout_->setContentsMargins(0, 0, 0, 0);
    videoLayout_->setSpacing(0);
    setLayout(videoLayout_);

    initializeUIComponents();
    setupUIComponentsLayout();
    setupConnections();


    // 注意：渲染器通过 setRenderer() 方法设置，不在构造函数中创建
    if (logger) {
        logger->info("StereoVideoWidget: Created, waiting for renderer to be set");
    }
}

StereoVideoWidget::~StereoVideoWidget()
{
    StopRendering();


    // 注意：CameraOpenGLWidget 现在由 MainWindow 管理，不需要在这里清理

    // ShaderManager 会自动清理（QObject 父对象管理）

    // 清理渲染器
    if (videoRenderer_) {
        videoRenderer_->close();
        videoRenderer_.reset();
    }
}

// VideoWidgetBase 接口实现
void StereoVideoWidget::setRenderer(VideoRendererPtr renderer)
{
    // 关闭旧渲染器并移除旧的OpenGL widget
    if (videoRenderer_) {
        // 移除旧的OpenGL widget（如果存在）
        auto *oglRenderer = dynamic_cast<OpenGLRenderer *>(videoRenderer_.get());
        if (oglRenderer && oglRenderer->widget() && videoLayout_) {
            videoLayout_->removeWidget(oglRenderer->widget());
            oglRenderer->widget()->setParent(nullptr);
        }
        videoRenderer_->close();
    }

    videoRenderer_ = renderer;

    if (videoRenderer_) {
        // 设置渲染目标
        videoRenderer_->setTarget(this);

        // 打开渲染器
        if (!videoRenderer_->isOpen()) {
            videoRenderer_->open();
        }

        // 关键修复：将OpenGL widget添加到布局中，确保正确显示和大小
        auto *oglRenderer = dynamic_cast<OpenGLRenderer *>(videoRenderer_.get());
        if (oglRenderer && oglRenderer->widget() && videoLayout_) {
            QWidget *glWidget = oglRenderer->widget();
            
            // 移除widget（如果已经在其他布局中）
            if (glWidget->parentWidget() && glWidget->parentWidget() != this) {
                QLayout *oldLayout = glWidget->parentWidget()->layout();
                if (oldLayout) {
                    oldLayout->removeWidget(glWidget);
                }
            }
            
            // 设置父窗口为StereoVideoWidget
            glWidget->setParent(this);
            
            // 设置大小策略：水平和垂直都Expanding，确保填充整个区域
            glWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            
            // 添加到布局（作为第一个item，这样视频在底层，UI组件在上层）
            videoLayout_->insertWidget(0, glWidget);
            
            // 确保widget显示
            glWidget->show();
            glWidget->lower(); // 放在底层，让UI组件在上层
            
            if (logger) {
                logger->info("StereoVideoWidget::setRenderer: OpenGL widget added to layout, size: {}x{}", 
                    width(), height());
            }
        } else {
            if (logger) {
                logger->warn("StereoVideoWidget::setRenderer: Failed to get OpenGL widget from renderer");
            }
        }

        // 应用当前的 3D 参数
        auto *stereoRenderer = dynamic_cast<StereoOpenGLRenderer *>(videoRenderer_.get());
        if (stereoRenderer) {
            stereoRenderer->setStereoFormat(currentStereoFormat_);
            stereoRenderer->setStereoInputFormat(currentStereoInputFormat_);
            stereoRenderer->setStereoOutputFormat(currentStereoOutputFormat_);
            stereoRenderer->setParallaxShift(parallaxShift_);
        }

        useNewRenderer_ = true;

        if (logger) {
            logger->info("StereoVideoWidget::setRenderer: Renderer set to {}", videoRenderer_->name().toStdString());
        }
    } else {
        useNewRenderer_ = false;
    }
}

VideoRendererPtr StereoVideoWidget::renderer() const
{
    return videoRenderer_;
}

void StereoVideoWidget::setStereoFormat(StereoFormat format)
{
    currentStereoFormat_ = format;

    // 更新渲染器
    if (videoRenderer_) {
        auto *stereoRenderer = dynamic_cast<StereoOpenGLRenderer *>(videoRenderer_.get());
        if (stereoRenderer) {
            stereoRenderer->setStereoFormat(format);
        }
    }

    emit stereoFormatChanged(format);

    if (logger) {
        logger->info("StereoVideoWidget::setStereoFormat: Format set to {}", static_cast<int>(format));
    }
}

void StereoVideoWidget::setStereoInputFormat(StereoInputFormat inputFormat)
{
    currentStereoInputFormat_ = inputFormat;

    if (videoRenderer_) {
        auto *stereoRenderer = dynamic_cast<StereoOpenGLRenderer *>(videoRenderer_.get());
        if (stereoRenderer) {
            stereoRenderer->setStereoInputFormat(inputFormat);
        }
    }

    if (logger) {
        logger->info("StereoVideoWidget::setStereoInputFormat: Input format set to {}", static_cast<int>(inputFormat));
    }
}

void StereoVideoWidget::setStereoOutputFormat(StereoOutputFormat outputFormat)
{
    currentStereoOutputFormat_ = outputFormat;

    if (videoRenderer_) {
        auto *stereoRenderer = dynamic_cast<StereoOpenGLRenderer *>(videoRenderer_.get());
        if (stereoRenderer) {
            stereoRenderer->setStereoOutputFormat(outputFormat);
        }
    }

    if (logger) {
        logger->info("StereoVideoWidget::setStereoOutputFormat: Output format set to {}", static_cast<int>(outputFormat));
    }
}

void StereoVideoWidget::setParallaxShift(int shift)
{
    parallaxShift_ = shift;

    if (videoRenderer_) {
        auto *stereoRenderer = dynamic_cast<StereoOpenGLRenderer *>(videoRenderer_.get());
        if (stereoRenderer) {
            stereoRenderer->setParallaxShift(shift);
        }
    }

    if (logger) {
        logger->info("StereoVideoWidget::setParallaxShift: Parallax shift set to {}", shift);
    }
}

QImage StereoVideoWidget::grabFrame()
{
    // 使用新的渲染器
    if (videoRenderer_ && videoRenderer_->isOpen()) {
        // 新的渲染器需要实现 grabFrame 功能
        // 这里暂时返回空图像，需要后续实现
        return QImage();
    }

    return QImage();
}

void StereoVideoWidget::clear()
{
    if (videoRenderer_) {
        videoRenderer_->clear();
    }
}

bool StereoVideoWidget::isReady() const
{
    return videoRenderer_ && videoRenderer_->isReady();
}

void StereoVideoWidget::initializeUIComponents()
{
    /**
     * 初始化 UI 组件（与 FFmpegView 兼容）
     * - 创建所有UI组件
     * - 设置初始状态（默认隐藏）
     * - 加载配置（FPS显示）
     */

    // FloatButton：播放列表显示按钮
    // - 初始状态：隐藏（进入窗口时显示）
    // - 功能：点击触发播放列表显示/隐藏
    if (!butWidget) {
        butWidget = new FloatButton(this);
        butWidget->hide();
    }

    // mWindowLogo：播放窗口Logo标签
    // - 初始状态：隐藏
    // - 功能：未播放视频时显示Logo，播放时隐藏
    if (!mWindowLogo) {
        mWindowLogo = new QLabel(this);
        mWindowLogPM.load(GlobalDef::getInstance()->PLAY_WINDOW_LOGO_PATH);
        mWindowLogo->resize(mWindowLogPM.size());
        mWindowLogo->setPixmap(mWindowLogPM);
        mWindowLogo->hide();
    }

    // FullscreenTipsWidget：全屏提示显示组件
    // - 初始状态：隐藏
    // - 功能：显示全屏模式和局部3D提示
    // - 位置：左上角(0, 0)
    if (!mFullscreenTipsWidget) {
        mFullscreenTipsWidget = new FullscreenTipsWidget(this);
        mFullscreenTipsWidget->move(QPoint(0, 0));
        mFullscreenTipsWidget->hide();
    }

    // SubtitleWidget：字幕显示组件
    // - 初始状态：隐藏
    // - 功能：显示字幕文本
    // - 位置：底部
    // - 字体高度：mSubtitleFontHeight（默认50px）
    if (!mSubtitleWidget) {
        mSubtitleWidget = new SubtitleWidget(this);
        mSubtitleWidget->move(QPoint(0, height() - mSubtitleFontHeight));
        mSubtitleWidget->resize(QSize(width(), mSubtitleFontHeight));
        mSubtitleWidget->hide();
    }

    // FPS 显示功能已移除（非刚需功能）
}

void StereoVideoWidget::setupUIComponentsLayout()
{
    /**
     * 设置 UI 组件的布局和层级（z-order）
     * - z-order决定组件的显示优先级
     * - 顶层组件会覆盖底层组件
     */

    // FloatButton 应该在顶层（z-order 最高）
    // - 原因：需要快速访问播放列表按钮
    if (butWidget) {
        butWidget->raise();
    }

    // SubtitleWidget 应该在视频上方
    // - 原因：字幕需要覆盖在视频上方，但不能覆盖其他UI组件
    if (mSubtitleWidget) {
        mSubtitleWidget->raise();
    }

    // FullscreenTipsWidget 应该在顶层
    // - 原因：全屏提示需要始终可见，不被其他组件覆盖
    if (mFullscreenTipsWidget) {
        mFullscreenTipsWidget->raise();
    }

    // 播放窗口 Logo 应该在顶层
    // - 原因：Logo需要始终可见，不被其他组件覆盖
    if (mWindowLogo) {
        mWindowLogo->raise();
    }
}

void StereoVideoWidget::setupConnections()
{
    // 连接定时器
    connect(renderTimer_, &QTimer::timeout, this, &StereoVideoWidget::OnRenderTimer);
    connect(statusUpdateTimer_, &QTimer::timeout, this, &StereoVideoWidget::OnUpdateStatusTimer);
}

// OpenGL 初始化由 VideoRenderer 内部的 widget 处理，不需要在这里实现

void StereoVideoWidget::keyPressEvent(QKeyEvent *event)
{
    QWidget::keyPressEvent(event);
    // 处理键盘事件（与 FFmpegView 兼容）
}

void StereoVideoWidget::enterEvent(QEnterEvent *e)
{
    QWidget::enterEvent(e);
    // 显示 UI 组件
    if (butWidget) {
        butWidget->show();
    }
}

void StereoVideoWidget::leaveEvent(QEvent *e)
{
    QWidget::leaveEvent(e);
    // 隐藏 UI 组件
    if (butWidget) {
        butWidget->hide();
    }
}

void StereoVideoWidget::moveEvent(QMoveEvent *event)
{
    QWidget::moveEvent(event);
}

void StereoVideoWidget::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);

    /**
     * 调整 UI 组件位置（与 FFmpegView 兼容）
     * - 窗口大小改变时重新计算每个组件的位置
     * - 确保组件在窗口中的相对位置正确
     * 
     * 注意：OpenGL widget由布局管理，会自动调整大小，不需要手动设置
     */

    // FloatButton 位置：右侧中间
    // - X坐标：窗口宽度 - 按钮宽度
    // - Y坐标：(窗口高度 - 按钮高度) / 2
    if (butWidget) {
        butWidget->move((width() - butWidget->width()), (height() - butWidget->height()) / 2);
    }

    // SubtitleWidget 位置：底部
    // - X坐标：0（左对齐）
    // - Y坐标：窗口高度 - 字幕字体高度
    // - 宽度：窗口宽度（自适应）
    if (mSubtitleWidget) {
        mSubtitleWidget->move(QPoint(0, height() - mSubtitleFontHeight));
        mSubtitleWidget->resize(width(), mSubtitleFontHeight);
    }

    // FullscreenTipsWidget 位置：左上角
    // - X坐标：0
    // - Y坐标：0
    if (mFullscreenTipsWidget) {
        mFullscreenTipsWidget->move(QPoint(0, 0));
    }

    // mWindowLogo 位置：居中（仅移动位置，不控制显示/隐藏）
    // Logo 的显示/隐藏由 StartRendering()/StopRendering() 统一管理
    // 修复：移除此处的 show() 调用，避免在视频切换过程中 Logo 被错误地显示
    // - X坐标：(窗口宽度 - Logo宽度) / 2
    // - Y坐标：(窗口高度 - Logo高度) / 2
    if (mWindowLogo) {
        mWindowLogo->move((width() - mWindowLogo->width()) / 2, (height() - mWindowLogo->height()) / 2);
        // 注意：不要在这里调用 show()，Logo 的显示由 StopRendering() 控制
        // 只有在非渲染状态下才显示 Logo（此时 Logo 应该已经被 StopRendering() 显示了）
    }

    // 确保OpenGL widget正确调整大小（布局会自动处理，但这里可以添加日志）
    if (videoRenderer_) {
        auto *oglRenderer = dynamic_cast<OpenGLRenderer *>(videoRenderer_.get());
        if (oglRenderer && oglRenderer->widget()) {
            // 布局会自动调整widget大小，这里只需要确保widget可见
            if (!oglRenderer->widget()->isVisible()) {
                oglRenderer->widget()->show();
            }
        }
    }
}

void StereoVideoWidget::paintEvent(QPaintEvent *e)
{
    QWidget::paintEvent(e);

    // 渲染由 VideoRenderer 内部的 widget 处理
}

int StereoVideoWidget::StartRendering(StereoFormat stereoFormat, StereoInputFormat stereoInputFormat, StereoOutputFormat stereoOutputFormat, float frameRate)
{
    // 如果已经在渲染，先停止再重新启动（用于视频切换场景）
    // if (isRendering_) {
    //     if (logger) {
    //         logger->info("StereoVideoWidget::StartRendering: Already rendering, stopping first before restarting");
    //     }
    //     StopRendering();
    // }

    currentStereoFormat_ = stereoFormat;
    currentStereoInputFormat_ = stereoInputFormat;
    currentStereoOutputFormat_ = stereoOutputFormat;

    // 设置 VideoRenderer 的参数（仅用于视频文件渲染）
    // 注意：这些方法只在StereoOpenGLRenderer中可用，需要dynamic_cast
    auto *stereoRenderer = dynamic_cast<StereoOpenGLRenderer *>(videoRenderer_.get());
    if (stereoRenderer) {
        stereoRenderer->setStereoFormat(stereoFormat);
        stereoRenderer->setStereoInputFormat(stereoInputFormat);
        stereoRenderer->setStereoOutputFormat(stereoOutputFormat);
    }

    // 启动状态更新定时器（用于更新进度条等UI）
    if (statusUpdateTimer_) {
        statusUpdateTimer_->start(100); // 100ms 更新一次状态（更频繁的更新，确保进度条同步）
    }

    // 根据渲染输入源决定是否启动 renderTimer_
    if (currentRenderInputSource_ == RenderInputSource::RIS_CAMERA) {
        // Camera 模式：需要定时器驱动渲染
        if (renderTimer_ && frameRate > 0) {
            int intervalMs = static_cast<int>(1000.0 / frameRate);
            renderTimer_->start(intervalMs);
            if (logger) {
                logger->info("StereoVideoWidget::StartRendering: Started renderTimer_ for Camera mode, interval: {} ms", intervalMs);
            }
        }
    } else {
        // 视频文件模式：不启动 renderTimer_，渲染由 VideoThread 通过 writeVideo() -> updateGL() 驱动
        // 这样可以避免双重驱动冲突，并确保渲染及时
        if (logger) {
            logger->debug("StereoVideoWidget::StartRendering: renderTimer_ not started for VideoFile mode (driven by VideoThread)");
        }
    }

    isRendering_ = true;

    // BUG-035 修复：确保 Logo 被正确隐藏
    // 使用 hide() + lower() + clearFocus() 确保 Logo 完全隐藏且不在顶层
    if (mWindowLogo) {
        mWindowLogo->hide();
        mWindowLogo->lower();  // 确保在 z-order 最底层
        mWindowLogo->clearFocus();
    }

    // BUG-035 修复：触发立即重绘，确保 Logo 隐藏生效
    // 使用 repaint() 而不是 update() 来强制同步重绘
    repaint();

    if (logger) {
        logger->info(
            "StereoVideoWidget::StartRendering: Started with format {}, input {}, output {}, logo hidden",
            int(stereoFormat),
            int(stereoInputFormat),
            int(stereoOutputFormat));
    }

    return 0;
}

bool StereoVideoWidget::StopRendering()
{
    if (!isRendering_) {
        return false;
    }

    if (renderTimer_) {
        renderTimer_->stop();
    }
    if (statusUpdateTimer_) {
        statusUpdateTimer_->stop();
    }

    isRendering_ = false;

    // BUG-034 修复：先清空视频渲染器，确保 OpenGL 缓冲区被清除
    // 避免旧帧残留在 Logo 背景中
    if (videoRenderer_) {
        videoRenderer_->clear();
    }

    // BUG-034 补充：通过渲染器获取 StereoOpenGLCommon 并强制清除
    // 注意：videoRenderer_->clear() 会清除帧数据，但 paintGLStereo() 在 hasImage==false 时
    // 已经有了 glClear() 处理，这里我们触发一次 update 来确保清除生效
    auto *oglRenderer = dynamic_cast<OpenGLRenderer *>(videoRenderer_.get());
    if (oglRenderer && oglRenderer->widget()) {
        // 触发重绘，paintGLStereo() 中的 glClear() 会清除缓冲区
        oglRenderer->widget()->update();
    }

    // 触发立即重绘，确保 OpenGL 缓冲区被清空后再显示 Logo
    // 使用 repaint() 而不是 update() 来强制立即重绘
    repaint();

    // 在缓冲区清除后显示 Logo
    if (mWindowLogo) {
        // 确保 Logo 在正确的位置并显示
        mWindowLogo->move((width() - mWindowLogo->width()) / 2, (height() - mWindowLogo->height()) / 2);
        mWindowLogo->raise();  // 确保 Logo 在顶层
        mWindowLogo->show();
    }

    if (logger) {
        logger->info("StereoVideoWidget::StopRendering: Stopped, Logo visible={}", mWindowLogo ? mWindowLogo->isVisible() : false);
    }

    return true;
}

bool StereoVideoWidget::PlayPause(bool isPause)
{
    if (playController_) {
        if (isPause) {
            // 暂停播放
            playController_->pause();
        } else {
            // 继续播放
            playController_->play();
        }
        return true;
    }
    return false;
}

void StereoVideoWidget::SetPlayController(PlayController *controller)
{
    playController_ = controller;

    // 将 VideoRenderer 设置到 PlayController
    // 必须在 PlayController::open() 之前设置，这样 initializeCodecs() 就不会创建默认的渲染器
    if (playController_ && videoRenderer_) {
        // 设置 VideoRenderer 到 PlayController
        playController_->setVideoRenderer(videoRenderer_);

        // 连接 PlayController 的信号
        connect(playController_, &PlayController::playbackStateChanged, this, &StereoVideoWidget::onPlaybackStateChanged);

        if (logger) {
            logger->info("StereoVideoWidget::SetPlayController: VideoRenderer set to PlayController");
        }
    }
}

void StereoVideoWidget::SetFullscreenMode(FullscreenMode mode)
{
    currentFullscreenMode_ = mode;

    // BUG-037 修复：设置全屏模式时同时更新渲染器的拉伸模式
    bool stretch = (mode == FULLSCREEN_PLUS_STRETCH);
    auto *stereoRenderer = dynamic_cast<StereoOpenGLRenderer *>(videoRenderer_.get());
    if (stereoRenderer) {
        stereoRenderer->setFullscreenPlusStretch(stretch);
    }

    // 设置全屏模式
    // 注意：实际的窗口全屏由 MainWindow 控制，这里只负责设置渲染模式
    if (mFullscreenTipsWidget) {
        switch (mode) {
        case FULLSCREEN_KEEP_RATIO:
        default:
            mFullscreenTipsWidget->SetRenderInfo(true, QString(tr("Fullscreen")));
            break;
        case FULLSCREEN_PLUS_STRETCH:
            mFullscreenTipsWidget->SetRenderInfo(true, QString(tr("Fullscreen+")));
            break;
        }
    }

    // TODO: 通知 StereoWriter 更新渲染模式（如果需要）
    // 目前 StereoWriter 的渲染模式由 OpenGLCommon 的 ratio 控制
    // 全屏+拉伸模式需要在渲染时调整顶点坐标
    // 可以通过 StereoOpenGLCommon 的 setMatrix 或类似方法更新渲染参数
}

bool StereoVideoWidget::IsRendering()
{
    return isRendering_;
}

void StereoVideoWidget::DebugPrintStatus()
{
    if (logger) {
        logger->info(
            "StereoVideoWidget::DebugPrintStatus: isRendering={}, stereoFormat={}, inputFormat={}, outputFormat={}",
            isRendering_,
            int(currentStereoFormat_),
            int(currentStereoInputFormat_),
            int(currentStereoOutputFormat_));
    }
}

// StereoControl 接口实现
bool StereoVideoWidget::SetRenderInputSource(RenderInputSource ris)
{
    currentRenderInputSource_ = ris;

    // 注意：Camera Widget 切换现在由 MainWindow 管理
    // 这里只更新状态，实际的 Widget 切换在 MainWindow 中处理
    if (ris == RenderInputSource::RIS_CAMERA) {
        if (logger) {
            logger->info("StereoVideoWidget: Render input source set to Camera (widget switching handled by MainWindow)");
        }
    } else {
        if (logger) {
            logger->info("StereoVideoWidget: Render input source set to VideoFile (widget switching handled by MainWindow)");
        }
        // 确保视频 Widget 显示（实际的 Widget 切换由 MainWindow 处理）
    }

    return true;
}

StereoFormat StereoVideoWidget::ToggleStereo(bool stereo_checked)
{
    StereoFormat newFormat = stereo_checked ? STEREO_FORMAT_3D : STEREO_FORMAT_NORMAL_2D;
    return SetStereoFormat(newFormat);
}

StereoFormat StereoVideoWidget::SetStereoFormat(StereoFormat stereoFormat)
{
    currentStereoFormat_ = stereoFormat;
    auto *stereoRenderer = dynamic_cast<StereoOpenGLRenderer *>(videoRenderer_.get());
    if (stereoRenderer) {
        stereoRenderer->setStereoFormat(stereoFormat);
    }
    return currentStereoFormat_;
}

StereoInputFormat StereoVideoWidget::SetStereoInputFormat(StereoInputFormat inputFormat)
{
    currentStereoInputFormat_ = inputFormat;
    auto *stereoRenderer = dynamic_cast<StereoOpenGLRenderer *>(videoRenderer_.get());
    if (stereoRenderer) {
        stereoRenderer->setStereoInputFormat(inputFormat);
    }
    return currentStereoInputFormat_;
}

StereoOutputFormat StereoVideoWidget::SetStereoOutputFormat(StereoOutputFormat outputFormat)
{
    currentStereoOutputFormat_ = outputFormat;
    auto *stereoRenderer = dynamic_cast<StereoOpenGLRenderer *>(videoRenderer_.get());
    if (stereoRenderer) {
        stereoRenderer->setStereoOutputFormat(outputFormat);
    }
    return currentStereoOutputFormat_;
}

StereoOutputFormat StereoVideoWidget::GetStereoOutputFormat()
{
    return currentStereoOutputFormat_;
}

bool StereoVideoWidget::IsStereoRegion()
{
    return stereoRegionEnabled_;
}

bool StereoVideoWidget::SetStereoEnableRegion(bool enabled, int blX, int blY, int trX, int trY)
{
    if (enabled) {
        // 计算归一化坐标（考虑实际渲染区域和上下黑边）
        float vecTopLeftX = blX / float(width());
        float vecBottomRightX = trX / float(width());

        // Y轴坐标需要转换，以便UI选择的坐标系与实际渲染坐标系选择对齐
        // 注意：这里需要获取实际的视频宽高比来计算压缩高度和上下黑边
        // 暂时使用简化版本，后续可以从 StereoWriter 获取实际视频尺寸
        float vecTopLeftY = blY / float(height());
        float vecBottomRightY = trY / float(height());

        stereoRegion_[0] = vecTopLeftX;
        stereoRegion_[1] = vecTopLeftY;
        stereoRegion_[2] = vecBottomRightX;
        stereoRegion_[3] = vecBottomRightY;

        stereoRegionEnabled_ = true;

        // 启用 region 时，自动切换到 3D 模式和全屏+拉伸模式
        SetStereoFormat(STEREO_FORMAT_3D);
        SetFullscreenMode(FULLSCREEN_PLUS_STRETCH);

        if (logger) {
            logger
                ->info("StereoVideoWidget::SetStereoEnableRegion: enabled, region=({}, {}, {}, {})", vecTopLeftX, vecTopLeftY, vecBottomRightX, vecBottomRightY);
        }
    } else {
        stereoRegionEnabled_ = false;
        stereoRegion_[0] = 0.0f;
        stereoRegion_[1] = 0.0f;
        stereoRegion_[2] = 1.0f;
        stereoRegion_[3] = 1.0f;

        if (logger) {
            logger->info("StereoVideoWidget::SetStereoEnableRegion: disabled");
        }
    }

    // 设置到 StereoOpenGLRenderer
    auto *stereoRenderer = dynamic_cast<StereoOpenGLRenderer *>(videoRenderer_.get());
    if (stereoRenderer) {
        stereoRenderer->setStereoEnableRegion(enabled, stereoRegion_[0], stereoRegion_[1], stereoRegion_[2], stereoRegion_[3]);
    }

    return true;
}

bool StereoVideoWidget::CancelStereoRegion()
{
    return SetStereoEnableRegion(false, 0, 0, 100, 100);
}

bool StereoVideoWidget::SaveImage(QString screenShotRootFolder, QString &savedFilePath)
{
    if (!QDir::current().exists(screenShotRootFolder)) {
        bool createdOk = QDir::current().mkdir(screenShotRootFolder);
        if (!createdOk) {
            if (logger)
                logger->warn("StereoVideoWidget: Failed to create screenshot folder: {}", screenShotRootFolder.toStdString());
            return false;
        }
    }

    // 通过 VideoRenderer 的 widget 截图
    QWidget *videoWidget = nullptr;
    if (videoRenderer_) {
        auto *oglRenderer = dynamic_cast<OpenGLRenderer *>(videoRenderer_.get());
        if (oglRenderer) {
            videoWidget = oglRenderer->widget();
        }
    }
    if (videoWidget) {
        QPixmap pixmap = videoWidget->grab();
        if (pixmap.isNull()) {
            if (logger)
                logger->error("StereoVideoWidget: Failed to grab framebuffer");
            return false;
        }

        QDateTime now = QDateTime::currentDateTime();
        QString timeStr = now.toString("yyyyMMddHHmmss");
        savedFilePath = screenShotRootFolder + "/Snapshots_" + timeStr + ".png";

        bool saved = pixmap.save(savedFilePath);
        if (logger && saved) {
            logger->info("StereoVideoWidget: Screenshot saved to: {}", savedFilePath.toStdString());
        }
        return saved;
    }

    return false;
}

void StereoVideoWidget::IncreaseParallax()
{
    parallaxShift_ += 1;
    auto *stereoRenderer = dynamic_cast<StereoOpenGLRenderer *>(videoRenderer_.get());
    if (stereoRenderer) {
        stereoRenderer->setParallaxShift(parallaxShift_);
    }
}

void StereoVideoWidget::DecreaseParallax()
{
    parallaxShift_ -= 1;
    auto *stereoRenderer = dynamic_cast<StereoOpenGLRenderer *>(videoRenderer_.get());
    if (stereoRenderer) {
        stereoRenderer->setParallaxShift(parallaxShift_);
    }
}

void StereoVideoWidget::ResetParallax()
{
    parallaxShift_ = 0;
    auto *stereoRenderer = dynamic_cast<StereoOpenGLRenderer *>(videoRenderer_.get());
    if (stereoRenderer) {
        stereoRenderer->setParallaxShift(parallaxShift_);
    }
}

void StereoVideoWidget::ToggleViewParallaxSideStrip()
{
    // TODO: 实现视差侧边条切换
}

void StereoVideoWidget::OnRenderTimer()
{
    // 视频文件模式：不需要额外操作，由 VideoThread 驱动
}

void StereoVideoWidget::OnUpdateStatusTimer()
{
    // 更新 FullscreenTipsWidget（显示"局部3D"等提示）
    if (mFullscreenTipsWidget) {
        if (stereoRegionEnabled_) {
            mFullscreenTipsWidget->SetRenderInfoWithoutTimer(true, QString(tr("局部3D")));
            mFullscreenTipsWidget->update();
        } else {
            mFullscreenTipsWidget->SetRenderInfoWithoutTimer(false, "");
            mFullscreenTipsWidget->update();
        }
    }

    // 更新播放状态和进度（仅对视频文件模式）
    if (currentRenderInputSource_ != RenderInputSource::RIS_VIDEO_FILE) {
        // Camera 模式不需要更新播放进度
        return;
    }

    if (!playController_) {
        if (logger) {
            static int logCounter = 0;
            if (logCounter++ % 100 == 0) {
                logger->warn("StereoVideoWidget::OnUpdateStatusTimer: playController_ is null");
            }
        }
        return;
    }

    if (!playController_->isOpened()) {
        if (logger) {
            static int logCounter = 0;
            if (logCounter++ % 100 == 0) {
                logger->debug("StereoVideoWidget::OnUpdateStatusTimer: playController_ is not opened");
            }
        }
        return;
    }

    // 检查是否正在 Seeking（避免在 Seeking 时更新进度）
    if (playController_ && playController_->isSeeking()) {
        // 正在seeking，跳过进度更新
        return;
    }

    // 检查是否暂停（避免在暂停时更新进度，防止UI显示变化的时间）
    if (playController_ && playController_->isPaused()) {
        // 暂停中，跳过进度更新
        return;
    }

    int64_t currentPositionMs = playController_->getCurrentPositionMs();
    // 验证返回值是否有效（避免负数或过大值导致UI错误）
    if (currentPositionMs < 0) {
        if (logger) {
            static int invalidPositionLogCount = 0;
            if (++invalidPositionLogCount % 100 == 1) {
                logger->warn("StereoVideoWidget::OnUpdateStatusTimer: Invalid position ({} ms), skipping update", currentPositionMs);
            }
        }
        return;
    }

    // 转换为秒（MainWindow::onUpdatePlayProcess 期望的是秒）
    int64_t currentPositionSeconds = currentPositionMs / 1000;

    // BUG-038 修复：使用成员变量替代 static 变量，避免视频切换时状态残留
    if (currentPositionSeconds != lastPositionSeconds_) {
        if (logger) {
            logger->debug(
                "StereoVideoWidget::OnUpdateStatusTimer: Emitting updatePlayProcess signal, position: {} seconds ({} ms)",
                currentPositionSeconds,
                currentPositionMs);
        }
        emit updatePlayProcess(currentPositionSeconds);
        lastPositionSeconds_ = currentPositionSeconds;
    }

    // 更新字幕位置（字幕使用毫秒）
    if (mSubtitleWidget) {
        UpdateSubtitlePosition(currentPositionMs);
    }
}

void StereoVideoWidget::onPlaybackStateChanged(PlaybackState state)
{
    // BUG-038 修复：当播放状态变为 Playing 时，重置位置跟踪
    // 这确保了当切换到新视频时，进度条位置从头开始
    if (state == PlaybackState::Playing) {
        resetPositionTracking();
        if (logger) {
            logger->debug("StereoVideoWidget::onPlaybackStateChanged: Reset position tracking for new playback");
        }
    }

    // 添加异常保护
    try {
        switch (state) {
        case PlaybackState::Stopped:
        case PlaybackState::Error:
            // 停止渲染
            StopRendering();
            break;
        case PlaybackState::Paused:
            // 暂停状态：可以保持当前画面，或者显示暂停提示
            // 当前实现：保持当前画面，不停止渲染
            break;
        case PlaybackState::Playing:
            // 播放状态：确保渲染正常进行
            // 如果之前停止了渲染，需要重新启动
            if (!isRendering_) {
                // 获取当前视频信息并重新启动渲染
                if (playController_ && playController_->isOpened()) {
                    // TODO: 从 PlayController 获取视频信息（格式、帧率等）
                    // 当前先不处理，因为 StartRendering 需要参数
                }
            }
            break;
        default:
            break;
        }
    } catch (const std::exception &e) {
        if (logger) {
            logger->error("StereoVideoWidget::onPlaybackStateChanged: Exception: {}", e.what());
        }
    } catch (...) {
        if (logger) {
            logger->error("StereoVideoWidget::onPlaybackStateChanged: Unknown exception");
        }
    }
}

void StereoVideoWidget::resetPositionTracking()
{
    // BUG-038 修复：重置进度条位置跟踪状态
    // 当切换到新视频时调用，确保进度条从头开始
    lastPositionSeconds_ = -1;
}


// 其他接口实现（与 FFmpegView 兼容）
bool StereoVideoWidget::TakeScreenshot()
{
    QString screenShotRootFolder = GlobalDef::getInstance()->SCREENSHOT_DIR;

    // 通过 VideoRenderer 的 widget 截图
    QWidget *videoWidget = nullptr;
    if (videoRenderer_) {
        auto *oglRenderer = dynamic_cast<OpenGLRenderer *>(videoRenderer_.get());
        if (oglRenderer) {
            videoWidget = oglRenderer->widget();
        }
    }
    if (videoWidget) {
        QPixmap pixmap = videoWidget->grab();
        if (!pixmap.isNull()) {
            // 创建截图文件夹（如果不存在）
            if (!QDir::current().exists(screenShotRootFolder)) {
                bool createdOk = QDir::current().mkdir(screenShotRootFolder);
                if (!createdOk) {
                    if (logger)
                        logger->warn("StereoVideoWidget: Failed to create screenshot folder: {}", screenShotRootFolder.toStdString());
                    return false;
                }
            }

            // 生成文件名
            QDateTime now = QDateTime::currentDateTime();
            QString timeStr = now.toString("yyyyMMddHHmmss");
            QString savedFilePath = screenShotRootFolder + "/Snapshots_" + timeStr + ".png";

            // 保存截图
            bool saved = pixmap.save(savedFilePath);
            if (logger && saved) {
                logger->info("StereoVideoWidget: Screenshot saved to: {}", savedFilePath.toStdString());
            }
            return saved;
        }
    }
    return false;
}

void StereoVideoWidget::SetSeeking(const std::string &parentFunc, bool seekFlag)
{
    Q_UNUSED(parentFunc)
    // 设置 seeking 状态（可以用于暂停字幕更新等）
    if (mSubtitleWidget && seekFlag) {
        // 在 seeking 时暂停字幕
        mSubtitleWidget->PlayPause(false);
    } else if (mSubtitleWidget && !seekFlag) {
        // seeking 结束后恢复字幕
        mSubtitleWidget->PlayPause(true);
    }
}

bool StereoVideoWidget::LoadSubtitle(QString filename)
{
    if (mSubtitleWidget) {
        // 委托给 SubtitleWidget 加载字幕
        return mSubtitleWidget->SetSubtitleFile(filename);
    }
    return false;
}

void StereoVideoWidget::StopSubtitle()
{
    if (mSubtitleWidget) {
        // 委托给 SubtitleWidget 停止字幕
        mSubtitleWidget->Stop();
        mSubtitleWidget->hide();
    }
}

bool StereoVideoWidget::UpdateSubtitlePosition(int64_t timestamp)
{
    if (mSubtitleWidget) {
        // 委托给 SubtitleWidget 更新字幕位置（通过 Seek）
        return mSubtitleWidget->Seek(timestamp);
    }
    return false;
}

void StereoVideoWidget::setUseDefault2DShader(bool use)
{
    // setUseDefault2DShader在OpenGLCommon中，需要通过drawable访问
    auto *oglRenderer = dynamic_cast<OpenGLRenderer *>(videoRenderer_.get());
    if (oglRenderer && oglRenderer->drawable()) {
        oglRenderer->drawable()->setUseDefault2DShader(use);
        if (logger) {
            logger->info("StereoVideoWidget::setUseDefault2DShader: Set to {}", use);
        }
    }
}

bool StereoVideoWidget::isUsingDefault2DShader() const
{
    // isUsingDefault2DShader在OpenGLCommon中，需要通过drawable访问
    auto *oglRenderer = dynamic_cast<OpenGLRenderer *>(videoRenderer_.get());
    if (oglRenderer && oglRenderer->drawable()) {
        return oglRenderer->drawable()->isUsingDefault2DShader();
    }
    return false;
}
