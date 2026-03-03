/*
    CameraRenderer: Camera 渲染器（从 FFmpegView 分离）
    职责：处理 Camera 帧的接收和渲染
*/

#pragma once

#include <QObject>
#include <QVideoFrame>
#include <QImage>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>

class QOpenGLWidget;

/**
 * CameraRenderer: Camera 渲染器
 * 
 * 职责：
 * - 接收 Camera 帧（通过 QVideoSink）
 * - 将 Camera 帧渲染到 OpenGL 上下文
 * - 管理 Camera 相关的 OpenGL 资源
 */
class CameraRenderer : public QObject, protected QOpenGLFunctions_3_3_Core
{
    Q_OBJECT

public:
    explicit CameraRenderer(QObject *parent = nullptr);
    ~CameraRenderer();

    // 初始化 OpenGL 资源（需要在 OpenGL 上下文中调用）
    bool initializeGL();

    // 渲染 Camera 帧（需要在 OpenGL 上下文中调用）
    void renderCameraFrame(int width, int height);

    // 清理 OpenGL 资源
    void cleanupGL();
    
    // 检查是否已初始化
    bool isInitialized() const { return mInitialized; }

public slots:
    // 接收 Camera 帧
    void onCameraFrameChanged(const QVideoFrame &frame);

signals:
    // 帧更新信号（用于触发重绘）
    void frameUpdated();

private:
    // Camera 帧数据
    QImage mCameraImage;
    
    // OpenGL 资源
    QOpenGLShaderProgram* mShaderProgram;
    QOpenGLTexture* mCameraTexture;
    GLuint VAO, VBO, EBO;
    GLuint mCameraTextureId = 0;  // 原生 OpenGL 纹理 ID（用于直接使用 glTexImage2D）
    
    bool mInitialized;
    
    // 纹理和视口尺寸（用于宽高比计算）
    int texWidth = 0;
    int texHeight = 0;
    int viewWidth = 0;
    int viewHeight = 0;
    float ratio = -1.0f;  // 宽高比，初始化为 -1 确保首次计算
    
    // 调试图片保存器（条件编译）
    class DebugImageSaver* debugImageSaver_ = nullptr;
    int frameSaveCounter_ = 0;
    
    // 初始化 Shader
    bool initializeShader();
};
