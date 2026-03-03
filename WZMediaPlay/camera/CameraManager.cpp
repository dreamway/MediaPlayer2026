/*
    CameraManager: Camera 管理类实现
*/

#include "CameraManager.h"
#include "CameraOpenGLWidget.hpp"
#include "CameraRenderer.h"
#include <spdlog/spdlog.h>
#include <QMediaDevices>

extern spdlog::logger *logger;

CameraManager::CameraManager(QObject *parent)
    : QObject(parent)
    , mCamera(nullptr)
    , mCameraWidget(nullptr)
{
    // 连接 VideoSink 的信号
    connect(&mCameraSink, &QVideoSink::videoFrameChanged, this, &CameraManager::onCameraFrameChanged);

    if (logger) {
        logger->info("CameraManager: Created");
    }
}

CameraManager::~CameraManager()
{
    closeCamera();

    if (logger) {
        logger->info("CameraManager: Destroyed");
    }
}

void CameraManager::setCameraWidget(CameraOpenGLWidget *widget)
{
    mCameraWidget = widget;

    if (logger) {
        logger->info("CameraManager: Camera widget set");
    }
}

QList<QCameraDevice> CameraManager::getAvailableCameras() const
{
    return QMediaDevices::videoInputs();
}

bool CameraManager::openCamera(const QCameraDevice &camDev)
{
    // 先关闭已打开的 Camera
    if (mCamera) {
        closeCamera();
    }

    // 创建新的 QCamera
    mCamera = new QCamera(camDev, this);
    if (!mCamera) {
        if (logger) {
            logger->error("CameraManager: Failed to create QCamera");
        }
        return false;
    }

    // 设置 Camera Session
    mCameraSession.setCamera(mCamera);
    mCameraSession.setVideoSink(&mCameraSink);

    if (logger) {
        logger->info("CameraManager: Camera opened: {}", camDev.description().toStdString());
    }

    emit cameraOpened();
    return true;
}

bool CameraManager::startCamera()
{
    if (!mCamera) {
        if (logger) {
            logger->warn("CameraManager: Cannot start camera, camera not opened");
        }
        return false;
    }

    mCamera->start();

    if (logger) {
        logger->info("CameraManager: Camera started");
    }

    emit cameraStarted();
    return true;
}

void CameraManager::stopCamera()
{
    if (mCamera && mCamera->isActive()) {
        mCamera->stop();

        if (logger) {
            logger->info("CameraManager: Camera stopped");
        }

        emit cameraStopped();
    }
}

void CameraManager::closeCamera()
{
    if (mCamera) {
        stopCamera();

        disconnect(&mCameraSink, &QVideoSink::videoFrameChanged, this, &CameraManager::onCameraFrameChanged);
        mCamera->deleteLater();
        mCamera = nullptr;

        if (logger) {
            logger->info("CameraManager: Camera closed");
        }

        emit cameraClosed();
    }
}

bool CameraManager::isCameraRunning() const
{
    return mCamera && mCamera->isActive();
}

void CameraManager::onCameraFrameChanged(const QVideoFrame &frame)
{
    // 转发 Camera 帧到 CameraOpenGLWidget 的 CameraRenderer
    if (mCameraWidget && mCameraWidget->cameraRenderer()) {
        mCameraWidget->cameraRenderer()->onCameraFrameChanged(frame);
    }
}
