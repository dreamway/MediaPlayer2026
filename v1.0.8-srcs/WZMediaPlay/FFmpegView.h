#pragma once

#include <string>
#include <QLabel>
#include <QObject>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLPixelTransferOptions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLWidget>
#include <QResizeEvent>
#include <QTimer>
#include <QVideoFrame>
#include <QWidget>
#include <chrono>
using namespace std;
#include "GlobalDef.h"
#include "movie.h"

#include <QCamera>
#include <QCameraFormat>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QVideoSink>

class FullscreenTipsWidget;
class SubtitleWidget;
class FloatButton;

/**
视频输出渲染控件
*/
class FFmpegView : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
    Q_OBJECT
public:
    FFmpegView(QWidget *parent = nullptr);
    ~FFmpegView();
signals:
    void updatePlayProcess(int64_t);
    //void stereoFormatChanged(StereoFormat mStereoFormat, StereoInputFormat mStereoInputFormat, StereoOutputFormat mStereoOutputFormat);

public slots:
    void OnRenderTimer();
    void OnUpdateStatusTimer();
    void onPlayStateChanged(PlayState state);
    void onCameraFrameChanged(const QVideoFrame &frame);

protected:
    virtual void initializeGL();
    virtual void resizeGL(int w, int h);
    virtual void paintGL();
    void keyPressEvent(QKeyEvent *event);

    void enterEvent(QEnterEvent *e);
    void leaveEvent(QEvent *e);

    void moveEvent(QMoveEvent *event);
    void resizeEvent(QResizeEvent *e);
    void paintEvent(QPaintEvent *e);
    ////////////////////////////////////////////////////////////////////////////////////////
public:
    int StartRendering(StereoFormat stereoFormat, StereoInputFormat stereoInputFormat, StereoOutputFormat stereoOutputFormat, float frameRate);
    bool StopRendering();

    bool PlayPause(bool isPause);
    void SetRelativeMovie(Movie *movie);

    void SetFullscreenMode(FullscreenMode mode);
    bool IsRendering();

public:
    void DebugPrintStatus();

public: //StereoControl
    const string KEY_RENDER_INPUT_SOURCE = "iRenderInputSource";
    const string KEY_STEREO_FLAG = "iStereoFlag";
    const string KEY_STEREO_INPUT_FORMAT = "iStereoInputFormat";
    const string KEY_STEREO_OUTPUT_FORMAT = "iStereoOutputFormat";
    const string KEY_ENABLE_REGION = "bEnableRegion";
    const string KEY_VEC_REGION = "VecRegion";
    const string KEY_PARALLAX_SHIFT = "iParallaxOffset";

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
    bool TakeScreenshot();
    void SetSeeking(string parentFunc, bool seekFlag);
    bool LoadSubtitle(QString filename);
    void StopSubtitle();  // 停止并清除字幕显示
    bool UpdateSubtitlePosition(int64_t timestamp);

private:
    QString vertexSource_;
    QString fragmentSource_;

    QString externalVertexSource_;
    QString externalFragmentSource_;

private:
    bool loadInternalShaders();
    bool loadExternalShaders();

    // 若有外部特定目录的shaderFile, 则载入外部shader覆盖内部自带的shaderSource
    bool shaderInit(const char *vertexSource, const char *fragmentSource);
    bool shaderInit(QString &vertexSource, QString &fragmentSource);

    void shaderUse();
    void shaderSetBool(const std::string &name, bool value);
    void shaderSetInt(const std::string &name, int value);
    void shaderSetRect(const std::string &name, float topLeftX, float topLeftY, float bottomRightX, float bottomRightY);

    void programDraw(int width, int height, uint8_t **data, int *linesize);

    void cameraFrameDraw(int width, int height);

public:
    FloatButton *butWidget = nullptr;
    QLabel *mWindowLogo = nullptr;
    QPixmap mWindowLogPM;

private:
    //Stereo 3D Related, Definitions  (多窗口时应每个窗口有单独的Stereo状态)
    //iStereoFlag, 2d/3d, 0--for normal 2d, 1 --- for stereo(3d)
    //iStereoInputFormat = default, 0 --- lr
    //                   = 1 -- rl
    //                   = 2 -- ud
    // iStereoOutputFormat = default, 0 -- vertical-barrier
    //                 = 1, horizontal-barrier
    //                 = 2, chess-barrier
    //                 = 3,  only-2d, if 3d, only display 2d(only left view)
    StereoFormat mStereoFormat = STEREO_FORMAT_NORMAL_2D; //  0 2D - 1 3D
    StereoInputFormat mStereoInputFormat = STEREO_INPUT_FORMAT_LR;
    StereoOutputFormat mStereoOutputFormat = STEREO_OUTPUT_FORMAT_HORIZONTAL;
    RenderInputSource mRenderInputSource = RenderInputSource::RIS_VIDEO_FILE;
    bool mEnableStereoRegion = false;
    float vecStereoRegion[4] = {0.0, 0.0, 1.0, 1.0};

    int mParallaxShift = 0;
    bool mEnableStripParallaxSideView = true;

    bool mSeeking = false;
    bool mIsRendering = false;
    
    // 渲染健康检查：监控paintGL是否正常调用
    std::chrono::steady_clock::time_point mLastPaintGLTime;
    int mRenderStallCounter = 0;  // 渲染停滞计数器
    
    // FPS 显示相关
    bool mShowFPS = false;  // 是否显示 FPS
    QLabel *mFPSLabel = nullptr;  // FPS 显示标签
    std::chrono::steady_clock::time_point mLastFPSTime;  // 上次 FPS 计算时间
    int mFPSCounter = 0;  // FPS 计数器
    float mCurrentFPS = 0.0f;  // 当前 FPS 值

private:
    FullscreenMode mFullscreenMode;
    FullscreenTipsWidget *mFullscreenTipsWidget = nullptr;

    SubtitleWidget *mSubtitleWidget = nullptr;
    bool mLoadSubtitleSuccess = false;
    int mSubtitleFontHeight = 50;

    int mMinorShiftXForRightStereo = 0;
    int mMinorShiftYForRightStereo = 0;
    QPoint mLastGpt = QPoint(800, 600); // 对于moveEvent的gpt修正， 其中会用到move函数，在某些特殊情况下，会导致move函数调用的递归，使用此lastGpt进行判断，避免递归调用


private:
    // 兼容以前改动最小的方式，暂时用友元实现，后续可考虑改成信号槽（Movie & renderer 仅有对应currentFrame的数据交互)
    friend class Movie;
    Movie *movie_;

    int64_t pts_;
    float fps_;

    //  play contorl
    QTimer renderTimer;
    QTimer mPlayStatusUpdateTimer;
    int elapsedInSeconds_;

    QOpenGLShaderProgram m_shaderProgram;
    unsigned int VBO, VAO, EBO;
    unsigned int texs[3];

    int mPixelFormat;

    float ratio{1};
    int viewWidth = 300;
    int viewHeight = 200;
    int texWidth{-1};
    int texHeight{-1};

    // clang-format off
    float vertices1[20] = {
         1.0,  1.0, 0.0,     1.0, 0.0,
         1.0, -1.0, 0.0,     1.0, 1.0,
        -1.0, -1.0, 0.0,     0.0, 1.0,
        -1.0,  1.0, 0.0,     0.0, 0.0,
    };

    unsigned int glIndices[6] = {
        0, 1, 3,
        1, 2, 3
    };

    //--------------------------------------------------------
    //  Camera 相关
private:
    QCamera* mCamera;
    QVideoSink mCameraSink;
    QMediaCaptureSession mCameraSession;
    QImage mCameraImage;
public:
    void cameraTest();
    QList<QCameraDevice> getCamerasInfo();
    void openCamera(QCameraDevice camDev);
    void startCamera();
    void stopCamera();
    void closeCamera();
};