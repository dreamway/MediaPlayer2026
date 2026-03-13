#include "ApplicationSettings.h"
#include "GlobalDef.h"
#include "spdlog/spdlog.h"
#include <sstream>
#include <string>
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>
#include <QString>
using namespace std;

ApplicationSettings::ApplicationSettings(QObject *parent)
    : QObject(parent)
{
    //QString setting_dir_path = QCoreApplication::applicationDirPath() + QString("/config");
    //QDir setting_dir(setting_dir_path);
    //if (!setting_dir.exists()) {
    //	setting_dir.mkdir(setting_dir_path);
    //}
}

ApplicationSettings::~ApplicationSettings() {}

void ApplicationSettings::read_ALL()
{
    read_ApplicationGeneral();
    read_WindowSizeState();
    read_PlayState();
    read_Hotkey();

    read_about_copyright();
}

void ApplicationSettings::write_ALL()
{
    write_ApplicationGeneral();
    write_WindowSizeState();
    write_PlayState();
    write_PlayList();
}

QString ApplicationSettings::ToString()
{
    std::ostringstream ostr;
    ostr << "======== ApplicationGeneral =========" << endl;
    ostr << "Language:" << GlobalDef::getInstance()->LANGUAGE << endl;
    ostr << "/MediaPlay/PlayWindowLogoPath:" << GlobalDef::getInstance()->PLAY_WINDOW_LOGO_PATH.toUtf8().constData() << endl;
    ostr << "/MediaPlay/MainWindowLogoPath:" << GlobalDef::getInstance()->MAIN_WINDOW_LOGO_PATH.toUtf8().constData() << endl;
    ostr << "/MediaPlay/ScreenshotDir:" << GlobalDef::getInstance()->SCREENSHOT_DIR.toUtf8().constData() << endl;
    ostr << "/MediaPlay/MoveWindowFit:" << GlobalDef::getInstance()->MOVE_FIT_WINDOW_SATAE << endl;
    ostr << "/MediaPlay/SubtitleLoadSameDirSameNameState:" << GlobalDef::getInstance()->SUBTITLE_AUTO_LOAD_SAMEDIR_SAMENAME << endl;
    ostr << "/MediaPlay/SPLASH_LOGO_PATH:" << GlobalDef::getInstance()->SPLASH_OPACITY << endl;
    ostr << "/System/LogMode:" << GlobalDef::getInstance()->LOG_MODE << endl;
    ostr << "/System/LogLevel:" << GlobalDef::getInstance()->LOG_LEVEL << endl;
    ostr << "======= WindowSizeState ========== " << endl;
    ostr << "/MediaPlay/Window_Width:" << GlobalDef::getInstance()->MIN_WINDOW_WIDTH << endl;
    ostr << "/MediaPlay/Window_Heigth:" << GlobalDef::getInstance()->MIN_WINDOW_HEIGHT << endl;
    ostr << "/MediaPlay/Window_Position_X:" << GlobalDef::getInstance()->MIN_WINDOW_POSITION_X << endl;
    ostr << "/MediaPlay/Window_Position_Y:" << GlobalDef::getInstance()->MIN_WINDOW_POSITION_Y << endl;
    ostr << "/MediaPlay/ADV_TYPE_LEFT:" << GlobalDef::getInstance()->ADV_TYPE_LEFT << endl;
    ostr << "/MediaPlay/ADV_SOURCE_PATH_LEFT:" << GlobalDef::getInstance()->ADV_SOURCE_PATH_LEFT.toUtf8().constData() << endl;
    ostr << "/MediaPlay/ADV_WIDTH_LEFT:" << GlobalDef::getInstance()->ADV_WIDTH_LEFT << endl;
    ostr << "/MediaPlay/ADV_HEIGHT_LEFT:" << GlobalDef::getInstance()->ADV_HEIGHT_LEFT << endl;
    ostr << "/MediaPlay/ADV_TYPE_RIGHT:" << GlobalDef::getInstance()->ADV_TYPE_RIGHT << endl;
    ostr << "/MediaPlay/ADV_SOURCE_PATH_RIGHT:" << GlobalDef::getInstance()->ADV_SOURCE_PATH_RIGHT.toUtf8().constData() << endl;
    ostr << "/MediaPlay/ADV_WIDTH_RIGHT:" << GlobalDef::getInstance()->ADV_WIDTH_RIGHT << endl;
    ostr << "/MediaPlay/ADV_HEIGHT_RIGHT:" << GlobalDef::getInstance()->ADV_HEIGHT_RIGHT << endl;
    ostr << "/MediaPlay/PLAY_3D_VIEW_WIDTH:" << GlobalDef::getInstance()->PLAY_3D_VIEW_WIDTH << endl;
    ostr << "/MediaPlay/PLAY_3D_VIEW_HEIGHT:" << GlobalDef::getInstance()->PLAY_3D_VIEW_HEIGHT << endl;
    ostr << "============ PlayState ==============" << endl;

    ostr << "/MediaPlay/B_VOLUME_MUTE" << GlobalDef::getInstance()->B_VOLUME_MUTE << endl;
    ostr << "/MediaPlay/VOLUME_VAULE" << GlobalDef::getInstance()->VOLUME_VAULE << endl;
    ostr << "/MediaPlay/PLAY_LOOP" << GlobalDef::getInstance()->PLAY_LOOP_ORDER << endl;

    ostr << "=================== HotKey ===========================" << endl;

    ostr << "/USER/FileTab_OpenFile:" << GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["FileTab_OpenFile"].toString().toUtf8().constData() << endl;
    ostr << "/USER/FileTab_CloseFile:" << GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["FileTab_CloseFile"].toString().toUtf8().constData() << endl;
    ostr << "/USER/FileTab_Previous:" << GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["FileTab_Previous"].toString().toUtf8().constData() << endl;
    ostr << "/USER/FileTab_Next:" << GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["FileTab_Next"].toString().toUtf8().constData() << endl;
    ostr << "/USER/PlayTab_Pause:" << GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["PlayTab_Pause"].toString().toUtf8().constData() << endl;
    ostr << "/USER/PlayTab_2D3D:" << GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["PlayTab_2D3D"].toString().toUtf8().constData() << endl;
    ostr << "/USER/PlayTab_LR:" << GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["PlayTab_LR"].toString().toUtf8().constData() << endl;
    ostr << "/USER/PlayTab_RL:" << GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["PlayTab_RL"].toString().toUtf8().constData() << endl;
    ostr << "/USER/PlayTab_UD:" << GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["PlayTab_UD"].toString().toUtf8().constData() << endl;
    ostr << "/USER/PlayTab_Vertical:" << GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["PlayTab_Vertical"].toString().toUtf8().constData() << endl;
    ostr << "/USER/PlayTab_Horizontal:" << GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["PlayTab_Horizontal"].toString().toUtf8().constData() << endl;
    ostr << "/USER/PlayTab_Chess:" << GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["PlayTab_Chess"].toString().toUtf8().constData() << endl;
    ostr << "/USER/PlayTab_Region:" << GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["PlayTab_Region"].toString().toUtf8().constData() << endl;
    ostr << "/USER/ImageTab_Screenshot:" << GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["ImageTab_Screenshot"].toString().toUtf8().constData()
         << endl;
    ostr << "/USER/VoiceTab_Volup:" << GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["VoiceTab_Volup"].toString().toUtf8().constData() << endl;
    ostr << "/USER/VoiceTab_Volde:" << GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["VoiceTab_Volde"].toString().toUtf8().constData() << endl;
    ostr << "/USER/VoiceTab_Mute:" << GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["VoiceTab_Mute"].toString().toUtf8().constData() << endl;
    ostr << "/USER/SubtitleTab_LoadSubtitle:"
         << GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["SubtitleTab_LoadSubtitle"].toString().toUtf8().constData() << endl;
    ostr << "/USER/SubtitleTab_ChangeSubtitle:"
         << GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["SubtitleTab_ChangeSubtitle"].toString().toUtf8().constData() << endl;
    ostr << "/USER/OthersTab_PlayList:" << GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["OthersTab_PlayList"].toString().toUtf8().constData() << endl;
    ostr << "/USER/OthersTab_FullScreenPlus:" << GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["OthersTab_FullScreenPlus"].toString().toUtf8().constData()
         << endl;
    ostr << "/USER/OthersTab_FullScreen:" << GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["OthersTab_FullScreen"].toString().toUtf8().constData() << endl;
    ostr << "/USER/OthersTab_IncreaseParallax:"
         << GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["OthersTab_IncreaseParallax"].toString().toUtf8().constData() << endl;
    ostr << "/USER/OthersTab_DecreaseParallax:"
         << GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["OthersTab_DecreaseParallax"].toString().toUtf8().constData() << endl;
    ostr << "/USER/OthersTab_ResetParallax:" << GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["OthersTab_ResetParallax"].toString().toUtf8().constData()
         << endl;

    ostr << "=========== copyright ================= " << endl;
    ostr << "/MediaPlay/ABOUT_CR_ZHCN:" << GlobalDef::getInstance()->ABOUT_CR_ZHCN.toUtf8().constData() << endl;
    ostr << "/MediaPlay/ABOUT_CR_EN:" << GlobalDef::getInstance()->ABOUT_CR_EN.toUtf8().constData() << endl;
    ostr << "/MediaPlay/ABOUT_CR_ZHHANT:" << GlobalDef::getInstance()->ABOUT_CR_ZHHANT.toUtf8().constData() << endl;

    return QString(ostr.str().c_str());
}

void ApplicationSettings::read_SplashLogoPath()
{
    QString cfgPath = QCoreApplication::applicationDirPath() + "/config/SystemConfig.ini";
    QSettings setting(cfgPath, QSettings::IniFormat);

    QVariant variant = setting.value("/MediaPlay/SplashLogoPath");
    QString splashPath = variant.isNull() ? QString(QCoreApplication::applicationDirPath() + "/Resources/logo/SPElg.png")
                                           : variant.toString();

    // 修复：将相对路径转换为绝对路径
    // 如果路径以 "./" 开头，则相对于应用程序目录
    if (splashPath.startsWith("./")) {
        splashPath = QCoreApplication::applicationDirPath() + "/" + splashPath.mid(2);
    } else if (!splashPath.isEmpty() && !QDir::isAbsolutePath(splashPath)) {
        // 如果不是绝对路径，则相对于应用程序目录
        splashPath = QCoreApplication::applicationDirPath() + "/" + splashPath;
    }

    GlobalDef::getInstance()->SPLASH_LOGO_PATH = splashPath;
    variant = setting.value("/MediaPlay/SplashLogoWidth");
    GlobalDef::getInstance()->SPLASH_LOGO_WIDTH = variant.isNull() ? 100 : variant.toInt();
    variant = setting.value("/MediaPlay/SplashLogoHeight");
    GlobalDef::getInstance()->SPLASH_LOGO_HEIGHT = variant.isNull() ? 100 : variant.toInt();
    variant = setting.value("/MediaPlay/SplashLogoOpacity");
    GlobalDef::getInstance()->SPLASH_OPACITY = variant.isNull() ? 1.0 : variant.toFloat();
}

void ApplicationSettings::write_SplashLogoPath(QString path)
{
    QString cfgPath = QCoreApplication::applicationDirPath() + "/config/SystemConfig.ini";
    QSettings setting(cfgPath, QSettings::IniFormat);
    setting.setValue("/MediaPlay/SplashLogoPath", path);
}

void ApplicationSettings::read_ApplicationGeneral()
{
    QString cfgPath = QCoreApplication::applicationDirPath() + "/config/SystemConfig.ini";
    QSettings setting(cfgPath, QSettings::IniFormat);

    QVariant variant = setting.value("/MediaPlay/Language");
    GlobalDef::getInstance()->LANGUAGE = variant.isNull() ? 0 : variant.toInt();

    variant = setting.value("/MediaPlay/PlayWindowLogoPath");
    GlobalDef::getInstance()->PLAY_WINDOW_LOGO_PATH = variant.isNull() ? QString(QCoreApplication::applicationDirPath() + "/Resources/logo/PWlg.png")
                                                                       : variant.toString();

    variant = setting.value("/MediaPlay/MainWindowLogoPath");
    GlobalDef::getInstance()->MAIN_WINDOW_LOGO_PATH = variant.isNull() ? QString(QCoreApplication::applicationDirPath() + "/Resources/logo/MWlg.png")
                                                                       : variant.toString();

    variant = setting.value("/MediaPlay/ScreenshotDir");
    GlobalDef::getInstance()->SCREENSHOT_DIR = variant.isNull() ? QString(QCoreApplication::applicationDirPath() + "/Screenshot") : variant.toString();

    variant = setting.value("/MediaPlay/MoveWindowFit");
    GlobalDef::getInstance()->MOVE_FIT_WINDOW_SATAE = variant.isNull() ? 0 : variant.toInt();

    variant = setting.value("/MediaPlay/SubtitleLoadSameDirSameNameState");
    GlobalDef::getInstance()->SUBTITLE_AUTO_LOAD_SAMEDIR_SAMENAME = variant.isNull() ? Qt::Unchecked : variant.toInt();

    variant = setting.value("/System/LogMode");
    GlobalDef::getInstance()->LOG_MODE = variant.isNull() ? 1 : variant.toInt();
    variant = setting.value("/System/LogLevel");
    GlobalDef::getInstance()->LOG_LEVEL = variant.isNull() ? 2 : variant.toInt();
}

void ApplicationSettings::write_ApplicationGeneral()
{
    QString cfgPath = QCoreApplication::applicationDirPath() + "/config/SystemConfig.ini";
    QSettings setting(cfgPath, QSettings::IniFormat);

    setting.setValue("/MediaPlay/Language", GlobalDef::getInstance()->LANGUAGE);
    setting.setValue("/MediaPlay/PlayWindowLogoPath", GlobalDef::getInstance()->PLAY_WINDOW_LOGO_PATH);
    setting.setValue("/MediaPlay/MainWindowLogoPath", GlobalDef::getInstance()->MAIN_WINDOW_LOGO_PATH);
    setting.setValue("/MediaPlay/ScreenshotDir", GlobalDef::getInstance()->SCREENSHOT_DIR);
    setting.setValue("/MediaPlay/MoveWindowFit", GlobalDef::getInstance()->MOVE_FIT_WINDOW_SATAE);
    setting.setValue("/MediaPlay/SubtitleLoadSameDirSameNameState", GlobalDef::getInstance()->SUBTITLE_AUTO_LOAD_SAMEDIR_SAMENAME);
}

void ApplicationSettings::read_Hotkey()
{
    QString cfgPath = QCoreApplication::applicationDirPath() + "/config/SystemConfig.ini";
    QSettings setting(cfgPath, QSettings::IniFormat);

    QVariant variant = setting.value("/USER/FileTab_OpenFile");
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["FileTab_OpenFile"]
        = variant.isNull() ? QKeySequence(GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("FileTab_OpenFile").toString())
                           : QKeySequence(variant.toString());
    variant = setting.value("/USER/FileTab_CloseFile");
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["FileTab_CloseFile"]
        = variant.isNull() ? QKeySequence(GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("FileTab_CloseFile").toString())
                           : QKeySequence(variant.toString());
    variant = setting.value("/USER/FileTab_Previous");
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["FileTab_Previous"]
        = variant.isNull() ? QKeySequence(GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("FileTab_Previous").toString())
                           : QKeySequence(variant.toString());
    variant = setting.value("/USER/FileTab_Next");
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["FileTab_Next"]
        = variant.isNull() ? QKeySequence(GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("FileTab_Next").toString())
                           : QKeySequence(variant.toString());

    variant = setting.value("/USER/PlayTab_Pause");
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["PlayTab_Pause"]
        = variant.isNull() ? QKeySequence(GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("PlayTab_Pause").toString())
                           : QKeySequence(variant.toString());
    variant = setting.value("/USER/PlayTab_2D3D");
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["PlayTab_2D3D"]
        = variant.isNull() ? QKeySequence(GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("PlayTab_2D3D").toString())
                           : QKeySequence(variant.toString());
    variant = setting.value("/USER/PlayTab_LR");
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["PlayTab_LR"]
        = variant.isNull() ? QKeySequence(GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("PlayTab_LR").toString())
                           : QKeySequence(variant.toString());
    variant = setting.value("/USER/PlayTab_RL");
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["PlayTab_RL"]
        = variant.isNull() ? QKeySequence(GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("PlayTab_RL").toString())
                           : QKeySequence(variant.toString());
    variant = setting.value("/USER/PlayTab_UD");
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["PlayTab_UD"]
        = variant.isNull() ? QKeySequence(GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("PlayTab_UD").toString())
                           : QKeySequence(variant.toString());
    variant = setting.value("/USER/PlayTab_Vertical");
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["PlayTab_Vertical"]
        = variant.isNull() ? QKeySequence(GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("PlayTab_Vertical").toString())
                           : QKeySequence(variant.toString());
    variant = setting.value("/USER/PlayTab_Horizontal");
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["PlayTab_Horizontal"]
        = variant.isNull() ? QKeySequence(GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("PlayTab_Horizontal").toString())
                           : QKeySequence(variant.toString());
    variant = setting.value("/USER/PlayTab_Chess");
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["PlayTab_Chess"]
        = variant.isNull() ? QKeySequence(GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("PlayTab_Chess").toString())
                           : QKeySequence(variant.toString());
    variant = setting.value("/USER/PlayTab_Region");
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["PlayTab_Region"]
        = variant.isNull() ? QKeySequence(GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("PlayTab_Region").toString())
                           : QKeySequence(variant.toString());

    //variant = setting.value("/USER/ImageTab_Fullscreen");
    //GlobalDef::getInstance()->userWZKeySequence.ImageTab_Fullscreen = variant.isNull() ? QKeySequence(GlobalDef::getInstance()->defWZKeySequence.ImageTab_Fullscreen.toString()) : QKeySequence(variant.toString());
    variant = setting.value("/USER/ImageTab_Screenshot");
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["ImageTab_Screenshot"]
        = variant.isNull() ? QKeySequence(GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("ImageTab_Screenshot").toString())
                           : QKeySequence(variant.toString());

    variant = setting.value("/USER/VoiceTab_Volup");
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["VoiceTab_Volup"]
        = variant.isNull() ? QKeySequence(GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("VoiceTab_Volup").toString())
                           : QKeySequence(variant.toString());
    variant = setting.value("/USER/VoiceTab_Volde");
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["VoiceTab_Volde"]
        = variant.isNull() ? QKeySequence(GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("VoiceTab_Volde").toString())
                           : QKeySequence(variant.toString());
    variant = setting.value("/USER/VoiceTab_Mute");
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["VoiceTab_Mute"]
        = variant.isNull() ? QKeySequence(GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("VoiceTab_Mute").toString())
                           : QKeySequence(variant.toString());

    variant = setting.value("/USER/SubtitleTab_LoadSubtitle");
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["SubtitleTab_LoadSubtitle"]
        = variant.isNull() ? QKeySequence(GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("SubtitleTab_LoadSubtitle").toString())
                           : QKeySequence(variant.toString());
    variant = setting.value("/USER/SubtitleTab_ChangeSubtitle");
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["SubtitleTab_ChangeSubtitle"]
        = variant.isNull() ? QKeySequence(GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("SubtitleTab_ChangeSubtitle").toString())
                           : QKeySequence(variant.toString());

    variant = setting.value("/USER/OthersTab_PlayList");
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["OthersTab_PlayList"]
        = variant.isNull() ? QKeySequence(GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("OthersTab_PlayList").toString())
                           : QKeySequence(variant.toString());
    variant = setting.value("/USER/OthersTab_FullScreenPlus");
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["OthersTab_FullScreenPlus"]
        = variant.isNull() ? QKeySequence(GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("OthersTab_FullScreenPlus").toString())
                           : QKeySequence(variant.toString());
    variant = setting.value("/USER/OthersTab_FullScreen");
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["OthersTab_FullScreen"]
        = variant.isNull() ? QKeySequence(GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("OthersTab_FullScreen").toString())
                           : QKeySequence(variant.toString());
    variant = setting.value("/USER/OthersTab_IncreaseParallax");
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["OthersTab_IncreaseParallax"]
        = variant.isNull() ? QKeySequence(GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("OthersTab_IncreaseParallax").toString())
                           : QKeySequence(variant.toString());
    variant = setting.value("/USER/OthersTab_DecreaseParallax");
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["OthersTab_DecreaseParallax"]
        = variant.isNull() ? QKeySequence(GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("OthersTab_DecreaseParallax").toString())
                           : QKeySequence(variant.toString());
    variant = setting.value("/USER/OthersTab_ResetParallax");
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["OthersTab_ResetParallax"]
        = variant.isNull() ? QKeySequence(GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("OthersTab_ResetParallax").toString())
                           : QKeySequence(variant.toString());
}

void ApplicationSettings::write_Hotkey()
{
    QString cfgPath = QCoreApplication::applicationDirPath() + "/config/SystemConfig.ini";
    QSettings setting(cfgPath, QSettings::IniFormat);

    setting.setValue("/USER/FileTab_OpenFile", GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("FileTab_OpenFile").toString());
    setting.setValue("/USER/FileTab_CloseFile", GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("FileTab_CloseFile").toString());
    setting.setValue("/USER/FileTab_Previous", GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("FileTab_Previous").toString());
    setting.setValue("/USER/FileTab_Next", GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("FileTab_Next").toString());

    setting.setValue("/USER/PlayTab_Pause", GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_Pause").toString());
    setting.setValue("/USER/PlayTab_2D3D", GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_2D3D").toString());
    setting.setValue("/USER/PlayTab_LR", GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_LR").toString());
    setting.setValue("/USER/PlayTab_RL", GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_RL").toString());
    setting.setValue("/USER/PlayTab_UD", GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_UD").toString());
    setting.setValue("/USER/PlayTab_Vertical", GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_Vertical").toString());
    setting.setValue("/USER/PlayTab_Horizontal", GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_Horizontal").toString());
    setting.setValue("/USER/PlayTab_Chess", GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_Chess").toString());
    setting.setValue("/USER/PlayTab_Region", GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_Region").toString());

    //setting.setValue("/USER/ImageTab_Fullscreen", GlobalDef::getInstance()->userWZKeySequence.ImageTab_Fullscreen.toString());
    setting.setValue("/USER/ImageTab_Screenshot", GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("ImageTab_Screenshot").toString());

    setting.setValue("/USER/VoiceTab_Volup", GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("VoiceTab_Volup").toString());
    setting.setValue("/USER/VoiceTab_Volde", GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("VoiceTab_Volde").toString());
    setting.setValue("/USER/VoiceTab_Mute", GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("VoiceTab_Mute").toString());

    setting.setValue("/USER/SubtitleTab_LoadSubtitle", GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("SubtitleTab_LoadSubtitle").toString());
    setting.setValue("/USER/SubtitleTab_ChangeSubtitle", GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("SubtitleTab_ChangeSubtitle").toString());

    setting.setValue("/USER/OthersTab_PlayList", GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_PlayList").toString());
    setting.setValue("/USER/OthersTab_FullScreenPlus", GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_FullScreenPlus").toString());
    setting.setValue("/USER/OthersTab_FullScreen", GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_FullScreen").toString());
    setting.setValue("/USER/OthersTab_IncreaseParallax", GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_IncreaseParallax").toString());
    setting.setValue("/USER/OthersTab_DecreaseParallax", GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_DecreaseParallax").toString());
    setting.setValue("/USER/OthersTab_ResetParallax", GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_ResetParallax").toString());
}

void ApplicationSettings::read_WindowSizeState()
{
    QString cfgPath = QCoreApplication::applicationDirPath() + "/config/SystemConfig.ini";
    QSettings setting(cfgPath, QSettings::IniFormat);

    QVariant variant = setting.value("/MediaPlay/Window_Width");
    GlobalDef::getInstance()->MIN_WINDOW_WIDTH = variant.isNull() ? 1280 : variant.toInt();
    variant = setting.value("/MediaPlay/Window_Heigth");
    GlobalDef::getInstance()->MIN_WINDOW_HEIGHT = variant.isNull() ? 720 : variant.toInt();
    //variant = setting.value("/MediaPlay/B_WINDOW_FULL_SCREEN");
    //GlobalDef::getInstance()->B_WINDOW_SIZE_MAXIMIZED = variant.isNull() ? false : variant.toBool();
    variant = setting.value("/MediaPlay/Window_Position_X");
    GlobalDef::getInstance()->MIN_WINDOW_POSITION_X = variant.isNull() ? 0 : variant.toInt();
    variant = setting.value("/MediaPlay/Window_Position_Y");
    GlobalDef::getInstance()->MIN_WINDOW_POSITION_Y = variant.isNull() ? 0 : variant.toInt();

    if (GlobalDef::getInstance()->MIN_WINDOW_POSITION_X < 0 && QGuiApplication::screens().size() == 1) {
        // FIX: 若之前保存的位置是在副屏，而当前仅有一个屏幕，则需要把坐标位置改回来，以便主界面能显示
        GlobalDef::getInstance()->MIN_WINDOW_POSITION_X = 0;
    } 

    variant = setting.value("/MediaPlay/ADV_TYPE_LEFT");
    GlobalDef::getInstance()->ADV_TYPE_LEFT = variant.isNull() ? AdvType(ADV_TYPE_NULL) : AdvType(variant.toInt());
    variant = setting.value("/MediaPlay/ADV_SOURCE_PATH_LEFT");
    GlobalDef::getInstance()->ADV_SOURCE_PATH_LEFT = variant.isNull() ? 0 : variant.toString();
    variant = setting.value("/MediaPlay/ADV_WIDTH_LEFT");
    GlobalDef::getInstance()->ADV_WIDTH_LEFT = variant.isNull() ? 0 : variant.toInt();
    variant = setting.value("/MediaPlay/ADV_HEIGHT_LEFT");
    GlobalDef::getInstance()->ADV_HEIGHT_LEFT = variant.isNull() ? 0 : variant.toInt();

    variant = setting.value("/MediaPlay/ADV_TYPE_RIGHT");
    GlobalDef::getInstance()->ADV_TYPE_RIGHT = variant.isNull() ? AdvType(ADV_TYPE_NULL) : AdvType(variant.toInt());
    variant = setting.value("/MediaPlay/ADV_SOURCE_PATH_RIGHT");
    GlobalDef::getInstance()->ADV_SOURCE_PATH_RIGHT = variant.isNull() ? 0 : variant.toString();
    variant = setting.value("/MediaPlay/ADV_WIDTH_RIGHT");
    GlobalDef::getInstance()->ADV_WIDTH_RIGHT = variant.isNull() ? 0 : variant.toInt();
    variant = setting.value("/MediaPlay/ADV_HEIGHT_RIGHT");
    GlobalDef::getInstance()->ADV_HEIGHT_RIGHT = variant.isNull() ? 0 : variant.toInt();

    variant = setting.value("/MediaPlay/PLAY_3D_VIEW_WIDTH");
    GlobalDef::getInstance()->PLAY_3D_VIEW_WIDTH = variant.isNull() ? 0 : variant.toInt();
    variant = setting.value("/MediaPlay/PLAY_3D_VIEW_HEIGHT");
    GlobalDef::getInstance()->PLAY_3D_VIEW_HEIGHT = variant.isNull() ? 0 : variant.toInt();
}

void ApplicationSettings::write_WindowSizeState()
{
    QString cfgPath = QCoreApplication::applicationDirPath() + "/config/SystemConfig.ini";
    QSettings setting(cfgPath, QSettings::IniFormat);

    setting.setValue("/MediaPlay/Window_Width", GlobalDef::getInstance()->MIN_WINDOW_WIDTH);
    setting.setValue("/MediaPlay/Window_Heigth", GlobalDef::getInstance()->MIN_WINDOW_HEIGHT);

    setting.setValue("/MediaPlay/Window_Position_X", GlobalDef::getInstance()->MIN_WINDOW_POSITION_X);
    setting.setValue("/MediaPlay/Window_Position_Y", GlobalDef::getInstance()->MIN_WINDOW_POSITION_Y);

    setting.setValue("/MediaPlay/ADV_TYPE_LEFT", (int) GlobalDef::getInstance()->ADV_TYPE_LEFT);
    setting.setValue("/MediaPlay/ADV_SOURCE_PATH_LEFT", GlobalDef::getInstance()->ADV_SOURCE_PATH_LEFT);
    setting.setValue("/MediaPlay/ADV_WIDTH_LEFT", GlobalDef::getInstance()->ADV_WIDTH_LEFT);
    setting.setValue("/MediaPlay/ADV_HEIGHT_LEFT", GlobalDef::getInstance()->ADV_HEIGHT_LEFT);

    setting.setValue("/MediaPlay/ADV_TYPE_RIGHT", (int) GlobalDef::getInstance()->ADV_TYPE_RIGHT);
    setting.setValue("/MediaPlay/ADV_SOURCE_PATH_RIGHT", GlobalDef::getInstance()->ADV_SOURCE_PATH_RIGHT);
    setting.setValue("/MediaPlay/ADV_WIDTH_RIGHT", GlobalDef::getInstance()->ADV_WIDTH_RIGHT);
    setting.setValue("/MediaPlay/ADV_HEIGHT_RIGHT", GlobalDef::getInstance()->ADV_HEIGHT_RIGHT);

    setting.setValue("/MediaPlay/PLAY_3D_VIEW_WIDTH", GlobalDef::getInstance()->PLAY_3D_VIEW_WIDTH);
    setting.setValue("/MediaPlay/PLAY_3D_VIEW_HEIGHT", GlobalDef::getInstance()->PLAY_3D_VIEW_HEIGHT);
}

void ApplicationSettings::read_PlayState()
{
    QString cfgPath = QCoreApplication::applicationDirPath() + "/config/SystemConfig.ini";
    QSettings setting(cfgPath, QSettings::IniFormat);

    QVariant variant = setting.value("/MediaPlay/B_VOLUME_MUTE");
    GlobalDef::getInstance()->B_VOLUME_MUTE = variant.isNull() ? true : variant.toBool();

    variant = setting.value("/MediaPlay/VOLUME_VAULE");
    GlobalDef::getInstance()->VOLUME_VAULE = variant.isNull() ? 50 : variant.toInt();

    variant = setting.value("/MediaPlay/PLAY_Stereo_Format");
    int defaultStereoFormat = variant.isNull() ? 1 : variant.toInt();
    GlobalDef::getInstance()->DefaultStereoFormat = (StereoFormat) defaultStereoFormat;

    variant = setting.value("/MediaPlay/PLAY_STEREO_INPUT");
    int defaultStereoInputFormat = variant.isNull() ? 0 : variant.toInt();
    GlobalDef::getInstance()->DefaultStereoInputFormat = (StereoInputFormat) defaultStereoInputFormat;

    variant = setting.value("/MediaPlay/PLAY_STEREO_OUTPUT");
    int defaultStereoOutputFormat = variant.isNull() ? 0 : variant.toInt();
    GlobalDef::getInstance()->DefaultStereoOutputFormat = (StereoOutputFormat) defaultStereoOutputFormat;

    variant = setting.value("/MediaPlay/PLAY_LOOP");
    int defaultPlayLoop = variant.isNull() ? 101 : variant.toInt();
    GlobalDef::getInstance()->PLAY_LOOP_ORDER = (PlayLoop) defaultPlayLoop;
}

void ApplicationSettings::write_PlayState()
{
    QString cfgPath = QCoreApplication::applicationDirPath() + "/config/SystemConfig.ini";
    QSettings setting(cfgPath, QSettings::IniFormat);

    setting.setValue("/MediaPlay/B_VOLUME_MUTE", GlobalDef::getInstance()->B_VOLUME_MUTE);
    setting.setValue("/MediaPlay/VOLUME_VAULE", GlobalDef::getInstance()->VOLUME_VAULE);

    setting.setValue("/MediaPlay/PLAY_Stereo_Format", GlobalDef::getInstance()->DefaultStereoFormat);
    setting.setValue("/MediaPlay/PLAY_STEREO_INPUT", GlobalDef::getInstance()->DefaultStereoInputFormat);
    setting.setValue("/MediaPlay/PLAY_STEREO_OUTPUT", GlobalDef::getInstance()->DefaultStereoOutputFormat);
    setting.setValue("/MediaPlay/PLAY_LOOP", GlobalDef::getInstance()->PLAY_LOOP_ORDER);
}

void ApplicationSettings::read_PlayList()
{
    //QString cfgPath = QCoreApplication::applicationDirPath() + "/config/SystemConfig.ini";
    //QSettings setting(cfgPath, QSettings::IniFormat);

    //    list file
    QFile file(QCoreApplication::applicationDirPath() + "/config/playList.json");
    if (!file.open(QIODevice::ReadWrite)) {
        if (logger) {
            logger->warn("can't open json file...");
        }
        return;
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    QString jsonStr = stream.readAll();
    file.close();

    QJsonParseError jsonError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonStr.toUtf8(), &jsonError);

    if (jsonError.error != QJsonParseError::NoError && !jsonDoc.isNull()) {
        if (logger) {
            logger->error("Json 格式错误!: {}", int(jsonError.error));
        }
        return;
    }

    QJsonObject rootObj = jsonDoc.object();
    GlobalDef::getInstance()->PLAY_LIST_DATA.playlist_current_index = rootObj.value("playlist_current_index").toInt();
    GlobalDef::getInstance()->PLAY_LIST_DATA.playlist_current_video = rootObj.value("playlist_current_video").toInt();
    GlobalDef::getInstance()->PLAY_LIST_DATA.playlist_video_play_time = qJsonValue2longlong(rootObj.value("playlist_video_play_time"));
    QJsonValue playlistListArrayJsonArray = rootObj.value("playlist_list_array");
    if (playlistListArrayJsonArray.type() == QJsonValue::Array) {
        QJsonArray jsonArrayPlayListList = playlistListArrayJsonArray.toArray();
        GlobalDef::getInstance()->PLAY_LIST_DATA.play_list.clear();
        for (int i = 0; i < jsonArrayPlayListList.size(); ++i) {
            QJsonObject videoRootObj = jsonArrayPlayListList[i].toObject();
            PlayList itemPlayList;
            itemPlayList.list_name = videoRootObj.value("playlist_list_name").toString();
            QJsonValue videoListJsonValue = videoRootObj.value("video_array");
            if (videoListJsonValue.type() == QJsonValue::Array) {
                QJsonArray videoListArray = videoListJsonValue.toArray();
                for (int j = 0; j < videoListArray.size(); ++j) {
                    QJsonObject videoObj = videoListArray[j].toObject();
                    VideoInfo itemVideoInfo;
                    itemVideoInfo.video_path = videoObj.value("video_path").toString();
                    itemVideoInfo.video_total_time = videoObj.value("video_time").toString();
                    itemPlayList.video_list.push_back(itemVideoInfo);
                }
            }
            GlobalDef::getInstance()->PLAY_LIST_DATA.play_list.push_back(itemPlayList);
        }
    }
    if (GlobalDef::getInstance()->PLAY_LIST_DATA.play_list.size() < 1) {
        PlayList defPlayList;
        defPlayList.list_name = "def";
        GlobalDef::getInstance()->PLAY_LIST_DATA.play_list.push_back(defPlayList);
    }

    //----------------------------------------------------------------
    //std::cout << std::endl << std::endl;
    //std::cout << GlobalDef::getInstance()->PLAY_LIST_DATA.playlist_current_index << std::endl;
    //std::cout << GlobalDef::getInstance()->PLAY_LIST_DATA.playlist_current_video << std::endl;
    //std::cout << GlobalDef::getInstance()->PLAY_LIST_DATA.playlist_video_play_time << std::endl;
    //for (int i = 0; i < GlobalDef::getInstance()->PLAY_LIST_DATA.play_list.size(); ++i) {
    //    std::cout<< GlobalDef::getInstance()->PLAY_LIST_DATA.play_list[i].list_name.toStdString()<<endl;
    //    for (int j = 0; j < GlobalDef::getInstance()->PLAY_LIST_DATA.play_list[i].video_list.size(); ++j) {
    //        std::cout<< GlobalDef::getInstance()->PLAY_LIST_DATA.play_list[i].video_list[j].video_path.toStdString()<<endl;
    //        std::cout<< GlobalDef::getInstance()->PLAY_LIST_DATA.play_list[i].video_list[j].video_total_time.toStdString()<<endl;
    //    }
    //}

    ////--------------------------------------------------------------
    //QJsonValue vplStr = rootObj.value("videoPathList");
    //
    //if (vplStr.type() == QJsonValue::Array) {
    //	QJsonArray avJsonArray = vplStr.toArray();
    //	GlobalDef::getInstance()->PLAY_LIST.clear();
    //	for (int i = 0; i < avJsonArray.size(); ++i) {
    //		GlobalDef::getInstance()->PLAY_LIST.append(avJsonArray[i].toString());
    //	}
    //}
}

void ApplicationSettings::write_PlayList()
{
    QFile file(QCoreApplication::applicationDirPath() + "/config/playList.json");
    if (!file.open(QIODevice::ReadWrite)) {
        if (logger) {
            logger->error("can't open json file: {}", file.fileName().toStdString());
        }
        return;
    }
    file.resize(0);

    QJsonObject rootObj;
    rootObj.insert("playlist_current_index", GlobalDef::getInstance()->PLAY_LIST_DATA.playlist_current_index);
    rootObj.insert("playlist_current_video", GlobalDef::getInstance()->PLAY_LIST_DATA.playlist_current_video);
    rootObj.insert("playlist_video_play_time", static_cast<qint64>(GlobalDef::getInstance()->PLAY_LIST_DATA.playlist_video_play_time));
    QJsonArray playListJsonArray;
    for (int i = 0; i < GlobalDef::getInstance()->PLAY_LIST_DATA.play_list.size(); ++i) {
        QJsonObject itemObj;
        itemObj.insert("playlist_list_name", GlobalDef::getInstance()->PLAY_LIST_DATA.play_list[i].list_name);
        QJsonArray videoJsonArray;
        for (int j = 0; j < GlobalDef::getInstance()->PLAY_LIST_DATA.play_list[i].video_list.size(); ++j) {
            QJsonObject videoObj;
            videoObj.insert("video_path", GlobalDef::getInstance()->PLAY_LIST_DATA.play_list[i].video_list[j].video_path);
            videoObj.insert("video_time", GlobalDef::getInstance()->PLAY_LIST_DATA.play_list[i].video_list[j].video_total_time);
            videoJsonArray.append(videoObj);
        }
        itemObj.insert("video_array", videoJsonArray);
        playListJsonArray.append(itemObj);
    }

    rootObj.insert("playlist_list_array", playListJsonArray);
    //-----------------------------------------------------------------------
    //QJsonArray avJsonArray;
    //for (int i = 0; i < GlobalDef::getInstance()->PLAY_LIST.size(); ++i) {
    //	avJsonArray.append(QJsonValue(GlobalDef::getInstance()->PLAY_LIST[i]));
    //}
    //QJsonObject jsonObject;
    //jsonObject.insert("videoPathList", avJsonArray);
    //QJsonDocument jsonDoc;
    //jsonDoc.setObject(jsonObject);

    QJsonDocument jsonDoc;
    jsonDoc.setObject(rootObj);
    file.write(jsonDoc.toJson());
    file.close();
}

void ApplicationSettings::clear_PlayList()
{
    QFile file(QCoreApplication::applicationDirPath() + "/config/playList.json");
    if (!file.open(QIODevice::ReadWrite)) {
        if (logger) {
            logger->error("can't open json file.. {} ", file.fileName().toStdString());
        }
        return;
    }
    file.resize(0);
    file.close();
}

PlayList ApplicationSettings::load_PlayList(QString path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadWrite)) {
        if (logger) {
            logger->error("can't open json file.. {} ", file.fileName().toStdString());
        }
        return PlayList();
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    QString jsonStr = stream.readAll();
    file.close();

    QJsonParseError jsonError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonStr.toUtf8(), &jsonError);

    if (jsonError.error != QJsonParseError::NoError && !jsonDoc.isNull()) {
        if (logger) {
            logger->error("Json format wrong {} ", int(jsonError.error));
        }
        return PlayList();
    }

    PlayList readPlayList;
    QJsonObject rootObj = jsonDoc.object();
    readPlayList.list_name = rootObj.value("playlist_list_name").toString();
    QJsonValue videoListJsonValue = rootObj.value("video_array");
    if (videoListJsonValue.type() == QJsonValue::Array) {
        QJsonArray videoListArray = videoListJsonValue.toArray();
        for (int j = 0; j < videoListArray.size(); ++j) {
            QJsonObject videoObj = videoListArray[j].toObject();
            VideoInfo itemVideoInfo;
            itemVideoInfo.video_path = videoObj.value("video_path").toString();
            itemVideoInfo.video_total_time = videoObj.value("video_time").toString();
            readPlayList.video_list.push_back(itemVideoInfo);
        }
    }
    return readPlayList;
}

void ApplicationSettings::export_PlayList(QString path, int index)
{
    QFile file(path + "/" + GlobalDef::getInstance()->PLAY_LIST_DATA.play_list[index].list_name + ".json");
    if (!file.open(QIODevice::ReadWrite)) {
        if (logger) {
            logger->error("can't open json file.. {} ", file.fileName().toStdString());
        }
        return;
    }
    file.resize(0);

    QJsonObject rootObj;
    rootObj.insert("playlist_list_name", GlobalDef::getInstance()->PLAY_LIST_DATA.play_list[index].list_name);
    QJsonArray videoJsonArray;
    for (int j = 0; j < GlobalDef::getInstance()->PLAY_LIST_DATA.play_list[index].video_list.size(); ++j) {
        QJsonObject videoObj;
        videoObj.insert("video_path", GlobalDef::getInstance()->PLAY_LIST_DATA.play_list[index].video_list[j].video_path);
        videoObj.insert("video_time", GlobalDef::getInstance()->PLAY_LIST_DATA.play_list[index].video_list[j].video_total_time);
        videoJsonArray.append(videoObj);
    }
    rootObj.insert("video_array", videoJsonArray);

    QJsonDocument jsonDoc;
    jsonDoc.setObject(rootObj);
    file.write(jsonDoc.toJson());
    file.close();
}

void ApplicationSettings::read_about_copyright()
{
    QString cfgPath = QCoreApplication::applicationDirPath() + "/config/SystemConfig.ini";
    QSettings setting(cfgPath, QSettings::IniFormat);

    QVariant variant = setting.value("/MediaPlay/ABOUT_CR_ZHCN");
    GlobalDef::getInstance()->ABOUT_CR_ZHCN = variant.isNull() ? QString(
                                                  tr("维真公司版权所有<br>Copyright © 2024 - 2026[WeiZheng]<br>宁波维真科技有限公司，保留所有权利<br>"))
                                                               : variant.toString();

    variant = setting.value("/MediaPlay/ABOUT_CR_EN");
    GlobalDef::getInstance()->ABOUT_CR_EN = variant.isNull()
                                                ? QString(tr("维真公司版权所有<br>Copyright © 2024 - 2026[WeiZheng]<br>宁波维真科技有限公司，保留所有权利<br>"))
                                                : variant.toString();

    variant = setting.value("/MediaPlay/ABOUT_CR_ZHHANT");
    GlobalDef::getInstance()->ABOUT_CR_ZHHANT = variant.isNull() ? QString(
                                                    tr("维真公司版权所有<br>Copyright © 2024 - 2026[WeiZheng]<br>宁波维真科技有限公司，保留所有权利<br>"))
                                                                 : variant.toString();
}

qint64 ApplicationSettings::qJsonValue2longlong(QJsonValue value)
{
    return QString::number(value.toDouble(), 'f', 0).toLongLong();
}