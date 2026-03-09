#include "FFmpegView.h"
#include "GlobalDef.h"
#include <iostream>
#include <QDateTime>
#include <QDebug>
#include <QMessageBox>
#include <QObject>
#include <QPainter>
#include <QSettings>
#include <QCoreApplication>

#include <atomic>
#include <fstream>
#include <sstream>
#include <streambuf>
#include <string>
#include <QDir>
#include <QPixmap>

#include "FloatButton.h"
#include "FullscreenTipsWidget.h"
#include "SubtitleWidget.h"
#include "spdlog/spdlog.h"

FFmpegView::FFmpegView(QWidget *parent)
    : QOpenGLWidget(parent)
    , mCamera(nullptr)
{
    //直接将OK的shader放进resources，外部不放shader文件，直接导入，但若外部存在shader文件时，则替换掉原有的shader文件
    bool loadShaderOk = false;
    if (false == loadInternalShaders()) {
        logger->warn("loadInternalShaders failed.");
    } else {
        logger->info("loadInternalShaders ok.");
        loadShaderOk = true;
    }

    bool loadExternalShaderOk = false;
    if (false == loadExternalShaders()) {
        logger->warn("loadExternalShaders failed.");
    } else {
        logger->info("loadExternalShaders ok.");
        loadExternalShaderOk = true;
    }

    if (!loadShaderOk && loadExternalShaderOk) {
        logger->critical("ShaderInit Failed");
        return;
    }

    //当读外部shader正常时，替换掉原有的内部shader
    if (loadExternalShaderOk) {
        vertexSource_ = externalVertexSource_;
        fragmentSource_ = externalFragmentSource_;
    }

    //关联widgets初始化
    if (!butWidget) {
        butWidget = new FloatButton(this);
        butWidget->hide();
    }

    if (!mWindowLogo) {
        mWindowLogo = new QLabel(this);
        mWindowLogPM.load(GlobalDef::getInstance()->PLAY_WINDOW_LOGO_PATH);
        mWindowLogo->resize(mWindowLogPM.size());
        mWindowLogo->setPixmap(mWindowLogPM);
    }

    mFullscreenTipsWidget = new FullscreenTipsWidget(this);
    mFullscreenTipsWidget->move(QPoint(0, 0));
    mFullscreenTipsWidget->show();

    mSubtitleWidget = new SubtitleWidget(this);
    mSubtitleWidget->move(QPoint(0, height() - mSubtitleFontHeight));
    mSubtitleWidget->resize(QSize(width(), mSubtitleFontHeight));
    mSubtitleWidget->show();

    // 读取 ShowFPS 配置
    QString cfgPath = QCoreApplication::applicationDirPath() + "/config/SystemConfig.ini";
    QSettings setting(cfgPath, QSettings::IniFormat);
    QVariant variant = setting.value("/System/ShowFPS");
    mShowFPS = variant.isNull() ? false : variant.toBool();
    
    // 初始化 FPS 显示标签
    if (!mFPSLabel) {
        mFPSLabel = new QLabel(this);
        mFPSLabel->setStyleSheet(
            "QLabel {"
            "background-color: rgba(0, 0, 0, 150);"
            "color: #00FF00;"
            "font-size: 16px;"
            "font-weight: bold;"
            "padding: 4px 8px;"
            "border-radius: 4px;"
            "}"
        );
        mFPSLabel->setText("FPS: --");
        mFPSLabel->setAlignment(Qt::AlignCenter);
        mFPSLabel->resize(100, 30);
        mFPSLabel->show();  // 默认显示，根据配置隐藏
    }
    
    // 初始化 FPS 计算相关变量
    mLastFPSTime = std::chrono::steady_clock::now();
    mFPSCounter = 0;
    mCurrentFPS = 0.0f;

    //默认3D渲染相关参数
    mStereoFormat = StereoFormat::STEREO_FORMAT_NORMAL_2D;
    mStereoInputFormat = StereoInputFormat::STEREO_INPUT_FORMAT_LR;
    mStereoOutputFormat = StereoOutputFormat::STEREO_OUTPUT_FORMAT_VERTICAL;

    connect(&renderTimer, &QTimer::timeout, this, &FFmpegView::OnRenderTimer);
    connect(&mPlayStatusUpdateTimer, &QTimer::timeout, this, &FFmpegView::OnUpdateStatusTimer);
}

FFmpegView::~FFmpegView()
{
    if (mWindowLogo) {
        delete mWindowLogo;
        mWindowLogo = nullptr;
    }
    if (butWidget) {
        delete butWidget;
        butWidget = nullptr;
    }
    
    if (mFPSLabel) {
        delete mFPSLabel;
        mFPSLabel = nullptr;
    }

    if (renderTimer.isActive()) {
        renderTimer.stop();
    }
    if (mPlayStatusUpdateTimer.isActive()) {
        mPlayStatusUpdateTimer.stop();
    }
    if (mFullscreenTipsWidget) {
        delete mFullscreenTipsWidget;
        mFullscreenTipsWidget = nullptr;
    }
    if (mSubtitleWidget) {
        delete mSubtitleWidget;
        mSubtitleWidget = nullptr;
    }
}

bool FFmpegView::loadInternalShaders()
{
    QFile vertexFile(":/MainWindow/Shader/vertex.glsl");
    if (!vertexFile.exists()) {
        logger->warn("internal vertex file not exists.");
        return false;
    }
    logger->info("internal vertex file exists");
    if (!vertexFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        logger->warn("internal vertex open failed.");
        return false;
    }
    QTextStream vertexIn(&vertexFile);
    vertexSource_.clear();
    vertexSource_ = vertexIn.readAll();
    logger->info("read internal VertexContent ok");

    QFile fragmentFile(":/MainWindow/Shader/fragment.glsl");
    if (!fragmentFile.exists()) {
        logger->warn("internal fragment file not exists");
        return false;
    }
    logger->info("internal fragment file exists.");
    if (!fragmentFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        logger->warn("internal fragment open failed.");
        return false;
    }
    QTextStream fragIn(&fragmentFile);
    fragmentSource_.clear();
    fragmentSource_ = fragIn.readAll();
    logger->info("read internal fragmentContent ok");

    return true;
}

bool FFmpegView::loadExternalShaders()
{
    QFile vertexFile("./Shader/vertex.glsl");
    if (!vertexFile.exists()) {
        logger->warn("external vertex file not exists.");
        return false;
    }
    logger->info("external vertex file exists");
    if (!vertexFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        logger->warn("external vertex open failed.");
        return false;
    }
    QTextStream vertexIn(&vertexFile);
    externalVertexSource_.clear();
    externalVertexSource_ = vertexIn.readAll();
    logger->info("read ExternalVertexContent:");
    logger->info(externalVertexSource_.toUtf8().constData());

    QFile fragmentFile("./Shader/fragment.glsl");
    if (!fragmentFile.exists()) {
        logger->warn("external fragment file not exists");
        return false;
    }
    logger->info("external fragment file exists.");
    if (!fragmentFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        logger->warn("external fragment open failed.");
        return false;
    }
    QTextStream fragIn(&fragmentFile);
    externalFragmentSource_.clear();
    externalFragmentSource_ = fragIn.readAll();
    logger->info("read external fragmentContent:");
    logger->info(externalFragmentSource_.toUtf8().constData());

    return true;
}

int FFmpegView::StartRendering(StereoFormat stereoFormat, StereoInputFormat stereoInputFormat, StereoOutputFormat stereoOutputFormat, float frameRate)
{
    logger->info(
        "startRendering with: format: {}, inputFormat:{}, outputFormat:{}, frameRate：{}",
        int(stereoFormat),
        int(stereoInputFormat),
        int(stereoOutputFormat),
        float(frameRate));

    if (mWindowLogo) {
        mWindowLogo->hide();
    }

    mStereoFormat = stereoFormat;
    mStereoInputFormat = stereoInputFormat;
    mStereoOutputFormat = stereoOutputFormat;
    elapsedInSeconds_ = 0;

    // 初始化渲染健康检查 - 重置所有状态，确保新视频开始时是干净的状态
    mLastPaintGLTime = std::chrono::steady_clock::now();
    mRenderStallCounter = 0;
    
    // 重置 FPS 计数器
    mLastFPSTime = std::chrono::steady_clock::now();
    mFPSCounter = 0;
    mCurrentFPS = 0.0f;
    
    // 确保FPS标签在开始渲染时正确显示（如果配置启用）
    if (mFPSLabel && mShowFPS) {
        int labelWidth = 100;
        int labelHeight = 30;
        int margin = 10;
        int x = width() - labelWidth - margin;
        int y = margin;
        mFPSLabel->move(x, y);
        mFPSLabel->show();
        logger->debug("FPS label enabled and shown at start rendering");
    } else if (mFPSLabel) {
        mFPSLabel->hide();
        logger->debug("FPS label disabled");
    }

    connect(&renderTimer, &QTimer::timeout, this, &FFmpegView::OnRenderTimer);
    connect(&mPlayStatusUpdateTimer, &QTimer::timeout, this, &FFmpegView::OnUpdateStatusTimer);

    renderTimer.start(1000.0 / frameRate);
    mPlayStatusUpdateTimer.start(1000); //Update ProgressBar & timeElapsedUi every seconds
    
    // 强制触发一次更新，确保paintGL被调用，更新mLastPaintGLTime
    // 这对于从睡眠恢复或长时间播放后的视频切换很重要
    update();

    //在经过文件名解析出对应的格式后，应该通知MainWindow将对应Action状态更新，以防前后视频变更后UI状态变乱
    logger->info(
        "emit stereoFormatChanged with Format: {},  inputFormat:{},  outputFormat:{}", int(mStereoFormat), int(mStereoInputFormat), int(mStereoOutputFormat));
    //emit stereoFormatChanged(mStereoFormat, mStereoInputFormat, mStereoOutputFormat);

    pts_ = std::numeric_limits<int64_t>::min();

    fps_ = frameRate;

    logger->info("==========================>render time interval:{}", 1000.0 / frameRate);
    if (mSubtitleWidget && mLoadSubtitleSuccess) {
        mSubtitleWidget->Start();
    }

    mIsRendering = true;

    return 0;
}

bool FFmpegView::IsRendering()
{
    return mIsRendering;
}

void FFmpegView::OnRenderTimer()
{
    static int renderCounter = 0;
    renderCounter += 1;
    if (renderCounter % (int(fps_) / 2) == 0) {
        //防止打印过多信息，一秒打印两条
        logger->debug("000000----OnRenderTimer--------000000000000");
    }

    // 渲染健康检查：如果paintGL太久没被调用，说明渲染可能停滞了
    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastRender = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - mLastPaintGLTime).count();
    
    // 如果超过3秒（3000ms）没有渲染，说明渲染线程可能停止了
    // 这可能是因为系统休眠、显示器关闭或OpenGL上下文失效
    const int64_t RENDER_STALL_THRESHOLD_MS = 3000;
    
    if (mIsRendering && timeSinceLastRender > RENDER_STALL_THRESHOLD_MS) {
        mRenderStallCounter++;
        if (mRenderStallCounter == 1 || mRenderStallCounter % 10 == 0) {
            // 第一次或每10次打印一次警告
            logger->warn("Render stall detected! Last paintGL was {}ms ago (threshold: {}ms). "
                        "This may be due to system sleep, display off, or OpenGL context loss. "
                        "Attempting recovery by forcing frame consumption...",
                        timeSinceLastRender, RENDER_STALL_THRESHOLD_MS);
        }
        
        // 尝试恢复：强制调用currentFrame()来推进队列，并强制触发重绘
        // 这样可以防止解码线程死锁，并尝试恢复渲染
        if (movie_ && !movie_->IsStopped() && !movie_->IsPaused()) {
            try {
                // 检查OpenGL上下文是否有效
                if (context() && context()->isValid()) {
                    // 强制获取并释放一帧，推进队列
                    auto frameData = movie_->currentFrame();
                    if (frameData.first) {
                        logger->debug("Forced frame consumption during render stall, queue advanced");
                    }
                    // 强制触发重绘，尝试恢复渲染
                    // 使用QMetaObject::invokeMethod确保在主线程中执行
                    QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);
                } else {
                    logger->warn("OpenGL context invalid during render stall, cannot recover");
                }
            } catch (const std::exception& e) {
                logger->error("Exception during render stall recovery: {}", e.what());
            }
        }
    } else {
        // 渲染正常，重置计数器
        if (mRenderStallCounter > 0) {
            logger->info("Render stall recovered after {} attempts", mRenderStallCounter);
            mRenderStallCounter = 0;
        }
    }

    update();
}

void FFmpegView::OnUpdateStatusTimer()
{
    logger->debug("==updateStatus, elspedInSeconds:==> {}", elapsedInSeconds_);
    if (mEnableStereoRegion) {
        logger->info("OnUpdateStatusTimer, show Partial3D...");
        bool showOrHide = true;
        mFullscreenTipsWidget->SetRenderInfoWithoutTimer(showOrHide, QString(tr("局部3D")));
        mFullscreenTipsWidget->update();
    }
    else {
        bool showOrHide = false;
        mFullscreenTipsWidget->SetRenderInfoWithoutTimer(showOrHide, "");
        mFullscreenTipsWidget->update();
    }

    if (mSeeking) {
        logger->info(" is Seeking, not need to updatePlayProgress.");
        return;
    }

    emit updatePlayProcess(elapsedInSeconds_);
}

bool FFmpegView::PlayPause(bool isPause)
{
    logger->info("FFmpegView::PlayPause, isPause? {}", isPause);
    movie_->PlayPause(isPause);
    if (mSubtitleWidget && mLoadSubtitleSuccess) {
        mSubtitleWidget->PlayPause(isPause);
    }
    return true;
}

bool FFmpegView::StopRendering()
{
    logger->info("7777000 FFmpegView::StopRendering");
    mStereoFormat = STEREO_FORMAT_NORMAL_2D;
    mStereoInputFormat = STEREO_INPUT_FORMAT_LR;
    mStereoOutputFormat = STEREO_OUTPUT_FORMAT_VERTICAL;

    logger->info(
        " disconnect & stop Timer. before disconnect&stop, the activate status: activate? {},  mPlayStatusUpdateTimer:{}",
        renderTimer.isActive(),
        mPlayStatusUpdateTimer.isActive());
    if (renderTimer.isActive()) {
        renderTimer.disconnect();
        renderTimer.stop();
    }
    if (mPlayStatusUpdateTimer.isActive()) {
        mPlayStatusUpdateTimer.disconnect();
        mPlayStatusUpdateTimer.stop();
    }

    logger->info(
        " disconnect & stop Timer. after disconnect&stop, the activate status: activate?{}, mPlayStatusUpdateTimer:{}",
        renderTimer.isActive(),
        mPlayStatusUpdateTimer.isActive());

    // 重置渲染停滞计数器，确保下次启动时是干净的状态
    mRenderStallCounter = 0;
    mLastPaintGLTime = std::chrono::steady_clock::now();
    
    // 隐藏 FPS 标签
    if (mFPSLabel) {
        mFPSLabel->hide();
    }

    update();
    if (mWindowLogo) {
        mWindowLogo->show();
    }
    if (mSubtitleWidget) {
        mSubtitleWidget->Stop();
    }

    mIsRendering = false;

    return true;
}

StereoFormat FFmpegView::SetStereoFormat(StereoFormat stereoFormat)
{
    mStereoFormat = stereoFormat;
    shaderSetInt(KEY_STEREO_FLAG, mStereoFormat);
    return mStereoFormat;
}

StereoFormat FFmpegView::ToggleStereo(bool stereo_checked)
{
    logger->info(" mStereoFormat:{}, stereo_checked:{}", int(mStereoFormat), stereo_checked);
    if (stereo_checked) {
        mStereoFormat = StereoFormat::STEREO_FORMAT_3D;

        shaderSetInt(KEY_STEREO_FLAG, STEREO_FORMAT_3D);
    } else {
        mStereoFormat = StereoFormat::STEREO_FORMAT_NORMAL_2D;
        shaderSetInt(KEY_STEREO_FLAG, STEREO_FORMAT_NORMAL_2D);
    }
    return mStereoFormat;
}

StereoInputFormat FFmpegView::SetStereoInputFormat(StereoInputFormat inputFormat)
{
    if (inputFormat >= 3 or inputFormat < 0) {
        logger->warn("SetStereoInputFormat failed, inputFormat wrong:{}", int(inputFormat));
        return mStereoInputFormat;
    }
    logger->info("FFmpgView::SetStereoInputFormat, stereoInputFormat: {}", (int) inputFormat);
    mStereoInputFormat = inputFormat;
    shaderSetInt(KEY_STEREO_INPUT_FORMAT, (int) inputFormat);
}

bool FFmpegView::SetRenderInputSource(RenderInputSource ris)
{
    mRenderInputSource = ris;
    shaderSetInt(KEY_RENDER_INPUT_SOURCE, (int) ris);
    return true;
}

StereoOutputFormat FFmpegView::SetStereoOutputFormat(StereoOutputFormat outputFormat)
{
    if (outputFormat >= 4 or outputFormat < 0) {
        logger->warn("SetStereoOutputFormat failed, outputFormat wrong:{}", int(outputFormat));
        return mStereoOutputFormat;
    }
    mStereoOutputFormat = outputFormat;
    shaderSetInt(KEY_STEREO_OUTPUT_FORMAT, (int) outputFormat);
    return mStereoOutputFormat;
}

StereoOutputFormat FFmpegView::GetStereoOutputFormat()
{
    return mStereoOutputFormat;
}

bool FFmpegView::SetStereoEnableRegion(bool enabled, int tlX, int tlY, int brX, int brY)
{
    if (enabled) {
        shaderSetBool(KEY_ENABLE_REGION, true);
        logger->info("SetStereoEnableRegion, tx:{},ty:{}, brX:{}, bry:{}, width:{}, height:{}", tlX, tlY, brX, brY, width(), height());
        float vecTopLeftX = tlX / float(width());
        float vecBottomRightX = brX / float(width());

        //Y轴坐标需要转换，以便UI选择的坐标系与实际渲染坐标系选择对齐 (按比例对齐,并减去可能的上下黑边)
        float compressHeight = (texHeight / float(texWidth)) * viewWidth;
        int topMargin = int(0.5 * (viewHeight - compressHeight));
        int stereoTLY = tlY - topMargin;
        if (stereoTLY < 0) {
            stereoTLY = 0;
        }
        int stereoBRY = brY - topMargin;
        if (stereoBRY > compressHeight) {
            stereoBRY = compressHeight;
        }

        float vecTopLeftY = stereoTLY / compressHeight;
        float vecBottomRightY = stereoBRY / compressHeight;

        logger->info("SetStereoEnableRegion, vecTopleft({}x{}) vecBottomRight({}x{})", vecTopLeftX, vecTopLeftY, vecBottomRightX, vecBottomRightY);
        shaderSetRect(KEY_VEC_REGION, vecTopLeftX, vecTopLeftY, vecBottomRightX, vecBottomRightY);
        mEnableStereoRegion = true;
        vecStereoRegion[0] = vecTopLeftX;
        vecStereoRegion[1] = vecTopLeftY;
        vecStereoRegion[2] = vecBottomRightX;
        vecStereoRegion[3] = vecBottomRightY;
        mStereoFormat = STEREO_FORMAT_3D;
        SetFullscreenMode(FULLSCREEN_PLUS_STRETCH);
    } else {
        shaderSetBool(KEY_ENABLE_REGION, false);
        mEnableStereoRegion = false;
        vecStereoRegion[0] = 0.0;
        vecStereoRegion[1] = 0.0;
        vecStereoRegion[2] = 1.0;
        vecStereoRegion[3] = 1.0;
    }

    return true;
}

bool FFmpegView::CancelStereoRegion()
{
    shaderSetBool(KEY_ENABLE_REGION, false);
    mEnableStereoRegion = false;
    vecStereoRegion[0] = 0.0;
    vecStereoRegion[1] = 0.0;
    vecStereoRegion[2] = 1.0;
    vecStereoRegion[3] = 1.0;

    return true;
}

void FFmpegView::IncreaseParallax()
{
    if (mParallaxShift >= GlobalDef::getInstance()->MAX_ALLOW_ADJUST_PARALLAX) {
        logger->warn("IncreaseParallax failed, exceeds max:{}", GlobalDef::getInstance()->MAX_ALLOW_ADJUST_PARALLAX);
        return;
    }
    mParallaxShift += 2;

    ratio = -1; //重置ratio以便Render的时候，能够进到r!=ratio分支进行实际的parallaxShift修改
    logger->info("parallaxShift:{} ", mParallaxShift);
    update();
}

void FFmpegView::DecreaseParallax()
{
    if (mParallaxShift <= GlobalDef::getInstance()->MIN_ALLOW_ADJUST_PARALLAX) {
        logger->warn("DecreaseParallax failed, exceeds min:{}", GlobalDef::getInstance()->MIN_ALLOW_ADJUST_PARALLAX);
        return;
    }
    mParallaxShift -= 2;

    ratio = -1; //重置ratio以便Render的时候，能够进到r!=ratio分支进行实际的parallaxShift修改
    logger->info("parallaxShift:{} ", mParallaxShift);
    update();
}

void FFmpegView::ResetParallax()
{
    mParallaxShift = 0;

    ratio = -1; //重置ratio以便Render的时候，能够进到r!=ratio分支进行实际的parallaxShift修改
    logger->info("parallaxShift:{} ", mParallaxShift);
    update();
}

void FFmpegView::SetRelativeMovie(Movie *movie)
{
    movie_ = movie;
}

bool FFmpegView::TakeScreenshot()
{
    QString savedFilePath;
    QString screenShotRootFolder = GlobalDef::getInstance()->SCREENSHOT_DIR;
    bool screenShotOK = SaveImage(screenShotRootFolder, savedFilePath);
    if (screenShotOK) {
        QMessageBox::information(this, QString(tr("截图成功")), QString(tr("截图保存已存成文件：%1")).arg(savedFilePath));
        return true;
    }
    return false;
}

void FFmpegView::keyPressEvent(QKeyEvent *event)
{
    QKeyCombination keyCombination = event->keyCombination();

    switch (keyCombination.keyboardModifiers()) {
    case Qt::ControlModifier: {
        switch (keyCombination.key()) {
        //case Qt::Key_W: {
        //    //视差调节
        //    if (GlobalDef::getInstance()->mRunningMode == RunningMode::RM_PRO) {
        //        IncreaseParallax();
        //    }
        //} break;
        //case Qt::Key_E: {
        //    //视差调节
        //    if (GlobalDef::getInstance()->mRunningMode == RunningMode::RM_PRO) {
        //        DecreaseParallax();
        //    }
        //} break;
        //case Qt::Key_R: {
        //    //视差调节
        //    if (GlobalDef::getInstance()->mRunningMode == RunningMode::RM_PRO) {
        //        ResetParallax();
        //    }
        //} break;
        //case Qt::Key_N: {
        //    if (GlobalDef::getInstance()->mRunningMode == RunningMode::RM_PRO) {
        //        TakeScreenshot();
        //    }
        //} break;
        case Qt::Key_M: {
            if (GlobalDef::getInstance()->mRunningMode == RunningMode::RM_PRO) {
                ToggleViewParallaxSideStrip();
            }
        } break;
        default:
            break;
        }
    } break;
    default:
        break;
    }
}

void FFmpegView::ToggleViewParallaxSideStrip()
{
    mEnableStripParallaxSideView = !mEnableStripParallaxSideView;
    logger->info("!!!!!!!Parallax Adjustment. mEnableStripParallaxSideView:{}", mEnableStripParallaxSideView);
    update();
}

void FFmpegView::initializeGL()
{
    //////////////////////////////////////////////////////////////////////
    //  3D play

    initializeOpenGLFunctions();

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices1), vertices1, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(glIndices), glIndices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *) (3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glGenTextures(3, texs);
    for (int i = 0; i < 3; i++) {
        glBindTexture(GL_TEXTURE_2D, texs[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    if (false == shaderInit(vertexSource_, fragmentSource_)) {
        logger->critical("shaderInit failed.");
        return;
    }
    logger->info("shaderInit ok");

    shaderUse();
    shaderSetInt("textureY", 0);
    shaderSetInt("textureU", 1);
    shaderSetInt("textureV", 2);
    shaderSetInt("textureUV", 3);
}

void FFmpegView::resizeEvent(QResizeEvent *e)
{
    QOpenGLWidget::resizeEvent(e);
    
    // 更新 FPS 标签位置（右上角）
    if (mFPSLabel && mShowFPS) {
        int labelWidth = 100;
        int margin = 10;
        int x = width() - labelWidth - margin;
        int y = margin;
        mFPSLabel->move(x, y);
    }
}

void FFmpegView::moveEvent(QMoveEvent *event)
{
    if (event == nullptr) {
        return;
    }

    const int MahattenLengthCheckForDiffMoveAction = 4;
    // （本窗体相对父窗体+父窗体） 在全局中的位置
    QPoint gpt = this->mapToGlobal(mapToParent(pos()) + parentWidget()->pos());
    logger->info(
        "FFmpegView::moveEvent, pos:{}x{}, mapToParent:{}x{}, parentPos:{}x{} gpt: {}x{}",
        pos().x(),
        pos().y(),
        mapToParent(pos()).x(),
        mapToParent(pos()).y(),
        parentWidget()->pos().x(),
        parentWidget()->pos().y(),
        gpt.x(),
        gpt.y());

    if (gpt.x() % 2 != 0 || gpt.y() % 2 != 0) {
        QPoint pointDiff = gpt - mLastGpt;
        logger->warn(
            "!!FFmpegView. moveEvent origin %2 != 0, render may Failed. ori: {}x{}, with the last point{},{}, manhattenLength:{}",
            gpt.x(),
            gpt.y(),
            mLastGpt.x(),
            mLastGpt.y(),
            pointDiff.manhattanLength());
        if (pointDiff.manhattanLength() > MahattenLengthCheckForDiffMoveAction) {
            logger->info("FFmpegView.moveEvent, gpt:{}x{} will be a new moveMent, lastGpt:{}x{}, update lastGpt", gpt.x(), gpt.y(), mLastGpt.x(), mLastGpt.y());
        } else {
            logger->warn(
                "FFmpegView.moveEvent, gpt:{}x{} is the same movement, lastGpt:{}x{}, No need to update lastGpt", gpt.x(), gpt.y(), mLastGpt.x(), mLastGpt.y());
            return;
        }

        // 若需要修正，则仅修正当前窗体的位置
        QPoint oldPos = this->pos();
        QPoint newPos = oldPos;
        if (gpt.x() % 2 != 0) {
            int shiftX = mMinorShiftXForRightStereo == 1 ? -1 : 1;
            newPos = QPoint(oldPos.x() + shiftX, oldPos.y());
            logger->info("gpt.x()%2=={} (!=0), oldPos:{}x{}, newPos update:{}x{}", gpt.x() % 2, oldPos.x(), oldPos.y(), newPos.x(), newPos.y());
            mMinorShiftXForRightStereo = shiftX;
        }
        if (gpt.y() % 2 != 0) {
            int shiftY = mMinorShiftYForRightStereo == 1 ? -1 : 1;
            newPos = QPoint(newPos.x(), newPos.y() + shiftY);
            logger->info("gpt.y()%2=={} (!=0), newPos update:{}x{}", gpt.y() % 2, newPos.x(), newPos.y());
            mMinorShiftYForRightStereo = shiftY;
        }
        QPoint newgpt = this->mapToGlobal(mapToParent(newPos) + parentWidget()->pos());
        logger->warn(
            "!!!moveEvent, oldPos:{}x{}, newPos:{}x{}, oldGPT: {}x{} => newGPT:{}x{} SHOULD BE EVEN",
            oldPos.x(),
            oldPos.y(),
            newPos.x(),
            newPos.y(),
            gpt.x(),
            gpt.y(),
            newgpt.x(),
            newgpt.y());
        mLastGpt = newgpt;

        logger->info(
            "Finished, !! Move to NewPos:{}x{}, oldPos:{}x{}, lastGpt changed:{}x{}, mMinorSfhitXForRightStereo:{}, mMinorShiftYForRightStereo:{}",
            newPos.x(),
            newPos.y(),
            oldPos.x(),
            oldPos.y(),
            mLastGpt.x(),
            mLastGpt.y(),
            mMinorShiftXForRightStereo,
            mMinorShiftYForRightStereo);
        this->move(newPos);
    }

    //#if 0
    //    QOpenGLWidget::moveEvent(event);
    //#endif
}

void FFmpegView::paintEvent(QPaintEvent *e)
{
    QOpenGLWidget::paintEvent(e);
    
    // 更新 FPS 标签位置（右上角）和显示状态
    if (mFPSLabel && mShowFPS) {
        int labelWidth = 100;
        int labelHeight = 30;
        int margin = 10;
        int x = width() - labelWidth - margin;
        int y = margin;
        mFPSLabel->move(x, y);
        mFPSLabel->show();
    } else if (mFPSLabel) {
        mFPSLabel->hide();
    }
}

void FFmpegView::resizeGL(int w, int h)
{
    if (butWidget) {
        butWidget->move((width() - butWidget->width()), (height() - butWidget->height()) / 2);
    }
    if (mSubtitleWidget) {
        mSubtitleWidget->move(QPoint(0, height() - mSubtitleFontHeight));
        mSubtitleWidget->resize(width(), mSubtitleFontHeight);
    }

    if (mWindowLogo) {
        mWindowLogo->move((width() - mWindowLogo->width()) / 2, (height() - mWindowLogo->height()) / 2);
    }

    viewWidth = w;
    viewHeight = h;
    QOpenGLWidget::resizeGL(w, h);
    glViewport(0, 0, w, h);
}

void FFmpegView::paintGL()
{
    // 更新最后渲染时间，用于健康检查
    mLastPaintGLTime = std::chrono::steady_clock::now();
    
    // 计算 FPS
    // FPS 计算和显示（在paintGL中，每次渲染都会调用）
    if (mShowFPS && mFPSLabel) {
        mFPSCounter++;
        auto now = std::chrono::steady_clock::now();
        
        // 初始化时间戳（首次调用）
        if (mLastFPSTime.time_since_epoch().count() == 0) {
            mLastFPSTime = now;
            mFPSCounter = 0;
        }
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - mLastFPSTime).count();
        
        // 每秒更新一次 FPS
        if (elapsed >= 1000) {
            mCurrentFPS = (mFPSCounter * 1000.0f) / elapsed;
            mFPSCounter = 0;
            mLastFPSTime = now;
            
            // 更新 FPS 标签文本
            mFPSLabel->setText(QString("FPS: %1").arg(mCurrentFPS, 0, 'f', 1));
            // 确保标签可见且位置正确
            if (!mFPSLabel->isVisible()) {
                int labelWidth = 100;
                int margin = 10;
                int x = width() - labelWidth - margin;
                int y = margin;
                mFPSLabel->move(x, y);
                mFPSLabel->show();
            }
            logger->info("Rendering FPS updated: {}", mCurrentFPS);
        }
    }
    
    static int renderCounter = 0;
    renderCounter += 1;
    if (renderCounter >= 100000) {
        renderCounter = 0;
    }
    if (renderCounter % 30 == 0) {
        //适配于硬件，渲染时OpenGLWidget的原点应均为偶数

        //TEST: 检测全局坐标数值是否对
        //QPoint gpt = this->mapToGlobal(pos());
        //if (gpt.x() % 2 != 0 || gpt.y() % 2 != 0) {
        //    logger->warn("!! GLView origin %2 != 0, render may Failed. ori: {}x{}", gpt.x(), gpt.y());
        //}
    }
    
    // 检查OpenGL上下文是否有效
    if (!context() || !context()->isValid()) {
        logger->warn("OpenGL context is invalid in paintGL, skipping render");
        return;
    }

    //QPoint gpt = this->mapToGlobal(pos());
    //if (gpt.x() % 2 != 0 || gpt.y()%2 != 0) {
    //    logger->warn("!!! CHECK!!!!! GLView origin %2 != 0, render may Failed. ori: {}x{}", gpt.x(), gpt.y());
    //    QPoint oldPos = this->pos();
    //    QPoint newPos = oldPos;
    //    if (oldPos.x() % 2 != 0) {
    //        newPos = QPoint(oldPos.x()+1, oldPos.y());
    //    }
    //    if (oldPos.y() % 2 != 0) {
    //        newPos = QPoint(newPos.x(), newPos.y() + 1);
    //    }
    //    QPoint newgpt = this->mapToGlobal(newPos);
    //    logger->warn("!!! CHECK!!!!! GLView origin %2 != 0, render may Failed. ori: {}x{} SHOULD BE EVEN", newgpt.x(), newgpt.y());
    //}

    if (mRenderInputSource == RenderInputSource::RIS_VIDEO_FILE) {
        //  3D play for VideoFile
        // 添加超时机制获取帧，避免UI线程阻塞
        std::pair<AVFrame *, int64_t> frameData = {nullptr, 0};
        
        if (movie_ && !movie_->IsStopped()) {
            // 注意：QOpenGLWidget的paintGL已经自动调用了makeCurrent()
            // 我们只需要检查上下文是否有效
            if (!context() || !context()->isValid()) {
                logger->warn("OpenGL context invalid, cannot render frame");
                return;
            }
            
            try {
                frameData = movie_->currentFrame();
            } catch (const std::exception& e) {
                logger->error("Exception in currentFrame(): {}", e.what());
                return;
            }
            
            // 如果获取不到有效帧，记录日志但不直接返回，尝试继续渲染流程
            // 这样可以确保队列能够继续推进，防止死锁
            if (!frameData.first || frameData.first->width == 0 || frameData.first->height == 0) {
                static int invalidFrameCounter = 0;
                invalidFrameCounter++;
                if (invalidFrameCounter % 30 == 0) {  // 每30次无效帧才打印一次，避免日志过多
                    logger->debug("Invalid frame data (count:{}), frame ptr:{}, width:{}, height:{}", 
                                invalidFrameCounter, 
                                (void*)frameData.first,
                                frameData.first ? frameData.first->width : 0,
                                frameData.first ? frameData.first->height : 0);
                }
                // 即使帧无效，也不立即返回，继续执行下面的逻辑
                // 这样确保即使遇到无效帧，currentFrame()也会推进队列，防止死锁
                // 但我们需要确保下面的代码能处理空帧的情况
                if (!frameData.first) {
                    return;  // 只有帧指针为空时才返回，其他情况继续处理
                }
            }
        } else {
            logger->debug("Movie is stopped or null, skipping render");
            return;
        }
        
        auto [frame, pts] = frameData;
        shaderSetInt(KEY_STEREO_FLAG, mStereoFormat);
        if (mStereoFormat == StereoFormat::STEREO_FORMAT_3D) {
            //在每帧渲染的时候设置格式，修复部分情况切换不生效的情况
            shaderSetInt(KEY_STEREO_INPUT_FORMAT, (int) mStereoInputFormat);
            shaderSetInt(KEY_STEREO_OUTPUT_FORMAT, (int) mStereoOutputFormat);

            shaderSetInt(KEY_PARALLAX_SHIFT, mParallaxShift);
            if (mEnableStereoRegion) {
                shaderSetBool(KEY_ENABLE_REGION, mEnableStereoRegion);
                shaderSetRect(KEY_VEC_REGION, vecStereoRegion[0], vecStereoRegion[1], vecStereoRegion[2], vecStereoRegion[3]);
            } else {
                shaderSetBool(KEY_ENABLE_REGION, false);
                shaderSetRect(KEY_VEC_REGION, vecStereoRegion[0], vecStereoRegion[1], vecStereoRegion[2], vecStereoRegion[3]);
            }
        }

        mPixelFormat = frame->format;
        switch (mPixelFormat) {
        case AV_PIX_FMT_YUV420P:
        case AV_PIX_FMT_YUVJ420P:
            pts_ = pts;
            glClear(GL_COLOR_BUFFER_BIT);
            programDraw(frame->width, frame->height, frame->data, frame->linesize);
            break;
        default:
            logger->warn("Unsupported PixelFormat:{}", mPixelFormat);
            break;
        }

        elapsedInSeconds_ = int64_t(qRound(pts / (1000.0 * 1000 * 1000)));
    } else if (mRenderInputSource == RenderInputSource::RIS_CAMERA) {
        cameraFrameDraw(mCameraImage.width(), mCameraImage.height());
    } else {
        logger->warn("Unsupported RenderInputSource");
    }
}

void FFmpegView::enterEvent(QEnterEvent *e)
{
    if (butWidget) {
        butWidget->show();
    }
}

void FFmpegView::leaveEvent(QEvent *e)
{
    if (butWidget) {
        butWidget->hide();
    }
}

bool FFmpegView::shaderInit(const char *vertexSource, const char *fragmentSource)
{
    bool b_vertex = m_shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vertexSource);
    bool b_fragment = m_shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentSource);
    bool b_link = m_shaderProgram.link();
    if (b_vertex && b_fragment && b_link) {
        logger->info(
            "create/compile vertex shader program, success?:{},create/compiple fragment shader program, success?:{},link shader program, success?:{} ",
            b_vertex,
            b_fragment,
            b_link);
        return true;
    }
    logger->error(
        "create/compile vertex shader program, success?:{},create/compiple fragment shader program, success?:{},link shader program, success?:{} ",
        b_vertex,
        b_fragment,
        b_link);
    return false;
}

bool FFmpegView::shaderInit(QString &vertexSource, QString &fragmentSource)
{
    bool b_vertex = m_shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vertexSource);
    bool b_fragment = m_shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentSource);
    bool b_link = m_shaderProgram.link();
    if (b_vertex && b_fragment && b_link) {
        logger->info(
            "create/compile vertex shader program, success?:{},create/compiple fragment shader program, success?:{},link shader program, success?:{} ",
            b_vertex,
            b_fragment,
            b_link);
        return true;
    }
    logger->error(
        "create/compile vertex shader program, success?:{},create/compiple fragment shader program, success?:{},link shader program, success?:{} ",
        b_vertex,
        b_fragment,
        b_link);
    return false;
}

void FFmpegView::shaderUse()
{
    bool b_bind = m_shaderProgram.bind();
    logger->debug("shaderUse, m_shaderProgram bind, success?: {}", b_bind);
}

void FFmpegView::shaderSetBool(const std::string &name, bool value)
{
    bool b_bind = m_shaderProgram.bind();
    m_shaderProgram.setUniformValue(name.c_str(), (int) value);
}

void FFmpegView::shaderSetInt(const std::string &name, int value)
{
    bool b_bind = m_shaderProgram.bind();
    m_shaderProgram.setUniformValue(name.c_str(), value);
}

void FFmpegView::shaderSetRect(const std::string &name, float topLeftX, float topLeftY, float bottomRightX, float bottomRightY)
{
    bool b_bind = m_shaderProgram.bind();

    //将原窗口的坐标系转换成OpenGL 渲染坐标系
    logger->debug("name:{}, tl:{}x{}, br:{}x{}", name, topLeftX, topLeftY, bottomRightX, bottomRightY);
    //Should be Topleft & BottomRight
    m_shaderProgram.setUniformValue(name.c_str(), GLfloat(topLeftX), GLfloat(topLeftY), GLfloat(bottomRightX), GLfloat(bottomRightY));
}

void FFmpegView::programDraw(int width, int height, uint8_t **data, int *linesize)
{
    bool changed = false;

    if (texWidth != width) {
        texWidth = width;
        changed = true;
    }
    if (texHeight != height) {
        texHeight = height;
        changed = true;
    }

    float tr = (float) texWidth / texHeight;
    float vr = (float) viewWidth / viewHeight;
    float r = tr / vr;
    static int drawCounter = 0;
    drawCounter++;
    if (drawCounter % 10 == 0) {
        logger->debug("GLManager::draw frameWidth:{}x{}, textureSize:{}x{}, draw tr:{}, vr:{}, r:{}", width, height, texWidth, texHeight, tr, vr, r);
    }

    if (r != ratio) {
        ratio = r;
        std::vector<float> vertices;
        // clang-format off
        if (tr > vr) {
            // 上下边界不渲染，即保持原背景效果，
            float p = vr / tr;

            vertices = {
                1.0,  p, 0.0,     1.0, 0.0,
                1.0, -p, 0.0,     1.0, 1.0,
               -1.0, -p, 0.0,     0.0, 1.0,
               -1.0,  p, 0.0,     0.0, 0.0,
            };
            if (mParallaxShift != 0 && mEnableStripParallaxSideView) {
                //视差调节时，修剪两边 // TODO: 对于Horizontal可验证，而Vertical & Chess需切换对比效果
                float shiftOffset = 2.0 * std::abs(mParallaxShift) + 2.0; //+2是往内边再收一点，防止多显示的锯齿
                //  计算Rendering偏移 (以坐标系及比例来计算 =>   m:1 = parallax/2 : texWidth/2 ==> m = parallax/texWidth
                float sideViewStrip = shiftOffset / texWidth;  
                logger->debug("sideViewStrip:{}" , sideViewStrip);
                vertices = {
                    float(1.0) + sideViewStrip,  p, 0.0,     1.0, 0.0,
                    float(1.0) + sideViewStrip, -p, 0.0,     1.0, 1.0,
                    float(-1.0) - sideViewStrip, -p, 0.0,     0.0, 1.0,
                    float(-1.0) - sideViewStrip,  p, 0.0,     0.0, 0.0,
                };
            }
        }
        else if (tr < vr) {
            //左右两侧裁剪
            vertices = {
                r,  1.0, 0.0,     1.0, 0.0,
                r, -1.0, 0.0,     1.0, 1.0,
               -r, -1.0, 0.0,     0.0, 1.0,
               -r,  1.0, 0.0,     0.0, 0.0,
            };
            if (mParallaxShift != 0 && mEnableStripParallaxSideView) {
                //视差调节时，修剪两边 // TODO: 对于Horizontal可验证，而Vertical & Chess需切换对比效果
                float shiftOffset = 2.0 * std::abs(mParallaxShift) + 2.0; //+2是往内边再收一点，防止多显示的锯齿
                //  计算Rendering偏移 (以坐标系及比例来计算 =>   m:1 = parallax/2 : texWidth/2 ==> m = parallax/texWidth
                float sideViewStrip = shiftOffset / texWidth;  
                logger->debug("sideViewStrip:{}",sideViewStrip);
                vertices = {
                    float(r) + sideViewStrip,  1.0, 0.0,     1.0, 0.0,
                    float(r) + sideViewStrip, -1.0, 0.0,     1.0, 1.0,
                    float(-r) - sideViewStrip, -1.0, 0.0,     0.0, 1.0,
                    float(-r) - sideViewStrip,  1.0, 0.0,     0.0, 0.0,
                };
            }
        }
        else {
            //拉伸显示，全屏+
            vertices = {
                1.0,  1.0, 0.0,     1.0, 0.0,
                1.0, -1.0, 0.0,     1.0, 1.0,
               -1.0, -1.0, 0.0,     0.0, 1.0,
               -1.0,  1.0, 0.0,     0.0, 0.0,
            };


            if (mParallaxShift != 0 && mEnableStripParallaxSideView) {
                //视差调节时，修剪两边 // TODO: 对于Horizontal可验证，而Vertical & Chess需切换对比效果
                float shiftOffset = 2.0 * std::abs(mParallaxShift) + 2.0; //+2是往内边再收一点，防止多显示的锯齿
                //  计算Rendering偏移 (以坐标系及比例来计算 =>   m:1 = parallax/2 : texWidth/2 ==> m = parallax/texWidth
                float sideViewStrip = shiftOffset / texWidth; 
                logger->debug("sideViewStrip:{}" , sideViewStrip);
                vertices = {
                    float(1.0) + sideViewStrip,  1.0, 0.0,     1.0, 0.0,
                    float(1.0) + sideViewStrip, -1.0, 0.0,     1.0, 1.0,
                    float(-1.0) - sideViewStrip, -1.0, 0.0,     0.0, 1.0,
                    float(-1.0) - sideViewStrip,  1.0, 0.0,     0.0, 0.0,
                };
            }
        }

        // clang-format on
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(float), vertices.data());
    }

    if (mFullscreenMode == FullscreenMode::FULLSCREEN_PLUS_STRETCH) {
        std::vector<float> vertices;
        //若全屏模式为  全屏+， 则采用拉伸显示
        vertices = {
            1.0, 1.0, 0.0, 1.0, 0.0, 1.0, -1.0, 0.0, 1.0, 1.0, -1.0, -1.0, 0.0, 0.0, 1.0, -1.0, 1.0, 0.0, 0.0, 0.0,
        };

        if (mParallaxShift != 0 && mEnableStripParallaxSideView) {
            //视差调节时，修剪两边 // TODO: 对于Horizontal可验证，而Vertical & Chess需切换对比效果
            float shiftOffset = 2.0 * std::abs(mParallaxShift) + 2.0; //+2是往内边再收一点，防止多显示的锯齿
            //  计算Rendering偏移 (以坐标系及比例来计算 =>   m:1 = parallax/2 : texWidth/2 ==> m = parallax/texWidth
            float sideViewStrip = shiftOffset / texWidth;
            logger->debug("sideViewStrip:{}", sideViewStrip);
            vertices = {
                float(1.0) + sideViewStrip,  1.0,  0.0, 1.0, 0.0, float(1.0) + sideViewStrip,  -1.0, 0.0, 1.0, 1.0,
                float(-1.0) - sideViewStrip, -1.0, 0.0, 0.0, 1.0, float(-1.0) - sideViewStrip, 1.0,  0.0, 0.0, 0.0,
            };
        }

        // clang-format on
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(float), vertices.data());
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texs[0]);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, linesize[0]);
    if (changed) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, data[0]);
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, data[0]);
    }

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, texs[1]);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, linesize[1]);
    if (changed) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width / 2, height / 2, 0, GL_RED, GL_UNSIGNED_BYTE, data[1]);
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width / 2, height / 2, GL_RED, GL_UNSIGNED_BYTE, data[1]);
    }

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, texs[2]);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, linesize[2]);
    if (changed) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width / 2, height / 2, 0, GL_RED, GL_UNSIGNED_BYTE, data[2]);
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width / 2, height / 2, GL_RED, GL_UNSIGNED_BYTE, data[2]);
    }

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    //shaderUse();
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void FFmpegView::cameraFrameDraw(int width, int height)
{
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!mCameraImage.isNull()) {
        bool changed = false;
        logger->debug("width:{}xheight:{}, tex:{}x{}, ", width, height, texWidth, texHeight);

        if (texWidth != width) {
            texWidth = width;
            changed = true;
        }
        if (texHeight != height) {
            texHeight = height;
            changed = true;
        }

        float tr = (float) texWidth / texHeight;
        float vr = (float) viewWidth / viewHeight;
        float r = tr / vr;
        static int drawCounter = 0;
        drawCounter++;
        if (drawCounter % 10 == 0) {
            logger->debug("GLManager::draw frameWidth:{}x{}, textureSize:{}x{}, draw tr:{}, vr:{}, r:{}", width, height, texWidth, texHeight, tr, vr, r);
        }

        if (r != ratio) {
            ratio = r;
            std::vector<float> vertices;
            // clang-format off
            if (tr > vr) {
                // 上下边界不渲染，即保持原背景效果，
                float p = vr / tr;
                vertices = {
                    1.0,  p, 0.0,     1.0, 0.0,
                    1.0, -p, 0.0,     1.0, 1.0,
                   -1.0, -p, 0.0,     0.0, 1.0,
                   -1.0,  p, 0.0,     0.0, 0.0,
                };
            }
            else if (tr < vr) {
                //左右两侧裁剪
                vertices = {
                    r,  1.0, 0.0,     1.0, 0.0,
                    r, -1.0, 0.0,     1.0, 1.0,
                   -r, -1.0, 0.0,     0.0, 1.0,
                   -r,  1.0, 0.0,     0.0, 0.0,
                };
            }
            else {
                //拉伸显示，全屏+
                vertices = {
                    1.0,  1.0, 0.0,     1.0, 0.0,
                    1.0, -1.0, 0.0,     1.0, 1.0,
                   -1.0, -1.0, 0.0,     0.0, 1.0,
                   -1.0,  1.0, 0.0,     0.0, 0.0,
                };
            }

            // clang-format on
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(float), vertices.data());
        }

        if (mFullscreenMode == FullscreenMode::FULLSCREEN_PLUS_STRETCH) {
            std::vector<float> vertices;
            //若全屏模式为  全屏+， 则采用拉伸显示
            vertices = {
                1.0, 1.0, 0.0, 1.0, 0.0, 1.0, -1.0, 0.0, 1.0, 1.0, -1.0, -1.0, 0.0, 0.0, 1.0, -1.0, 1.0, 0.0, 0.0, 0.0,
            };
            // clang-format on
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(float), vertices.data());
        }

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texs[0]);

        //glPixelStorei(GL_UNPACK_ROW_LENGTH, linesize[0]);
        if (true) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, mCameraImage.bits());
        } else {
            //glTexSubImage2D(GL_TEXTURE_2D,
            //    0,
            //    0,
            //    0,
            //    width,
            //    height,
            //    GL_RED,
            //    GL_UNSIGNED_BYTE,
            //    data[0]);
        }

        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

        //shaderUse();
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }
}

bool FFmpegView::SaveImage(QString screenShotRootFolder, QString &savedFilePath)
{
    if (!QDir::current().exists(screenShotRootFolder)) {
        bool createdOk = QDir::current().mkdir(screenShotRootFolder);
        if (!createdOk) {
            logger->warn(" Create screenshotRootFolder:{} failed.", screenShotRootFolder.toStdString());
            return false;
        }
    }

    QImage img = grabFramebuffer();
    if (img.isNull()) {
        logger->error("Failed to grab framebuffer");
        return false;
    }

    QDateTime now = QDateTime::currentDateTime();
    QString timeStr = now.toString("yyyyMMddHHmmss");
    savedFilePath = screenShotRootFolder + "/Snapshots_" + timeStr + ".png";
    return img.save(savedFilePath);
}

void FFmpegView::onPlayStateChanged(PlayState state)
{
    logger->info(" ==============================================================state:{}", int(state));

    if (state == PlayState::STOP) {
        // 当播放完成或停止时，状态更新Timer停止
        logger->info("FFmpegView::onPlayStateChanged is Finished or Stopped:---------------------------------{}", int(state));
        StopRendering();
        logger->info("FFmpegView::onPlayStateChanged is Finished or Stopped, after StopRendering ----------{}", int(state));
    } else {
        logger->info(" state:{}", int(state));
    }
}

void FFmpegView::onCameraFrameChanged(const QVideoFrame &frame)
{
    QVideoFrame videoFrame = frame;

    logger->debug("frame size:{}x{}", frame.width(), frame.height());

    if (frame.isValid()) {
        mCameraImage = frame.toImage();
        //qDebug()<<"------- pixelFormat :" << frame.pixelFormat();  //  Format_NV12
        //frame.pix
        //int lineSize = 3;// frame.bytesPerLine();
        //uchar *imgData = mCameraImage.bits();
        //programDraw(frame.width(), frame.height(), &imgData, &lineSize);
        //mCameraImage.save(QString("./cameraTest.png"), "PNG", 100);
    }

    update();
}

void FFmpegView::SetFullscreenMode(FullscreenMode mode)
{
    mFullscreenMode = mode;
    ratio = -1; //修改FullscreenMode之后，重新计算ratio以便刷新比例
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

void FFmpegView::cameraTest()
{
    //mCamera = new QCamera(QMediaDevices::defaultVideoInput());
    //mCameraSession.setCamera(mCamera);
    //mCameraSession.setVideoSink(&mCameraSink);
    //connect(&mCameraSink, &QVideoSink::videoFrameChanged, this, &FFmpegView::onCameraFrameChanged);
    //mCamera->start();
}

QList<QCameraDevice> FFmpegView::getCamerasInfo()
{
    const QList<QCameraDevice> cameras = QMediaDevices::videoInputs();
    //logger->debug("camerasSize:{}, videoInputs:{}, defaultVideoInput:{}", cameras.size(), QMediaDevices::videoInputs(), QMediaDevices::defaultVideoInput());
    for (int i = 0; i < cameras.size(); ++i) {
        //logger->debug("camera [{}]: {} -- {}", i, cameras[i].description(), QVariant::fromValue(cameras[i]).toString());
    }

    return cameras;
}

void FFmpegView::openCamera(QCameraDevice camDev)
{
    mCamera = new QCamera(camDev);
    mCameraSession.setCamera(mCamera);
    mCameraSession.setVideoSink(&mCameraSink);

    connect(&mCameraSink, &QVideoSink::videoFrameChanged, this, &FFmpegView::onCameraFrameChanged);
}

void FFmpegView::startCamera()
{
    if (mCamera) {
        SetRenderInputSource(RenderInputSource::RIS_CAMERA);
        mCamera->start();
    }
}

void FFmpegView::stopCamera()
{
    if (mCamera) {
        mCamera->stop();
    }
}

void FFmpegView::closeCamera()
{
    if (mCamera) {
        disconnect(&mCameraSink, &QVideoSink::videoFrameChanged, this, &FFmpegView::onCameraFrameChanged);
        mCamera->deleteLater();
        mCamera = nullptr;
    }
    if (!mCameraImage.isNull()) {
        mCameraImage = QImage();
    }
}

void FFmpegView::DebugPrintStatus()
{
    logger->debug(
        "TimerActivate [Debug],    renderTime activate?{}, mPlayStatusUpdateTimer activate?{}", renderTimer.isActive(), mPlayStatusUpdateTimer.isActive());
}

void FFmpegView::SetSeeking(string parentFunc, bool seekFlag)
{
    logger->info("seekFlag:{},mPlayStatusUpdateTimer:{}", seekFlag, mPlayStatusUpdateTimer.isActive());
    mSeeking = seekFlag;
    if (mSeeking) {
        mPlayStatusUpdateTimer.stop();
    } else {
        mPlayStatusUpdateTimer.start();
    }
    logger->info("LEAVING, seekFlag:{},mPlayStatusUpdateTimer:{}", seekFlag, mPlayStatusUpdateTimer.isActive());
}

bool FFmpegView::UpdateSubtitlePosition(int64_t timestamp)
{
    if (mSubtitleWidget) {
        logger->info(" timestamp:{}", timestamp);
        mSubtitleWidget->Seek(timestamp);
        if (false == mSubtitleWidget->IsStarted()) {
            mSubtitleWidget->Start();
        }
    }

    return false;
}

bool FFmpegView::LoadSubtitle(QString filename)
{
    if (mSubtitleWidget) {
        mLoadSubtitleSuccess = mSubtitleWidget->SetSubtitleFile(filename);
        return mLoadSubtitleSuccess;
    }
    logger->warn("FFMpview::LoadSubtitle, subtitleWidget is nullptr");
    return false;
}

void FFmpegView::StopSubtitle()
{
    if (mSubtitleWidget) {
        logger->info("FFmpegView::StopSubtitle - stopping and clearing subtitle");
        mSubtitleWidget->Stop();
        mLoadSubtitleSuccess = false;
    }
}

bool FFmpegView::IsStereoRegion()
{
    return mEnableStereoRegion;
}