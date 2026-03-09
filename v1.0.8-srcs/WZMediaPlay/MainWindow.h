#pragma once
#include "globalDef.h"
#include "ui_MainWindow.h"
#include <QtWidgets/QMainWindow>

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE //必须定义这个宏,才能输出文件名和行号
#include "spdlog/spdlog.h"

class TitleWidget;
class DrawWidget;
class LogWindow;

typedef enum WindowSizeState { WINDOW_MINIMIZED = 100, WINDOW_MAXIMIZED = 101, WINDOW_FULLSCREEN = 102, WINDOW_NORMAL } WindowSizeState;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void mouseDoubleClickEvent(QMouseEvent *event);
    void keyPressEvent(QKeyEvent *event);

    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    virtual void resizeEvent(QResizeEvent *event);
    virtual void dragEnterEvent(QDragEnterEvent *event) override;
    virtual void dropEvent(QDropEvent *event) override;

    void contextMenuEvent(QContextMenuEvent *event);
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result);

private:
    QString readConfig();
    void writeConfig();
    bool setupLogger();
    QStringList getDeviceInfo();
    void getcpuid(unsigned int CPUInfo[4], unsigned int InfoType);
    void initLanguage();
    void initUI();
    void registerSysNotification();
    void openFile();
    int openPath(QString path, bool addPlayList = true);

    void updatePlayList(QStringList addList, int playListIndex);
    void drawPlayList();
    QString getWGText(QString widgetText);

private slots:
    void on_pushButton_min_clicked();
    void on_pushButton_max_clicked();
    void on_pushButton_fullScreen_clicked();
    void on_pushButton_close_clicked();

    void on_pushButton_stop_clicked();
    void on_pushButton_previous_clicked();
    void on_pushButton_playPause_clicked();
    void on_pushButton_next_clicked();
    void on_pushButton_open_clicked();

    void on_pushButton_add_clicked();
    void on_pushButton_clear_clicked();
    void on_pushButton_loadPlayList_clicked();
    void on_pushButton_exportPlayList_clicked();

    //void reply_switchButton_3D2D_statusChanged(bool checked);
    void on_comboBox_src2D_3D2D_currentIndexChanged(int index);
    void on_comboBox_3D_input_currentIndexChanged(int index);
    //void reply_switchButton_LRRL_statusChanged(bool checked);

    void on_pushButton_hs_clicked();
    void on_pushButton_a_clicked();

    //  menu
    void on_pushButton_menu_clicked();
    void on_actionOpenCamera_toggled(bool cam_checked);
    void on_actionOpen_triggered();

    //  stereo format (3D 2D-left 2D-src)
    //void on_action_3D_toggled(bool stereo_checked);
    //void on_action_2D_toggled(bool stereo_checked);
    void reply_action_3D_hotKey();
    void reply_actionStereoFormat_Selected(QAction *selectedAction);

    //  input - menu function
    void on_actionInputLR_triggered();
    void on_actionInputRL_triggered();
    void on_actionInputUD_triggered();
    void reply_actionInput_Selected(QAction *selectedAction);

    //  output - menu function
    void on_actionOutputHorizontal_triggered();
    void on_actionOutputVertical_triggered();
    void on_actionOutputChess_triggered();
    void on_actionOutputOnlyLeft_triggered();
    void reply_actionOutput_Selected(QAction *selectedAction);

    void on_actionFullscreen_triggered();
    void on_actionFullscreenPlus_triggered();
    void reply_action_3D_region_hotKey();
    void on_action_3D_region_toggled(bool region3d_checked);

    void on_actionSetting_triggered();
    void on_actionScreenshot_triggered();
    void on_actionRegistrationRequest_triggered();
    void on_actionRegistration_triggered();
    void on_actionAbout_triggered();
    void on_actionExit_triggered();
    void on_actionParallaxAdd_triggered();
    void on_actionParallaxSub_triggered();
    void on_actionParallaxReset_triggered();
    //Stereo
    void onStereoRegionUpdate(QRect region);
    void onStereoFormatChanged(StereoFormat stereoFormat, StereoInputFormat stereoInputFormat, StereoOutputFormat stereoOutputFormat);
    void reply_actionPlayOrder_Selected(QAction *selectedAction);
    void onTakeScreenShot();

    void reply_Esc_hotKey();

    //  play list
    void on_tabWidget_playList_currentChanged(int index);
    void reply_pushButton_pushButton_playList_tabBar_addList_clicked();
    void on_tabWidget_playList_tabCloseRequested(int index);
    void reply_playlistShowBut_Clicked(); //  使用on作为函数名开头会有警告
    void replayCurrentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);

    //Volume
    void onVolumeValueChanged(int value);
    void on_pushButton_volume_clicked();
    void replyVolumeUp();
    void replyVolumeDown();
    void replyVolumeMute();
    //void replyLoadSubtitle();
    //void replySubtitleChange();
    void onSubtitleSettingsChanged();

    //  play process
    void onUpdatePlayProcess(int64_t elapsedInSeconds);
    void onPlayProcessValueChanged(int value);
    void onPlaySliderMousePressEvent();
    void onPlaySliderMouseMoveEvent(int value);
    void onPlaySliderMouseReleaseEvent();
    void onPlaySliderCustomClicked(int seekTarget);

    void onPrimaryScreenChanged(QScreen *screen);
    void onScreenAdded(QScreen *screen);
    void onScreenRemoved(QScreen *screen);

    void onSettingsDialogAccepted();
    void onSettingsDialogRejected();

public slots:
    void on_listWidget_playlist_itemClicked(QListWidgetItem *item);
    void on_listWidget_playlist_itemDoubleClicked(QListWidgetItem *item);
    void reply_listWidget_playlist_itemSelectionChanged(QListWidgetItem *listWidgetItem);
    void replayListWidgetDrop(int index);
    void broadcastCallback(char cmd);
    void resetLanguage();
    void initHotKey();
    void onPlayStateChanged(PlayState state);
    void onSeekingFinished(int64_t value);

private:
    bool loadSubtitle(QString curMovieFilename, int seekPos);
    //非阻塞式等待，以便直接视频切换时，等待上一个视频完全结束
    void sleepMsec(int msec);
    QString mLastSubtitleFn;
    QString mLastMovieFn;
    bool checkFileIsVideo(QString path);

private:
    Ui::MainWindowClass ui;
    WindowSizeState mWindowSizeState;
    QPoint m_offPos;

    QActionGroup *stereoFormatActionGroup;
    QActionGroup *inputActionGroup;
    QActionGroup *outputActionGroup;
    QActionGroup *playOrderActionGroup;

    StereoFormat mStereoFormat = StereoFormat::STEREO_FORMAT_3D;
    StereoInputFormat mStereoInputFormat = StereoInputFormat::STEREO_INPUT_FORMAT_LR;
    StereoOutputFormat mStereoOutputFormat = StereoOutputFormat::STEREO_OUTPUT_FORMAT_HORIZONTAL;
    StereoOutputFormat mStereoOutputFormatWhenSwitch2D = mStereoOutputFormat;

    StereoOutputFormat mStereoOutputFormatWhenSwitchToRegion = mStereoOutputFormat;

    QShortcut *shortcut_Esc = nullptr;

    QShortcut *shortcut_OpenFile = nullptr;
    QShortcut *shortcut_CloseFile = nullptr;
    QShortcut *shortcut_Previous = nullptr;
    QShortcut *shortcut_Next = nullptr;

    QShortcut *shortcut_Pause = nullptr;
    QShortcut *shortcut_2D3D = nullptr;
    QShortcut *shortcut_LR = nullptr;
    QShortcut *shortcut_RL = nullptr;
    QShortcut *shortcut_UD = nullptr;
    QShortcut *shortcut_Vertical = nullptr;
    QShortcut *shortcut_Horizontal = nullptr;
    QShortcut *shortcut_Chess = nullptr;
    QShortcut *shortcut_3DOutput_OnlyLeft = nullptr;
    QShortcut *shortcut_Region = nullptr;

    QShortcut *shortcut_Screenshot = nullptr;

    QShortcut *shortcut_VolumeUp = nullptr;
    QShortcut *shortcut_VolumeDown = nullptr;
    QShortcut *shortcut_Mute = nullptr;

    //QShortcut* shortcut_LoadSubtitle = nullptr;
    //QShortcut* shortcut_SubtitleChange = nullptr;

    QShortcut *shortcut_PlayList = nullptr;
    QShortcut *shortcut_FullScreenPlus = nullptr;
    QShortcut *shortcut_FullScreen = nullptr;
    //QShortcut *shortcut_Min = nullptr;
    QShortcut *shortcut_IncreaseParallax = nullptr;
    QShortcut *shortcut_DecreaseParallax = nullptr;
    QShortcut *shortcut_ResetParallax = nullptr;

    DrawWidget *mDrawWidget = nullptr;

    //  Move  , 将Movie从FFMPEG中剥离出来，FFMPEGView只做渲染相关的事情
    Movie *movie_{nullptr};

    int currentElapsedInSeconds_ = 0;

    QString mCurMovieFilename = "";

    bool playerIsPlayingBeforeScreenLockOrPowerSleep;
    bool mIsNotRegistered = false;
#ifdef INSPECT
    QTimer *inspectTimer_;

private slots:
    void onInspectTimerTimeout();
#endif
};
