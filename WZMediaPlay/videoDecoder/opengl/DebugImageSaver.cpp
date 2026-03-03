/*
    DebugImageSaver: 异步图片保存器实现
*/

#include "DebugImageSaver.h"
#include "spdlog/spdlog.h"
#include <QDir>
#include <QDateTime>
#include <QFileInfo>

extern spdlog::logger *logger;

DebugImageSaver::DebugImageSaver(QObject *parent)
    : QObject(parent)
    , workerThread_(nullptr)
    , shouldStop_(false)
{
    workerThread_ = new QThread(this);
    // 将对象移动到工作线程
    this->moveToThread(workerThread_);
    // 连接线程启动信号到处理函数
    connect(workerThread_, &QThread::started, this, &DebugImageSaver::processQueue);
    workerThread_->start();
    
    if (logger) {
        logger->info("DebugImageSaver: Started background thread for image saving");
    }
}

DebugImageSaver::~DebugImageSaver()
{
    stop();
}

void DebugImageSaver::enqueueImage(const QImage& image, const QString& filename)
{
    QMutexLocker locker(&queueMutex_);
    
    // 限制队列大小，避免内存占用过大（最多保留100张图片）
    if (imageQueue_.size() >= 100) {
        if (logger) {
            static int dropCount = 0;
            if (dropCount++ % 100 == 0) {
                logger->warn("DebugImageSaver: Queue full, dropping image. Total dropped: {}", dropCount);
            }
        }
        return;
    }
    
    ImageTask task;
    task.image = image.copy();  // 深拷贝，确保数据安全
    task.filename = filename;
    imageQueue_.enqueue(task);
    queueCondition_.wakeOne();
}

void DebugImageSaver::stop()
{
    {
        QMutexLocker locker(&queueMutex_);
        shouldStop_ = true;
        queueCondition_.wakeAll();
    }
    
    if (workerThread_ && workerThread_->isRunning()) {
        workerThread_->wait(5000);  // 等待最多5秒
        if (workerThread_->isRunning()) {
            if (logger) logger->warn("DebugImageSaver: Worker thread did not stop in time");
        }
    }
}

void DebugImageSaver::processQueue()
{
    // 这个函数在工作线程中运行
    while (true) {
        ImageTask task;
        bool hasTask = false;
        
        {
            QMutexLocker locker(&queueMutex_);
            
            while (imageQueue_.isEmpty() && !shouldStop_) {
                queueCondition_.wait(&queueMutex_);
            }
            
            if (shouldStop_ && imageQueue_.isEmpty()) {
                break;
            }
            
            if (!imageQueue_.isEmpty()) {
                task = imageQueue_.dequeue();
                hasTask = true;
            }
        }
        
        if (hasTask) {
            // 确保目录存在
            QFileInfo fileInfo(task.filename);
            QDir dir = fileInfo.absoluteDir();
            if (!dir.exists()) {
                dir.mkpath(".");
            }
            
            // 保存图片
            if (task.image.save(task.filename, "PNG", 100)) {
                if (logger) {
                    static int savedCount = 0;
                    if (savedCount++ % 50 == 0) {
                        logger->debug("DebugImageSaver: Saved {} images, latest: {}", savedCount, task.filename.toStdString());
                    }
                }
            } else {
                if (logger) {
                    logger->warn("DebugImageSaver: Failed to save image: {}", task.filename.toStdString());
                }
            }
        }
    }
    
    if (logger) {
        logger->info("DebugImageSaver: Worker thread stopped");
    }
}
