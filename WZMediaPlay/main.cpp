#include "BroadcastReceiver.h"
#include "MainWindow.h"
#include <QBuffer>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QMovie>
#include <QSplashScreen>
#include <QThread>
#include <QTimer>
#include <QtWidgets/QApplication>
#include <QSurfaceFormat>

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

    // 设置 OpenGL 默认格式（必须在 QApplication 之前）
    // macOS OpenGL 4.1 支持 GLSL 150，使用 Core Profile
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setVersion(4, 1);  // macOS 最高支持 OpenGL 4.1
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    QSurfaceFormat::setDefaultFormat(format);

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

    // 处理命令行参数：如果传入了视频文件，则打开它
    QStringList args = a.arguments();
    if (args.size() > 1) {
        QString videoPath = args.at(1);
        QFileInfo fileInfo(videoPath);
        if (fileInfo.exists() && fileInfo.isFile()) {
            // 延迟一点让窗口完全初始化
            QTimer::singleShot(100, [&w, videoPath]() {
                w.openVideoFromCommandLine(videoPath);
            });
        }
    }

    return a.exec();
}
