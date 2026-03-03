/*
    CameraOpenGLWidget: Camera 专用的 OpenGL Widget
    与 StereoOpenGLWidget 同级别，负责 Camera 帧的渲染
*/

#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>

class CameraRenderer;

/**
 * CameraOpenGLWidget: Camera 专用的 OpenGL Widget
 * 
 * 职责：
 * - 提供 Camera 渲染的 OpenGL 上下文
 * - 委托 CameraRenderer 进行实际的渲染
 * - 与 StereoOpenGLWidget 同级别，通过 Widget 切换实现输入源切换
 */
class CameraOpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
    Q_OBJECT

public:
    explicit CameraOpenGLWidget(QWidget *parent = nullptr);
    ~CameraOpenGLWidget();

    // 获取 CameraRenderer（用于外部访问，如设置 Camera 帧）
    CameraRenderer* cameraRenderer() const { return m_cameraRenderer; }

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void showEvent(QShowEvent *event) override;

private:
    CameraRenderer* m_cameraRenderer = nullptr;
    bool m_initialized = false;
};
