#pragma once
#include "playlist/PlayListData.h"
#include "spdlog/logger.h"
#include <iostream>
#include <vector>
#include <QApplication>
#include <QKeySequence>
#include <QMap>
#include <QObject>

//#define INSPECT

// 全局 TRACE 宏：控制调试图片输出

// #ifndef ENABLE_VIDEO_TRACE
// #define ENABLE_VIDEO_TRACE 1 // 启用以调试渲染问题
// #endif

#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif

#define qApp (static_cast<QApplication *>(QCoreApplication::instance()))

//iStereoFlag, 2d/3d, 0--for normal 2d, 1 --- for stereo(3d)
//iStereoInputFormat = default, 0 --- lr
//           = 1 -- rl
//           = 2 -- ud
// iStereoOutputFormat = default, 0 -- vertical-barrier
//                 = 1, horizontal-barrier
//                 = 2, chess-barrier
//                 = 3,  only-2d, if 3d, only display 2d(only left view)

typedef enum { ADV_TYPE_NULL = 0, ADV_TYPE_IMAGE = 1, ADV_TYPE_VIDEO = 2 } AdvType;

typedef enum {
    STEREO_FORMAT_NORMAL_2D = 0,
    STEREO_FORMAT_3D,
} StereoFormat;

typedef enum {
    STEREO_INPUT_FORMAT_LR = 0,
    STEREO_INPUT_FORMAT_RL,
    STEREO_INPUT_FORMAT_UD,
} StereoInputFormat;

typedef enum {
    STEREO_OUTPUT_FORMAT_VERTICAL = 0,
    STEREO_OUTPUT_FORMAT_HORIZONTAL,
    STEREO_OUTPUT_FORMAT_CHESS,
    STEREO_OUTPUT_FORMAT_ONLY_LEFT,
} StereoOutputFormat;



typedef enum PlayLoop {
    PLAYLOOP_SINGLE_PLAY = 100,
    PLAYLOOP_SINGLE_CYCLE = 101,
    PLAYLOOP_SEQUENTIAL_PLAY = 102,
    PLAYLOOP_RANDOMLY_PLAY = 103,
    PLAYLOOP_LIST_LOOP = 104
} PlayLoop;

//运行模式：Release：通常的运行版本, PRO: 带更多高阶调试功能（视差调节,LogWindow等）的版本
typedef enum RunningMode {
    RM_RELEASE,
    RM_PRO,
} RunningMode;

typedef enum RenderInputSource {
    RIS_VIDEO_FILE = 0,
    RIS_CAMERA,
} RenderInputSource;

typedef enum FullscreenMode { FULLSCREEN_KEEP_RATIO = 0, FULLSCREEN_PLUS_STRETCH } FullscreenMode;

class WZKeySequence
{
public:
    WZKeySequence()
    {
        hotKeyMap.insert("FileTab_OpenFile", QKeySequence("Ctrl+O"));
        hotKeyMap.insert("FileTab_CloseFile", QKeySequence("Ctrl+C"));
        hotKeyMap.insert("FileTab_Previous", QKeySequence("Page Up"));
        hotKeyMap.insert("FileTab_Next", QKeySequence("Page Down"));

        hotKeyMap.insert("PlayTab_Pause", QKeySequence("Space"));
        hotKeyMap.insert("PlayTab_2D3D", QKeySequence("Ctrl+1"));
        hotKeyMap.insert("PlayTab_LR", QKeySequence("Ctrl+2"));
        hotKeyMap.insert("PlayTab_RL", QKeySequence("Ctrl+3"));
        hotKeyMap.insert("PlayTab_UD", QKeySequence("Ctrl+4"));
        hotKeyMap.insert("PlayTab_Vertical", QKeySequence("Ctrl+5"));
        hotKeyMap.insert("PlayTab_Horizontal", QKeySequence("Ctrl+6"));
        hotKeyMap.insert("PlayTab_Chess", QKeySequence("Ctrl+7"));
        hotKeyMap.insert("PlayTab_3DOutput_OnlyLeft", QKeySequence("Ctrl+8"));
        hotKeyMap.insert("PlayTab_Region", QKeySequence("Ctrl+9"));

        //hotKeyMap.insert("ImageTab_Fullscreen", QKeySequence("Ctrl+S"));
        hotKeyMap.insert("ImageTab_Screenshot", QKeySequence("Ctrl+S"));

        hotKeyMap.insert("VoiceTab_Volup", QKeySequence("Up"));
        hotKeyMap.insert("VoiceTab_Volde", QKeySequence("Down"));
        hotKeyMap.insert("VoiceTab_Mute", QKeySequence("M"));

        hotKeyMap.insert("SubtitleTab_LoadSubtitle", QKeySequence("Ctrl+A"));
        hotKeyMap.insert("SubtitleTab_ChangeSubtitle", QKeySequence("C"));

        hotKeyMap.insert("OthersTab_PlayList", QKeySequence("F3"));
        // BUG修复: 全屏快捷键默认值
        // Fullscreen+ (拉伸填充) = Ctrl+Return
        // Fullscreen (保持比例) = Return
        hotKeyMap.insert("OthersTab_FullScreenPlus", QKeySequence("Ctrl+Return"));
        hotKeyMap.insert("OthersTab_FullScreen", QKeySequence("Return"));
        //hotKeyMap.insert("OthersTab_Min", QKeySequence("F4"));

        hotKeyMap.insert("OthersTab_IncreaseParallax", QKeySequence("Ctrl+E"));
        hotKeyMap.insert("OthersTab_DecreaseParallax", QKeySequence("Ctrl+W"));
        hotKeyMap.insert("OthersTab_ResetParallax", QKeySequence("Ctrl+R"));
    }

public:
    QMap<QString, QKeySequence> hotKeyMap;
};

class GlobalDef
{
public:
    static GlobalDef *getInstance();

private:
    GlobalDef();
    ~GlobalDef();
    GlobalDef(const GlobalDef &);
    GlobalDef &operator=(const GlobalDef &);
    static GlobalDef mGlobalDef;

public:
    void SetRunningMode(RunningMode mode);

public:
    int CURRENT_WINDOW_WIDTH;  //  当前窗口宽
    int CURRENT_WINDOW_HEIGHT; //  当前窗口高
    int MIN_WINDOW_WIDTH;      //  用户拉伸窗口宽
    int MIN_WINDOW_HEIGHT;     //  用户拉伸窗口高
    int MIN_WINDOW_POSITION_X; //  窗口位置X
    int MIN_WINDOW_POSITION_Y; //  窗口位置Y
    int SPLASH_LOGO_WIDTH;     //  启动logo宽
    int SPLASH_LOGO_HEIGHT;    //  启动logo高
    float SPLASH_OPACITY; //logo 透明度

    AdvType ADV_TYPE_LEFT;        //  左侧广告栏类型
    QString ADV_SOURCE_PATH_LEFT; //  左侧广告栏资源路径
    int ADV_WIDTH_LEFT;           //  左侧广告栏宽
    int ADV_HEIGHT_LEFT;          //  左侧广告栏高

    AdvType ADV_TYPE_RIGHT;        //  右侧广告栏类型
    QString ADV_SOURCE_PATH_RIGHT; //  右侧广告栏资源路径
    int ADV_WIDTH_RIGHT;           //  右侧广告栏宽
    int ADV_HEIGHT_RIGHT;          //  右侧广告栏高

    int PLAY_3D_VIEW_WIDTH;
    int PLAY_3D_VIEW_HEIGHT;

    int LANGUAGE;                         //  语言选择
    QString ABOUT_CR_EN;                  //  copyright英文
    QString ABOUT_CR_ZHCN;                //  copyright简体中文
    QString ABOUT_CR_ZHHANT;              //  copyright繁体中文
    bool B_WINDOW_SIZE_MAXIMIZED = false; //  窗口是否最大化
    QString SPLASH_LOGO_PATH;             //  启动logo路径
    QString PLAY_WINDOW_LOGO_PATH;        //  播放窗口logo路径
    QString MAIN_WINDOW_LOGO_PATH;        //  主窗口logo路径
    QString SCREENSHOT_DIR;               //  截图保存路径
    int MOVE_FIT_WINDOW_SATAE;            //  视频-窗口适配状态

    int SUBTITLE_AUTO_LOAD_SAMEDIR_SAMENAME; //  自动载入同目录同名字幕
    QString CURRENT_MOVIE_PATH;              //  当前播放视频路径(不写配置)
    QString USER_SELECTED_SUBTITLE_FILENAME; //  当前播放字幕选择（不写配置）

    bool B_VOLUME_MUTE;                                        //  静音设置
    int VOLUME_VAULE;                                          //  音量设置
    PlayLoop PLAY_LOOP_ORDER = PlayLoop::PLAYLOOP_SINGLE_PLAY; //  视频播放循环状态

    //3D Settings
    StereoFormat DefaultStereoFormat;
    StereoInputFormat DefaultStereoInputFormat;
    StereoOutputFormat DefaultStereoOutputFormat;

    int LOG_MODE = 1; //LogMode, 1-- Console, 2 --file
    /** 
    spdlog log_level definition
    trace = SPDLOG_LEVEL_TRACE,   // 0 
    debug = SPDLOG_LEVEL_DEBUG,   // 1
    info = SPDLOG_LEVEL_INFO,      //2
    warn = SPDLOG_LEVEL_WARN,     //3
    err = SPDLOG_LEVEL_ERROR,    //4
    critical = SPDLOG_LEVEL_CRITICAL,  //5
    off = SPDLOG_LEVEL_OFF,  //6
    */
    int LOG_LEVEL = 2; //LogLevel -- spdlog definistion

    PlayListData PLAY_LIST_DATA; //  播放列表数据

    WZKeySequence defWZKeySequence;
    WZKeySequence userWZKeySequence;

    RunningMode mRunningMode = RunningMode::RM_RELEASE;
    int MIN_ALLOW_ADJUST_PARALLAX = -50;
    int MAX_ALLOW_ADJUST_PARALLAX = 50;

    QStringList VIDEO_FILE_TYPE;
};

extern spdlog::logger *logger;

#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE //必须定义这个宏,才能输出文件名和行号
#endif