/*
    CameraManager: Camera 管理类
    职责：管理 Camera 的初始化、开关、帧接收等功能
    设计原则：将 Camera 相关功能从 StereoVideoWidget 中分离出来
*/

#pragma once

#include <QObject>
#include <QCamera>
#include <QCameraDevice>
#include <QMediaCaptureSession>
#include <QVideoSink>
#include <QVideoFrame>
#include <QList>

class CameraOpenGLWidget;
class CameraRenderer;

/**
 * CameraManager: Camera 管理类
 * 
 * 职责：
 * - 管理 Camera 的初始化、开关、关闭
 * - 处理 Camera 帧的接收和转发
 * - 与 CameraOpenGLWidget 配合，实现 Camera 渲染
 * 
 * 设计原则：
 * - 将 Camera 相关功能从 StereoVideoWidget 中分离
 * - 提供清晰的接口供 MainWindow 使用
 * - Camera 渲染由 CameraOpenGLWidget 负责
 */
class CameraManager : public QObject
{
    Q_OBJECT

public:
    explicit CameraManager(QObject *parent = nullptr);
    ~CameraManager();

    /**
     * 设置 CameraOpenGLWidget（用于渲染 Camera 帧）
     */
    void setCameraWidget(CameraOpenGLWidget *widget);

    /**
     * 获取可用的 Camera 列表
     */
    QList<QCameraDevice> getAvailableCameras() const;

    /**
     * 打开指定的 Camera
     */
    bool openCamera(const QCameraDevice &camDev);

    /**
     * 启动 Camera（开始捕获）
     */
    bool startCamera();

    /**
     * 停止 Camera（停止捕获）
     */
    void stopCamera();

    /**
     * 关闭 Camera（释放资源）
     */
    void closeCamera();

    /**
     * 检查 Camera 是否已打开
     */
    bool isCameraOpen() const { return mCamera != nullptr; }

    /**
     * 检查 Camera 是否正在运行
     */
    bool isCameraRunning() const;

signals:
    /**
     * Camera 打开信号
     */
    void cameraOpened();

    /**
     * Camera 关闭信号
     */
    void cameraClosed();

    /**
     * Camera 启动信号
     */
    void cameraStarted();

    /**
     * Camera 停止信号
     */
    void cameraStopped();

private slots:
    /**
     * 处理 Camera 帧变化
     */
    void onCameraFrameChanged(const QVideoFrame &frame);

private:
    QCamera *mCamera = nullptr;
    QMediaCaptureSession mCameraSession;
    QVideoSink mCameraSink;
    CameraOpenGLWidget *mCameraWidget = nullptr;
};
