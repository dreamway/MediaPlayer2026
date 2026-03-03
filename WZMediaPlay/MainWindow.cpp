#include "MainWindow.h"
#include "gui/AboutDialog.h"
#include "ApplicationSettings.h"
#include "test_support/TestPipeServer.h"
#include "camera/CameraManager.h"
#include "gui/DrawWidget.h"
#include "gui/FloatButton.h"
#include "GlobalDef.h"
#include "PlayController.h"
#include "playlist/PlayListPage.h"
#include "playlist/PlayListManager.h"
#include "gui/SettingsDialog.h"
#include "videoDecoder/RendererFactory.h"
#include "camera/CameraOpenGLWidget.hpp"
#include <QVBoxLayout>
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"
#include "utils/ErrorCode.h"

#include "UdpWorker.h"
#ifdef Q_OS_WIN
#include <Windows.h>
#include <wtsapi32.h>
#endif
#include <algorithm>
#include <chrono>
#include <iostream>
#if !defined(_MSC_VER) && (defined(__x86_64__) || defined(__i386__))
#include <cpuid.h>
#endif
#include <QAction>
#include <QActionGroup>
#include <QCryptographicHash>
#include <QDir>
#include <QDropEvent>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QMimeData>
#include <QNetworkInterface>
#include <QRandomGenerator>
#include <QScreen>
#include <QShortcut>
#include <QEventLoop>
#include <QThread>
#include <QTimer>
#include <QTranslator>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    QString startupConfig = readConfig();

    GlobalDef::getInstance()->VIDEO_FILE_TYPE << ".mp4" << ".flv" << ".f4v" << ".webm" << ".m4v" << ".mov" << ".3gp" << ".3g2" << ".rm" << ".rmvb" << ".wmv"
                                              << ".avi"
                                              << ".asf" << ".mpg" << ".mpeg" << ".mpe" << ".ts" << ".div" << ".dv" << ".divx" << ".vob" << ".mkv" << ".wzmp4"
                                              << ".wzavi"
                                              << ".wzmkv" << ".wzmov" << ".wzmpg";

    if (GlobalDef::getInstance()->LOG_MODE == 1) {
#ifdef Q_OS_WIN
        AllocConsole();
        FILE *unused1, *unused2;
        freopen_s(&unused1, "CONOUT$", "w", stdout);
        freopen_s(&unused2, "CONOUT$", "w", stderr);
#endif
    }

    ////Setup Logger
    bool setupOk = setupLogger();
    if (false == setupOk) {
        QMessageBox::StandardButton button = QMessageBox::critical(static_cast<QWidget *>(this), QString(tr("Error")), QString(tr("Setup Logger Failed.")));
        std::cerr << "Logger setup failed.";
        exit(0);
    }

    setWindowFlags(Qt::CustomizeWindowHint);

    ui.setupUi(this);

    logger->info("------------------ Begin Startup Config ----------------------------");
    logger->info(startupConfig.toUtf8().constData());
    logger->info("------------------ End Startup Config ----------------------------");
#if 0
    //  cpu mac 验证
    {
        QStringList deviceInfo = getDeviceInfo();
        //  数据加密
        QString strVerification = QString("WeiZheng_") + deviceInfo[0] + QString("_verification"); // + deviceInfo[1];
        //logger->info("strVerification:{}", strVerification.toStdString());
        QByteArray byteArray = strVerification.toLocal8Bit();
        QByteArray hash = QCryptographicHash::hash(byteArray, QCryptographicHash::Md5);
        QString strMD5 = hash.toHex();
        //logger->info("Verification code:{}", strMD5.toStdString());

        QFile readFile(QCoreApplication::applicationDirPath() + "/Registration/license.reg");
        if (readFile.open(QIODevice::ReadOnly)) {
            QByteArray readHash = readFile.readAll();
            QString licenseMD5 = readHash.toHex();
            readFile.close();
            //std::cout << "read Verfication code:" << licenseMD5.toStdString() << std::endl;
            if (strMD5 != licenseMD5) {
                //logger->error("strMD5 {} != licenseMD5 {}", strMD5.toStdString(), licenseMD5.toStdString());
                QMessageBox::information(this, QString(tr("错误")), QString(tr("软件注册错误，程序将无法正常使用")), QMessageBox::NoButton, QMessageBox::Close);
                //close();
                QDateTime startUsingTime = QDateTime(QDate(2026, 01, 01), QTime(0, 0, 0, 0));
                QDateTime currentTime = QDateTime::currentDateTime();
                qint64 elapsedDay = startUsingTime.daysTo(currentTime);
                const int MAX_EVALUATE_DAY = 90;
                const int WARNING_EXPIRED_DAY = 30;
                if (elapsedDay > MAX_EVALUATE_DAY) {
                    QMessageBox::information(
                        this, QString(tr("警告")), QString(tr("软件注册错误，并已超过使用时限，程序将无法正常使用")), QMessageBox::NoButton, QMessageBox::Close);
                    //close();
                    mIsNotRegistered = true;
                } else {
                    logger->warn("app is not registered, elapsedDay:{}, since startUsingTime: {}", elapsedDay, startUsingTime.toString().toStdString());
                    if (elapsedDay > WARNING_EXPIRED_DAY) {
                        QMessageBox::information(
                            this,
                            QString(tr("错误")),
                            QString(tr("软件注册错误，即将超过可使用时限,再不注册将无法使用")),
                            QMessageBox::NoButton,
                            QMessageBox::Close);
                    }
                }
            } else {
                logger->info("App is Registered, OK");
                ui.actionRegistrationRequest->setEnabled(false);
                ui.actionRegistration->setEnabled(false);
            }
        } else {
            // 未注册情况下，仍可试用一段时间
            QDateTime startUsingTime = QDateTime(QDate(2025, 11, 01), QTime(0, 0, 0, 0));
            QDateTime currentTime = QDateTime::currentDateTime();
            qint64 elapsedDay = startUsingTime.daysTo(currentTime);
            const int MAX_EVALUATE_DAY = 90;
            const int WARNING_EXPIRED_DAY = 30;
            if (elapsedDay > MAX_EVALUATE_DAY) {
                QMessageBox::information(
                    this, QString(tr("警告")), QString(tr("软件未注册，并已超过使用时限, 程序无法正常使用")), QMessageBox::NoButton, QMessageBox::Close);
                //close();
                mIsNotRegistered = true;
            } else {
                logger->warn("app is not registered, elapsedDay:{}, since startUsingTime: {}", elapsedDay, startUsingTime.toString().toStdString());

                if (elapsedDay > WARNING_EXPIRED_DAY) {
                    QMessageBox::information(
                        this, QString(tr("错误")), QString(tr("软件未注册，即将超过可使用时限,再不注册将无法使用")), QMessageBox::NoButton, QMessageBox::Close);
                }
            }
        }
    }
#endif
    initLanguage();
    initUI();
    this->setAcceptDrops(true);

    // 创建PlayController（替代直接创建Movie，作为Movie和FFmpegView/MainWindow之间的中间层)
    playController_ = new PlayController(this);
    // 连接播放状态变化信号
    if (playController_) {
        connect(playController_, &PlayController::playbackStateChanged, this, &MainWindow::onPlaybackStateChanged);
        connect(playController_, &PlayController::playbackCompleted, this, &MainWindow::handlePlaybackFinished);
    }
    connect(playController_, &PlayController::seekingFinished, this, &MainWindow::onSeekingFinished);
    logger->info("after setup PlayController...");

    // 延迟创建渲染器，确保QApplication完全初始化后再创建OpenGL对象
    // 使用QTimer::singleShot延迟到下一个事件循环，确保QApplication已完全初始化
    QTimer::singleShot(0, this, [this]() {
        // 使用 RendererFactory 创建渲染器并设置到 StereoVideoWidget 和 PlayController
        // 根据当前的立体格式创建合适的渲染器
        StereoFormat initialFormat = mStereoFormat;
        auto renderer = RendererFactory::createRenderer(initialFormat);
        if (renderer) {
            // 设置渲染器到 StereoVideoWidget
            ui.playWidget->setRenderer(renderer);
            // 设置渲染器到 PlayController
            playController_->setVideoRenderer(renderer);
            // 设置 VideoWidgetBase 到 PlayController
            playController_->setVideoWidgetBase(ui.playWidget);
            logger->info("MainWindow: Created renderer using RendererFactory for format {} and set to both StereoVideoWidget and PlayController", static_cast<int>(initialFormat));
        } else {
            logger->error("MainWindow: Failed to create renderer using RendererFactory");
        }
    });

    ui.playWidget->SetPlayController(playController_);

    // 创建 CameraOpenGLWidget（与 StereoVideoWidget 同级）
    // 注意：Qt 6.4 on Linux 的 QOpenGLWidget 存在 RHI 合成 bug，
    // 即使 hide() 也会导致 rhiFlush 崩溃。Camera 功能在 Linux 上暂时禁用。
#ifdef Q_OS_WIN
    cameraWidget_ = new CameraOpenGLWidget(ui.playWidget->parentWidget());
    if (cameraWidget_) {
        cameraWidget_->hide();
        QWidget *parentWidget = ui.playWidget->parentWidget();
        if (parentWidget) {
            QVBoxLayout *parentLayout = qobject_cast<QVBoxLayout*>(parentWidget->layout());
            if (parentLayout) {
                int playWidgetIndex = parentLayout->indexOf(ui.playWidget);
                if (playWidgetIndex >= 0) {
                    parentLayout->insertWidget(playWidgetIndex + 1, cameraWidget_);
                } else {
                    parentLayout->addWidget(cameraWidget_);
                }
            }
        }
        logger->info("MainWindow: CameraOpenGLWidget created as sibling of StereoVideoWidget");
    }
#else
    logger->info("MainWindow: CameraOpenGLWidget disabled on Linux (Qt 6 QOpenGLWidget RHI bug)");
#endif

    // 创建 CameraManager 并设置 Camera Widget
    cameraManager_ = new CameraManager(this);
    if (cameraManager_ && cameraWidget_) {
        // 设置 Camera Widget 到 CameraManager
        cameraManager_->setCameraWidget(cameraWidget_);
        logger->info("MainWindow: CameraManager created and camera widget set");
    }

    // 创建播放列表管理器
    playListManager_ = new PlayListManager(this);
    if (playListManager_) {
        // 连接播放列表信号
        // 注意：currentVideoChanged信号由setCurrentVideo()触发，用于更新当前播放项
        // 但不自动播放，避免与openPath()重复触发
        connect(playListManager_, &PlayListManager::currentVideoChanged, this, [this](const QString &videoPath) {
            logger->debug("MainWindow: Current video changed to: {}", videoPath.toStdString());
            // 不在这里自动播放，由调用setCurrentVideo的地方负责播放
        });
        logger->info("MainWindow: PlayListManager created");
    }

    playController_->setVolume((float) ui.horizontalSlider_volume->value() / (float) ui.horizontalSlider_volume->maximum());
    if (ui.pushButton_volume->isChecked()) {
        playController_->toggleMute();
    }

    // 测试模式：启用命名管道输出音频状态，供 Python 自动化测试验证
    if (qEnvironmentVariable("WZ_TEST_MODE") == "1") {
        testPipeServer_ = new TestPipeServer(playController_, this);
        if (testPipeServer_->start()) {
            logger->info("TestPipeServer started for automation testing");
        } else {
            delete testPipeServer_;
            testPipeServer_ = nullptr;
        }
    }

#ifdef INSPECT
    inspectTimer_ = new QTimer(this);
    connect(inspectTimer_, &QTimer::timeout, this, &MainWindow::onInspectTimerTimeout);
    inspectTimer_->start(1000);
#endif

    // 运行模式(PRO, 全功能)
    GlobalDef::getInstance()->SetRunningMode(RunningMode::RM_PRO);

    if (qGuiApp->screens().size() == 1) {
        QRect primaryScreenRect = qGuiApp->primaryScreen()->availableGeometry();
        // 若默认配置的起始点是之前双屏的，而当前显示为只有一个屏幕，则需要将Player窗口移至当前屏幕
        if (GlobalDef::getInstance()->MIN_WINDOW_POSITION_X > primaryScreenRect.width()
            || GlobalDef::getInstance()->MIN_WINDOW_POSITION_Y > primaryScreenRect.height()) {
            logger->warn("MinWinPositionX {} > primaryScreen.width:{}, need to set window pos to primaryScreen");
            move(QPoint(0, 0));
            GlobalDef::getInstance()->MIN_WINDOW_POSITION_X = 0;
            GlobalDef::getInstance()->MIN_WINDOW_POSITION_Y = 0;
        } else {
            if (GlobalDef::getInstance()->MIN_WINDOW_POSITION_X % 2 == 0) {
                GlobalDef::getInstance()->MIN_WINDOW_POSITION_X = GlobalDef::getInstance()->MIN_WINDOW_POSITION_X + 1;
            }
            if (GlobalDef::getInstance()->MIN_WINDOW_POSITION_Y % 2 == 0) {
                GlobalDef::getInstance()->MIN_WINDOW_POSITION_Y = GlobalDef::getInstance()->MIN_WINDOW_POSITION_Y + 1;
            }
            move(GlobalDef::getInstance()->MIN_WINDOW_POSITION_X, GlobalDef::getInstance()->MIN_WINDOW_POSITION_Y);
        }

        // 若默认配置的窗口大小超过了当前屏幕的大小，需要限制其大小
        GlobalDef::getInstance()->B_WINDOW_SIZE_MAXIMIZED = false;
        if (GlobalDef::getInstance()->MIN_WINDOW_WIDTH > primaryScreenRect.width() || GlobalDef::getInstance()->MIN_WINDOW_HEIGHT > primaryScreenRect.height()) {
            resize(primaryScreenRect.width(), primaryScreenRect.height());
            GlobalDef::getInstance()->MIN_WINDOW_WIDTH = primaryScreenRect.width();
            GlobalDef::getInstance()->MIN_WINDOW_HEIGHT = primaryScreenRect.height();
        } else {
            resize(GlobalDef::getInstance()->MIN_WINDOW_WIDTH, GlobalDef::getInstance()->MIN_WINDOW_HEIGHT);
        }
    } else {
        if (GlobalDef::getInstance()->MIN_WINDOW_POSITION_X % 2 == 0) {
            GlobalDef::getInstance()->MIN_WINDOW_POSITION_X = GlobalDef::getInstance()->MIN_WINDOW_POSITION_X + 1;
        }
        if (GlobalDef::getInstance()->MIN_WINDOW_POSITION_Y % 2 == 0) {
            GlobalDef::getInstance()->MIN_WINDOW_POSITION_Y = GlobalDef::getInstance()->MIN_WINDOW_POSITION_Y + 1;
        }
        move(GlobalDef::getInstance()->MIN_WINDOW_POSITION_X, GlobalDef::getInstance()->MIN_WINDOW_POSITION_Y);

        GlobalDef::getInstance()->B_WINDOW_SIZE_MAXIMIZED = false;
        resize(GlobalDef::getInstance()->MIN_WINDOW_WIDTH, GlobalDef::getInstance()->MIN_WINDOW_HEIGHT);
    }

    mWindowSizeState = WINDOW_MINIMIZED;

    // Loading Default Settings
    mStereoFormat = GlobalDef::getInstance()->DefaultStereoFormat;
    mStereoInputFormat = GlobalDef::getInstance()->DefaultStereoInputFormat;
    mStereoOutputFormat = GlobalDef::getInstance()->DefaultStereoOutputFormat;
    mStereoOutputFormatWhenSwitch2D = mStereoOutputFormat;

    // 多屏幕时的消息处理（保存以记录用户行为)
    connect(qGuiApp, SIGNAL(primaryScreenChanged(QScreen *)), this, SLOT(onPrimaryScreenChanged(QScreen *)));
    connect(qGuiApp, SIGNAL(screenAdded(QScreen *)), this, SLOT(onScreenAdded(QScreen *)));
    connect(qGuiApp, SIGNAL(screenRemoved(QScreen *)), this, SLOT(onScreenRemoved(QScreen *)));
}

MainWindow::~MainWindow()
{
    writeConfig();

    if (testPipeServer_) {
        testPipeServer_->stop();
        testPipeServer_ = nullptr;
    }
    if (playController_) {
        playController_->stop();
        // PlayController 是 QObject，由 Qt 的父子关系自动管理
        playController_ = nullptr;
    }
    if (GlobalDef::getInstance()->LOG_MODE == 1) {
#ifdef Q_OS_WIN
        FreeConsole();
#endif
    }
    if (logger) {
        logger->flush();
        // 注意：只有当 LOG_MODE 为 0 或 1 时，logger 才是我们 new 出来的
        // LOG_MODE 为 2 时，logger 是 file_sink.get()，不应该 delete
        if (GlobalDef::getInstance()->LOG_MODE == 0 || GlobalDef::getInstance()->LOG_MODE == 1) {
            delete logger;
        }
        logger = nullptr;
    }
    spdlog::drop_all();
}

bool MainWindow::setupLogger()
{
    //各自配置console/file的pattern
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::debug);
    console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e][thread %t][%s:%#][%l] : %v");

    QString timeStr = QDateTime::currentDateTime().toString("yyyyMMddHHmmss");
    if (!QDir::current().exists("./logs")) {
        QDir::current().mkdir("./logs");
    }
    QString logFilename = QString("./logs/MediaPlayer_" + timeStr + ".log");
    //auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilename.toUtf8().data(), true);
    auto file_sink = spdlog::rotating_logger_mt("fileLogger", logFilename.toUtf8().data(), 1024 * 1024 * 5, 5);

    file_sink->set_level(spdlog::level::info);
    //file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^--%L--%$] [thread %t] %v");
    // 格式说明：%s = 文件名, %# = 行号, %! = 函数名, %L = 日志级别
    file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e][thread %t][%s:%#][%L] : %v");

    switch (GlobalDef::getInstance()->LOG_MODE) {
    case 1:
    case 0:
        logger = new spdlog::logger("logger", console_sink);
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e][thread %t][%s:%#][%L] : %v");
        break;
    case 2:
    default:
        // 对于文件日志，确保格式正确应用
        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e][thread %t][%s:%#][%L] : %v");
        logger = file_sink.get();
        break;
    }

    /**
    spdlog log_level definition
        trace = SPDLOG_LEVEL_TRACE, // 0
        debug = SPDLOG_LEVEL_DEBUG,                         // 1
        info = SPDLOG_LEVEL_INFO,                           //2
        warn = SPDLOG_LEVEL_WARN,                           //3
        err = SPDLOG_LEVEL_ERROR,                           //4
        critical = SPDLOG_LEVEL_CRITICAL,                   //5
        off = SPDLOG_LEVEL_OFF,                             //6
        */
    switch (GlobalDef::getInstance()->LOG_LEVEL) {
    case 0:
        console_sink->set_level(spdlog::level::trace);
        file_sink->set_level(spdlog::level::trace);
        logger->set_level(spdlog::level::trace);
        break;
    case 1:
        console_sink->set_level(spdlog::level::debug);
        file_sink->set_level(spdlog::level::debug);
        logger->set_level(spdlog::level::debug);
        break;
    case 2:
        console_sink->set_level(spdlog::level::info);
        file_sink->set_level(spdlog::level::info);
        logger->set_level(spdlog::level::info);
        break;
    case 3:
        console_sink->set_level(spdlog::level::warn);
        file_sink->set_level(spdlog::level::warn);
        logger->set_level(spdlog::level::warn);
        break;
    case 4:
        console_sink->set_level(spdlog::level::err);
        file_sink->set_level(spdlog::level::err);
        logger->set_level(spdlog::level::err);
        break;
    case 5:
        console_sink->set_level(spdlog::level::critical);
        file_sink->set_level(spdlog::level::critical);
        logger->set_level(spdlog::level::critical);
        break;
    case 6:
        console_sink->set_level(spdlog::level::off);
        file_sink->set_level(spdlog::level::off);
        logger->set_level(spdlog::level::off);
        break;
    }

    //配置全局的log pattern
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e][thread %t][%s:%#][%l] : %v");

    SPDLOG_LOGGER_INFO(logger,"logMode:{}, logLevel:{}", GlobalDef::getInstance()->LOG_MODE, GlobalDef::getInstance()->LOG_LEVEL);
    SPDLOG_LOGGER_INFO(logger, "Check for testing logger with FILE,LINE, log_level:{}", GlobalDef::getInstance()->LOG_LEVEL);
    ////Enable backtracking
    //logger->enable_backtrace(32);
    ////When needed, call dump_backtrace() to dump them to your log.
    //logger->dump_backtrace();

    return true;
}

void MainWindow::broadcastCallback(char cmd)
{
    //  TODO  cmd ACTION
    // 命令定义：
    //	L / R: 0x01    R / L : 0x02    2D : 0x03    3D : 0x04
    //	停止：0x05    播放：0x06    暂停：0x07    上一个：0x08
    //	下一个：0x09    快进：0x0A    快退：0x0B    音量减：0x0C
    //	音量加：0x0D    单个循环：0x0E    全部循环：0x0F
    switch (cmd) {
    case 0x00: {
        UdpWorker mUdpWorker;
        mUdpWorker.sendIPAddress();
    } break;
    case 0x01:
        logger->info("left right view..");
        if (ui.actionInputLR->isEnabled()) {
            ui.actionInputLR->setChecked(true);
            reply_actionInput_Selected(ui.actionInputLR);
        }
        break;
    case 0x02:
        logger->info("right left view");
        if (ui.actionInputRL->isEnabled()) {
            ui.actionInputRL->setChecked(true);
            reply_actionInput_Selected(ui.actionInputRL);
        }
        break;
    case 0x03:
        logger->info("2D left play");
        if (ui.action_3D_left->isEnabled()) {
            mStereoFormat = StereoFormat::STEREO_FORMAT_3D;
            ui.action_3D_left->setChecked(true);
            //  intput
            ui.comboBox_3D_input->setEnabled(true);
            ui.actionInputLR->setEnabled(true);
            ui.actionInputRL->setEnabled(true);
            ui.actionInputUD->setEnabled(true);
            //  output
            ui.actionOutputHorizontal->setDisabled(true);
            ui.actionOutputVertical->setDisabled(true);
            ui.actionOutputChess->setDisabled(true);
            ui.actionOutputOnlyLeft->setEnabled(true);

            if (mStereoOutputFormat != StereoOutputFormat::STEREO_OUTPUT_FORMAT_ONLY_LEFT) {
                mStereoOutputFormatWhenSwitch2D = mStereoOutputFormat;
            }
            mStereoOutputFormat = StereoOutputFormat::STEREO_OUTPUT_FORMAT_ONLY_LEFT;
            ui.actionOutputOnlyLeft->setChecked(true);

            onStereoFormatChanged(StereoFormat(STEREO_FORMAT_3D), mStereoInputFormat, mStereoOutputFormat);
            ui.comboBox_src2D_3D2D->setCurrentIndex(1);
            ui.playWidget->SetStereoFormat(StereoFormat(STEREO_FORMAT_3D));
            ui.playWidget->SetStereoOutputFormat(StereoOutputFormat::STEREO_OUTPUT_FORMAT_ONLY_LEFT);
        }
        //ui.action_2D->setChecked(true);
        ////ui.action_3D->setChecked(false);
        break;
    case 0x04:
        logger->info("3D play");
        //ui.action_3D->setChecked(true);
        //ui.action_3D->setChecked(true);
        if (ui.action_3D->isEnabled()) {
            //    3D + (水平、垂直、棋盘)
            mStereoFormat = StereoFormat::STEREO_FORMAT_3D;
            ui.action_3D->setChecked(true);
            ui.playWidget->SetStereoFormat(StereoFormat(STEREO_FORMAT_3D));
            ui.comboBox_src2D_3D2D->setCurrentIndex(0);
            //  intput
            ui.comboBox_3D_input->setEnabled(true);
            ui.actionInputLR->setEnabled(true);
            ui.actionInputRL->setEnabled(true);
            ui.actionInputUD->setEnabled(true);
            //  output
            ui.actionOutputHorizontal->setEnabled(true);
            ui.actionOutputVertical->setEnabled(true);
            ui.actionOutputChess->setEnabled(true);
            ui.actionOutputOnlyLeft->setEnabled(false);

            if (mStereoOutputFormat == StereoOutputFormat::STEREO_OUTPUT_FORMAT_ONLY_LEFT) {
                mStereoOutputFormat = mStereoOutputFormatWhenSwitch2D;
                ui.playWidget->SetStereoOutputFormat(mStereoOutputFormatWhenSwitch2D);
                switch (mStereoOutputFormatWhenSwitch2D) {
                case StereoOutputFormat::STEREO_OUTPUT_FORMAT_HORIZONTAL:
                    ui.actionOutputHorizontal->setChecked(true);
                    break;
                case StereoOutputFormat::STEREO_OUTPUT_FORMAT_VERTICAL:
                    ui.actionOutputVertical->setChecked(true);
                    break;
                case StereoOutputFormat::STEREO_OUTPUT_FORMAT_CHESS:
                    ui.actionOutputChess->setChecked(true);
                    break;
                }
            }
            onStereoFormatChanged(StereoFormat(STEREO_FORMAT_3D), mStereoInputFormat, mStereoOutputFormat);
        }
        break;
    case 0x10:
        logger->info("2D source play");
        if (ui.action_2D->isEnabled()) {
            //    原视频播放
            mStereoFormat = StereoFormat::STEREO_FORMAT_NORMAL_2D;
            ui.action_2D->setChecked(true);
            ui.comboBox_src2D_3D2D->setCurrentIndex(2);
            //  input
            ui.comboBox_3D_input->setEnabled(false);
            ui.actionInputLR->setDisabled(true);
            ui.actionInputRL->setDisabled(true);
            ui.actionInputUD->setDisabled(true);
            //  output
            ui.actionOutputHorizontal->setDisabled(true);
            ui.actionOutputVertical->setDisabled(true);
            ui.actionOutputChess->setDisabled(true);
            ui.actionOutputOnlyLeft->setDisabled(true);

            ui.playWidget->SetStereoFormat(StereoFormat(STEREO_FORMAT_NORMAL_2D));
            onStereoFormatChanged(StereoFormat(STEREO_FORMAT_NORMAL_2D), mStereoInputFormat, mStereoOutputFormat);
        }
        break;
    case 0x05:
        logger->info("0x05, stop");
        on_pushButton_stop_clicked();
        break;
    case 0x06:
        logger->info("0x06, play");
        if (!playController_->isPlaying()) {
            on_pushButton_playPause_clicked();
        }
        break;
    case 0x07:
        logger->info("0x07,pause");
        if (playController_->isPlaying()) {
            on_pushButton_playPause_clicked();
        }
        break;
    case 0x08:
        logger->info("0x08,previous");
        on_pushButton_previous_clicked();
        break;
    case 0x09:
        logger->info("0x09,next");
        on_pushButton_next_clicked();
        break;
    case 0x0A:
        logger->info("fast backward");
        break;
    case 0x0B:
        logger->info("fast forward");
        break;
    case 0x0C:
        logger->info("voleme -");
        replyVolumeDown();
        break;
    case 0x0D:
        logger->info("volume +");
        replyVolumeUp();
        break;
    case 0x0E:
        logger->info("signle loop");
        ui.actionSingleCycle->setChecked(true);
        reply_actionPlayOrder_Selected(ui.actionSinglePlay);
        break;
    case 0x0F:
        logger->info("all loop");
        ui.actionListLoop->setChecked(true);
        reply_actionPlayOrder_Selected(ui.actionListLoop);
        break;
    default:
        logger->info("unknow cmd");
        break;
    }
}

QString MainWindow::readConfig()
{
    ApplicationSettings appSettings;
    appSettings.read_ALL();
    appSettings.read_PlayList();

    return appSettings.ToString();
}

void MainWindow::writeConfig()
{
    logger->info("writeConfig, Format: {}, InputFormat:{}, OutputFormat:{}", (int) mStereoFormat, (int) mStereoInputFormat, (int) mStereoOutputFormat);

    ApplicationSettings appSettings;
    appSettings.write_ALL();
}

QStringList MainWindow::getDeviceInfo()
{
    ////  CPUע
    // QSettings cpu("HKEY_LOCAL_MACHINE\\HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", QSettings::NativeFormat);
    ////  CPU
    // QString m_cpu = cpu.value("ProcessorNameString").toString();
    //
    //   cpu  ID
    QString cpu_id = "";
    unsigned int dwBuf[4] = {0};
    unsigned long long ret = 0;
    getcpuid(dwBuf, 1);
    ret = dwBuf[3];
    ret = ret << 32;

    QString str0 = QString::number(dwBuf[3], 16).toUpper();
    QString str0_1 = str0.rightJustified(8, '0');
    QString str1 = QString::number(dwBuf[0], 16).toUpper();
    QString str1_1 = str1.rightJustified(8, '0');
    // cpu_id = cpu_id + QString::number(dwBuf[0], 16).toUpper();
    cpu_id = str0_1 + str1_1;

    QList<QNetworkInterface> nets = QNetworkInterface::allInterfaces();

    int nCnt = nets.count();
    QString strMacAddr = "";
    for (int i = 0; i < nCnt; i++) {
        if (nets[i].flags().testFlag(QNetworkInterface::IsUp) && nets[i].flags().testFlag(QNetworkInterface::IsRunning)
            && !nets[i].flags().testFlag(QNetworkInterface::IsLoopBack)) {
            //  get mac ip ipv4
            for (int j = 0; j < nets[i].addressEntries().size(); j++) {
                if (nets[i].addressEntries().at(j).ip() != QHostAddress::LocalHost
                    && nets[i].addressEntries().at(j).ip().protocol() == QAbstractSocket::IPv4Protocol) {
                    strMacAddr = nets[i].hardwareAddress();
                }
            }
        }
    }

    logger->info("cpu ID:{}", cpu_id.toStdString());
    logger->info("mac:{}", strMacAddr.toStdString());

    QStringList info;
    info.push_back(cpu_id);
    info.push_back(strMacAddr);

    return info;
}

void MainWindow::getcpuid(unsigned int CPUInfo[4], unsigned int InfoType)
{
#if defined(_MSC_VER)
    #if _MSC_VER >= 1400
        __cpuid((int *) (void *) CPUInfo, (int) (InfoType));
    #else
        getcpuidex(CPUInfo, InfoType, 0);
    #endif
#elif (defined(__GNUC__) || defined(__clang__)) && (defined(__x86_64__) || defined(__i386__))
    __cpuid(InfoType, CPUInfo[0], CPUInfo[1], CPUInfo[2], CPUInfo[3]);
#else
    CPUInfo[0] = CPUInfo[1] = CPUInfo[2] = CPUInfo[3] = 0;
#endif
}

void MainWindow::initLanguage()
{
    static QTranslator *translator;
    if (translator != NULL) {
        qApp->removeTranslator(translator);
        delete translator;
        translator = NULL;
    }
    translator = new QTranslator;

    QString languagePath = QCoreApplication::applicationDirPath();
    switch (GlobalDef::getInstance()->LANGUAGE) {
    case 0:
        languagePath = languagePath + QString("/Resources/language/language_zh_CN.qm");
        break;
    case 1:
        languagePath = languagePath + QString("/Resources/language/language_en.qm");
        break;
    case 2:
        languagePath = languagePath + QString("/Resources/language/language_zh_Hant.qm");
        break;
    }
    if (translator->load(languagePath)) {
        qApp->installTranslator(translator);
    }
    ui.retranslateUi(this);
}

void MainWindow::initUI()
{
    //  init menu
    QPixmap menuPx;
    menuPx.load(GlobalDef::getInstance()->MAIN_WINDOW_LOGO_PATH);
    if (!menuPx.isNull()) {
        ui.pushButton_menu->setIcon(QIcon(menuPx));
    }

    //
    stereoFormatActionGroup = new QActionGroup(this);
    stereoFormatActionGroup->setExclusive(true);
    stereoFormatActionGroup->addAction(ui.action_3D);
    stereoFormatActionGroup->addAction(ui.action_3D_left);
    stereoFormatActionGroup->addAction(ui.action_2D);
    connect(stereoFormatActionGroup, &QActionGroup::triggered, this, &MainWindow::reply_actionStereoFormat_Selected);
    //
    //  input Action
    inputActionGroup = new QActionGroup(this);
    inputActionGroup->setExclusive(true);
    inputActionGroup->addAction(ui.actionInputLR);
    inputActionGroup->addAction(ui.actionInputRL);
    inputActionGroup->addAction(ui.actionInputUD);
    connect(inputActionGroup, &QActionGroup::triggered, this, &MainWindow::reply_actionInput_Selected);

    //  output Action
    outputActionGroup = new QActionGroup(this);
    outputActionGroup->setExclusive(true);
    outputActionGroup->addAction(ui.actionOutputHorizontal);
    outputActionGroup->addAction(ui.actionOutputVertical);
    outputActionGroup->addAction(ui.actionOutputChess);
    outputActionGroup->addAction(ui.actionOutputOnlyLeft);
    connect(outputActionGroup, &QActionGroup::triggered, this, &MainWindow::reply_actionOutput_Selected);

    playOrderActionGroup = new QActionGroup(this);
    playOrderActionGroup->setExclusive(true);
    playOrderActionGroup->addAction(ui.actionSinglePlay);
    playOrderActionGroup->addAction(ui.actionSequentialPlay);
    playOrderActionGroup->addAction(ui.actionRandomlyPlay);
    playOrderActionGroup->addAction(ui.actionSingleCycle);
    playOrderActionGroup->addAction(ui.actionListLoop);
    connect(playOrderActionGroup, &QActionGroup::triggered, this, &MainWindow::reply_actionPlayOrder_Selected);

    // 同步 PlayListManager 的播放模式到 UI
    if (playListManager_) {
        switch (playListManager_->playMode()) {
        case PlayListManager::PlayMode::Sequential:
            // 顺序播放：根据 GlobalDef 判断是单曲播放还是顺序播放
            if (GlobalDef::getInstance()->PLAY_LOOP_ORDER == PlayLoop::PLAYLOOP_SINGLE_PLAY) {
                ui.actionSinglePlay->setChecked(true);
            } else {
                ui.actionSequentialPlay->setChecked(true);
            }
            break;
        case PlayListManager::PlayMode::SingleLoop:
            ui.actionSingleCycle->setChecked(true);
            break;
        case PlayListManager::PlayMode::Random:
            ui.actionRandomlyPlay->setChecked(true);
            break;
        case PlayListManager::PlayMode::Loop:
            ui.actionListLoop->setChecked(true);
            break;
        }
    } else {
        // PlayListManager 还未初始化，使用 GlobalDef 的默认设置
        switch (GlobalDef::getInstance()->PLAY_LOOP_ORDER) {
        case PlayLoop::PLAYLOOP_SINGLE_PLAY:
            ui.actionSinglePlay->setChecked(true);
            break;
        case PlayLoop::PLAYLOOP_SINGLE_CYCLE:
            ui.actionSingleCycle->setChecked(true);
            break;
        case PlayLoop::PLAYLOOP_SEQUENTIAL_PLAY:
            ui.actionSequentialPlay->setChecked(true);
            break;
        case PlayLoop::PLAYLOOP_RANDOMLY_PLAY:
            ui.actionRandomlyPlay->setChecked(true);
            break;
        case PlayLoop::PLAYLOOP_LIST_LOOP:
            ui.actionListLoop->setChecked(true);
            break;
        }
    }

    //  play status
    //connect(ui.switchButton_3D2D, &SwitchButton::statusChanged, this, &MainWindow::reply_switchButton_3D2D_statusChanged);
    {
        QStringList staList;
        staList << QString(tr("3D立体")) << QString(tr("2D左视图")) << QString(tr("2D原视频"));
        ui.comboBox_src2D_3D2D->addItems(staList);
        connect(ui.comboBox_src2D_3D2D, &UpShowComboBox::currentIndexChanged, this, &MainWindow::on_comboBox_src2D_3D2D_currentIndexChanged);
        ui.comboBox_3D_input->setCurrentIndex(0);
    }
    //  input comboBox
    {
        QStringList strList;
        strList << "LR"
                << "RL"
                << "UD";
        ui.comboBox_3D_input->addItems(strList);
        connect(ui.comboBox_3D_input, &UpShowComboBox::currentIndexChanged, this, &MainWindow::on_comboBox_3D_input_currentIndexChanged);
        ui.comboBox_3D_input->setCurrentIndex(0);
    }

    //connect(ui.switchButton_LR_RL, &SwitchButton::statusChanged, this, &MainWindow::reply_switchButton_LRRL_statusChanged);
    //ui.switchButton_LR_RL->setTextOn(QString(tr("LR")));
    //ui.switchButton_LR_RL->setTextOff(QString(tr("RL")));
    // ui.switchButton_LR_RL->setChecked(true);

    //  volume
    ui.pushButton_volume->setChecked(GlobalDef::getInstance()->B_VOLUME_MUTE);
    ui.horizontalSlider_volume->setValue(GlobalDef::getInstance()->VOLUME_VAULE);

    //  play list
    connect(ui.playWidget->butWidget, &FloatButton::signals_playListShow_clicked, this, &MainWindow::reply_playlistShowBut_Clicked);
    ((QTabBar *) (ui.tabWidget_playList->tabBar()))->setTabButton(ui.tabWidget_playList->indexOf(ui.tab_playListDef), QTabBar::RightSide, NULL);
    drawPlayList();
    ui.tabWidget_playList->setCurrentIndex(GlobalDef::getInstance()->PLAY_LIST_DATA.playlist_current_index);
    connect(
        ui.listWidget_playlist,
        SIGNAL(currentItemChanged(QListWidgetItem *, QListWidgetItem *)),
        this,
        SLOT(replayCurrentItemChanged(QListWidgetItem *, QListWidgetItem *)));
    connect(ui.listWidget_playlist, &DropListWidget::signalDropEvent, this, &MainWindow::replayListWidgetDrop);
    // if (GlobalDef::getInstance()->PLAY_LIST_DATA.playlist_current_index == 0) {
    //     ui.listWidget_playlist->setCurrentItem(ui.listWidget_playlist->item(GlobalDef::getInstance()->PLAY_LIST_DATA.playlist_current_video));
    // }
    // else {
    //     ((PlayListPage*)ui.tabWidget_playList->currentWidget())->getListWidget()->setCurrentItem(((PlayListPage*)ui.tabWidget_playList->currentWidget())->getListWidget()->item(GlobalDef::getInstance()->PLAY_LIST_DATA.playlist_current_video));
    // }

    QPushButton *pushButton_playList_tabBar_addList = new QPushButton();
    pushButton_playList_tabBar_addList->setIcon(QIcon(":/MainWindow/Resources/theme/black/add-playlist.svg"));
    pushButton_playList_tabBar_addList->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    ui.tabWidget_playList->setCornerWidget(pushButton_playList_tabBar_addList);
    pushButton_playList_tabBar_addList->setStyleSheet("margin-right:2px;width:24px;height:22px;");
    connect(pushButton_playList_tabBar_addList, &QPushButton::clicked, this, &MainWindow::reply_pushButton_pushButton_playList_tabBar_addList_clicked);
    pushButton_playList_tabBar_addList->setToolTip(QString(tr("添加列表")));

    connect(ui.horizontalSlider_volume, &QSlider::valueChanged, this, &MainWindow::onVolumeValueChanged);
    connect(ui.playWidget, &StereoVideoWidget::updatePlayProcess, this, &MainWindow::onUpdatePlayProcess);
    connect(ui.horizontalSlider_playProgress, &QSlider::valueChanged, this, &MainWindow::onPlayProcessValueChanged);
    connect(ui.horizontalSlider_playProgress, &CustomSlider::customSliderClicked, this, &MainWindow::onPlaySliderCustomClicked, Qt::DirectConnection);

    // NOTE:暂时不响应这些消息，但对于拖动来说，应该也要支持（暂先不实现）
    // connect(ui.horizontalSlider_playProgress, &QSlider::sliderPressed, this,
    // &MainWindow::onPlaySliderMousePressEvent); connect(ui.horizontalSlider_playProgress, &QSlider::sliderMoved, this,
    // &MainWindow::onPlaySliderMouseMoveEvent); connect(ui.horizontalSlider_playProgress, &QSlider::sliderReleased,
    // this, &MainWindow::onPlaySliderMouseReleaseEvent);

    // 初始化时，未开视频，playProgress就不可拖动
    ui.horizontalSlider_playProgress->setEnabled(false);
    ui.horizontalSlider_volume->setEnabled(false);

    mDrawWidget = new DrawWidget(ui.playWidget, Qt::FramelessWindowHint);
    connect(mDrawWidget, &DrawWidget::stereoRegionUpdate, this, &MainWindow::onStereoRegionUpdate);
    mDrawWidget->hide();

    ////    todo   for test
    //mDrawWidget->resize(800, 600);
    //mDrawWidget->move(0, 0);

    //  HotKey
    initHotKey();

    //  AdvertisementWidget
    int adv_left_width, play_3d_width, adv_right_width;
    if (GlobalDef::getInstance()->ADV_TYPE_LEFT == AdvType::ADV_TYPE_NULL) {
        ui.advertisementWidget_left->hide();
        adv_left_width = 0;
    } else {
        adv_left_width = GlobalDef::getInstance()->ADV_WIDTH_LEFT;
    }

    if (GlobalDef::getInstance()->ADV_TYPE_RIGHT == AdvType::ADV_TYPE_NULL) {
        ui.advertisementWidget_right->hide();
        adv_right_width = 0;
    } else {
        adv_right_width = GlobalDef::getInstance()->ADV_WIDTH_RIGHT;
    }
    play_3d_width = GlobalDef::getInstance()->PLAY_3D_VIEW_WIDTH == 0 ? ((adv_left_width + adv_right_width) / 2) : GlobalDef::getInstance()->PLAY_3D_VIEW_WIDTH;
    play_3d_width = play_3d_width == 0 ? 2 : play_3d_width;

    ui.horizontalLayout_2->setStretchFactor(ui.advertisementWidget_left, adv_left_width);
    ui.horizontalLayout_2->setStretchFactor(ui.widget_3dplay, play_3d_width);
    ui.horizontalLayout_2->setStretchFactor(ui.advertisementWidget_right, adv_right_width);

    if (GlobalDef::getInstance()->ADV_TYPE_LEFT != AdvType::ADV_TYPE_NULL) {
        ui.advertisementWidget_left->init(GlobalDef::getInstance()->ADV_TYPE_LEFT == AdvType::ADV_TYPE_IMAGE, GlobalDef::getInstance()->ADV_SOURCE_PATH_LEFT);
    }

    if (GlobalDef::getInstance()->ADV_TYPE_RIGHT != AdvType::ADV_TYPE_NULL) {
        ui.advertisementWidget_right->init(GlobalDef::getInstance()->ADV_TYPE_RIGHT == AdvType::ADV_TYPE_IMAGE, GlobalDef::getInstance()->ADV_SOURCE_PATH_RIGHT);
    }

    //  摄像头相关按钮（隐藏)
    ui.pushButton_hs->setHidden(true);
    ui.pushButton_a->setHidden(true);
}

void MainWindow::registerSysNotification()
{
#ifdef Q_OS_WIN
    // 注册指定窗口以接收会话更改通知，获取锁屏，解锁，登录，注销等消息
    bool ret = WTSRegisterSessionNotification((HWND) this->winId(), NOTIFY_FOR_THIS_SESSION);
    qDebug() << "会话事件通知注册" << (ret ? "成功" : "失败");
    // 注册以便在系统暂停或恢复时接收通知，最低支持 win8
    HPOWERNOTIFY res = RegisterSuspendResumeNotification((HWND) this->winId(), DEVICE_NOTIFY_WINDOW_HANDLE);
    qDebug() << "电源事件通知注册" << (res != NULL ? "成功" : "失败");
#endif
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    MSG *msg = (MSG *) message;
    switch (msg->message) {
    case WM_WTSSESSION_CHANGE:
    {
        switch (msg->wParam) {
        case WTS_SESSION_LOCK:
            logger->info("锁屏, Should Pause Playing");
            playerIsPlayingBeforeScreenLockOrPowerSleep = playController_->isPlaying();
            if (playerIsPlayingBeforeScreenLockOrPowerSleep) {
                bool shouldPause = true;
                playController_->pause();
                ui.playWidget->PlayPause(shouldPause);
            }
            break;
        case WTS_SESSION_UNLOCK:
            logger->info("解锁, Should Resume Playing");
            if (playerIsPlayingBeforeScreenLockOrPowerSleep) {
                if (playController_->isPaused()) {
                    playController_->play();
                    ui.playWidget->PlayPause(false);
                }
            }
            break;
        case WTS_SESSION_LOGON:
            qDebug() << "登录";
            break;
        case WTS_SESSION_LOGOFF:
            qDebug() << "注销";
            break;
        case WTS_SESSION_REMOTE_CONTROL:
            qDebug() << "被远程控制";
            break;
        default:
            break;
        }
        break;
    }
    case WM_POWERBROADCAST: {
        switch (msg->wParam) {
        case PBT_APMSUSPEND:
            logger->info("系统进入休眠状态");
            playerIsPlayingBeforeScreenLockOrPowerSleep = playController_->isPlaying();
            if (playerIsPlayingBeforeScreenLockOrPowerSleep) {
                bool shouldPause = true;
                playController_->pause();
                ui.playWidget->PlayPause(shouldPause);
            }
            break;
        case PBT_APMRESUMEAUTOMATIC:
            logger->info("系统从休眠状态恢复");
            if (playerIsPlayingBeforeScreenLockOrPowerSleep) {
                if (playController_->isPaused()) {
                    playController_->play();
                    ui.playWidget->PlayPause(false);
                }
                if (ui.playWidget && ui.playWidget->IsRendering()) {
                    ui.playWidget->update();
                    logger->info("System resumed, forced render update to check OpenGL context recovery");
                }
            }
            break;
        case PBT_APMPOWERSTATUSCHANGE:
            qDebug() << "系统电源状态更改";
            break;
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::openFile()
{
    QString filepath = QFileDialog::getOpenFileName(
        this,
        QString(tr("请选择播放文件")),
        "./",
        //"video(*.mp4 *.flv *.f4v *.webm *.m4v *.mov *.3gp *.3g2 *.rm *.rmvb *.wmv *.avi *.asf *.mpg *.mpeg *.mpe *.ts *.div *.dv *.divx *.vob *.mkv)"
        "video(*.mp4 *.flv *.f4v *.webm *.m4v *.mov *.3gp *.3g2 *.rm *.rmvb *.wmv *.avi *.asf *.mpg *.mpeg *.mpe *.ts *.div *.dv *.divx *.vob *.mkv *.wzmp4 "
        "*.wzavi *.wzmkv *.wzmov *.wzmpg)");
    if (!filepath.isEmpty()) {
        currentElapsedInSeconds_ = 0;
        openPath(filepath);
    }
}

/**
 * @brief      非阻塞延时
 * @param msec 延时毫秒
 */
void MainWindow::sleepMsec(int msec)
{
    if (msec <= 0)
        return;
    QEventLoop loop;                               //定义一个新的事件循环
    QTimer::singleShot(msec, &loop, SLOT(quit())); //创建单次定时器，槽函数为事件循环的退出函数
    loop.exec();                                   //事件循环开始执行，程序会卡在这里，直到定时时间到，本循环被退出
}

bool MainWindow::checkFileIsVideo(QString path)
{
    // 8 most common video file formats,    .wzmp4, .wzmkv
    QVector<QString> suffixList{"mp4", "avi", "mov", "wmv", "mkv", "flv", "mpg", "3gp"};

    for (auto suffix : suffixList) {
        if (path.endsWith(suffix, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

int MainWindow::openPath(QString path, bool addPlayList)
{
    if (mIsNotRegistered) {
        logger->error("soft is NOT registrated.");
        return -9999;
    }
    logger->info("MainWindow.openPath:{}", path.toUtf8().constData());
    if (false == QFile::exists(path)) {
        logger->warn("filename: {}   not exists. ", path.toStdString());
        return -1;
    }
    if (false == checkFileIsVideo(path)) {
        logger->warn("filename:{} is not video file.", path.toStdString());
        return -1;
    }

    // 优化：只在有视频正在播放时才等待停止
    // 首次打开视频时，isOpened() 返回 false，直接跳过等待，避免不必要的延迟
    bool switchStop = false;

    // 只有在真正有视频打开时才需要等待停止
    if (playController_ && playController_->isOpened()) {
        logger->info("Previous video is opened, waiting for it to stop before opening new video");

        const int MAX_WAIT_ATTEMPTS = 40;   // 增加等待次数，最多等待 40 次（2秒）
        const int SHORT_SLEEP_MS = 50;      // 短 sleep 时间（50ms）
        const int MAX_TOTAL_WAIT_MS = 5000; // 增加最大总等待时间（5秒）

        int waitAttempts = 0;
        auto waitStartTime = std::chrono::steady_clock::now();

        // 先调用 stop() 停止当前播放
        if (playController_) {
            playController_->stop();
        }
        switchStop = true;

        while (false == playController_->isStopped() && waitAttempts < MAX_WAIT_ATTEMPTS) {
            // 检查是否超时
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - waitStartTime).count();
            if (elapsed > MAX_TOTAL_WAIT_MS) {
                logger->warn("Stop wait timeout after {} ms, forcing continue (video thread may be stuck)", elapsed);
                break;
            }

            // 使用较短的 sleep 时间，提高响应速度
            sleepMsec(SHORT_SLEEP_MS);
            waitAttempts++;

            // 每10次尝试打印一次日志，避免日志过多
            if (waitAttempts % 10 == 0) {
                logger->debug("Waiting for movie to stop, attempts: {}/{}, elapsed: {}ms", waitAttempts, MAX_WAIT_ATTEMPTS, elapsed);
            }
        }

        if (waitAttempts >= MAX_WAIT_ATTEMPTS) {
            logger->warn("Stop wait reached max attempts ({}), forcing continue (video thread may need more time to exit)", MAX_WAIT_ATTEMPTS);
        }

        // 额外等待检查
        if (false == playController_->isStopped()) {
            logger->warn("Movie still not stopped after initial wait, waiting additional time...");
            const int ADDITIONAL_WAIT_MS = 500; // 增加额外等待时间到 500ms
            sleepMsec(ADDITIONAL_WAIT_MS);

            if (false == playController_->isStopped()) {
                logger->error("Movie still not stopped after additional wait! Video thread may be stuck. Forcing continue anyway.");
            } else {
                logger->info("Movie stopped after additional wait");
            }
        } else {
            logger->info("Movie stopped successfully, no additional wait needed");
        }
    } else {
        logger->debug("No video is currently opened, skipping stop wait (first time opening or already stopped)");
    }

    //判断当前文件是否与上一次相同，若相同，则在当前字幕为空时把上一次的字幕也导入
    if (mLastMovieFn == path) {
        if (GlobalDef::getInstance()->USER_SELECTED_SUBTITLE_FILENAME.isEmpty()) {
            logger->info("LastMoveFn == path, && USER_SELECTED_SUBTITLE_FILENAME is empty. upload with the last subtitle");
            GlobalDef::getInstance()->USER_SELECTED_SUBTITLE_FILENAME = mLastSubtitleFn;
        } else {
            logger->info("LastMoveFn == path, && USER_SELECTED_SUBTITLE_FILENAME is NOT empty, user selected a new subtitle.");
        }
    } else {
        // 切换到新视频文件时，清除上一个视频的字幕
        if (ui.playWidget) {
            logger->info(
                "Switching to new video file, stopping previous subtitle. LastMovie: {}, NewMovie: {}",
                mLastMovieFn.toUtf8().constData(),
                path.toUtf8().constData());
            // 停止并清除字幕
            ui.playWidget->StopSubtitle();
        }

        if (ui.playWidget->IsStereoRegion()) {
            ui.playWidget->CancelStereoRegion();
            logger->info(
                "LastMovieFn{} != path{} && Last is StereoRegion, should cancelStereoRegion when playing new Movie.",
                mLastMovieFn.toUtf8().constData(),
                path.toUtf8().constData());
        }
    }

    logger->info("start Actual Open file:{}", path.toUtf8().constData());
    currentElapsedInSeconds_ = 0;
    
    // 切换到视频文件 Widget（如果当前在 Camera 模式）
    switchToVideoFile();
    
    bool openOk = playController_->open(path);
    if (!openOk) {
        logger->error("playController_->open failed");
        return -1;
    }
    mCurMovieFilename = path;

    // 重构，以Movie为主，ffmpegView仅做显示，逻辑部分移至Movie（原来FFMPEGView也仅是透传，真正功能实现在Movie）
    // 对文件格式的判断上移至主程序，不在View中做(VIEW仅保留与渲染相关代码)
    QFileInfo fi(path);
    QString filename = fi.fileName();

    //      新播放控制逻辑
    if (filename.contains("WZLR")) {
        //  stereo
        //  input -- stereo 3D or 3D_left
        ui.comboBox_3D_input->setEnabled(true);
        ui.actionInputLR->setEnabled(true);
        ui.actionInputRL->setEnabled(true);
        ui.actionInputUD->setEnabled(true);
        //  input -- checked
        ui.actionInputLR->setChecked(true);
        reply_actionInput_Selected(ui.actionInputLR);

        //  output -- stereo 3D or 3D_left
        if (filename.contains("WZLR2")) {
            //  3D-ONLY_LEFT  LR
            ui.actionOutputHorizontal->setEnabled(false);
            ui.actionOutputVertical->setEnabled(false);
            ui.actionOutputChess->setEnabled(false);
            ui.actionOutputOnlyLeft->setEnabled(true);

            ui.action_3D_left->setChecked(true);
            ui.comboBox_src2D_3D2D->setCurrentIndex(1);
        } else {
            //  3D-Stereo
            ui.actionOutputHorizontal->setEnabled(true);
            ui.actionOutputVertical->setEnabled(true);
            ui.actionOutputChess->setEnabled(true);
            ui.actionOutputOnlyLeft->setEnabled(false);

            ui.action_3D->setChecked(true);
            ui.comboBox_src2D_3D2D->setCurrentIndex(0);
        }
    } else if (filename.contains("WZRL")) {
        //  stereo
        //  input -- stereo 3D or 3D_left
        ui.comboBox_3D_input->setEnabled(true);
        ui.actionInputLR->setEnabled(true);
        ui.actionInputRL->setEnabled(true);
        ui.actionInputUD->setEnabled(true);
        //  input -- checked
        ui.actionInputRL->setChecked(true);
        reply_actionInput_Selected(ui.actionInputRL);

        //  output -- stereo 3D or 3D_left
        if (filename.contains("WZRL2")) {
            //  3D-ONLY_LEFT  LR
            ui.actionOutputHorizontal->setEnabled(false);
            ui.actionOutputVertical->setEnabled(false);
            ui.actionOutputChess->setEnabled(false);
            ui.actionOutputOnlyLeft->setEnabled(true);

            ui.action_3D_left->setChecked(true);
            ui.comboBox_src2D_3D2D->setCurrentIndex(1);
        } else {
            //  3D-Stereo
            ui.actionOutputHorizontal->setEnabled(true);
            ui.actionOutputVertical->setEnabled(true);
            ui.actionOutputChess->setEnabled(true);
            ui.actionOutputOnlyLeft->setEnabled(false);

            ui.action_3D->setChecked(true);
            ui.comboBox_src2D_3D2D->setCurrentIndex(0);
        }
    } else if (filename.contains("WZUD")) {
        //  stereo
        //  input -- stereo 3D or 3D_left
        ui.comboBox_3D_input->setEnabled(true);
        ui.actionInputLR->setEnabled(true);
        ui.actionInputRL->setEnabled(true);
        ui.actionInputUD->setEnabled(true);
        //  input -- checked
        ui.actionInputUD->setChecked(true);
        reply_actionInput_Selected(ui.actionInputUD);

        //  output -- stereo 3D or 3D_left
        if (filename.contains("WZUD2")) {
            //  3D-ONLY_LEFT  UD
            ui.actionOutputHorizontal->setEnabled(false);
            ui.actionOutputVertical->setEnabled(false);
            ui.actionOutputChess->setEnabled(false);
            ui.actionOutputOnlyLeft->setEnabled(true);

            ui.action_3D_left->setChecked(true);
            ui.comboBox_src2D_3D2D->setCurrentIndex(1);
        } else {
            //  3D-Stereo
            ui.actionOutputHorizontal->setEnabled(true);
            ui.actionOutputVertical->setEnabled(true);
            ui.actionOutputChess->setEnabled(true);
            ui.actionOutputOnlyLeft->setEnabled(false);

            ui.action_3D->setChecked(true);
            ui.comboBox_src2D_3D2D->setCurrentIndex(0);
        }
    } else {
        //  原视频播放--真2D
        ui.action_2D->setChecked(true);
        ui.comboBox_src2D_3D2D->setCurrentIndex(2);
        //  input -- 2D source
        ui.comboBox_3D_input->setDisabled(true);
        ui.actionInputLR->setDisabled(true);
        ui.actionInputRL->setDisabled(true);
        ui.actionInputUD->setDisabled(true);
        //  output -- 2D source
        ui.actionOutputHorizontal->setDisabled(true);
        ui.actionOutputVertical->setDisabled(true);
        ui.actionOutputChess->setDisabled(true);
        ui.actionOutputOnlyLeft->setDisabled(true);
    }
    //-------------------------------------------------------------------------------------------

    //更新对应的UI选项
    switch (mStereoInputFormat) {
    case StereoInputFormat::STEREO_INPUT_FORMAT_LR:
    default:
        ui.actionInputLR->setChecked(true);
        ui.comboBox_3D_input->setCurrentIndex(0);
        break;
    case StereoInputFormat::STEREO_INPUT_FORMAT_RL:
        ui.actionInputRL->setChecked(true);
        ui.comboBox_3D_input->setCurrentIndex(1);
        break;
    case StereoInputFormat::STEREO_INPUT_FORMAT_UD:
        ui.actionInputUD->setChecked(true);
        ui.comboBox_3D_input->setCurrentIndex(2);
        break;
    }

    switch (mStereoOutputFormat) {
    case StereoOutputFormat::STEREO_OUTPUT_FORMAT_HORIZONTAL:
        ui.actionOutputHorizontal->setChecked(true);
        break;
    case StereoOutputFormat::STEREO_OUTPUT_FORMAT_VERTICAL:
        ui.actionOutputVertical->setChecked(true);
        break;
    case StereoOutputFormat::STEREO_OUTPUT_FORMAT_CHESS:
        ui.actionOutputChess->setChecked(true);
        break;
    case StereoOutputFormat::STEREO_OUTPUT_FORMAT_ONLY_LEFT:
        ui.actionOutputOnlyLeft->setChecked(true);
        break;
    }

    ui.playWidget->SetRenderInputSource(RenderInputSource::RIS_VIDEO_FILE);
    //  playPause
    ui.pushButton_playPause->setChecked(true);
    ui.horizontalSlider_playProgress->setEnabled(true);
    ui.horizontalSlider_volume->setEnabled(true);
    //  set TotalTime
    int64_t mvTotalTimeMs = playController_->getDurationMs();
    int mvTotalTimeSecond = static_cast<int>(mvTotalTimeMs / 1000);
    
    // 如果时长为0，可能是还没有正确获取，使用延迟设置
    if (mvTotalTimeSecond <= 0) {
        logger->warn("openPath: Video duration is 0 or negative ({} ms), will retry later", mvTotalTimeMs);
        // 使用一个临时值，稍后会在 onUpdatePlayProcess 中更新
        mvTotalTimeSecond = 1; // 至少设置为1秒，避免进度条异常
    }
    
    int totalHour = mvTotalTimeSecond / 3600;
    int totalMinute = (mvTotalTimeSecond - totalHour * 3600) / 60;
    int totalSecond = mvTotalTimeSecond % 60;
    QString strTotalTime
        = QString("%1:%2:%3").arg(totalHour, 2, 10, QLatin1Char('0')).arg(totalMinute, 2, 10, QLatin1Char('0')).arg(totalSecond, 2, 10, QLatin1Char('0'));
    ui.label_totalTime->setText(strTotalTime);
    //  set play progress bar
    ui.horizontalSlider_playProgress->setMaximum(mvTotalTimeSecond);
    ui.horizontalSlider_playProgress->setSingleStep(1);
    
    logger->info("openPath: Set progress bar max to {} seconds ({} ms)", mvTotalTimeSecond, mvTotalTimeMs);
    //  set volume
    // float volume = ui.playWidget->GetVolume();
    // ui.horizontalSlider_volume->setValue(volume * 100);
    float frameRate = playController_->getVideoFrameRate();
    if (addPlayList) {
        QStringList addList;
        addList.append(path);
        logger->info("PLAY_LIST_DATA playlist_current_index:{} ", GlobalDef::getInstance()->PLAY_LIST_DATA.playlist_current_index);
        updatePlayList(addList, GlobalDef::getInstance()->PLAY_LIST_DATA.playlist_current_index);
    }

    // 暂时隐藏旧的 FFmpegView，避免冲突
    // StereoVideoWidget 已经通过 setPlayController 连接到 PlayController
    // 不需要额外的 test window，StereoVideoWidget 会直接显示视频
    ui.playWidget->show();

    // 启动渲染（只启动状态更新定时器，不启动 renderTimer_）
    // 渲染由 VideoThread 通过 writeVideo() -> updateGL() 驱动，避免双重驱动冲突
    ui.playWidget->StartRendering(mStereoFormat, mStereoInputFormat, mStereoOutputFormat, frameRate);

    int seekPos = 0;
    loadSubtitle(mCurMovieFilename, seekPos);

    //  update playList currenteIndex
    if (playListManager_) {
        int currentPlayListIndex = ui.tabWidget_playList->currentIndex();
        int listSize = playListManager_->getPlayListSize(currentPlayListIndex);
        for (int i = 0; i < listSize; ++i) {
            if (playListManager_->getVideoPath(i, currentPlayListIndex) == path) {
                // 设置当前播放项（不发送信号，避免与openPath()重复触发）
                playListManager_->setCurrentVideo(i, currentPlayListIndex, false);
                // 更新UI
                if (currentPlayListIndex == 0) {
                    ui.listWidget_playlist->clearSelection();
                    ui.listWidget_playlist->setCurrentRow(i);
                } else {
                    auto *widget = dynamic_cast<PlayListPage *>(ui.tabWidget_playList->currentWidget());
                    if (widget && widget->getListWidget()) {
                        widget->getListWidget()->clearSelection();
                        widget->getListWidget()->setCurrentRow(i);
                    }
                }
                break;
            }
        }
    } 

    return 0;
}

void MainWindow::updatePlayList(QStringList addList, int playListIndex)
{
    // 使用PlayListManager优化后的逻辑
    if (!playListManager_) {
        logger->error("MainWindow::updatePlayList: playListManager_ is null, cannot update playlist");
        return;
    }
    for (const QString &videoPath : addList) {
        QString fileType = videoPath.right((videoPath.size() - videoPath.lastIndexOf(".")));
        if (!GlobalDef::getInstance()->VIDEO_FILE_TYPE.contains(fileType, Qt::CaseInsensitive)) {
            continue;
        }

        if (playListManager_->addVideoToPlayList(videoPath, playListIndex)) {
            // 更新UI
            QString mvPath = videoPath;
            mvPath = mvPath.replace("\\", "/");
            QStringList mvPathArr = mvPath.split("/");
            QString displayName = QString::number(playListManager_->getPlayListSize(playListIndex)) + "." + mvPathArr[mvPathArr.size() - 1];
            
            if (playListIndex == 0) {
                ui.listWidget_playlist->addItem(displayName);
            } else {
                ((PlayListPage *) ui.tabWidget_playList->widget(playListIndex))->getListWidget()->addItem(displayName);
            }
        }
    }

    // 保存播放列表
    ApplicationSettings mWZSetting;
    mWZSetting.write_PlayList();
}

void MainWindow::drawPlayList()
{
    ui.listWidget_playlist->clear();
    for (int i = 0; i < GlobalDef::getInstance()->PLAY_LIST_DATA.play_list.size(); ++i) {
        PlayListPage *itemPage = nullptr;
        if (i > 0) {
            itemPage = new PlayListPage(this, ui.tabWidget_playList);
            ui.tabWidget_playList->addTab(itemPage, GlobalDef::getInstance()->PLAY_LIST_DATA.play_list[i].list_name);
        }
        for (int j = 0; j < GlobalDef::getInstance()->PLAY_LIST_DATA.play_list[i].video_list.size(); ++j) {
            if (i == 0) {
                QString mvPath = GlobalDef::getInstance()->PLAY_LIST_DATA.play_list[i].video_list[j].video_path;
                mvPath = mvPath.replace("\\", "/");
                QStringList mvPathArr = mvPath.split("/");
                ui.listWidget_playlist->addItem(QString::number(j + 1) + "." + mvPathArr[mvPathArr.size() - 1]);
            } else {
                QString mvPath = GlobalDef::getInstance()->PLAY_LIST_DATA.play_list[i].video_list[j].video_path;
                mvPath = mvPath.replace("\\", "/");
                QStringList mvPathArr = mvPath.split("/");
                itemPage->getListWidget()->addItem(QString::number(j + 1) + "." + mvPathArr[mvPathArr.size() - 1]);
            }
        }
    }
}

QString MainWindow::getWGText(QString widgetText)
{
    QStringList strList = widgetText.split("  ");
    if (strList.size() > 0) {
        QString rStr = strList[0];
        QString strFill = QString("").fill(QChar(' '), 12 - strList[0].size());
        rStr = rStr + strFill;
        return rStr;
    }
    return QString("").fill(QChar(' '), 12);
}

//    windows
void MainWindow::on_pushButton_min_clicked()
{
    this->showMinimized();
}

void MainWindow::on_pushButton_max_clicked()
{
    if (mWindowSizeState == WINDOW_MINIMIZED) {
        this->showMaximized();
        mWindowSizeState = WINDOW_MAXIMIZED;
        GlobalDef::getInstance()->B_WINDOW_SIZE_MAXIMIZED = true;
    } else if (mWindowSizeState == WINDOW_NORMAL) {
        this->showMaximized();
        mWindowSizeState = WINDOW_MAXIMIZED;
        GlobalDef::getInstance()->B_WINDOW_SIZE_MAXIMIZED = true;
    } else if (mWindowSizeState == WINDOW_MAXIMIZED) {
        resize(GlobalDef::getInstance()->MIN_WINDOW_WIDTH, GlobalDef::getInstance()->MIN_WINDOW_HEIGHT);
        if (GlobalDef::getInstance()->MIN_WINDOW_POSITION_X % 2 == 0) {
            GlobalDef::getInstance()->MIN_WINDOW_POSITION_X = GlobalDef::getInstance()->MIN_WINDOW_POSITION_X + 1;
        }
        if (GlobalDef::getInstance()->MIN_WINDOW_POSITION_Y % 2 == 0) {
            GlobalDef::getInstance()->MIN_WINDOW_POSITION_Y = GlobalDef::getInstance()->MIN_WINDOW_POSITION_Y + 1;
        }
        move(GlobalDef::getInstance()->MIN_WINDOW_POSITION_X, GlobalDef::getInstance()->MIN_WINDOW_POSITION_Y);
        mWindowSizeState = WINDOW_NORMAL;
        GlobalDef::getInstance()->B_WINDOW_SIZE_MAXIMIZED = false;
    } else if (mWindowSizeState == WINDOW_FULLSCREEN) {
        ui.widget_title->show();
        ui.widget_playControl->show();
        if (GlobalDef::getInstance()->B_WINDOW_SIZE_MAXIMIZED) {
            this->showMaximized();
            ui.verticalLayout->insertWidget(0, ui.widget_title);
            ui.widget_playList->show();
            ui.verticalLayout_4->insertWidget(1, ui.widget_playControl);
            mWindowSizeState = WINDOW_MAXIMIZED;
            //GlobalDef::getInstance()->B_WINDOW_SIZE_MAXIMIZED = true;
        } else {
            this->showNormal();
            ui.verticalLayout->insertWidget(0, ui.widget_title);
            ui.widget_playList->show();
            ui.verticalLayout_4->insertWidget(1, ui.widget_playControl);
            resize(GlobalDef::getInstance()->MIN_WINDOW_WIDTH, GlobalDef::getInstance()->MIN_WINDOW_HEIGHT);
            if (GlobalDef::getInstance()->MIN_WINDOW_POSITION_X % 2 == 0) {
                GlobalDef::getInstance()->MIN_WINDOW_POSITION_X = GlobalDef::getInstance()->MIN_WINDOW_POSITION_X + 1;
            }
            if (GlobalDef::getInstance()->MIN_WINDOW_POSITION_Y % 2 == 0) {
                GlobalDef::getInstance()->MIN_WINDOW_POSITION_Y = GlobalDef::getInstance()->MIN_WINDOW_POSITION_Y + 1;
            }
            move(GlobalDef::getInstance()->MIN_WINDOW_POSITION_X, GlobalDef::getInstance()->MIN_WINDOW_POSITION_Y);

            mWindowSizeState = WINDOW_NORMAL;
            //GlobalDef::getInstance()->B_WINDOW_SIZE_MAXIMIZED = false;
        }
    }

    logger->info("Shell_TrayWnd show when on_pushButton_max_clicked");
#ifdef Q_OS_WIN
    HWND handle = FindWindowA("Shell_TrayWnd", "");
    ShowWindow(handle, SW_SHOW);
#endif
}

void MainWindow::on_pushButton_fullScreen_clicked()
{
    if (mWindowSizeState == WINDOW_FULLSCREEN) {
        // 原状态 为Fullscreen, 点击fullscreen按钮后，切换回 原状态（是原normal or 原maximized取决于GlobalDef::getInstace()->WINDOW_SIZE, 这个变量可以认为 是类似于 lastState)
        setWindowFlags(Qt::CustomizeWindowHint);
        ui.widget_title->show();
        ui.widget_playControl->show();

        if (GlobalDef::getInstance()->B_WINDOW_SIZE_MAXIMIZED) {
            this->showMaximized();
            ui.verticalLayout->insertWidget(0, ui.widget_title);
            ui.widget_playList->show();
            ui.verticalLayout_4->insertWidget(1, ui.widget_playControl);
            mWindowSizeState = WINDOW_MAXIMIZED;
        } else {
            this->showNormal();
            ui.verticalLayout->insertWidget(0, ui.widget_title);
            ui.widget_playList->show();
            ui.verticalLayout_4->insertWidget(1, ui.widget_playControl);
            resize(GlobalDef::getInstance()->MIN_WINDOW_WIDTH, GlobalDef::getInstance()->MIN_WINDOW_HEIGHT);
            if (GlobalDef::getInstance()->MIN_WINDOW_POSITION_X % 2 == 0) {
                GlobalDef::getInstance()->MIN_WINDOW_POSITION_X = GlobalDef::getInstance()->MIN_WINDOW_POSITION_X + 1;
            }
            if (GlobalDef::getInstance()->MIN_WINDOW_POSITION_Y % 2 == 0) {
                GlobalDef::getInstance()->MIN_WINDOW_POSITION_Y = GlobalDef::getInstance()->MIN_WINDOW_POSITION_Y + 1;
            }
            move(GlobalDef::getInstance()->MIN_WINDOW_POSITION_X, GlobalDef::getInstance()->MIN_WINDOW_POSITION_Y);
            mWindowSizeState = WINDOW_NORMAL;
        }
        // 还原之前隐藏的任务栏
        logger->info("Shell_TrayWnd show when Not in Fullscreen Mode");
#ifdef Q_OS_WIN
        HWND handle = FindWindowA("Shell_TrayWnd", "");
        ShowWindow(handle, SW_SHOW);
#endif
    } else {
        //  Normal/Max 状态切  Fullscreen 状态
        //  //this->setWindowState(Qt::WindowFullScreen); //全屏GLWidget显示会有BUG

        //qtbug report: https://bugreports.qt.io/browse/QTBUG-41309, Fullscreen显示时，OpenGLWidget会影响其他widget渲染,使用官方推荐的方法暂时规避
        this->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);

        switch (mWindowSizeState) {
        case WINDOW_NORMAL:
        case WINDOW_MINIMIZED:
        default: {
            logger->info("Switch from Window_NORMAL to Fullscreen");
            //logger->info("Shell_TrayWnd show when Not in Fullscreen Mode");
            //HWND handle = FindWindowA("Shell_TrayWnd", "");
            //ShowWindow(handle, SW_HIDE);

            logger->info(
                "max->fullscreen / normal->fullscreen, winState(old):{}, currentScreen.name:{}, geometry:{}x{}, {}x{}",
                (int) mWindowSizeState,
                screen()->name().toStdString(),
                screen()->geometry().x(),
                screen()->geometry().y(),
                screen()->geometry().width(),
                screen()->geometry().height());
            QRect screenGeometry = screen()->geometry();
            this->move(screenGeometry.left() - 1, screenGeometry.top() - 1);
            this->resize(screenGeometry.width() + 2, screenGeometry.height() + 2);
            this->show();
            this->raise();

            ui.playWidget->resize(screenGeometry.width(), screenGeometry.height());
            ui.verticalLayout->removeWidget(ui.widget_title);
            ui.widget_title->resize(screenGeometry.width(), ui.widget_title->height());
            ui.widget_title->move(0, 0);
            ui.widget_title->raise();

            if (!ui.widget_playList->isHidden()) {
                ui.widget_playList->hide();
            }

            ui.verticalLayout_4->removeWidget(ui.widget_playControl);
            ui.widget_playControl->resize(screenGeometry.width(), ui.widget_playControl->height());
            ui.widget_playControl->move(0, screenGeometry.height() - ui.widget_playControl->height());
            // ui.widget_playControl->hide();
            mWindowSizeState = WINDOW_FULLSCREEN;

            // 把当前窗口置在TopLevel Highest
#ifdef Q_OS_WIN
            SetWindowPos((HWND) this->winId(), HWND_TOPMOST, this->pos().x(), this->pos().y(), this->width(), this->height(), SWP_SHOWWINDOW);
#endif

            logger->info(
                "max->fullscreen/normal->fullscreen, ui.playWidget: {}x{}, {}x{}",
                ui.playWidget->x(),
                ui.playWidget->y(),
                ui.playWidget->width(),
                ui.playWidget->height());

            QTimer::singleShot(1000, [&] {
                if (mWindowSizeState == WINDOW_FULLSCREEN) {
                    ui.widget_title->hide();
                    ui.widget_playControl->hide();

                    // 把当前窗口置在TopLevel Highest
#ifdef Q_OS_WIN
                    SetWindowPos((HWND) this->winId(), HWND_TOPMOST, this->pos().x(), this->pos().y(), this->width(), this->height(), SWP_SHOWWINDOW);
#endif
                }
            });
        } break;
        case WINDOW_MAXIMIZED: {
            logger->info("Switch From WINDOW_MAXIMIZED to Fullscreen");
            logger->info(
                "max->fullscreen / normal->fullscreen, winState(old):{}, currentScreen.name:{}, geometry:{}x{}, {}x{}",
                (int) mWindowSizeState,
                screen()->name().toStdString(),
                screen()->geometry().x(),
                screen()->geometry().y(),
                screen()->geometry().width(),
                screen()->geometry().height());
            showNormal(); //FIX: Maxmize->Fullscreen Have Bug, change to showNormal , then Fullscreen
            QRect screenGeometry = screen()->geometry();
            this->move(screenGeometry.left() - 1, screenGeometry.top() - 1);
            this->resize(screenGeometry.width() + 2, screenGeometry.height() + 2);
            this->show();
            this->raise();

            ui.playWidget->resize(screenGeometry.width(), screenGeometry.height());
            ui.verticalLayout->removeWidget(ui.widget_title);
            ui.widget_title->resize(screenGeometry.width(), ui.widget_title->height());
            ui.widget_title->move(0, 0);
            ui.widget_title->raise();

            if (!ui.widget_playList->isHidden()) {
                ui.widget_playList->hide();
            }

            ui.verticalLayout_4->removeWidget(ui.widget_playControl);
            ui.widget_playControl->resize(screenGeometry.width(), ui.widget_playControl->height());
            ui.widget_playControl->move(0, screenGeometry.height() - ui.widget_playControl->height());
            // ui.widget_playControl->hide();
            mWindowSizeState = WINDOW_FULLSCREEN;

            // 把当前窗口置在TopLevel Highest
#ifdef Q_OS_WIN
            SetWindowPos((HWND) this->winId(), HWND_TOPMOST, this->pos().x(), this->pos().y(), this->width(), this->height(), SWP_SHOWWINDOW);
#endif

            logger->info(
                "max->fullscreen/normal->fullscreen, ui.playWidget: {}x{}, {}x{}",
                ui.playWidget->x(),
                ui.playWidget->y(),
                ui.playWidget->width(),
                ui.playWidget->height());

            QTimer::singleShot(1000, [&] {
                if (mWindowSizeState == WINDOW_FULLSCREEN) {
                    ui.widget_title->hide();
                    ui.widget_playControl->hide();

                    // 把当前窗口置在TopLevel Highest
#ifdef Q_OS_WIN
                    SetWindowPos((HWND) this->winId(), HWND_TOPMOST, this->pos().x(), this->pos().y(), this->width(), this->height(), SWP_SHOWWINDOW);
#endif
                }
            });
        } break;
        } //switch
    } // else
}

void MainWindow::on_pushButton_close_clicked()
{
    this->close();
}

//    play
void MainWindow::on_pushButton_stop_clicked()
{
    if (playController_->isStopped()) {
        logger->debug("already Stopped, no need to stop");
        return;
    }

    playController_->stop();

    ui.playWidget->StopRendering();
    if (!playController_->isPlaying()) {
        ui.pushButton_playPause->setChecked(false);
    }
}

void MainWindow::on_pushButton_previous_clicked()
{
    if (ui.tabWidget_playList->currentIndex() == 0) {
        int index = ui.listWidget_playlist->row(ui.listWidget_playlist->currentItem());
        index = (index - 1) < 0 ? 0 : (index - 1);
        ui.listWidget_playlist->setCurrentItem(ui.listWidget_playlist->item(index));
        // GlobalDef::getInstance()->PLAY_LIST_DATA.playlist_current_video = index;
        if (index >= 0) {
            QString mvPath = GlobalDef::getInstance()->PLAY_LIST_DATA.play_list[0].video_list[index].video_path;
            logger->debug(" current path:{}", mvPath.toUtf8().constData());
            openPath(mvPath, false);
        } else {
            logger->debug(" current index: {}", index);
        }
    } else {
        int index = ((PlayListPage *) ui.tabWidget_playList->currentWidget())
                        ->getListWidget()
                        ->row(((PlayListPage *) ui.tabWidget_playList->currentWidget())->getListWidget()->currentItem());
        index = (index - 1) < 0 ? 0 : (index - 1);
        ((PlayListPage *) ui.tabWidget_playList->currentWidget())
            ->getListWidget()
            ->setCurrentItem(((PlayListPage *) ui.tabWidget_playList->currentWidget())->getListWidget()->item(index));
        // GlobalDef::getInstance()->PLAY_LIST_DATA.playlist_current_video = index;
        if (index >= 0) {
            QString mvPath = GlobalDef::getInstance()->PLAY_LIST_DATA.play_list[ui.tabWidget_playList->currentIndex()].video_list[index].video_path;
            logger->debug(" current path:{}", mvPath.toUtf8().constData());
            openPath(mvPath, false);
        } else {
            logger->debug(" current index:{}", index);
        }
    }
}

void MainWindow::on_pushButton_playPause_clicked()
{
    logger->info("playWidget isPlaying:{}, isPause:{}", playController_->isPlaying(), playController_->isPaused());

    if (playController_->isPlaying()) {
        playController_->pause();
        ui.pushButton_playPause->setChecked(false);
    } else if (playController_->isPaused()) {
        playController_->play();
        ui.pushButton_playPause->setChecked(true);
    } else {
        if (ui.tabWidget_playList->currentIndex() == 0) {
            int index = ui.listWidget_playlist->row(ui.listWidget_playlist->currentItem());
            if (index >= 0) {
                QString mvPath = GlobalDef::getInstance()->PLAY_LIST_DATA.play_list[0].video_list[index].video_path;
                logger->info("on_pushButton_playPause_clicked,index:{}, current path:{}", index, mvPath.toUtf8().constData());
                openPath(mvPath, false);
            } else {
                ui.pushButton_playPause->setChecked(false);
                logger->info(" current index:{}", index);
            }
        } else {
            int index = ((PlayListPage *) ui.tabWidget_playList->currentWidget())
                            ->getListWidget()
                            ->row(((PlayListPage *) ui.tabWidget_playList->currentWidget())->getListWidget()->currentItem());
            if (index >= 0) {
                QString mvPath = GlobalDef::getInstance()->PLAY_LIST_DATA.play_list[ui.tabWidget_playList->currentIndex()].video_list[index].video_path;
                logger->info(" currentIndex!=0, index:{}, current path:{}", index, mvPath.toUtf8().constData());
                openPath(mvPath, false);
            } else {
                ui.pushButton_playPause->setChecked(false);
                logger->info("index>=0 is false, current index:{}", index);
            }
        }
    }
}

void MainWindow::on_pushButton_next_clicked()
{
    if (ui.tabWidget_playList->currentIndex() == 0) {
        int index = ui.listWidget_playlist->row(ui.listWidget_playlist->currentItem());
        index = (index + 1) > (ui.listWidget_playlist->count() - 1) ? (ui.listWidget_playlist->count() - 1) : (index + 1);
        ui.listWidget_playlist->setCurrentItem(ui.listWidget_playlist->item(index));
        // GlobalDef::getInstance()->PLAY_LIST_DATA.playlist_current_video = index;
        if (index >= 0) {
            QString mvPath = GlobalDef::getInstance()->PLAY_LIST_DATA.play_list[0].video_list[index].video_path;
            logger->debug(" current path:{} ", mvPath.toUtf8().constData());
            openPath(mvPath, false);
        } else {
            logger->debug(" current index:{}", index);
        }
    } else {
        int index = ((PlayListPage *) ui.tabWidget_playList->currentWidget())
                        ->getListWidget()
                        ->row(((PlayListPage *) ui.tabWidget_playList->currentWidget())->getListWidget()->currentItem());
        index = (index + 1) > (((PlayListPage *) ui.tabWidget_playList->currentWidget())->getListWidget()->count() - 1)
                    ? (((PlayListPage *) ui.tabWidget_playList->currentWidget())->getListWidget()->count() - 1)
                    : (index + 1);
        ((PlayListPage *) ui.tabWidget_playList->currentWidget())
            ->getListWidget()
            ->setCurrentItem(((PlayListPage *) ui.tabWidget_playList->currentWidget())->getListWidget()->item(index));
        // GlobalDef::getInstance()->PLAY_LIST_DATA.playlist_current_video = index;
        if (index >= 0) {
            QString mvPath = GlobalDef::getInstance()->PLAY_LIST_DATA.play_list[ui.tabWidget_playList->currentIndex()].video_list[index].video_path;
            logger->debug(" current path:{}", mvPath.toUtf8().constData());
            openPath(mvPath, false);
        } else {
            logger->debug(" current index:{}", index);
        }
    }
}

void MainWindow::on_pushButton_open_clicked()
{
    openFile();
}

//    list
void MainWindow::on_pushButton_add_clicked()
{
    QString filepath = QFileDialog::getOpenFileName(
        this,
        QString(tr("请选择要添加的视频文件")),
        "./",
        "video(*.mp4 *.flv *.f4v *.webm *.m4v *.mov *.3gp *.3g2 *.rm *.rmvb *.wmv *.avi *.asf *.mpg *.mpeg *.mpe *.ts *.div *.dv *.divx *.vob *.mkv *.wzmp4 "
        "*.wzavi *.wzmkv *.wzmov *.wzmpg)");
    if (!filepath.isEmpty()) {
        QStringList addList;
        addList.append(filepath);
        updatePlayList(addList, GlobalDef::getInstance()->PLAY_LIST_DATA.playlist_current_index);
    }
}

void MainWindow::on_pushButton_clear_clicked()
{
    QItemSelectionModel *model;

    if (ui.tabWidget_playList->currentIndex() == 0) {
        model = ui.listWidget_playlist->selectionModel();
    } else {
        model = ((PlayListPage *) ui.tabWidget_playList->widget(GlobalDef::getInstance()->PLAY_LIST_DATA.playlist_current_index))
                    ->getListWidget()
                    ->selectionModel();
    }

    if (model) {
        QModelIndexList indexlist = model->selectedIndexes();
        QList<int> listRows;
        for (int i = 0; i < indexlist.size(); ++i) {
            // std::cout << "============= row:" << indexlist[i].row() << std::endl;
            listRows.push_back(indexlist[i].row());
        }
        std::sort(listRows.begin(), listRows.end(), [](int a, int b) { return a > b; });

        //  删除 playList
        if (ui.tabWidget_playList->currentIndex() == 0) {
            for (int i = 0; i < listRows.size(); ++i) {
                GlobalDef::getInstance()->PLAY_LIST_DATA.play_list[ui.tabWidget_playList->currentIndex()].video_list.remove(listRows[i]);
                ui.listWidget_playlist->model()->removeRow(listRows[i]);
            }

            for (int i = 0; i < ui.listWidget_playlist->count(); ++i) {
                QString itemText = ui.listWidget_playlist->item(i)->text();
                int dpos = itemText.indexOf(".");
                itemText = itemText.mid(dpos);
                itemText = QString::number(i + 1) + itemText;
                ui.listWidget_playlist->item(i)->setText(itemText);
            }
        } else {
            for (int i = 0; i < listRows.size(); ++i) {
                GlobalDef::getInstance()->PLAY_LIST_DATA.play_list[ui.tabWidget_playList->currentIndex()].video_list.remove(listRows[i]);
                ((PlayListPage *) ui.tabWidget_playList->widget(GlobalDef::getInstance()->PLAY_LIST_DATA.playlist_current_index))
                    ->getListWidget()
                    ->model()
                    ->removeRow(listRows[i]);
            }

            for (int i = 0;
                 i < ((PlayListPage *) ui.tabWidget_playList->widget(GlobalDef::getInstance()->PLAY_LIST_DATA.playlist_current_index))->getListWidget()->count();
                 ++i) {
                QString itemText = ((PlayListPage *) ui.tabWidget_playList->widget(GlobalDef::getInstance()->PLAY_LIST_DATA.playlist_current_index))
                                       ->getListWidget()
                                       ->item(i)
                                       ->text();
                int dpos = itemText.indexOf(".");
                itemText = itemText.mid(dpos);
                itemText = QString::number(i + 1) + itemText;
                ((PlayListPage *) ui.tabWidget_playList->widget(GlobalDef::getInstance()->PLAY_LIST_DATA.playlist_current_index))
                    ->getListWidget()
                    ->item(i)
                    ->setText(itemText);
            }
        }

        ApplicationSettings mWZSetting;
        mWZSetting.write_PlayList();
    }
}

void MainWindow::on_pushButton_loadPlayList_clicked()
{
    QString filepath = QFileDialog::getOpenFileName(this, QString(tr("请选择要导入的播放列表文件")), "./", "file(*.json)");
    ApplicationSettings mWZSetting;
    PlayList readPlayList = mWZSetting.load_PlayList(filepath);

    bool bExist = false;
    int sameNameID = -1;
    for (int i = 0; i < GlobalDef::getInstance()->PLAY_LIST_DATA.play_list.size(); ++i) {
        if (GlobalDef::getInstance()->PLAY_LIST_DATA.play_list[i].list_name == readPlayList.list_name) {
            bExist = true;
            sameNameID = i;
            break;
        }
    }

    if (bExist) {
        QStringList pathList;
        for (int i = 0; i < readPlayList.video_list.size(); ++i) {
            pathList.push_back(readPlayList.video_list[i].video_path);
        }
        updatePlayList(pathList, sameNameID);
    } else {
        GlobalDef::getInstance()->PLAY_LIST_DATA.play_list.push_back(readPlayList);
        PlayListPage *itemPage = nullptr;
        itemPage = new PlayListPage(this, ui.tabWidget_playList);
        ui.tabWidget_playList->addTab(itemPage, readPlayList.list_name);
        for (int i = 0; i < readPlayList.video_list.size(); ++i) {
            QString mvPath = readPlayList.video_list[i].video_path;
            mvPath = mvPath.replace("\\", "/");
            QStringList mvPathArr = mvPath.split("/");
            itemPage->getListWidget()->addItem(mvPathArr[mvPathArr.size() - 1]);
        }
    }

    mWZSetting.write_PlayList();
}

void MainWindow::on_pushButton_exportPlayList_clicked()
{
    int index = ui.tabWidget_playList->currentIndex();
    QString dirpath = QFileDialog::getExistingDirectory(this, QString(tr("选择文件保存目录")), "./", QFileDialog::ShowDirsOnly);
    ApplicationSettings mWZSetting;
    mWZSetting.export_PlayList(dirpath, index);
    QMessageBox::StandardButton result = QMessageBox::information(
        this, QString(tr("提示")), QString(tr("播放列表<")) + GlobalDef::getInstance()->PLAY_LIST_DATA.play_list[index].list_name + QString(tr(">已导出")));
}

//void MainWindow::reply_switchButton_3D2D_statusChanged(bool checked)
//{
//    if (ui.action_2D->isChecked()) {
//        //    原视频播放
//        ui.playWidget->SetStereoFormat(StereoFormat(STEREO_FORMAT_NORMAL_2D));
//    } else {
//        ui.playWidget->SetStereoFormat(StereoFormat(STEREO_FORMAT_3D));
//        if (!checked) {
//            //    3D + (水平、垂直、棋盘)
//            ui.action_3D->setText(QString(tr("3D")));
//            ui.action_3D->setChecked(true);
//
//            if (mStereoOutputFormat == StereoOutputFormat::STEREO_OUTPUT_FORMAT_ONLY_LEFT) {
//                mStereoOutputFormat = mStereoOutputFormatWhenSwitch2D;
//                ui.playWidget->SetStereoOutputFormat(mStereoOutputFormatWhenSwitch2D);
//                switch (mStereoOutputFormatWhenSwitch2D) {
//                case StereoOutputFormat::STEREO_OUTPUT_FORMAT_HORIZONTAL:
//                    ui.actionOutputHorizontal->setChecked(true);
//                    break;
//                case StereoOutputFormat::STEREO_OUTPUT_FORMAT_VERTICAL:
//                    ui.actionOutputVertical->setChecked(true);
//                    break;
//                case StereoOutputFormat::STEREO_OUTPUT_FORMAT_CHESS:
//                    ui.actionOutputChess->setChecked(true);
//                    break;
//                }
//            }
//
//            ui.actionOutputHorizontal->setEnabled(true);
//            ui.actionOutputVertical->setEnabled(true);
//            ui.actionOutputChess->setEnabled(true);
//            ui.actionOutputOnlyLeft->setDisabled(true);
//        } else {
//            //    3D + onlyleft
//            ui.action_3D->setText(QString(tr("2D")));
//            ui.action_3D->setChecked(false);
//
//            ui.actionOutputOnlyLeft->setChecked(true);
//            if (mStereoOutputFormat != StereoOutputFormat::STEREO_OUTPUT_FORMAT_ONLY_LEFT) {
//                mStereoOutputFormatWhenSwitch2D = mStereoOutputFormat;
//            }
//            mStereoOutputFormat = StereoOutputFormat::STEREO_OUTPUT_FORMAT_ONLY_LEFT;
//            ui.playWidget->SetStereoOutputFormat(StereoOutputFormat::STEREO_OUTPUT_FORMAT_ONLY_LEFT);
//
//            ui.actionOutputHorizontal->setDisabled(true);
//            ui.actionOutputVertical->setDisabled(true);
//            ui.actionOutputChess->setDisabled(true);
//            ui.actionOutputOnlyLeft->setEnabled(true);
//        }
//    }
//}

void MainWindow::on_comboBox_src2D_3D2D_currentIndexChanged(int index)
{
    switch (index) {
    case 0:
        //    3D + (水平、垂直、棋盘)
        mStereoFormat = StereoFormat::STEREO_FORMAT_3D;
        ui.action_3D->setChecked(true);
        ui.playWidget->SetStereoFormat(StereoFormat(STEREO_FORMAT_3D));
        //  intput
        ui.comboBox_3D_input->setEnabled(true);
        ui.actionInputLR->setEnabled(true);
        ui.actionInputRL->setEnabled(true);
        ui.actionInputUD->setEnabled(true);
        //  output
        ui.actionOutputHorizontal->setEnabled(true);
        ui.actionOutputVertical->setEnabled(true);
        ui.actionOutputChess->setEnabled(true);
        ui.actionOutputOnlyLeft->setEnabled(false);

        if (mStereoOutputFormat == StereoOutputFormat::STEREO_OUTPUT_FORMAT_ONLY_LEFT) {
            mStereoOutputFormat = mStereoOutputFormatWhenSwitch2D;
            ui.playWidget->SetStereoOutputFormat(mStereoOutputFormatWhenSwitch2D);
            switch (mStereoOutputFormatWhenSwitch2D) {
            case StereoOutputFormat::STEREO_OUTPUT_FORMAT_HORIZONTAL:
                ui.actionOutputHorizontal->setChecked(true);
                break;
            case StereoOutputFormat::STEREO_OUTPUT_FORMAT_VERTICAL:
                ui.actionOutputVertical->setChecked(true);
                break;
            case StereoOutputFormat::STEREO_OUTPUT_FORMAT_CHESS:
                ui.actionOutputChess->setChecked(true);
                break;
            }
        }
        onStereoFormatChanged(StereoFormat(STEREO_FORMAT_3D), mStereoInputFormat, mStereoOutputFormat);

        break;
    case 1:
        //    3D + onlyleft
        mStereoFormat = StereoFormat::STEREO_FORMAT_3D;
        ui.action_3D_left->setChecked(true);
        //  intput
        ui.comboBox_3D_input->setEnabled(true);
        ui.actionInputLR->setEnabled(true);
        ui.actionInputRL->setEnabled(true);
        ui.actionInputUD->setEnabled(true);
        //  output
        ui.actionOutputHorizontal->setDisabled(true);
        ui.actionOutputVertical->setDisabled(true);
        ui.actionOutputChess->setDisabled(true);
        ui.actionOutputOnlyLeft->setEnabled(true);

        if (mStereoOutputFormat != StereoOutputFormat::STEREO_OUTPUT_FORMAT_ONLY_LEFT) {
            mStereoOutputFormatWhenSwitch2D = mStereoOutputFormat;
        }
        mStereoOutputFormat = StereoOutputFormat::STEREO_OUTPUT_FORMAT_ONLY_LEFT;
        ui.actionOutputOnlyLeft->setChecked(true);

        onStereoFormatChanged(StereoFormat(STEREO_FORMAT_3D), mStereoInputFormat, mStereoOutputFormat);
        ui.playWidget->SetStereoFormat(StereoFormat(STEREO_FORMAT_3D));
        ui.playWidget->SetStereoOutputFormat(StereoOutputFormat::STEREO_OUTPUT_FORMAT_ONLY_LEFT);

        break;
    case 2:
        //    原视频播放
        mStereoFormat = StereoFormat::STEREO_FORMAT_NORMAL_2D;
        ui.action_2D->setChecked(true);

        //  input
        ui.comboBox_3D_input->setEnabled(false);
        ui.actionInputLR->setDisabled(true);
        ui.actionInputRL->setDisabled(true);
        ui.actionInputUD->setDisabled(true);
        //  output
        ui.actionOutputHorizontal->setDisabled(true);
        ui.actionOutputVertical->setDisabled(true);
        ui.actionOutputChess->setDisabled(true);
        ui.actionOutputOnlyLeft->setDisabled(true);

        ui.playWidget->SetStereoFormat(StereoFormat(STEREO_FORMAT_NORMAL_2D));
        onStereoFormatChanged(StereoFormat(STEREO_FORMAT_NORMAL_2D), mStereoInputFormat, mStereoOutputFormat);

        break;
    }
}

void MainWindow::on_comboBox_3D_input_currentIndexChanged(int index)
{
    switch (index) {
    case 0:
        ui.actionInputLR->setChecked(true);
        mStereoInputFormat = StereoInputFormat::STEREO_INPUT_FORMAT_LR;
        break;
    case 1:
        mStereoInputFormat = StereoInputFormat::STEREO_INPUT_FORMAT_RL;
        ui.actionInputRL->setChecked(true);
        break;
    case 2:
        mStereoInputFormat = StereoInputFormat::STEREO_INPUT_FORMAT_UD;
        ui.actionInputUD->setChecked(true);
        break;
    }
    ui.playWidget->SetStereoInputFormat(mStereoInputFormat);
    GlobalDef::getInstance()->DefaultStereoInputFormat = mStereoInputFormat;
}

//void MainWindow::reply_switchButton_LRRL_statusChanged(bool checked)
//{
//    if (checked) {
//        ui.actionInputLR->setChecked(true);
//        ui.playWidget->SetStereoInputFormat(StereoInputFormat::STEREO_INPUT_FORMAT_LR);
//    } else {
//        ui.actionInputRL->setChecked(true);
//        ui.playWidget->SetStereoInputFormat(StereoInputFormat::STEREO_INPUT_FORMAT_RL);
//    }
//}

void MainWindow::on_pushButton_hs_clicked()
{
    // QList<QCameraDevice> cameras = ui.playWidget->getCamerasInfo();
    // qDebug() << __FUNCTION__ << "..............ABC, camera.size:"<<cameras.size();

    // if (cameras.size() > 0) {
    //     ui.playWidget->openCamera(cameras[0]);
    //     ui.playWidget->startCamera();
    // }
}

void MainWindow::on_pushButton_a_clicked()
{
   // 使用 CameraManager 停止和关闭 Camera
   if (cameraManager_) {
    cameraManager_->stopCamera();
    cameraManager_->closeCamera();
   } 
}

//    key press event
void MainWindow::keyPressEvent(QKeyEvent *event)
{
    QKeyCombination keyCombination = event->keyCombination();

    switch (keyCombination.keyboardModifiers()) {
    case Qt::NoModifier:
        break;
    case Qt::ControlModifier: {
        switch (keyCombination.key()) {
        case Qt::Key_N: {
            // 视差调节
            if (GlobalDef::getInstance()->mRunningMode == RunningMode::RM_PRO) {
                onTakeScreenShot();
            }
        } break;
        case Qt::Key_M: {
            if (GlobalDef::getInstance()->mRunningMode == RunningMode::RM_PRO) {
                ui.playWidget->ToggleViewParallaxSideStrip();
            }
        } break;
        default:
            break;
        }
    } break;
    }

    return QWidget::keyPressEvent(event);
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (ui.widget_title->underMouse() && (event->button() == Qt::LeftButton)) {
        QPoint startPos = event->globalPosition().toPoint();
        m_offPos = startPos - geometry().topLeft();
    }

    if (mWindowSizeState == WINDOW_FULLSCREEN) {
        ui.widget_title->show();
        ui.widget_playControl->show();

        QTimer::singleShot(2000, [&] {
            if (mWindowSizeState == WINDOW_FULLSCREEN) {
                ui.widget_title->hide();
                ui.widget_playControl->hide();
            }
        });
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (event == nullptr) {
        return;
    }

    if (ui.widget_title->underMouse() && (event->buttons() == Qt::LeftButton) && mWindowSizeState != WINDOW_FULLSCREEN) {
        QPoint endPoint = event->globalPosition().toPoint();
        QPoint movePoint = endPoint - m_offPos;
        move(movePoint);
        GlobalDef::getInstance()->MIN_WINDOW_POSITION_X = movePoint.x();
        GlobalDef::getInstance()->MIN_WINDOW_POSITION_Y = movePoint.y();
    }
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (mWindowSizeState != WINDOW_FULLSCREEN && !GlobalDef::getInstance()->B_WINDOW_SIZE_MAXIMIZED) {
        if (GlobalDef::getInstance()->MIN_WINDOW_POSITION_X % 2 == 0) {
            GlobalDef::getInstance()->MIN_WINDOW_POSITION_X = GlobalDef::getInstance()->MIN_WINDOW_POSITION_X + 1;
        }
        if (GlobalDef::getInstance()->MIN_WINDOW_POSITION_Y % 2 == 0) {
            GlobalDef::getInstance()->MIN_WINDOW_POSITION_Y = GlobalDef::getInstance()->MIN_WINDOW_POSITION_Y + 1;
        }
        move(GlobalDef::getInstance()->MIN_WINDOW_POSITION_X, GlobalDef::getInstance()->MIN_WINDOW_POSITION_Y);
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    if (mDrawWidget) {
        mDrawWidget->move(ui.playWidget->pos());
        mDrawWidget->resize(ui.playWidget->size());
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls();
    QStringList addList;

    for (int i = 0; i < urls.size(); ++i) {
        // urls[i].toLocalFile
        addList.append(urls[i].toLocalFile());
    }

    updatePlayList(addList, ui.tabWidget_playList->currentIndex());

    if (playController_->isPaused() || playController_->isPlaying()) {
        logger->info("movie is already playing, drag file will not play.");
    } else {
        logger->info("movie is not playing, drag file will be played.");
        if (urls.size() > 0) {
            openPath(urls[0].toLocalFile(), true);
        }
    }
}

void MainWindow::contextMenuEvent(QContextMenuEvent *event)
{
    logger->debug(
        "MainWindow::contextMenuEvent, reason:{}, global:{}x{}, pos:{}x{}, cursorPos:{}x{}",
        int(event->reason()),
        event->globalX(),
        event->globalY(),
        event->x(),
        event->y(),
        QCursor::pos().x(),
        QCursor::pos().y());
    logger->info("MainWindow::contextMenuEvent, currentFullscreenFlag: {}, contextMenuPolicy:{}", this->isFullScreen(), int(contextMenuPolicy()));

    //ui.menu->exec(QCursor::pos());
    ui.menu->popup(QCursor::pos());
    ui.menu->raise();

    logger->info(
        "MainWindow::contextMenuEvent, when fullscreen? {} , check whethere ui.menu is topmost, isActiveWindow:{}",
        this->isFullScreen(),
        ui.menu->isActiveWindow());
}

//    menu
void MainWindow::on_pushButton_menu_clicked()
{
    QPoint gpt = ui.pushButton_menu->mapToGlobal(QPoint(0, 0));
    QPoint pt(gpt.x() + 1, gpt.y() + ui.pushButton_menu->height());
    logger->debug(
        "ui.pushButton_menu.pos :{}x{}, map to glbal gpt:{}x{}, pt:{}x{}",
        ui.pushButton_menu->pos().x(),
        ui.pushButton_menu->pos().y(),
        gpt.x(),
        gpt.y(),
        pt.x(),
        pt.y());

    ui.menu->exec(pt);
}

void MainWindow::on_actionOpenCamera_toggled(bool cam_checked)
{
    if (playController_->isOpened()) {
        playController_->stop();
        // 同步等待播放完全停止后再切换，避免视频在后台继续运行（BUG 14）
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        QMetaObject::Connection conn = connect(playController_, &PlayController::playbackStateChanged, this, [&loop](PlaybackState state) {
            if (state == PlaybackState::Stopped) {
                loop.quit();
            }
        });
        timeout.start(3000);
        loop.exec();
        disconnect(conn);
        if (!playController_->isStopped()) {
            logger->warn("on_actionOpenCamera_toggled: Stop did not complete within 3s, proceeding anyway");
        }
    }

    if (cam_checked) {
        // 使用 CameraManager 管理 Camera
        if (cameraManager_) {
            QList<QCameraDevice> cameras = cameraManager_->getAvailableCameras();
            if (cameras.size() > 0) {
                logger->info("on_actionOpenCamera_toggled: Found {} cameras, opening first one: {}", 
                    cameras.size(), cameras[0].description().toStdString());
                
                // 使用 CameraManager 打开和启动 Camera
                if (cameraManager_->openCamera(cameras[0])) {
                    if (cameraManager_->startCamera()) {
                        // 切换到 Camera Widget（在启动 Camera 之后）
                        switchToCamera();
                        
                        // 确保 cameraWidget_ 的 OpenGL 上下文已初始化
                        if (cameraWidget_) {
                            cameraWidget_->show();
                            cameraWidget_->raise();
                            // 强制更新以触发 initializeGL
                            cameraWidget_->update();
                            logger->info("on_actionOpenCamera_toggled: Camera widget shown and updated");
                        }
                    } else {
                        logger->error("on_actionOpenCamera_toggled: Failed to start camera");
                    }
                } else {
                    logger->error("on_actionOpenCamera_toggled: Failed to open camera");
                }
            } else {
                logger->warn("on_actionOpenCamera_toggled: No cameras found");
            }
        } else {
            logger->error("on_actionOpenCamera_toggled: CameraManager is null");
        }
    } else {
        // 切换到视频文件 Widget
        switchToVideoFile();
        
        // 使用 CameraManager 停止和关闭 Camera
        if (cameraManager_) {
            cameraManager_->stopCamera();
            cameraManager_->closeCamera();
            logger->info("on_actionOpenCamera_toggled: Camera stopped and closed");
        } 
    }
}

void MainWindow::on_actionOpen_triggered()
{
    openFile();
}

void MainWindow::reply_action_3D_hotKey()
{
    /**
     * 2D/3D模式切换快捷键处理
     * - 根据当前状态切换2D/3D模式
     * - 使用trigger()方法触发对应的action，确保所有相关的UI和逻辑都正确执行
     * 
     * 修复说明：
     * - 原问题：只设置checked状态，没有真正切换2D/3D模式
     * - 修复方案：使用trigger()方法触发action_3D或action_2D
     * - trigger()方法会自动调用对应的槽函数，完成完整的模式切换
     */
    logger->debug("replay_action_3D_hotkey triggered");

    // 切换2D/3D模式：如果当前是2D则切换到3D，反之亦然
    if (ui.action_3D->isChecked()) {
        // 当前是3D，切换到2D
        ui.action_2D->trigger();
    } else {
        // 当前是2D，切换到3D
        ui.action_3D->trigger();
    }
}

//void MainWindow::on_action_3D_toggled(bool stereo_checked)
//{
//    const StereoFormat curStereoFormat = ui.playWidget->ToggleStereo(stereo_checked);
//    logger->info("after ToggleStereo, curStereoFormat: {}", int(curStereoFormat));
//
//    if (ui.action_2D->isChecked()) {
//        //    原视频播放
//        ui.playWidget->SetStereoFormat(StereoFormat(STEREO_FORMAT_NORMAL_2D));
//    } else {
//        ui.playWidget->SetStereoFormat(StereoFormat(STEREO_FORMAT_3D));
//        if (!stereo_checked) {
//            //    3D + onlyleft
//            ui.comboBox_3D_input->setEnabled(true);
//            //ui.switchButton_3D2D->setChecked(true);
//            //ui.switchButton_LR_RL->setEnabled(false);
//            if (mStereoOutputFormat != StereoOutputFormat::STEREO_OUTPUT_FORMAT_ONLY_LEFT) {
//                mStereoOutputFormatWhenSwitch2D = mStereoOutputFormat;
//            }
//
//            mStereoOutputFormat = StereoOutputFormat::STEREO_OUTPUT_FORMAT_ONLY_LEFT;
//            ui.actionOutputOnlyLeft->setChecked(true);
//            ui.playWidget->SetStereoOutputFormat(StereoOutputFormat::STEREO_OUTPUT_FORMAT_ONLY_LEFT);
//
//            ui.actionOutputHorizontal->setDisabled(true);
//            ui.actionOutputVertical->setDisabled(true);
//            ui.actionOutputChess->setDisabled(true);
//            ui.actionOutputOnlyLeft->setEnabled(true);
//        } else {
//            //    3D + (水平、垂直、棋盘)
//            ui.comboBox_3D_input->setEnabled(true);
//            //ui.switchButton_3D2D->setChecked(false);
//            //ui.switchButton_LR_RL->setEnabled(true);
//
//            if (mStereoOutputFormat == StereoOutputFormat::STEREO_OUTPUT_FORMAT_ONLY_LEFT) {
//                mStereoOutputFormat = mStereoOutputFormatWhenSwitch2D;
//                ui.playWidget->SetStereoOutputFormat(mStereoOutputFormatWhenSwitch2D);
//                switch (mStereoOutputFormatWhenSwitch2D) {
//                case StereoOutputFormat::STEREO_OUTPUT_FORMAT_HORIZONTAL:
//                    ui.actionOutputHorizontal->setChecked(true);
//                    break;
//                case StereoOutputFormat::STEREO_OUTPUT_FORMAT_VERTICAL:
//                    ui.actionOutputVertical->setChecked(true);
//                    break;
//                case StereoOutputFormat::STEREO_OUTPUT_FORMAT_CHESS:
//                    ui.actionOutputChess->setChecked(true);
//                    break;
//                }
//            }
//
//            ui.actionOutputHorizontal->setEnabled(true);
//            ui.actionOutputVertical->setEnabled(true);
//            ui.actionOutputChess->setEnabled(true);
//            ui.actionOutputOnlyLeft->setDisabled(true);
//        }
//    }
//}

void MainWindow::reply_actionStereoFormat_Selected(QAction *stereoFormatAction)
{
    stereoFormatAction->setChecked(true);
    QString selectedObjectName = stereoFormatAction->objectName();

    if (selectedObjectName == "action_3D") {
        //    3D + (水平、垂直、棋盘)
        mStereoFormat = StereoFormat::STEREO_FORMAT_3D;
        ui.comboBox_src2D_3D2D->setCurrentIndex(0);
        //  intput
        ui.comboBox_3D_input->setEnabled(true);
        ui.actionInputLR->setEnabled(true);
        ui.actionInputRL->setEnabled(true);
        ui.actionInputUD->setEnabled(true);
        //  output
        ui.actionOutputHorizontal->setEnabled(true);
        ui.actionOutputVertical->setEnabled(true);
        ui.actionOutputChess->setEnabled(true);
        ui.actionOutputOnlyLeft->setEnabled(false);

        if (mStereoOutputFormat == StereoOutputFormat::STEREO_OUTPUT_FORMAT_ONLY_LEFT) {
            mStereoOutputFormat = mStereoOutputFormatWhenSwitch2D;
            switch (mStereoOutputFormatWhenSwitch2D) {
            case StereoOutputFormat::STEREO_OUTPUT_FORMAT_HORIZONTAL:
                ui.actionOutputHorizontal->setChecked(true);
                break;
            case StereoOutputFormat::STEREO_OUTPUT_FORMAT_VERTICAL:
                ui.actionOutputVertical->setChecked(true);
                break;
            case StereoOutputFormat::STEREO_OUTPUT_FORMAT_CHESS:
                ui.actionOutputChess->setChecked(true);
                break;
            }
        }

        onStereoFormatChanged(StereoFormat(STEREO_FORMAT_3D), mStereoInputFormat, mStereoOutputFormat);
        ui.comboBox_src2D_3D2D->setCurrentIndex(0);
        ui.playWidget->SetStereoFormat(StereoFormat(STEREO_FORMAT_3D));

    } else if (selectedObjectName == "action_3D_left") {
        mStereoFormat = StereoFormat::STEREO_FORMAT_3D;

        //  intput
        ui.comboBox_3D_input->setEnabled(true);
        ui.actionInputLR->setEnabled(true);
        ui.actionInputRL->setEnabled(true);
        ui.actionInputUD->setEnabled(true);
        //  output
        ui.actionOutputHorizontal->setDisabled(true);
        ui.actionOutputVertical->setDisabled(true);
        ui.actionOutputChess->setDisabled(true);
        ui.actionOutputOnlyLeft->setEnabled(true);

        if (mStereoOutputFormat != StereoOutputFormat::STEREO_OUTPUT_FORMAT_ONLY_LEFT) {
            mStereoOutputFormatWhenSwitch2D = mStereoOutputFormat;
        }
        mStereoOutputFormat = StereoOutputFormat::STEREO_OUTPUT_FORMAT_ONLY_LEFT;
        ui.actionOutputOnlyLeft->setChecked(true);

        onStereoFormatChanged(StereoFormat(STEREO_FORMAT_3D), mStereoInputFormat, mStereoOutputFormat);
        ui.comboBox_src2D_3D2D->setCurrentIndex(1);
        ui.playWidget->SetStereoFormat(StereoFormat(STEREO_FORMAT_3D));
        ui.playWidget->SetStereoOutputFormat(StereoOutputFormat::STEREO_OUTPUT_FORMAT_ONLY_LEFT);

    } else if (selectedObjectName == "action_2D") {
        //  souce 2D
        //  stereo format
        ui.comboBox_src2D_3D2D->setCurrentIndex(2);
        mStereoFormat = StereoFormat::STEREO_FORMAT_NORMAL_2D;

        //  input
        ui.comboBox_3D_input->setEnabled(false);
        ui.actionInputLR->setDisabled(true);
        ui.actionInputRL->setDisabled(true);
        ui.actionInputUD->setDisabled(true);
        //  output
        ui.actionOutputHorizontal->setDisabled(true);
        ui.actionOutputVertical->setDisabled(true);
        ui.actionOutputChess->setDisabled(true);
        ui.actionOutputOnlyLeft->setDisabled(true);

        ui.playWidget->SetStereoFormat(StereoFormat(STEREO_FORMAT_NORMAL_2D));
        onStereoFormatChanged(StereoFormat(STEREO_FORMAT_NORMAL_2D), mStereoInputFormat, mStereoOutputFormat);
    }

    //logger->info("selectedObjectName(Input):{}, inputFormat:{}", selectedObjectName.toStdString(), int(mStereoInputFormat));
    //ui.playWidget->SetStereoInputFormat(mStereoInputFormat);
}

void MainWindow::on_actionInputLR_triggered()
{
    if (!ui.actionInputLR->isEnabled())
        return;
    ui.actionInputLR->setChecked(true);
    GlobalDef::getInstance()->DefaultStereoInputFormat = STEREO_INPUT_FORMAT_LR;
    reply_actionInput_Selected(ui.actionInputLR);
}

void MainWindow::on_actionInputRL_triggered()
{
    if (!ui.actionInputRL->isEnabled())
        return;
    ui.actionInputRL->setChecked(true);
    GlobalDef::getInstance()->DefaultStereoInputFormat = STEREO_INPUT_FORMAT_RL;
    reply_actionInput_Selected(ui.actionInputRL);
}

void MainWindow::on_actionInputUD_triggered()
{
    if (!ui.actionInputUD->isEnabled())
        return;
    ui.actionInputUD->setChecked(true);
    GlobalDef::getInstance()->DefaultStereoInputFormat = STEREO_INPUT_FORMAT_UD;
    reply_actionInput_Selected(ui.actionInputUD);
}

void MainWindow::reply_actionInput_Selected(QAction *stereoInputAction)
{
    stereoInputAction->setChecked(true);
    //StereoInputFormat inputFormat = StereoInputFormat::STEREO_INPUT_FORMAT_LR;
    QString selectedObjectName = stereoInputAction->objectName();

    if (selectedObjectName == "actionInputLR") {
        mStereoInputFormat = StereoInputFormat::STEREO_INPUT_FORMAT_LR;
        //ui.switchButton_LR_RL->setChecked(true);
        ui.comboBox_3D_input->setCurrentIndex(0);
    } else if (selectedObjectName == "actionInputRL") {
        mStereoInputFormat = StereoInputFormat::STEREO_INPUT_FORMAT_RL;
        //ui.switchButton_LR_RL->setChecked(false);
        ui.comboBox_3D_input->setCurrentIndex(1);
    } else if (selectedObjectName == "actionInputUD") {
        mStereoInputFormat = StereoInputFormat::STEREO_INPUT_FORMAT_UD;
        ui.comboBox_3D_input->setCurrentIndex(2);
    }

    logger->info("selectedObjectName(Input):{}, inputFormat:{}", selectedObjectName.toStdString(), int(mStereoInputFormat));
    ui.playWidget->SetStereoInputFormat(mStereoInputFormat);
}

void MainWindow::on_actionOutputHorizontal_triggered()
{
    if (!ui.actionOutputHorizontal->isEnabled())
        return;

    ui.actionOutputHorizontal->setChecked(true);
    GlobalDef::getInstance()->DefaultStereoOutputFormat = STEREO_OUTPUT_FORMAT_HORIZONTAL;

    reply_actionOutput_Selected(ui.actionOutputHorizontal);
}

void MainWindow::on_actionOutputVertical_triggered()
{
    if (!ui.actionOutputVertical->isEnabled())
        return;

    ui.actionOutputVertical->setChecked(true);
    GlobalDef::getInstance()->DefaultStereoOutputFormat = STEREO_OUTPUT_FORMAT_VERTICAL;

    reply_actionOutput_Selected(ui.actionOutputVertical);
}

void MainWindow::on_actionOutputChess_triggered()
{
    if (!ui.actionOutputChess->isEnabled())
        return;

    ui.actionOutputChess->setChecked(true);
    GlobalDef::getInstance()->DefaultStereoOutputFormat = STEREO_OUTPUT_FORMAT_CHESS;

    reply_actionOutput_Selected(ui.actionOutputChess);
}

void MainWindow::on_actionOutputOnlyLeft_triggered()
{
    if (!ui.actionOutputOnlyLeft->isEnabled())
        return;

    ui.actionOutputOnlyLeft->setChecked(true);
    GlobalDef::getInstance()->DefaultStereoOutputFormat = STEREO_OUTPUT_FORMAT_ONLY_LEFT;

    reply_actionOutput_Selected(ui.actionOutputOnlyLeft);
}

void MainWindow::reply_actionOutput_Selected(QAction *selectedAction)
{
    selectedAction->setChecked(true);
    //StereoOutputFormat outputFormat = StereoOutputFormat::STEREO_OUTPUT_FORMAT_VERTICAL;
    QString selectedObjectName = selectedAction->objectName();

    // MainWindow.ui 中关于action的objectName 不能变（暂时只找到这一个不受语言影响的变量),
    // 且objectName不能改成多语言，这是一个内部的标识
    if (selectedObjectName == "actionOutputVertical") {
        mStereoOutputFormat = StereoOutputFormat::STEREO_OUTPUT_FORMAT_VERTICAL;
    } else if (selectedObjectName == "actionOutputHorizontal") {
        mStereoOutputFormat = StereoOutputFormat::STEREO_OUTPUT_FORMAT_HORIZONTAL;
    } else if (selectedObjectName == "actionOutputChess") {
        mStereoOutputFormat = StereoOutputFormat::STEREO_OUTPUT_FORMAT_CHESS;
    } else if (selectedObjectName == "actionOutputOnlyLeft") {
        mStereoOutputFormat = StereoOutputFormat::STEREO_OUTPUT_FORMAT_ONLY_LEFT;
    }
    logger->info("selectActionOutput is :{},outputFormat:{} ", selectedObjectName.toStdString(), int(mStereoOutputFormat));

    ui.playWidget->SetStereoOutputFormat(mStereoOutputFormat);
    GlobalDef::getInstance()->DefaultStereoOutputFormat = mStereoOutputFormat;
}

void MainWindow::onStereoRegionUpdate(QRect region)
{
    QPoint tl = region.topLeft();
    QPoint br = region.bottomRight();
    logger->info("onStereoRegionUpdate, region:{}x{}, {}x{}", tl.x(), tl.y(), br.x(), br.y());

    ui.playWidget->SetStereoFormat(STEREO_FORMAT_3D);
    ui.playWidget->SetStereoOutputFormat(mStereoOutputFormatWhenSwitchToRegion);
    ui.playWidget->SetStereoEnableRegion(true, tl.x(), tl.y(), br.x(), br.y());
    ui.playWidget->repaint();
}

void MainWindow::reply_actionPlayOrder_Selected(QAction *selectedAction)
{
    // 使用 PlayListManager 的播放模式
    if (ui.actionSinglePlay->isChecked()) {
        playListManager_->setPlayMode(PlayListManager::PlayMode::Sequential);
        // 顺序播放到最后一首后停止，相当于原来的单曲播放（播放一次）
        GlobalDef::getInstance()->PLAY_LOOP_ORDER = PlayLoop::PLAYLOOP_SINGLE_PLAY;
    } else if (ui.actionSingleCycle->isChecked()) {
        playListManager_->setPlayMode(PlayListManager::PlayMode::SingleLoop);
        GlobalDef::getInstance()->PLAY_LOOP_ORDER = PlayLoop::PLAYLOOP_SINGLE_CYCLE;
    } else if (ui.actionSequentialPlay->isChecked()) {
        playListManager_->setPlayMode(PlayListManager::PlayMode::Sequential);
        GlobalDef::getInstance()->PLAY_LOOP_ORDER = PlayLoop::PLAYLOOP_SEQUENTIAL_PLAY;
    } else if (ui.actionRandomlyPlay->isChecked()) {
        playListManager_->setPlayMode(PlayListManager::PlayMode::Random);
        GlobalDef::getInstance()->PLAY_LOOP_ORDER = PlayLoop::PLAYLOOP_RANDOMLY_PLAY;
    } else if (ui.actionListLoop->isChecked()) {
        playListManager_->setPlayMode(PlayListManager::PlayMode::Loop);
        GlobalDef::getInstance()->PLAY_LOOP_ORDER = PlayLoop::PLAYLOOP_LIST_LOOP;
    }
    
    logger->info("Play mode changed to: {}", static_cast<int>(playListManager_->playMode()));
}

void MainWindow::on_actionSetting_triggered()
{
    SettingsDialog mSettingsDialog;
    connect(&mSettingsDialog, &SettingsDialog::updateLanguage, this, &MainWindow::resetLanguage);
    connect(&mSettingsDialog, &SettingsDialog::hotKeyChanged, this, &MainWindow::initHotKey);
    connect(&mSettingsDialog, SIGNAL(subtitleSettingsChanged()), this, SLOT(onSubtitleSettingsChanged()), Qt::DirectConnection);
    connect(&mSettingsDialog, SIGNAL(accepted()), this, SLOT(onSettingsDialogAccepted()));
    connect(&mSettingsDialog, SIGNAL(rejected()), this, SLOT(onSettingsDialogRejected()));

    // FIX: 若为全屏，则暂时将主窗口最上取消（以便显示SettingsDialog)
    if (mWindowSizeState == WINDOW_FULLSCREEN) {
        logger->info("WindowSizeState is FULLScreen, SetWindowPos not as TopMost");
#ifdef Q_OS_WIN
        SetWindowPos((HWND) this->winId(), HWND_NOTOPMOST, this->pos().x(), this->pos().y(), this->width(), this->height(), SWP_SHOWWINDOW);
#endif
    }

    mSettingsDialog.exec();
}

void MainWindow::on_actionScreenshot_triggered()
{
    onTakeScreenShot();
}

void MainWindow::on_actionRegistrationRequest_triggered()
{
    QStringList deviceInfo = getDeviceInfo();

    QString strVerification = /*QString("WeiZheng_") + */ deviceInfo[0] + QString("_verification"); //+deviceInfo[1];
    QByteArray byteArray = strVerification.toLocal8Bit();
    for (int i = 0; i < byteArray.size(); ++i) {
        byteArray[i] = byteArray[i] ^ 0xE;
    }

    QDir dir;
    QString dirPath = QFileDialog::getExistingDirectory(this, QString(tr("选择文件保存目录")), "./", QFileDialog::ShowDirsOnly);
    if (!dir.exists(dirPath)) {
        dir.mkdir(dirPath);
    }

    if (dir.exists(dirPath)) {
        QFile writeFile(dirPath + QString("/RegCode.bin"));
        writeFile.open(QIODevice::ReadWrite);
        writeFile.write(byteArray, byteArray.size());
        writeFile.close();
        QMessageBox::information(
            this, QString(tr("信息")), QString(tr("注册验证请求文件已经生成，请联系相关人员获取注册码")), QMessageBox::NoButton, QMessageBox::Close);
    }
}

void MainWindow::on_actionRegistration_triggered()
{
    bool bRegFlag = false;
    QString fileName = QFileDialog::getOpenFileName(this, QString(tr("请选择注册申请文件")), QCoreApplication::applicationDirPath(), "file(*.reg)");
    fileName.replace("\\", "/");
    if (!fileName.isEmpty()) {
        QString regDir = QCoreApplication::applicationDirPath() + "/Registration";
        regDir.replace("\\", "/");
        QDir dir;
        if (!dir.exists(regDir)) {
            dir.mkpath(regDir);
        }
        if (dir.exists(regDir)) {
            QStringList regPathArr = fileName.split("/");
            if (QFile::copy(fileName, regDir + "/" + regPathArr[regPathArr.size() - 1])) {
                bRegFlag = true;
            }
        }
    }
    if (!bRegFlag) {
        QMessageBox::warning(
            this, QString(tr("警告")), QString(tr("注册失败，请检查注册文件是否正确或是否授权读写磁盘")), QMessageBox::NoButton, QMessageBox::Close);
    }
}

void MainWindow::on_actionAbout_triggered()
{
    AboutDialog mAboutDialog;
    mAboutDialog.exec();
}

void MainWindow::on_actionExit_triggered()
{
    //writeConfig();
    this->close();
}

void MainWindow::on_actionParallaxAdd_triggered()
{
    // 视差调节
    if (GlobalDef::getInstance()->mRunningMode == RunningMode::RM_PRO) {
        ui.playWidget->IncreaseParallax();
    }
}

void MainWindow::on_actionParallaxSub_triggered()
{
    // 视差调节
    if (GlobalDef::getInstance()->mRunningMode == RunningMode::RM_PRO) {
        ui.playWidget->DecreaseParallax();
    }
}

void MainWindow::on_actionParallaxReset_triggered()
{
    // 视差调节
    if (GlobalDef::getInstance()->mRunningMode == RunningMode::RM_PRO) {
        ui.playWidget->ResetParallax();
    }
}

void MainWindow::on_tabWidget_playList_currentChanged(int index)
{
    GlobalDef::getInstance()->PLAY_LIST_DATA.playlist_current_index = index;
}

void MainWindow::reply_pushButton_pushButton_playList_tabBar_addList_clicked()
{
    bool bRet = false;
    QString item = QInputDialog::getText(this, QString(tr("创建新播放列表")), QString(tr("请输入播放列表名:")), QLineEdit::Normal, "", &bRet);
    if (bRet && !item.isEmpty()) {
        logger->info("press ok item = {}", item.toStdString());

        //    TODO 因为有log  index是错的
        PlayListPage *itemPage = new PlayListPage(this, ui.tabWidget_playList);
        // ui.tabWidget_playList->insertTab(GlobalDef::getInstance()->PLAY_LIST_DATA.playlist_current_index, itemPage,
        // item); ui.tabWidget_playList->insertTab(ui.tabWidget_playList->count() - 1, itemPage, item);
        ui.tabWidget_playList->addTab(itemPage, item);
        ui.tabWidget_playList->setCurrentIndex(ui.tabWidget_playList->count() - 1);
        GlobalDef::getInstance()->PLAY_LIST_DATA.playlist_current_index = ui.tabWidget_playList->count() - 1;

        PlayList addPlayList;
        addPlayList.list_name = item;
        GlobalDef::getInstance()->PLAY_LIST_DATA.play_list.push_back(addPlayList);
    } else {
        logger->info("press Cancel item = {}", item.toStdString());
    }
}

void MainWindow::on_tabWidget_playList_tabCloseRequested(int index)
{
    QString msg = ui.tabWidget_playList->tabText(index) + QString(tr("<br>确定要删除当前播放列表吗？"));
    QMessageBox::StandardButton result = QMessageBox::information(NULL, QString(tr("提示")), msg, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (result == QMessageBox::Yes) { // && index != ui.tabWidget_playList->count()-1
        // int oCount = ui.tabWidget_playList->count();
        ui.tabWidget_playList->removeTab(index);
        GlobalDef::getInstance()->PLAY_LIST_DATA.play_list.remove(index);
        GlobalDef::getInstance()->PLAY_LIST_DATA.playlist_current_index = ui.tabWidget_playList->currentIndex();

        // if (index == oCount - 2) {
        //     GlobalDef::getInstance()->PLAY_LIST_DATA.playlist_current_index = oCount - 3;
        //     ui.tabWidget_playList->setCurrentIndex(oCount - 3);
        // }
        ApplicationSettings appSettings;
        appSettings.write_PlayList();
    }
}

void MainWindow::reply_playlistShowBut_Clicked()
{
    if (ui.widget_playList->isHidden()) {
        ui.widget_playList->show();
        // ui.widget_playView->setCurrentIndex(0);
    } else {
        ui.widget_playList->hide();
    }
}

void MainWindow::on_listWidget_playlist_itemClicked(QListWidgetItem *item)
{
    int index = -1;
    if (ui.tabWidget_playList->currentIndex() == 0) {
        index = ui.listWidget_playlist->row(item);
        QString mvPath
            = GlobalDef::getInstance()->PLAY_LIST_DATA.play_list[GlobalDef::getInstance()->PLAY_LIST_DATA.playlist_current_index].video_list[index].video_path;
    } else {
        index = ((PlayListPage *) ui.tabWidget_playList->widget(GlobalDef::getInstance()->PLAY_LIST_DATA.playlist_current_index))->getListWidget()->row(item);
        QString mvPath
            = GlobalDef::getInstance()->PLAY_LIST_DATA.play_list[GlobalDef::getInstance()->PLAY_LIST_DATA.playlist_current_index].video_list[index].video_path;
    }
}

void MainWindow::on_listWidget_playlist_itemDoubleClicked(QListWidgetItem *item)
{
    int index = -1;
    int playListIndex = ui.tabWidget_playList->currentIndex();
    
    if (playListIndex == 0) {
        index = ui.listWidget_playlist->row(item);
    } else {
        index = ((PlayListPage *) ui.tabWidget_playList->widget(playListIndex))->getListWidget()->row(item);
    }

    // 使用PlayListManager获取视频路径
    if (playListManager_) {
        QString mvPath = playListManager_->getVideoPath(index, playListIndex);
        if (!mvPath.isEmpty()) {
            // 设置当前播放项（不发送信号，避免与openPath()重复触发）
            playListManager_->setCurrentVideo(index, playListIndex, false);
            openPath(mvPath, false);
        } else {
            logger->warn("MainWindow::on_listWidget_playlist_itemDoubleClicked: Invalid video path at index {}", index);
        }
    } else {
        // 回退到旧逻辑
        QString mvPath
            = GlobalDef::getInstance()->PLAY_LIST_DATA.play_list[playListIndex].video_list[index].video_path;
        openPath(mvPath, false);
    }
}

void MainWindow::reply_listWidget_playlist_itemSelectionChanged(QListWidgetItem *listWidgetItem)
{
    if (listWidgetItem) {
        logger->info("#############3333       listWidgetItem:{}", listWidgetItem->text().toStdString());
    }
}

void MainWindow::replayCurrentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
    if (current) {
        logger->info("################3333333333 replayCurrentItemChanged. current:{}", current->text().toStdString());
    }
}

void MainWindow::replayListWidgetDrop(int index)
{
    QItemSelectionModel *model;
    if (ui.tabWidget_playList->currentIndex() == 0) {
        model = ui.listWidget_playlist->selectionModel();
    } else {
        model = ((PlayListPage *) ui.tabWidget_playList->widget(GlobalDef::getInstance()->PLAY_LIST_DATA.playlist_current_index))
                    ->getListWidget()
                    ->selectionModel();
    }

    if (model) {
        QModelIndexList indexlist = model->selectedIndexes();
        QList<int> listRows;
        for (int i = 0; i < indexlist.size(); ++i) {
            // std::cout << "============= row:" << indexlist[i].row() << std::endl;
            listRows.push_back(indexlist[i].row());
        }
        std::sort(listRows.begin(), listRows.end(), [](int a, int b) { return a < b; });

        QList<VideoInfo> newList;
        for (int i = 0; i < GlobalDef::getInstance()->PLAY_LIST_DATA.play_list[ui.tabWidget_playList->currentIndex()].video_list.size(); ++i) {
            if (i == index) {
                for (int j = 0; j < listRows.size(); ++j) {
                    newList.push_back(GlobalDef::getInstance()->PLAY_LIST_DATA.play_list[ui.tabWidget_playList->currentIndex()].video_list[listRows[j]]);
                }
            }
            if (!listRows.contains(i)) {
                newList.push_back(GlobalDef::getInstance()->PLAY_LIST_DATA.play_list[ui.tabWidget_playList->currentIndex()].video_list[i]);
            }
        }
        if (index == -1) {
            for (int j = 0; j < listRows.size(); ++j) {
                newList.push_back(GlobalDef::getInstance()->PLAY_LIST_DATA.play_list[ui.tabWidget_playList->currentIndex()].video_list[listRows[j]]);
            }
        }
        GlobalDef::getInstance()->PLAY_LIST_DATA.play_list[ui.tabWidget_playList->currentIndex()].video_list = newList;

        //for (int i = 0; i < newList.size(); ++i) {
        //    qDebug() << "========" << newList[i].video_path;
        //}

        //  update UI
        if (ui.tabWidget_playList->currentIndex() == 0) {
            ui.listWidget_playlist->clear();
            for (int i = 0; i < newList.size(); ++i) {
                QString mvPath = newList[i].video_path;
                mvPath = mvPath.replace("\\", "/");
                QStringList mvPathArr = mvPath.split("/");
                ui.listWidget_playlist->addItem(QString::number(i + 1) + "." + mvPathArr[mvPathArr.size() - 1]);
            }
        } else {
            ((PlayListPage *) ui.tabWidget_playList->widget(GlobalDef::getInstance()->PLAY_LIST_DATA.playlist_current_index))->getListWidget()->clear();
            for (int i = 0; i < newList.size(); ++i) {
                QString mvPath = newList[i].video_path;
                mvPath = mvPath.replace("\\", "/");
                QStringList mvPathArr = mvPath.split("/");
                ((PlayListPage *) ui.tabWidget_playList->widget(GlobalDef::getInstance()->PLAY_LIST_DATA.playlist_current_index))
                    ->getListWidget()
                    ->addItem(QString::number(i + 1) + "." + mvPathArr[mvPathArr.size() - 1]);
            }
        }

        //  write playList
        ApplicationSettings mWZSetting;
        mWZSetting.write_PlayList();
    }
}

void MainWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    logger->info("mouseDoubleClickEvent, screenName:{}", screen()->name().toStdString());

    if (ui.playWidget) {
        ui.playWidget->SetFullscreenMode(FullscreenMode::FULLSCREEN_KEEP_RATIO);
    }

    on_pushButton_fullScreen_clicked();
}

void MainWindow::onVolumeValueChanged(int value)
{
    GlobalDef::getInstance()->VOLUME_VAULE = value;
    float volume = (float) value / (float) ui.horizontalSlider_volume->maximum();
    playController_->setVolume(volume);
}

void MainWindow::on_pushButton_volume_clicked()
{
    GlobalDef::getInstance()->B_VOLUME_MUTE = ui.pushButton_volume->isChecked();
    if (ui.pushButton_volume->isChecked()) {
        playController_->toggleMute();
    } else {
        float volume = (float) GlobalDef::getInstance()->VOLUME_VAULE / (float) ui.horizontalSlider_volume->maximum();
        playController_->setVolume(volume);
        playController_->toggleMute();
    }
}

void MainWindow::replyVolumeUp()
{
    int valume = (ui.horizontalSlider_volume->value() + 5) > 100 ? 100 : (ui.horizontalSlider_volume->value() + 5);
    ui.horizontalSlider_volume->setValue(valume);
}

void MainWindow::replyVolumeDown()
{
    int valume = (ui.horizontalSlider_volume->value() - 5) < 0 ? 0 : (ui.horizontalSlider_volume->value() - 5);
    ui.horizontalSlider_volume->setValue(valume);
}

void MainWindow::replyVolumeMute()
{
    ui.pushButton_volume->setChecked(!ui.pushButton_volume->isChecked());
    on_pushButton_volume_clicked();
}

//void MainWindow::replyLoadSubtitle()
//{
//    qInfo() << __FUNCTION__ << ", 222222222222222222 loadSameName?" << GlobalDef::getInstance()->SUBTITLE_AUTO_LOAD_SAMEDIR_SAMENAME
//            << ", subtitlePath:" << GlobalDef::getInstance()->USER_SELECTED_SUBTITLE_FILENAME;
//    if (GlobalDef::getInstance()->SUBTITLE_AUTO_LOAD_SAMEDIR_SAMENAME) {
//        // 当采用同名字幕时，获取当前播放的文件名，并使用同文件夹中的同名subtitle
//        // 当采用同目录字幕时，获取当前目录下的.srt文件，并使用该srt文件
//        QString moviePath = GlobalDef::getInstance()->CURRENT_MOVIE_PATH;
//
//        return;
//    }
//
//    if (!GlobalDef::getInstance()->USER_SELECTED_SUBTITLE_FILENAME.isEmpty()) {
//        // 若当前subtitlePath 非空，则采用配置的文件
//    }
//}

//void MainWindow::replySubtitleChange()
//{
//    qInfo() << __FUNCTION__ << ", 22222222222222222     changeSubtitle:" << GlobalDef::getInstance()->USER_SELECTED_SUBTITLE_FILENAME;
//
//    if (!GlobalDef::getInstance()->USER_SELECTED_SUBTITLE_FILENAME.isEmpty()) {
//        // 若当前subtitlePath 非空，则采用配置的文件
//    }
//}

void MainWindow::onSubtitleSettingsChanged()
{
    if (playController_->isOpened()) {
        loadSubtitle(mCurMovieFilename, currentElapsedInSeconds_);
    }
}

void MainWindow::onUpdatePlayProcess(int64_t elapsedInSeconds)
{
    // elapsedInSeconds 是从 StereoVideoWidget 传来的秒数（已转换）
    logger->debug("onUpdatePlayProcess: elapsedInSeconds={}", elapsedInSeconds);
    if (playController_->isSeeking()) {
        logger->debug("onUpdatePlayProcess: Seeking, skipping progress update");
        return;
    }

    // 检查并更新进度条最大值（如果初始时获取的时长不正确）
    int currentMax = ui.horizontalSlider_playProgress->maximum();
    int64_t actualDurationSec = playController_->getDurationMs() / 1000;
    if (actualDurationSec > 0 && currentMax != actualDurationSec) {
        logger->info("onUpdatePlayProcess: Updating progress bar max from {} to {}", currentMax, actualDurationSec);
        ui.horizontalSlider_playProgress->setMaximum(static_cast<int>(actualDurationSec));
        
        // 同时更新总时长显示
        int totalHour = static_cast<int>(actualDurationSec / 3600);
        int totalMinute = static_cast<int>((actualDurationSec - totalHour * 3600) / 60);
        int totalSecond = static_cast<int>(actualDurationSec % 60);
        QString strTotalTime = QString("%1:%2:%3")
            .arg(totalHour, 2, 10, QLatin1Char('0'))
            .arg(totalMinute, 2, 10, QLatin1Char('0'))
            .arg(totalSecond, 2, 10, QLatin1Char('0'));
        ui.label_totalTime->setText(strTotalTime);
    }

    ui.horizontalSlider_playProgress->setValue(elapsedInSeconds);
    currentElapsedInSeconds_ = elapsedInSeconds;

    // 验证slider实际值（调试用）
    if (elapsedInSeconds % 5 == 0) { // 每5秒验证一次，避免日志过多
        int actualSliderValue = ui.horizontalSlider_playProgress->value();
        if (actualSliderValue != elapsedInSeconds) {
            logger->warn("onUpdatePlayProcess: Slider value mismatch! Expected={}, Actual={}", elapsedInSeconds, actualSliderValue);
        }
    }

    // 检查是否播放完成，如果完成则触发自动切换
    // 注意：只在非 Seeking 状态下检查，避免误判
    if (!playController_->isSeeking() && playController_->isOpened()) {
        if (playController_->checkAndStopIfFinished(elapsedInSeconds)) {
            logger->info("onUpdatePlayProcess: Playback finished detected, will trigger playlist switch");
            // checkAndStopIfFinished 已经设置了 quit_，会触发 stop，然后在 onPlayStateChanged 中处理切换
        }
    }

    int playHour = elapsedInSeconds / 3600;
    int playMinute = (elapsedInSeconds - playHour * 3600) / 60;
    int playSecond = elapsedInSeconds % 60;
    QString strPlayTime
        = QString("%1:%2:%3").arg(playHour, 2, 10, QLatin1Char('0')).arg(playMinute, 2, 10, QLatin1Char('0')).arg(playSecond, 2, 10, QLatin1Char('0'));
    ui.label_playTime->setText(strPlayTime);
}

void MainWindow::onPlaySliderCustomClicked(int seekTarget)
{
    logger->info("onPlaySliderCustomClicked, seekTarget: {} seconds", seekTarget);

    if (!playController_->isOpened()) {
        logger->warn("Movie not opened, cannot seek");
        return;
    }

    if (playController_->isSeeking()) {
        logger->warn("Already seeking, ignoring new seek request: {} seconds", seekTarget);
        return;
    }

    logger->info("Seeking from {} to {} seconds (UI request)", currentElapsedInSeconds_, seekTarget);

    // 设置 seeking 状态（UI 反馈）
    ui.playWidget->SetSeeking(__FUNCTION__, true);

    // 执行 seek（seekTarget 是秒，转换为毫秒）
    int64_t seekTargetMs = seekTarget * 1000;
    bool seekOk = playController_->seek(seekTargetMs);

    if (!seekOk) {
        logger->warn("PlayController::seek failed for target: {} ms", seekTargetMs);
        ui.playWidget->SetSeeking(__FUNCTION__, false);
    }
    // 如果成功，seeking 状态会在 onSeekingFinished 中清除
}

void MainWindow::onPlayProcessValueChanged(int value)
{
    // 进度条值变化时的处理（通常由用户拖动触发）
    // 注意：这里不直接 seek，而是通过 customSliderClicked 信号处理
    logger->debug("PlayProgress value changed to: {} seconds", value);
}

void MainWindow::onSeekingFinished(int64_t seekTargetMs)
{
    logger->info("MainWindow::onSeekingFinished: received positionMs={}ms ({} seconds)", seekTargetMs, seekTargetMs / 1000);

    // 检查是否seek失败（-1表示失败）
    if (seekTargetMs < 0) {
        logger->warn("MainWindow::onSeekingFinished: Seek failed (positionMs: {}), clearing seeking state only", seekTargetMs);
        ui.playWidget->SetSeeking(__FUNCTION__, false);
        return;
    }

    // 清除 seeking 状态
    ui.playWidget->SetSeeking(__FUNCTION__, false);

    // 更新进度条和字幕位置
    int posInSeconds = static_cast<int>(seekTargetMs / 1000);
    currentElapsedInSeconds_ = posInSeconds;

    // 关键修复：使用 blockSignals 避免与 valueChanged 信号循环触发
    // 同时防止与 StereoVideoWidget::updatePlayProcess 信号冲突
    // 确保进度条范围正确
    int maxValue = ui.horizontalSlider_playProgress->maximum();
    if (posInSeconds > maxValue) {
        logger->warn("MainWindow::onSeekingFinished: posInSeconds ({}) > maxValue ({}), clamping", posInSeconds, maxValue);
        posInSeconds = maxValue;
    }
    if (posInSeconds < 0) {
        logger->warn("MainWindow::onSeekingFinished: posInSeconds ({}) < 0, clamping to 0", posInSeconds);
        posInSeconds = 0;
    }

    ui.horizontalSlider_playProgress->blockSignals(true);
    ui.horizontalSlider_playProgress->setValue(posInSeconds);
    ui.horizontalSlider_playProgress->blockSignals(false);

    // 验证slider实际值
    int actualSliderValue = ui.horizontalSlider_playProgress->value();
    logger->info("UI updated: progress bar set to {} seconds, actual value: {}", posInSeconds, actualSliderValue);

    if (actualSliderValue != posInSeconds) {
        logger->warn("MainWindow::onSeekingFinished: Slider value mismatch! Expected={}, Actual={}, retrying...", posInSeconds, actualSliderValue);
        // 重试一次
        ui.horizontalSlider_playProgress->blockSignals(true);
        ui.horizontalSlider_playProgress->setValue(posInSeconds);
        ui.horizontalSlider_playProgress->blockSignals(false);
        actualSliderValue = ui.horizontalSlider_playProgress->value();
        if (actualSliderValue != posInSeconds) {
            logger->error("MainWindow::onSeekingFinished: Slider value still mismatch after retry! Expected={}, Actual={}", posInSeconds, actualSliderValue);
        }
    }

    // 更新字幕位置（使用毫秒）
    ui.playWidget->UpdateSubtitlePosition(seekTargetMs);

    update();
}

void MainWindow::onPlaySliderMousePressEvent()
{
    logger->debug("PlaySlider mouse pressed, current value: {} seconds", ui.horizontalSlider_playProgress->value());
}

void MainWindow::onPlaySliderMouseMoveEvent(int value)
{
    logger->debug("PlaySlider mouse moved, value: {} seconds", value);
}

void MainWindow::onPlaySliderMouseReleaseEvent()
{
    logger->debug("PlaySlider mouse released, final value: {} seconds", ui.horizontalSlider_playProgress->value());
}

void MainWindow::onPlaybackStateChanged(PlaybackState state)
{
    logger->info("MainWindow.onPlaybackStateChanged, state: {}", int(state));

    // 添加异常保护
    try {
        if (state == PlaybackState::Stopped) {
            // 清除上一次播放视频关联的 Subtitle 文件
            mLastMovieFn = mCurMovieFilename;
            if (!GlobalDef::getInstance()->USER_SELECTED_SUBTITLE_FILENAME.isEmpty()) {
                GlobalDef::getInstance()->USER_SELECTED_SUBTITLE_FILENAME.clear();
                logger->info("Cleared user selected subtitle file on playback stop");
            }
            mLastSubtitleFn = GlobalDef::getInstance()->USER_SELECTED_SUBTITLE_FILENAME;
            GlobalDef::getInstance()->USER_SELECTED_SUBTITLE_FILENAME = "";
        }

        // 重置 UI 状态
        ui.pushButton_playPause->setChecked(false);
        ui.horizontalSlider_playProgress->setValue(0);
        ui.horizontalSlider_playProgress->setEnabled(false);
        ui.horizontalSlider_volume->setEnabled(false);
        ui.label_totalTime->setText("00:00:00");
        ui.label_playTime->setText("00:00:00");

        // 检查是否是正常完播（用于播放列表切换）
        if (!isPlaybackFinished()) {
            logger->debug("onPlayStateChanged: Stop is not playback finished, skipping playlist switch");
            return;
        }

        // 根据播放循环设置处理完播后的行为
        handlePlaybackFinished();
    } catch (const std::exception &e) {
        logger->error("MainWindow::onPlaybackStateChanged: Exception: {}", e.what());
    } catch (...) {
        logger->error("MainWindow::onPlaybackStateChanged: Unknown exception");
    }
}

bool MainWindow::isPlaybackFinished() const
{
    int64_t movieTotalMs = playController_->getDurationMs();
    int movieTotalSeconds = movieTotalMs / 1000;

    // 如果总时长无效，不能判断为播放完成
    if (movieTotalMs <= 0) {
        logger->warn("Cannot determine playback finished: invalid duration ({} ms)", movieTotalMs);
        return false;
    }

    // 检查播放是否完成（时间差 <= 3秒 且 总时长 > 3秒）
    // 注意：使用 <= 而不是 <，确保在接近结束时能正确判断
    // 同时检查当前播放位置是否接近或超过总时长
    bool timeBasedFinished
        = (movieTotalSeconds > 3 && currentElapsedInSeconds_ >= movieTotalSeconds - 3 && currentElapsedInSeconds_ <= movieTotalSeconds + 2); // 允许2秒的误差

    // 使用综合检测方法
    bool comprehensiveFinished = playController_->isPlaybackFinished(currentElapsedInSeconds_);

    bool finished = timeBasedFinished || comprehensiveFinished;

    if (finished) {
        logger->info(
            "Playback finished detected: duration={}s, elapsed={}s, timeBased={}, comprehensive={}",
            movieTotalSeconds,
            currentElapsedInSeconds_,
            timeBasedFinished,
            comprehensiveFinished);
    }

    return finished;
}

void MainWindow::handlePlaybackFinished()
{
    // 使用 PlayListManager 的播放模式
    auto playMode = playListManager_->playMode();
    logger->info("Handling playback finished, play mode: {}", static_cast<int>(playMode));

    switch (playMode) {
    case PlayListManager::PlayMode::Sequential:
        logger->info("Sequential mode: Playing next video");
        if (!playListManager_->switchToNextVideo()) {
            logger->info("Sequential mode: No next video, stopping playback");
            ui.playWidget->StopRendering();
        } else {
            QString path = playListManager_->getCurrentVideoPath();
            if (!path.isEmpty()) {
                openPath(path, false);
            }
        }
        break;

    case PlayListManager::PlayMode::Loop:
        logger->info("Loop mode: Playing next video (with loop)");
        if (!playListManager_->switchToNextVideo()) {
            // 如果没有下一首，回到第一首
            if (playListManager_->getPlayListSize() > 0) {
                playListManager_->switchToVideo(0);
                QString firstVideoPath = playListManager_->getVideoPath(0);
                if (!firstVideoPath.isEmpty()) {
                    openPath(firstVideoPath, false);
                }
            }
        } else {
            QString path = playListManager_->getCurrentVideoPath();
            if (!path.isEmpty()) {
                openPath(path, false);
            }
        }
        break;

    case PlayListManager::PlayMode::Random:
        logger->info("Random mode: Playing random video");
        if (!playListManager_->switchToNextVideo()) {
            logger->info("Random mode: Failed to switch, stopping playback");
            ui.playWidget->StopRendering();
        } else {
            QString path = playListManager_->getCurrentVideoPath();
            if (!path.isEmpty()) {
                openPath(path, false);
            }
        }
        break;

    case PlayListManager::PlayMode::SingleLoop:
        logger->info("SingleLoop mode: Restarting current video");
        QTimer::singleShot(1000, [this] {
            if (playController_->isStopped()) {
                on_pushButton_playPause_clicked();
            }
        });
        break;

    default:
        logger->warn("Unknown play mode: {}, stopping playback", static_cast<int>(playMode));
        ui.playWidget->StopRendering();
        break;
    }
}

void MainWindow::playNextVideoInList(bool loop)
{
    if (!playListManager_) {
        logger->warn("MainWindow::playNextVideoInList: playListManager_ is null, using fallback logic");
        // 回退到旧逻辑
        int tabIndex = ui.tabWidget_playList->currentIndex();
        const auto &videoList = GlobalDef::getInstance()->PLAY_LIST_DATA.play_list[tabIndex].video_list;

        if (videoList.empty()) {
            logger->warn("Video list is empty, stopping playback");
            ui.playWidget->StopRendering();
            return;
        }

        int currentIndex = -1;
        if (tabIndex == 0) {
            currentIndex = ui.listWidget_playlist->currentRow();
        } else {
            auto *widget = dynamic_cast<PlayListPage *>(ui.tabWidget_playList->currentWidget());
            if (widget && widget->getListWidget()) {
                currentIndex = widget->getListWidget()->currentRow();
            }
        }

        int nextIndex = currentIndex + 1;
        if (loop && nextIndex >= static_cast<int>(videoList.size())) {
            nextIndex = 0; // 循环到开头
        }

        if (nextIndex >= 0 && nextIndex < static_cast<int>(videoList.size())) {
            QString mvPath = videoList[nextIndex].video_path;
            logger->info("Playing next video: index={}, path={}", nextIndex, mvPath.toStdString());

            if (tabIndex == 0) {
                ui.listWidget_playlist->setCurrentRow(nextIndex);
            } else {
                auto *widget = dynamic_cast<PlayListPage *>(ui.tabWidget_playList->currentWidget());
                if (widget && widget->getListWidget()) {
                    widget->getListWidget()->setCurrentRow(nextIndex);
                }
            }

            openPath(mvPath, false);
        } else {
            logger->info("Reached end of playlist, stopping playback");
            ui.playWidget->StopRendering();
        }
        return;
    }

    // 使用PlayListManager优化后的逻辑
    int currentPlayListIndex = 0; // 默认使用第一个播放列表
    int currentVideoIndex = playListManager_->getCurrentVideoIndex(currentPlayListIndex);
    
    // 检查是否有下一首
    bool hasNext = playListManager_->hasNextVideo();
    if (!hasNext && loop) {
        // 循环模式：如果没有下一首，回到第一首
        int listSize = playListManager_->getPlayListSize(currentPlayListIndex);
        if (listSize > 0) {
            QString firstVideoPath = playListManager_->getVideoPath(0, currentPlayListIndex);
            if (!firstVideoPath.isEmpty()) {
                logger->info("PlayNextVideoInList: Looping to first video: {}", firstVideoPath.toStdString());
                // 设置当前播放项（不发送信号，避免与openPath()重复触发）
                playListManager_->setCurrentVideo(0, currentPlayListIndex, false);
                // 更新UI
                if (currentPlayListIndex == 0) {
                    ui.listWidget_playlist->setCurrentRow(0);
                } else {
                    auto *widget = dynamic_cast<PlayListPage *>(ui.tabWidget_playList->widget(currentPlayListIndex));
                    if (widget && widget->getListWidget()) {
                        widget->getListWidget()->setCurrentRow(0);
                    }
                }
                openPath(firstVideoPath, false);
                return;
            }
        }
    }
    
    if (hasNext) {
        QString nextVideoPath = playListManager_->getNextVideoPath();
        if (!nextVideoPath.isEmpty()) {
            logger->info("PlayNextVideoInList: Switching to next video: {}", nextVideoPath.toStdString());
            int nextIndex = currentVideoIndex + 1;
            // 设置当前播放项（不发送信号，避免与openPath()重复触发）
            playListManager_->setCurrentVideo(nextIndex, currentPlayListIndex, false);
            // 更新UI
            if (currentPlayListIndex == 0) {
                ui.listWidget_playlist->setCurrentRow(nextIndex);
            } else {
                auto *widget = dynamic_cast<PlayListPage *>(ui.tabWidget_playList->widget(currentPlayListIndex));
                if (widget && widget->getListWidget()) {
                    widget->getListWidget()->setCurrentRow(nextIndex);
                }
            }
            openPath(nextVideoPath, false);
        } else {
            logger->warn("PlayNextVideoInList: Next video path is empty");
            ui.playWidget->StopRendering();
        }
    } else {
        logger->info("PlayNextVideoInList: No next video, stopping playback");
        ui.playWidget->StopRendering();
    }
}

// Widget 切换方法实现
void MainWindow::switchToVideoFile()
{
    if (cameraWidget_) {
        cameraWidget_->hide();
    }
    // 从摄像头切回视频时关闭摄像头，避免摄像头在后台继续运行（BUG 14 完全修复）
    if (cameraManager_) {
        cameraManager_->stopCamera();
        cameraManager_->closeCamera();
        if (logger) {
            logger->info("MainWindow: Camera stopped and closed when switching to video file");
        }
        // 同步菜单状态，使“打开摄像头”变为未勾选
        if (ui.actionOpenCamera && ui.actionOpenCamera->isChecked()) {
            ui.actionOpenCamera->setChecked(false);
        }
    }
    if (ui.playWidget) {
        ui.playWidget->show();
    }
    if (logger) {
        logger->info("MainWindow: Switched to video file widget");
    }
}

void MainWindow::switchToCamera()
{
    if (ui.playWidget) {
        ui.playWidget->hide();
    }
    if (cameraWidget_) {
        cameraWidget_->show();
        cameraWidget_->raise(); // 确保在最上层
        
        // 确保 OpenGL 上下文已初始化
        // 如果 widget 还没有被显示过，initializeGL 可能还没有被调用
        // 强制更新以触发 OpenGL 初始化
        cameraWidget_->update();
        
        if (logger) {
            logger->info("MainWindow: Switched to camera widget (visible: {}, size: {}x{})", 
                cameraWidget_->isVisible(), 
                cameraWidget_->width(), 
                cameraWidget_->height());
        }
    } else {
        if (logger) {
            logger->error("MainWindow: cameraWidget_ is null!");
        }
    }
}

void MainWindow::playRandomVideoInList()
{
    if (!playListManager_) {
        logger->warn("MainWindow::playRandomVideoInList: playListManager_ is null, using fallback logic");
        // 回退到旧逻辑
        int tabIndex = ui.tabWidget_playList->currentIndex();
        const auto &videoList = GlobalDef::getInstance()->PLAY_LIST_DATA.play_list[tabIndex].video_list;

        if (videoList.empty()) {
            logger->warn("Video list is empty, stopping playback");
            ui.playWidget->StopRendering();
            return;
        }

        int randomIndex = QRandomGenerator::global()->bounded(static_cast<int>(videoList.size()));
        QString mvPath = videoList[randomIndex].video_path;
        logger->info("Playing random video: index={}, path={}", randomIndex, mvPath.toStdString());

        if (tabIndex == 0) {
            openPath(mvPath, false);
        } else {
            auto *widget = dynamic_cast<PlayListPage *>(ui.tabWidget_playList->currentWidget());
            if (widget && widget->getListWidget()) {
                widget->getListWidget()->setCurrentRow(randomIndex);
                openPath(mvPath, false);
            }
        }
        return;
    }

    // 使用PlayListManager优化后的逻辑
    int currentPlayListIndex = 0; // 默认使用第一个播放列表
    int listSize = playListManager_->getPlayListSize(currentPlayListIndex);
    
    if (listSize == 0) {
        logger->warn("Video list is empty, stopping playback");
        ui.playWidget->StopRendering();
        return;
    }

    int randomIndex = QRandomGenerator::global()->bounded(listSize);
    QString mvPath = playListManager_->getVideoPath(randomIndex, currentPlayListIndex);
    
    if (!mvPath.isEmpty()) {
        logger->info("Playing random video: index={}, path={}", randomIndex, mvPath.toStdString());
        // 设置当前播放项（不发送信号，避免与openPath()重复触发）
        playListManager_->setCurrentVideo(randomIndex, currentPlayListIndex, false);
        
        // 更新UI
        if (currentPlayListIndex == 0) {
            ui.listWidget_playlist->setCurrentRow(randomIndex);
        } else {
            auto *widget = dynamic_cast<PlayListPage *>(ui.tabWidget_playList->widget(currentPlayListIndex));
            if (widget && widget->getListWidget()) {
                widget->getListWidget()->setCurrentRow(randomIndex);
            }
        }
        
        openPath(mvPath, false);
    } else {
        logger->warn("PlayRandomVideoInList: Random video path is empty");
        ui.playWidget->StopRendering();
    }
}

void MainWindow::resetLanguage()
{
    ui.retranslateUi(this);
    //    快捷键显示
    ui.actionOpen->setText(getWGText(ui.actionOpen->text()) + GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("FileTab_OpenFile").toString());

    ui.action_3D->setText(getWGText(ui.action_3D->text()) + GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_2D3D").toString());
    ui.actionInputLR->setText(getWGText(ui.actionInputLR->text()) + GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_LR").toString());
    ui.actionInputRL->setText(getWGText(ui.actionInputRL->text()) + GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_RL").toString());
    ui.actionInputUD->setText(getWGText(ui.actionInputUD->text()) + GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_UD").toString());
    ui.actionOutputVertical->setText(
        getWGText(ui.actionOutputVertical->text()) + GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_Vertical").toString());
    ui.actionOutputHorizontal->setText(
        getWGText(ui.actionOutputHorizontal->text()) + GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_Horizontal").toString());
    ui.actionOutputChess->setText(
        getWGText(ui.actionOutputChess->text()) + GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_Chess").toString());

    ui.actionOutputOnlyLeft->setText(
        getWGText(ui.actionOutputOnlyLeft->text()) + GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_3DOutput_OnlyLeft").toString());
    ui.action_3D_region->setText(
        getWGText(ui.action_3D_region->text()) + GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_Region").toString());

    ui.actionScreenshot->setText(
        getWGText(ui.actionScreenshot->text()) + GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("ImageTab_Screenshot").toString());

    ui.actionFullscreenPlus->setText(
        getWGText(ui.actionFullscreenPlus->text()) + GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_FullScreenPlus").toString());

    ui.actionFullscreen->setText(
        getWGText(ui.actionFullscreen->text()) + GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_FullScreen").toString());

    ui.actionParallaxAdd->setText(
        getWGText(ui.actionParallaxAdd->text()) + GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_IncreaseParallax").toString());
    ui.actionParallaxSub->setText(
        getWGText(ui.actionParallaxSub->text()) + GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_DecreaseParallax").toString());
    ui.actionParallaxReset->setText(
        getWGText(ui.actionParallaxReset->text()) + GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_ResetParallax").toString());
}

void MainWindow::initHotKey()
{
    if (shortcut_Esc == nullptr) {
        shortcut_Esc = new QShortcut(QKeySequence("Esc"), this);
        connect(shortcut_Esc, &QShortcut::activated, this, &MainWindow::reply_Esc_hotKey);
    }
    // Seek Tab (新增，用于自动化测试)
    if (shortcut_SeekLeft == nullptr) {
        shortcut_SeekLeft = new QShortcut(QKeySequence("Left"), this);
        connect(shortcut_SeekLeft, &QShortcut::activated, this, &MainWindow::onSeekLeftKey);
    }
    if (shortcut_SeekRight == nullptr) {
        shortcut_SeekRight = new QShortcut(QKeySequence("Right"), this);
        connect(shortcut_SeekRight, &QShortcut::activated, this, &MainWindow::onSeekRightKey);
    }
    if (shortcut_SeekLeftLarge == nullptr) {
        shortcut_SeekLeftLarge = new QShortcut(QKeySequence("Ctrl+Left"), this);
        connect(shortcut_SeekLeftLarge, &QShortcut::activated, this, &MainWindow::onSeekLeftLargeKey);
    }

    if (shortcut_SeekRightLarge == nullptr) {
        shortcut_SeekRightLarge = new QShortcut(QKeySequence("Ctrl+Right"), this);
        connect(shortcut_SeekRightLarge, &QShortcut::activated, this, &MainWindow::onSeekRightLargeKey);
    }

    //    file tab
    if (shortcut_OpenFile == nullptr) {
        shortcut_OpenFile = new QShortcut(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("FileTab_OpenFile"), this);
    } else {
        disconnect(shortcut_OpenFile, &QShortcut::activated, this, &MainWindow::on_actionOpen_triggered);
        shortcut_OpenFile->setKey(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("FileTab_OpenFile"));
    }

    if (shortcut_CloseFile == nullptr) {
        shortcut_CloseFile = new QShortcut(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("FileTab_CloseFile"), this);
    } else {
        disconnect(shortcut_CloseFile, &QShortcut::activated, this, &MainWindow::on_pushButton_stop_clicked);
        shortcut_CloseFile->setKey(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("FileTab_CloseFile"));
    }

    if (shortcut_Previous == nullptr) {
        shortcut_Previous = new QShortcut(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("FileTab_Previous"), this);
    } else {
        disconnect(shortcut_Previous, &QShortcut::activated, this, &MainWindow::on_pushButton_previous_clicked);
        shortcut_Previous->setKey(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("FileTab_Previous"));
    }

    if (shortcut_Next == nullptr) {
        shortcut_Next = new QShortcut(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("FileTab_Next"), this);
    } else {
        disconnect(shortcut_Next, &QShortcut::activated, this, &MainWindow::on_pushButton_next_clicked);
        shortcut_Next->setKey(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("FileTab_Next"));
    }
    //    play tab
    if (shortcut_Pause == nullptr) {
        shortcut_Pause = new QShortcut(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_Pause"), this);
    } else {
        disconnect(shortcut_Pause, &QShortcut::activated, this, &MainWindow::on_pushButton_playPause_clicked);
        shortcut_Pause->setKey(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_Pause"));
    }

    if (shortcut_2D3D == nullptr) {
        shortcut_2D3D = new QShortcut(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_2D3D"), this);
    } else {
        disconnect(shortcut_2D3D, &QShortcut::activated, this, &MainWindow::reply_action_3D_hotKey);
        shortcut_2D3D->setKey(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_2D3D"));
    }

    if (shortcut_LR == nullptr) {
        shortcut_LR = new QShortcut(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_LR"), this);
    } else {
        disconnect(shortcut_LR, &QShortcut::activated, this, &MainWindow::on_actionInputLR_triggered);
        shortcut_LR->setKey(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_LR"));
    }

    if (shortcut_RL == nullptr) {
        shortcut_RL = new QShortcut(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_RL"), this);
    } else {
        disconnect(shortcut_RL, &QShortcut::activated, this, &MainWindow::on_actionInputRL_triggered);
        shortcut_RL->setKey(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_RL"));
    }

    if (shortcut_UD == nullptr) {
        shortcut_UD = new QShortcut(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_UD"), this);
    } else {
        disconnect(shortcut_UD, &QShortcut::activated, this, &MainWindow::on_actionInputUD_triggered);
        shortcut_UD->setKey(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_UD"));
    }

    if (shortcut_Vertical == nullptr) {
        shortcut_Vertical = new QShortcut(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_Vertical"), this);
    } else {
        disconnect(shortcut_Vertical, &QShortcut::activated, this, &MainWindow::on_actionOutputVertical_triggered);
        shortcut_Vertical->setKey(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_Vertical"));
    }

    if (shortcut_Horizontal == nullptr) {
        shortcut_Horizontal = new QShortcut(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_Horizontal"), this);
    } else {
        disconnect(shortcut_Horizontal, &QShortcut::activated, this, &MainWindow::on_actionOutputHorizontal_triggered);
        shortcut_Horizontal->setKey(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_Horizontal"));
    }

    if (shortcut_Chess == nullptr) {
        shortcut_Chess = new QShortcut(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_Chess"), this);
    } else {
        disconnect(shortcut_Chess, &QShortcut::activated, this, &MainWindow::on_actionOutputChess_triggered);
        shortcut_Chess->setKey(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_Chess"));
    }

    if (shortcut_3DOutput_OnlyLeft == nullptr) {
        shortcut_3DOutput_OnlyLeft = new QShortcut(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_3DOutput_OnlyLeft"), this);
    } else {
        disconnect(shortcut_3DOutput_OnlyLeft, &QShortcut::activated, this, &MainWindow::on_actionOutputOnlyLeft_triggered);
        shortcut_3DOutput_OnlyLeft->setKey(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_3DOutput_OnlyLeft"));
    }

    if (shortcut_Region == nullptr) {
        shortcut_Region = new QShortcut(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_Region"), this);
    } else {
        disconnect(shortcut_Region, &QShortcut::activated, this, &MainWindow::reply_action_3D_region_hotKey);
        shortcut_Region->setKey(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_Region"));
    }
    //    image tab
    if (shortcut_Screenshot == nullptr) {
        shortcut_Screenshot = new QShortcut(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("ImageTab_Screenshot"), this);
    } else {
        disconnect(shortcut_Screenshot, &QShortcut::activated, this, &MainWindow::on_actionScreenshot_triggered);
        shortcut_Screenshot->setKey(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("ImageTab_Screenshot"));
    }
    //    voice tab
    if (shortcut_VolumeUp == nullptr) {
        shortcut_VolumeUp = new QShortcut(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("VoiceTab_Volup"), this);
    } else {
        disconnect(shortcut_VolumeUp, &QShortcut::activated, this, &MainWindow::replyVolumeUp);
        shortcut_VolumeUp->setKey(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("VoiceTab_Volup"));
    }

    if (shortcut_VolumeDown == nullptr) {
        shortcut_VolumeDown = new QShortcut(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("VoiceTab_Volde"), this);
    } else {
        disconnect(shortcut_VolumeDown, &QShortcut::activated, this, &MainWindow::replyVolumeDown);
        shortcut_VolumeDown->setKey(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("VoiceTab_Volde"));
    }

    if (shortcut_Mute == nullptr) {
        shortcut_Mute = new QShortcut(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("VoiceTab_Mute"), this);
    } else {
        disconnect(shortcut_Mute, &QShortcut::activated, this, &MainWindow::replyVolumeMute);
        shortcut_Mute->setKey(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("VoiceTab_Mute"));
    }

    ////    subtitle tab
    //if (shortcut_LoadSubtitle == nullptr) {
    //    shortcut_LoadSubtitle = new QShortcut(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("SubtitleTab_LoadSubtitle"), this);
    //} else {
    //    disconnect(shortcut_LoadSubtitle, &QShortcut::activated, this, &MainWindow::replyLoadSubtitle);
    //    shortcut_LoadSubtitle->setKey(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("SubtitleTab_LoadSubtitle"));
    //}

    //if (shortcut_SubtitleChange == nullptr) {
    //    shortcut_SubtitleChange = new QShortcut(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("SubtitleTab_ChangeSubtitle"), this);
    //} else {
    //    disconnect(shortcut_SubtitleChange, &QShortcut::activated, this, &MainWindow::replySubtitleChange);
    //    shortcut_SubtitleChange->setKey(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("SubtitleTab_ChangeSubtitle"));
    //}

    //    other tab
    if (shortcut_PlayList == nullptr) {
        shortcut_PlayList = new QShortcut(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_PlayList"), this);
    } else {
        disconnect(shortcut_PlayList, &QShortcut::activated, this, &MainWindow::reply_playlistShowBut_Clicked);
        shortcut_PlayList->setKey(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_PlayList"));
    }

    if (shortcut_FullScreenPlus == nullptr) {
        shortcut_FullScreenPlus = new QShortcut(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_FullScreenPlus"), this);
    } else {
        disconnect(shortcut_FullScreenPlus, &QShortcut::activated, this, &MainWindow::on_actionFullscreenPlus_triggered);
        shortcut_FullScreenPlus->setKey(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_FullScreenPlus"));
    }

    if (shortcut_FullScreen == nullptr) {
        shortcut_FullScreen = new QShortcut(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_FullScreen"), this);
    } else {
        disconnect(shortcut_FullScreen, &QShortcut::activated, this, &MainWindow::on_actionFullscreen_triggered);
        shortcut_FullScreen->setKey(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_FullScreen"));
    }

    if (shortcut_IncreaseParallax == nullptr) {
        shortcut_IncreaseParallax = new QShortcut(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_IncreaseParallax"), this);
    } else {
        disconnect(shortcut_IncreaseParallax, &QShortcut::activated, this, &MainWindow::on_actionParallaxAdd_triggered);
        shortcut_IncreaseParallax->setKey(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_IncreaseParallax"));
    }

    if (shortcut_DecreaseParallax == nullptr) {
        shortcut_DecreaseParallax = new QShortcut(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_DecreaseParallax"), this);
    } else {
        disconnect(shortcut_DecreaseParallax, &QShortcut::activated, this, &MainWindow::on_actionParallaxSub_triggered);
        shortcut_DecreaseParallax->setKey(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_DecreaseParallax"));
    }

    if (shortcut_ResetParallax == nullptr) {
        shortcut_ResetParallax = new QShortcut(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_ResetParallax"), this);
    } else {
        disconnect(shortcut_ResetParallax, &QShortcut::activated, this, &MainWindow::on_actionParallaxReset_triggered);
        shortcut_ResetParallax->setKey(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_ResetParallax"));
    }

    //======================================================================================================
    //    绑定
    connect(shortcut_OpenFile, &QShortcut::activated, this, &MainWindow::on_actionOpen_triggered);
    ui.actionOpen->setText(getWGText(ui.actionOpen->text()) + GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("FileTab_OpenFile").toString());
    connect(shortcut_CloseFile, &QShortcut::activated, this, &MainWindow::on_pushButton_stop_clicked);
    connect(shortcut_Previous, &QShortcut::activated, this, &MainWindow::on_pushButton_previous_clicked);
    connect(shortcut_Next, &QShortcut::activated, this, &MainWindow::on_pushButton_next_clicked);

    connect(shortcut_Pause, &QShortcut::activated, this, &MainWindow::on_pushButton_playPause_clicked);
    connect(shortcut_2D3D, &QShortcut::activated, this, &MainWindow::reply_action_3D_hotKey);
    ui.action_3D->setText(getWGText(ui.action_3D->text()) + GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_2D3D").toString());
    connect(shortcut_LR, &QShortcut::activated, this, &MainWindow::on_actionInputLR_triggered);
    ui.actionInputLR->setText(getWGText(ui.actionInputLR->text()) + GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_LR").toString());
    connect(shortcut_RL, &QShortcut::activated, this, &MainWindow::on_actionInputRL_triggered);
    ui.actionInputRL->setText(getWGText(ui.actionInputRL->text()) + GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_RL").toString());
    connect(shortcut_UD, &QShortcut::activated, this, &MainWindow::on_actionInputUD_triggered);
    ui.actionInputUD->setText(getWGText(ui.actionInputUD->text()) + GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_UD").toString());
    connect(shortcut_Vertical, &QShortcut::activated, this, &MainWindow::on_actionOutputVertical_triggered);
    ui.actionOutputVertical->setText(
        getWGText(ui.actionOutputVertical->text()) + GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_Vertical").toString());
    connect(shortcut_Horizontal, &QShortcut::activated, this, &MainWindow::on_actionOutputHorizontal_triggered);
    ui.actionOutputHorizontal->setText(
        getWGText(ui.actionOutputHorizontal->text()) + GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_Horizontal").toString());
    connect(shortcut_Chess, &QShortcut::activated, this, &MainWindow::on_actionOutputChess_triggered);
    ui.actionOutputChess->setText(
        getWGText(ui.actionOutputChess->text()) + GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_Chess").toString());
    connect(shortcut_3DOutput_OnlyLeft, &QShortcut::activated, this, &MainWindow::on_actionOutputOnlyLeft_triggered);
    ui.actionOutputOnlyLeft->setText(
        getWGText(ui.actionOutputOnlyLeft->text()) + GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_3DOutput_OnlyLeft").toString());
    connect(shortcut_Region, &QShortcut::activated, this, &MainWindow::reply_action_3D_region_hotKey);
    ui.action_3D_region->setText(
        getWGText(ui.action_3D_region->text()) + GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_Region").toString());

    connect(shortcut_Screenshot, &QShortcut::activated, this, &MainWindow::on_actionScreenshot_triggered);
    ui.actionScreenshot->setText(
        getWGText(ui.actionScreenshot->text()) + GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("ImageTab_Screenshot").toString());

    connect(shortcut_VolumeUp, &QShortcut::activated, this, &MainWindow::replyVolumeUp);
    connect(shortcut_VolumeDown, &QShortcut::activated, this, &MainWindow::replyVolumeDown);
    connect(shortcut_Mute, &QShortcut::activated, this, &MainWindow::replyVolumeMute);

    //connect(shortcut_LoadSubtitle, &QShortcut::activated, this, &MainWindow::replyLoadSubtitle);
    //connect(shortcut_SubtitleChange, &QShortcut::activated, this, &MainWindow::replySubtitleChange);

    connect(shortcut_PlayList, &QShortcut::activated, this, &MainWindow::reply_playlistShowBut_Clicked);

    connect(shortcut_FullScreenPlus, &QShortcut::activated, this, &MainWindow::on_actionFullscreenPlus_triggered);
    ui.actionFullscreenPlus->setText(
        getWGText(ui.actionFullscreenPlus->text()) + GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_FullScreenPlus").toString());

    connect(shortcut_FullScreen, &QShortcut::activated, this, &MainWindow::on_actionFullscreen_triggered);
    ui.actionFullscreen->setText(
        getWGText(ui.actionFullscreen->text()) + GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_FullScreen").toString());

    ui.actionParallaxAdd->setText(
        getWGText(ui.actionParallaxAdd->text()) + GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_IncreaseParallax").toString());
    connect(shortcut_IncreaseParallax, &QShortcut::activated, this, &MainWindow::on_actionParallaxAdd_triggered);
    ui.actionParallaxSub->setText(
        getWGText(ui.actionParallaxSub->text()) + GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_DecreaseParallax").toString());
    connect(shortcut_DecreaseParallax, &QShortcut::activated, this, &MainWindow::on_actionParallaxSub_triggered);
    ui.actionParallaxReset->setText(
        getWGText(ui.actionParallaxReset->text()) + GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_ResetParallax").toString());
    connect(shortcut_ResetParallax, &QShortcut::activated, this, &MainWindow::on_actionParallaxReset_triggered);
}

void MainWindow::on_actionFullscreen_triggered()
{
    if (mWindowSizeState != WINDOW_FULLSCREEN) {
        on_pushButton_fullScreen_clicked();
    }
    if (ui.playWidget) {
        ui.playWidget->SetFullscreenMode(FullscreenMode::FULLSCREEN_KEEP_RATIO);
    }
}

void MainWindow::on_actionFullscreenPlus_triggered()
{
    if (mWindowSizeState != WINDOW_FULLSCREEN) {
        on_pushButton_fullScreen_clicked();
    }
    if (ui.playWidget) {
        ui.playWidget->SetFullscreenMode(FullscreenMode::FULLSCREEN_PLUS_STRETCH);
    }
}

void MainWindow::reply_action_3D_region_hotKey()
{
    ui.action_3D_region->setChecked(!ui.action_3D_region->isChecked());
}

void MainWindow::on_action_3D_region_toggled(bool region3d_checked)
{
    if (region3d_checked) {
        mStereoOutputFormatWhenSwitchToRegion = mStereoOutputFormat;
        logger->info("enter 3D region, backup outputFormat:{}", (int) mStereoOutputFormatWhenSwitchToRegion);
        ui.playWidget->SetStereoFormat(STEREO_FORMAT_3D);
        ui.playWidget->SetStereoOutputFormat(STEREO_OUTPUT_FORMAT_ONLY_LEFT);

        mDrawWidget->resize(ui.playWidget->width(), ui.playWidget->height());
        mDrawWidget->move(0, 0);
        mDrawWidget->show();
    } else {
        mDrawWidget->hide();
        ui.playWidget->CancelStereoRegion();
        logger->info("leaving 3D region, restore backup outputFormat:{}", (int) mStereoOutputFormatWhenSwitchToRegion);
        ui.playWidget->SetStereoOutputFormat(mStereoOutputFormatWhenSwitchToRegion);
    }
}

//void MainWindow::on_action_2D_toggled(bool stereo_checked)
//{
//    if (stereo_checked) {
//        mStereoFormat = StereoFormat::STEREO_FORMAT_NORMAL_2D;
//        ui.playWidget->SetStereoFormat(StereoFormat(STEREO_FORMAT_NORMAL_2D));
//        onStereoFormatChanged(StereoFormat(STEREO_FORMAT_NORMAL_2D), mStereoInputFormat, mStereoOutputFormat);
//
//        //ui.action_3D->setDisabled(true);
//        ui.actionInputLR->setDisabled(true);
//        ui.actionInputRL->setDisabled(true);
//        ui.actionInputUD->setDisabled(true);
//        ui.comboBox_3D_input->setEnabled(false);
//        ui.actionOutputHorizontal->setDisabled(true);
//        ui.actionOutputVertical->setDisabled(true);
//        ui.actionOutputChess->setDisabled(true);
//        ui.actionOutputOnlyLeft->setDisabled(true);
//        //ui.switchButton_3D2D->setDisabled(true);
//    } else {
//        mStereoFormat = StereoFormat::STEREO_FORMAT_3D;
//        ui.playWidget->SetStereoFormat(StereoFormat(STEREO_FORMAT_3D));
//        onStereoFormatChanged(StereoFormat(STEREO_FORMAT_3D), mStereoInputFormat, mStereoOutputFormat);
//
//        //ui.action_3D->setEnabled(true);
//        //ui.actionInputLR->setEnabled(true);
//        //ui.actionInputRL->setEnabled(true);
//        //ui.actionInputUD->setEnabled(true);
//        //ui.actionOutputHorizontal->setEnabled(true);
//        //ui.actionOutputVertical->setEnabled(true);
//        //ui.actionOutputChess->setEnabled(true);
//        //ui.actionOutputOnlyLeft->setEnabled(true);
//        //ui.switchButton_3D2D->setEnabled(true);
//    }
//}

void MainWindow::onTakeScreenShot()
{
    if (ui.playWidget && playController_->isOpened()) {
        ui.playWidget->TakeScreenshot();
    }
}

void MainWindow::reply_Esc_hotKey()
{
    if (mWindowSizeState == WINDOW_FULLSCREEN) {
        on_pushButton_fullScreen_clicked();
    }
}

/// <summary>
///  当经由文件名解析确定3D或2D格式后，
///  该解析的播放的格式应该通知UI，否则播放列表中不一样的文件名会导致UI显示的状态与实际播放状态不一致
/// </summary>
/// <param name="stereoFormat"></param>
/// <param name="stereoInputFormat"></param>
/// <param name="stereoOutputFormat"></param>
void MainWindow::onStereoFormatChanged(StereoFormat stereoFormat, StereoInputFormat stereoInputFormat, StereoOutputFormat stereoOutputFormat)
{
    mStereoInputFormat = stereoInputFormat;
    mStereoOutputFormat = stereoOutputFormat;
    if (!ui.action_2D->isChecked()) {
        if (stereoOutputFormat != StereoOutputFormat::STEREO_OUTPUT_FORMAT_ONLY_LEFT) {
            //  stereo 3D 分支
            //  stereo -- stereo_3D
            ui.action_3D->setChecked(true);
            //  input -- stereo_3D
            ui.comboBox_3D_input->setEnabled(true);
            ui.actionInputLR->setEnabled(true);
            ui.actionInputRL->setEnabled(true);
            ui.actionInputUD->setEnabled(true);
            switch (stereoInputFormat) {
            case STEREO_INPUT_FORMAT_LR:
            default:
                ui.actionInputLR->setChecked(true);
                break;
            case STEREO_INPUT_FORMAT_RL:
                ui.actionInputRL->setChecked(true);
                break;
            case STEREO_INPUT_FORMAT_UD:
                ui.actionInputUD->setChecked(true);
                break;
            }

            //  output -- stereo_3D
            ui.actionOutputHorizontal->setEnabled(true);
            ui.actionOutputVertical->setEnabled(true);
            ui.actionOutputChess->setEnabled(true);
            ui.actionOutputOnlyLeft->setDisabled(true);
            switch (stereoOutputFormat) {
            case STEREO_OUTPUT_FORMAT_HORIZONTAL:
                ui.actionOutputHorizontal->setChecked(true);
                break;
            case STEREO_OUTPUT_FORMAT_VERTICAL:
            default:
                ui.actionOutputVertical->setChecked(true);
                break;
            case STEREO_OUTPUT_FORMAT_CHESS:
                ui.actionOutputChess->setChecked(true);
                break;
            case STEREO_OUTPUT_FORMAT_ONLY_LEFT:
                ui.actionOutputOnlyLeft->setChecked(true);
                break;
            }
        } else {
            //  3D left
            //  stereo -- 3D_left
            ui.action_3D_left->setChecked(true);
            //  input -- 3D_left
            ui.comboBox_3D_input->setEnabled(true);
            ui.actionInputLR->setEnabled(true);
            ui.actionInputRL->setEnabled(true);
            ui.actionInputUD->setEnabled(true);
            switch (stereoInputFormat) {
            case STEREO_INPUT_FORMAT_LR:
            default:
                ui.actionInputLR->setChecked(true);
                break;
            case STEREO_INPUT_FORMAT_RL:
                ui.actionInputRL->setChecked(true);
                break;
            case STEREO_INPUT_FORMAT_UD:
                ui.actionInputUD->setChecked(true);
                break;
            }

            //  output -- 3D_left
            ui.actionOutputHorizontal->setChecked(false);
            ui.actionOutputVertical->setChecked(false);
            ui.actionOutputChess->setChecked(false);
            ui.actionOutputOnlyLeft->setChecked(true);
            ui.actionOutputHorizontal->setDisabled(true);
            ui.actionOutputVertical->setDisabled(true);
            ui.actionOutputChess->setDisabled(true);
            ui.actionOutputOnlyLeft->setEnabled(true);
        }
    } else {
        //  2D source
        //  stereo -- 2D_source
        ui.action_2D->setChecked(true);
        //  input -- 2D_source
        ui.comboBox_3D_input->setEnabled(false);
        ui.actionInputLR->setEnabled(false);
        ui.actionInputRL->setEnabled(false);
        ui.actionInputUD->setEnabled(false);
        //  output -- 2D_source
        ui.actionOutputHorizontal->setEnabled(false);
        ui.actionOutputVertical->setEnabled(false);
        ui.actionOutputChess->setEnabled(false);
        ui.actionOutputOnlyLeft->setEnabled(false);
    }
}

bool MainWindow::loadSubtitle(QString curMovieFilename, int seekPosInSeconds)
{
    // 载入字幕
    if (GlobalDef::getInstance()->SUBTITLE_AUTO_LOAD_SAMEDIR_SAMENAME) {
        // 当采用同名字幕时，获取当前播放的文件名，并使用同文件夹中的同名subtitle
        // 当采用同目录字幕时，获取当前目录下的.srt文件，并使用该srt文件

        QFileInfo movieFileInfo(curMovieFilename);
        QString baseName = movieFileInfo.baseName();
        QDir fileDir = movieFileInfo.dir();
        QFileInfo targetSubtitleFI = QFileInfo(fileDir, baseName + ".srt");

        if (false == targetSubtitleFI.exists()) {
            logger->warn("1.1 targetSubtitle:{}  not exists.", targetSubtitleFI.absoluteFilePath().toStdString());
            return false;
        } else {
            if (ui.playWidget) {
                // 使用absoluteFilePath()确保使用绝对路径，避免路径问题
                QString absoluteSubtitlePath = targetSubtitleFI.absoluteFilePath();
                logger->info("Loading subtitle from: {}", absoluteSubtitlePath.toStdString());
                bool loadOk = ui.playWidget->LoadSubtitle(absoluteSubtitlePath);
                if (seekPosInSeconds >= 0 && loadOk) {
                    logger->info("1.2.1 loadSubtitle ok & start to set seekPos:{}", seekPosInSeconds);
                    ui.playWidget->UpdateSubtitlePosition(seekPosInSeconds * 1000);

                    return true;
                } else {
                    logger->warn("1.2.2 loadSubtitle ok?{} or seekPosInSeconds <=0, seekPosInSeconds:{}", loadOk, seekPosInSeconds);
                    return false;
                }
            }
        }
    }

    logger->info("loadSubtitle, name:{}", GlobalDef::getInstance()->USER_SELECTED_SUBTITLE_FILENAME.toStdString());
    if (!GlobalDef::getInstance()->USER_SELECTED_SUBTITLE_FILENAME.isEmpty()) {
        // 若当前subtitlePath 非空，则采用配置的文件
        QString subtitleFilename = GlobalDef::getInstance()->USER_SELECTED_SUBTITLE_FILENAME;
        QFileInfo subtitleFileInfo(subtitleFilename);
        if (false == subtitleFileInfo.exists()) {
            logger->warn("2.1 targetSubtitle: {} not exists.", subtitleFilename.toStdString());
            return false;
        } else {
            if (ui.playWidget) {
                // 使用absoluteFilePath()确保使用绝对路径
                QString absoluteSubtitlePath = subtitleFileInfo.absoluteFilePath();
                logger->info("Loading user selected subtitle from: {}", absoluteSubtitlePath.toStdString());
                bool loadOk = ui.playWidget->LoadSubtitle(absoluteSubtitlePath);
                if (seekPosInSeconds >= 0 && loadOk) {
                    logger->info("2.2.1 loadSubtitle ok & start to set seekPos:{}", seekPosInSeconds);
                    ui.playWidget->UpdateSubtitlePosition(seekPosInSeconds * 1000);
                    return true;
                } else {
                    logger->warn("2.2.2 loadSubtitle ok?{} or seekPosInSeconds <=0, seekPosInSeconds:{}", loadOk, seekPosInSeconds);
                    return false;
                }
            }
        }
    }
}

void MainWindow::onPrimaryScreenChanged(QScreen *screen)
{
    logger->info(
        "MainWindow::onPrimaryScreenChanged, screenSize:{}, screenName:{}, screenGeometry:{}x{}",
        qGuiApp->screens().size(),
        screen->name().toStdString(),
        screen->geometry().width(),
        screen->geometry().height());
}

void MainWindow::onScreenAdded(QScreen *screen)
{
    logger->info(
        "MainWindow::onScreenAdded, screenCount:{}, screenName:{}, screenGeometry:{}x{}",
        qGuiApp->screens().size(),
        screen->name().toStdString(),
        screen->geometry().width(),
        screen->geometry().height());
}

void MainWindow::onScreenRemoved(QScreen *screen)
{
    logger->info(
        "MainWindow::onScreenRemoved, screenCount:{}, screenName:{}, screenGeometry:{}x{}",
        qGuiApp->screens().size(),
        screen->name().toStdString(),
        screen->geometry().width(),
        screen->geometry().height());
}

void MainWindow::onSettingsDialogAccepted()
{
    logger->info("onSettingsDialogAccepted");
}

void MainWindow::onSettingsDialogRejected()
{
    logger->info("onSettingsDialogRejected");
}

// Seek左（少量seek）
void MainWindow::onSeekLeftKey()
{
    if (playController_ && playController_->isOpened()) {
        // seek当前时间-5秒，seek() 参数为毫秒
        int64_t currentPosMs = playController_->getCurrentPositionMs();
        int64_t seekPosMs = std::max(static_cast<int64_t>(0), currentPosMs - 5000);
        playController_->seek(seekPosMs);
        logger->info("Seek left 5s, from {}ms to {}ms", currentPosMs, seekPosMs);
    }
}

// Seek右（少量seek）
void MainWindow::onSeekRightKey()
{
    if (playController_ && playController_->isOpened()) {
        // seek当前时间+5秒，seek() 参数为毫秒
        int64_t currentPosMs = playController_->getCurrentPositionMs();
        int64_t durationMs = playController_->getDurationMs();
        int64_t seekPosMs = std::min(durationMs - 1000, currentPosMs + 5000);
        playController_->seek(seekPosMs);
        logger->info("Seek right 5s, from {}ms to {}ms", currentPosMs, seekPosMs);
    }
}

// Seek左大量（10%seek）
void MainWindow::onSeekLeftLargeKey()
{
    if (playController_ && playController_->isOpened()) {
        // seek当前时间-10%，seek() 参数为毫秒
        int64_t currentPosMs = playController_->getCurrentPositionMs();
        int64_t durationMs = playController_->getDurationMs();
        int64_t seekPosMs = std::max(static_cast<int64_t>(0), currentPosMs - durationMs / 10);
        playController_->seek(seekPosMs);
        logger->info("Seek left 10%, from {}ms to {}ms", currentPosMs, seekPosMs);
    }
}

// Seek右大量（10%seek）
void MainWindow::onSeekRightLargeKey()
{
    if (playController_ && playController_->isOpened()) {
        // seek当前时间+10%，seek() 参数为毫秒
        int64_t currentPosMs = playController_->getCurrentPositionMs();
        int64_t durationMs = playController_->getDurationMs();
        int64_t seekPosMs = std::min(durationMs - 1000, currentPosMs + durationMs / 10);
        playController_->seek(seekPosMs);
        logger->info("Seek right 10%, from {}ms to {}ms", currentPosMs, seekPosMs);
    }
}