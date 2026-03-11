/*
    DebugImageSaver: 异步图片保存器实现
    支持保存元数据（分辨率、格式、参数等）
*/

#include "DebugImageSaver.h"
#include "spdlog/spdlog.h"
#include <QDir>
#include <QDateTime>
#include <QFileInfo>
#include <QJsonDocument>
#include <QFile>

extern spdlog::logger *logger;

DebugImageSaver::DebugImageSaver(QObject *parent)
    : QObject(parent)
    , workerThread_(nullptr)
    , shouldStop_(false)
    , outputDir_("debug_frames_stereo")
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

void DebugImageSaver::setOutputDirectory(const QString& dir)
{
    outputDir_ = dir;
    QDir d(dir);
    if (!d.exists()) {
        d.mkpath(".");
    }
}

void DebugImageSaver::enqueueImage(const QImage& image, const QString& filename)
{
    QJsonObject emptyMetadata;
    enqueueImageWithMetadata(image, filename, emptyMetadata);
}

void DebugImageSaver::enqueueImageWithMetadata(const QImage& image, const QString& filename, const QJsonObject& metadata)
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
    task.metadata = metadata;
    task.hasMetadata = !metadata.isEmpty();
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

void DebugImageSaver::saveMetadata(const QString& imagePath, const QJsonObject& metadata)
{
    QString metadataPath = imagePath;
    metadataPath.replace(".png", "_metadata.json");

    QJsonDocument doc(metadata);
    QFile file(metadataPath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
    } else {
        if (logger) {
            logger->warn("DebugImageSaver: Failed to save metadata: {}", metadataPath.toStdString());
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

                // 保存元数据
                if (task.hasMetadata) {
                    saveMetadata(task.filename, task.metadata);
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