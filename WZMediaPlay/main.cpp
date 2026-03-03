#include "BroadcastReceiver.h"
#include "MainWindow.h"
#include <QBuffer>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QMovie>
#include <QSplashScreen>
#include <QThread>
#include <QtWidgets/QApplication>

#include "ApplicationSettings.h"
#include <QRandomGenerator>
#include <QTime>
#include "RemoteControlServer.h"
#include "crash_handler.h"

#include "UdpWorker.h"

int main(int argc, char *argv[])
{
    // 初始化崩溃处理器（必须在QApplication之前，跨平台支持）
    CrashHandler::initialize();
    
    QApplication a(argc, argv);
    MainWindow w;
    w.setMinimumSize(QSize(800, 600));
    w.hide();

    //---------------------------------------------------------------------------------------------
    //  读取Logo图像
    ApplicationSettings mWZSetting;
    mWZSetting.read_SplashLogoPath();
    QString pngPath = GlobalDef::getInstance()->SPLASH_LOGO_PATH;
    QDir dir;
    if (pngPath.isEmpty() || false == dir.exists(pngPath)) {
        //若logo文件未设置或者不存在该文件，则使用默认的logo文件
        pngPath = (QCoreApplication::applicationDirPath() + QString("/Resources/logo/company_logo.png"));
    }
    QPixmap pixmap;
    pixmap.load(pngPath);

    //根据配置的宽高进行缩放
    if (GlobalDef::getInstance()->SPLASH_LOGO_WIDTH != 0 && GlobalDef::getInstance()->SPLASH_LOGO_HEIGHT != 0)
        pixmap = pixmap.scaled(
            QSize(GlobalDef::getInstance()->SPLASH_LOGO_WIDTH, GlobalDef::getInstance()->SPLASH_LOGO_HEIGHT), Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QSplashScreen splash(pixmap); 
    splash.setWindowOpacity(GlobalDef::getInstance()->SPLASH_OPACITY); // 设置窗口透明度
    splash.show();
    
    QDateTime time = QDateTime::currentDateTime();
    QDateTime currentTime = QDateTime::currentDateTime(); //记录当前时间
    while (time.secsTo(currentTime) <= 1)                 // 5为需要延时的秒数
    {
        currentTime = QDateTime::currentDateTime();
        a.processEvents();
    };

    w.show();
    splash.finish(&w);

    BroadcastReceiver mBroadcastReceiver;
    QObject::connect(&mBroadcastReceiver, &BroadcastReceiver::sendCMD, &w, &MainWindow::broadcastCallback);
    mBroadcastReceiver.startBroadcastListen();

    RemoteControlServer mRemoteControlServer(&w);
    QObject::connect(&mRemoteControlServer, &RemoteControlServer::sendCMD, &w, &MainWindow::broadcastCallback);

    return a.exec();
}
