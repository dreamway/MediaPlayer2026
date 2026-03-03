/*
    CameraOpenGLWidget: Camera 专用的 OpenGL Widget 实现
*/

#include "CameraOpenGLWidget.hpp"
#include "CameraRenderer.h"
#include <QObject>
#include <spdlog/spdlog.h>

extern spdlog::logger *logger;

CameraOpenGLWidget::CameraOpenGLWidget(QWidget *parent)
    : QOpenGLWidget(parent)
    , m_cameraRenderer(nullptr)
    , m_initialized(false)
{
    // 创建并管理 CameraRenderer（由 CameraOpenGLWidget 负责生命周期）
    m_cameraRenderer = new CameraRenderer(this);
    
    // 连接 CameraRenderer 的信号，自动触发 Widget 更新
    connect(m_cameraRenderer, &CameraRenderer::frameUpdated, this, [this]() {
        update();  // 触发重绘
    });
    
    if (logger) {
        logger->info("CameraOpenGLWidget::CameraOpenGLWidget: CameraRenderer created and managed internally");
    }
}

CameraOpenGLWidget::~CameraOpenGLWidget()
{
    makeCurrent();
    // CameraRenderer 会自动清理（QObject 父对象管理）
    if (m_cameraRenderer) {
        m_cameraRenderer->cleanupGL();
    }
}

void CameraOpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    
    // 初始化 OpenGL 状态
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glEnable(GL_TEXTURE_2D);
    
    // 初始化 CameraRenderer（如果已设置）
    if (m_cameraRenderer && !m_cameraRenderer->isInitialized()) {
        if (m_cameraRenderer->initializeGL()) {
            m_initialized = true;
            if (logger) {
                logger->info("CameraOpenGLWidget::initializeGL: CameraRenderer initialized successfully");
            }
        } else {
            if (logger) {
                logger->error("CameraOpenGLWidget::initializeGL: Failed to initialize CameraRenderer");
            }
        }
    }
    
    m_initialized = true;
}

void CameraOpenGLWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    if (logger && (w > 0 && h > 0)) {
        static int resizeCounter = 0;
        if (resizeCounter++ % 100 == 0) {
            logger->debug("CameraOpenGLWidget::resizeGL: {}x{}", w, h);
        }
    }
}

void CameraOpenGLWidget::paintGL()
{
    if (!m_cameraRenderer) {
        // 如果没有 CameraRenderer，只清空背景
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }
    
    // 确保 CameraRenderer 已初始化
    if (!m_cameraRenderer->isInitialized()) {
        if (!m_cameraRenderer->initializeGL()) {
            if (logger) {
                logger->error("CameraOpenGLWidget::paintGL: Failed to initialize CameraRenderer");
            }
            glClear(GL_COLOR_BUFFER_BIT);
            return;
        }
    }
    
    // 清空背景
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // 获取当前 viewport 尺寸
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    int width = viewport[2] > 0 ? viewport[2] : this->width();
    int height = viewport[3] > 0 ? viewport[3] : this->height();
    
    // 委托 CameraRenderer 进行渲染
    m_cameraRenderer->renderCameraFrame(width, height);
}

void CameraOpenGLWidget::showEvent(QShowEvent *event)
{
    QOpenGLWidget::showEvent(event);
    
    if (logger) {
        logger->info("CameraOpenGLWidget::showEvent: Widget shown, size: {}x{}", width(), height());
    }
    
    // 确保 OpenGL 上下文已初始化
    if (m_cameraRenderer && !m_cameraRenderer->isInitialized()) {
        // 强制触发一次重绘以初始化 OpenGL
        update();
    }
}
