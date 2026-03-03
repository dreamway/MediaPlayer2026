/*
    DebugImageSaver: 异步图片保存器
    用于在后台线程保存调试图片，避免影响渲染性能
*/

#pragma once

#include <QObject>
#include <QImage>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include <QThread>
#include <QString>
#include <memory>

class DebugImageSaver : public QObject
{
    Q_OBJECT

public:
    explicit DebugImageSaver(QObject *parent = nullptr);
    ~DebugImageSaver();

    // 添加图片到保存队列（线程安全）
    void enqueueImage(const QImage& image, const QString& filename);

    // 停止保存线程
    void stop();

private slots:
    void processQueue();

private:
    struct ImageTask {
        QImage image;
        QString filename;
    };

    QQueue<ImageTask> imageQueue_;
    QMutex queueMutex_;
    QWaitCondition queueCondition_;
    QThread* workerThread_;
    bool shouldStop_;
};
