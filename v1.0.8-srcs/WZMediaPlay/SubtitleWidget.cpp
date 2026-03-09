#include "SubtitleWidget.h"
#include "GlobalDef.h"
#include "spdlog/spdlog.h"
#include <QDateTime>
#include <QPainter>
#include <QResizeEvent>
#include <QFileInfo>
#include <QFile>
#include <QTemporaryFile>
#include <QTextStream>
#ifdef _WIN32
#include <QDir>
#endif

SubtitleWidget::SubtitleWidget(QWidget *parent)
    : QWidget(parent)
{
    textPen.setColor(Qt::white);
    textFont.setPixelSize(20);

    mSubtitleDisplayTimer = new QTimer(this);
    connect(mSubtitleDisplayTimer, SIGNAL(timeout()), this, SLOT(onSubtitleDisplayTimerTimeout()), Qt::DirectConnection);
    mSubtitleDisplayTimer->setInterval(mSubtitleCheckingInterval); //set 100ms as interval
    mSubtitleItems.clear();
}

SubtitleWidget::~SubtitleWidget()
{
    if (mSubtitleDisplayTimer) {
        mSubtitleDisplayTimer->stop();
        delete mSubtitleDisplayTimer;
        mSubtitleDisplayTimer = nullptr;
    }
    mSubtitleItems.clear();

    if (parser) {
        delete parser;
        parser = nullptr;
    }

    if (subParserFactory) {
        delete subParserFactory;
        subParserFactory = nullptr;
    }
    
#ifdef _WIN32
    if (tempSubtitleFile_) {
        delete tempSubtitleFile_;
        tempSubtitleFile_ = nullptr;
    }
#endif
}

void SubtitleWidget::paintEvent(QPaintEvent *event)
{
    if (mDisplaySubtitleFlag) {
        QPainter painter;
        painter.begin(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(textPen);
        painter.setFont(textFont);
        painter.setBackground(QBrush(QColor(Qt::green)));
        painter.drawText(rect(), Qt::AlignCenter, QString::fromStdString(mCurSubtitleText));
        painter.end();
    } else {
        QPainter painter;
        painter.begin(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(textPen);
        painter.setFont(textFont);
        painter.setBackground(QBrush(QColor(Qt::green)));
        painter.drawText(rect(), Qt::AlignCenter, "");
        painter.end();
    }
}

bool SubtitleWidget::SetSubtitleFile(QString &subtitleFilename)
{
    if (false == subtitleFilename.endsWith(".srt")) {
        logger->warn("subtitleFilename {} not ends with srt", subtitleFilename.toStdString());
        mIsEnabled = false;
        return false;
    }

    // 检查文件是否存在
    QFileInfo fileInfo(subtitleFilename);
    if (!fileInfo.exists()) {
        logger->error("Subtitle file does not exist: {}", subtitleFilename.toStdString());
        mIsEnabled = false;
        return false;
    }

    logger->info("Loading subtitle file: {}", subtitleFilename.toStdString());

    mSubtitleFilename = subtitleFilename;
    if (subParserFactory) {
        delete subParserFactory;
        subParserFactory = nullptr;
    }
    if (parser) {
        delete parser;
        parser = nullptr;
    }
    
#ifdef _WIN32
    // 清理之前的临时文件
    if (tempSubtitleFile_) {
        delete tempSubtitleFile_;
        tempSubtitleFile_ = nullptr;
    }
#endif

    // 在Windows上，std::ifstream无法正确处理UTF-8编码的中文路径
    // 使用QFile读取文件内容，然后写入临时文件（使用ASCII路径）进行解析
    
    QString filePathForParser;
    
#ifdef _WIN32
    // Windows平台：使用QFile读取文件，然后创建临时文件
    QFile file(subtitleFilename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        logger->error("Failed to open subtitle file: {}, error: {}", 
                     subtitleFilename.toStdString(), file.errorString().toStdString());
        mIsEnabled = false;
        return false;
    }
    
    // 读取文件内容
    QByteArray fileContent = file.readAll();
    file.close();
    
    if (fileContent.isEmpty()) {
        logger->error("Subtitle file is empty: {}", subtitleFilename.toStdString());
        mIsEnabled = false;
        return false;
    }
    
    logger->debug("Read subtitle file content, size: {} bytes", fileContent.size());
    
    // 创建临时文件（使用ASCII路径，避免中文路径问题）
    tempSubtitleFile_ = new QTemporaryFile();
    // 设置临时文件模板，确保生成.srt扩展名
    tempSubtitleFile_->setFileTemplate(QDir::tempPath() + "/subtitle_XXXXXX.srt");
    // 设置不自动删除，直到我们明确删除它
    tempSubtitleFile_->setAutoRemove(false);
    if (!tempSubtitleFile_->open()) {
        logger->error("Failed to create temporary file for subtitle parsing");
        delete tempSubtitleFile_;
        tempSubtitleFile_ = nullptr;
        mIsEnabled = false;
        return false;
    }
    
    // 写入文件内容
    tempSubtitleFile_->write(fileContent);
    tempSubtitleFile_->flush();
    // 关闭文件句柄，但文件仍然存在（因为setAutoRemove(false)）
    tempSubtitleFile_->close();
    
    // 获取临时文件路径（ASCII路径）
    filePathForParser = tempSubtitleFile_->fileName();
    logger->debug("Using temporary file for parsing: {}", filePathForParser.toStdString());
    
    // 使用临时文件路径进行解析（使用Local8Bit，因为临时文件路径是ASCII）
    subParserFactory = new SubtitleParserFactory(filePathForParser.toLocal8Bit().constData());
#else
    // Linux/Mac平台：直接使用UTF-8路径
    filePathForParser = subtitleFilename;
    subParserFactory = new SubtitleParserFactory(filePathForParser.toUtf8().constData());
#endif
    
    parser = subParserFactory->getParser();
    if (!parser) {
        logger->error("Failed to create subtitle parser");
        mIsEnabled = false;
        return false;
    }

    mSubtitleItems = parser->getSubtitles();
    logger->info("Parsed subtitle items count: {}", mSubtitleItems.size());
    
    if (mSubtitleItems.size() < 1) {
        logger->warn("subTitles size error. subtitleItems.size: {}, file: {}", 
                    mSubtitleItems.size(), subtitleFilename.toStdString());
        
        // 尝试读取文件内容并打印前几行，用于调试
        QFile debugFile(subtitleFilename);
        if (debugFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&debugFile);
            in.setEncoding(QStringConverter::Encoding::Utf8);
            QString firstLines;
            for (int i = 0; i < 10 && !in.atEnd(); i++) {
                firstLines += in.readLine() + "\n";
            }
            logger->debug("First 10 lines of subtitle file:\n{}", firstLines.toStdString());
            debugFile.close();
        }
        
        mIsEnabled = false;
        return false;
    }

    mCurSubtitleItemIndex = 0;
    mElapsedSinceStart = 0;
    mCurBeginTime = mSubtitleItems[0]->getStartTime();
    mCurEndTime = mSubtitleItems[0]->getEndTime();
    mIsEnabled = true;
    return true;
}

bool SubtitleWidget::Start()
{
    if (false == mIsEnabled) {
        return false;
    }

    mSubtitleDisplayTimer->start();
    mPauseFlag = false;
    return true;
}

bool SubtitleWidget::IsStarted()
{
    return mSubtitleDisplayTimer->isActive();
}

bool SubtitleWidget::Stop()
{
    mSubtitleDisplayTimer->stop();
    mDisplaySubtitleFlag = false;
    mCurSubtitleText = "";
    mSubtitleItems.clear();
    if (subParserFactory) {
        delete subParserFactory;
        subParserFactory = nullptr;
    }
    if (parser) {
        delete parser;
        parser = nullptr;
    }
    
#ifdef _WIN32
    // 清理临时文件
    if (tempSubtitleFile_) {
        // 删除临时文件（如果还存在）
        if (QFile::exists(tempSubtitleFile_->fileName())) {
            QFile::remove(tempSubtitleFile_->fileName());
        }
        delete tempSubtitleFile_;
        tempSubtitleFile_ = nullptr;
    }
#endif
    
    repaint();
    return true;
}

void SubtitleWidget::onSubtitleDisplayTimerTimeout()
{
    logger->debug(
        "!!!!onSubtitleDisplayTimerTimeout, check current elapsed, mElapsedSinceStart:{},mCurBeginTime:{}, mCurEndTime:{}",
        mElapsedSinceStart,
        mCurBeginTime,
        mCurEndTime);
    if (mPauseFlag) {
        logger->info("pause, not rendering");
        return;
    }

    mElapsedSinceStart += mSubtitleCheckingInterval;

    if (mCurBeginTime < mElapsedSinceStart && mElapsedSinceStart < mCurEndTime) {
        logger->debug(" should display, mElapsedSinceStart:{},mCurBeginTime:{}, mCurEndTime:{}", mElapsedSinceStart, mCurBeginTime, mCurEndTime);
        mDisplaySubtitleFlag = true;
        mCurSubtitleText = mSubtitleItems.at(mCurSubtitleItemIndex)->getText();
        repaint();
    } else {
        if (mElapsedSinceStart < mCurBeginTime) {
            mDisplaySubtitleFlag = false;
            repaint();
        } else if (mElapsedSinceStart > mCurEndTime) {
            mDisplaySubtitleFlag = false;
            //当前已播放完毕，更新下一条待显示subtitle
            mCurSubtitleItemIndex += 1;
            if (mCurSubtitleItemIndex >= mSubtitleItems.size()) {
                mDisplaySubtitleFlag = false;
                return;
            }
            mCurBeginTime = mSubtitleItems.at(mCurSubtitleItemIndex)->getStartTime();
            mCurEndTime = mSubtitleItems.at(mCurSubtitleItemIndex)->getEndTime();
            logger->debug(" index update, mElapsedSinceStart:{},mCurBeginTime:{}, mCurEndTime:{}", mElapsedSinceStart, mCurBeginTime, mCurEndTime);

            repaint();
        }
    }
}

bool SubtitleWidget::Seek(int64_t timestamp)
{
    if (false == mIsEnabled) {
        return false;
    }

    //根据timestamp搜索合适的curSubtitleItemIndex
    logger->info(" seek timestamp:{}", timestamp);
    mElapsedSinceStart = timestamp;
    int totalSize = mSubtitleItems.size();
    for (int i = 0; i < totalSize - 1; i++) {
        long beginTime = mSubtitleItems[i]->getStartTime();
        long nextBeginTime = mSubtitleItems[i + 1]->getStartTime();
        if (mElapsedSinceStart > beginTime && mElapsedSinceStart < nextBeginTime) {
            logger->info("found index:{}", i);
            mCurSubtitleItemIndex = i;
            mCurBeginTime = mSubtitleItems.at(mCurSubtitleItemIndex)->getStartTime();
            mCurEndTime = mSubtitleItems.at(mCurSubtitleItemIndex)->getEndTime();
            logger->debug(" index update, mCurSubtitleItemIndex:{},mCurBeginTime:{}, mCurEndTime:{}", mCurSubtitleItemIndex, mCurBeginTime, mCurEndTime);
            repaint();
            return true;
        }
    }
    return false;
}

bool SubtitleWidget::PlayPause(bool pauseFlag)
{
    if (pauseFlag) {
        mSubtitleDisplayTimer->stop();
        mPauseFlag = true;
    } else {
        mSubtitleDisplayTimer->start();
        mPauseFlag = false;
    }
    return true;
}