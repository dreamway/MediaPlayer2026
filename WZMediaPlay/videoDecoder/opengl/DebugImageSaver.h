/*
    DebugImageSaver: 异步图片保存器
    用于在后台线程保存调试图片，避免影响渲染性能
    支持保存元数据（分辨率、格式、参数等）
*/

#pragma once

#include <QObject>
#include <QImage>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include <QThread>
#include <QString>
#include <QJsonObject>
#include <memory>

class DebugImageSaver : public QObject
{
    Q_OBJECT

public:
    explicit DebugImageSaver(QObject *parent = nullptr);
    ~DebugImageSaver();

    // 添加图片到保存队列（线程安全）
    void enqueueImage(const QImage& image, const QString& filename);

    // 添加带元数据的图片到保存队列（线程安全）
    void enqueueImageWithMetadata(const QImage& image, const QString& filename, const QJsonObject& metadata);

    // 停止保存线程
    void stop();

    // 设置输出目录
    void setOutputDirectory(const QString& dir);

    // 获取输出目录
    QString outputDirectory() const { return outputDir_; }

private slots:
    void processQueue();

private:
    struct ImageTask {
        QImage image;
        QString filename;
        QJsonObject metadata;  // 元数据（可选）
        bool hasMetadata = false;
    };

    // 保存元数据到JSON文件
    void saveMetadata(const QString& imagePath, const QJsonObject& metadata);

    QQueue<ImageTask> imageQueue_;
    QMutex queueMutex_;
    QWaitCondition queueCondition_;
    QThread* workerThread_;
    bool shouldStop_;
    QString outputDir_;  // 输出目录
};
