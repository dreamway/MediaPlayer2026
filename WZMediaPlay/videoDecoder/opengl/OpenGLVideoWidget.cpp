/*
    OpenGLVideoWidget: OpenGL video display widget implementation
*/

#include "OpenGLVideoWidget.h"
#include "OpenGLRenderer.h"
#include "StereoOpenGLRenderer.h"

#include <spdlog/spdlog.h>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QGestureEvent>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>

extern spdlog::logger *logger;

OpenGLVideoWidget::OpenGLVideoWidget(QWidget *parent)
    : VideoWidgetBase(parent)
    , QOpenGLWidget(parent)
{
    // 设置 OpenGL 上下文
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setVersion(3, 3);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setSwapInterval(1); // VSync
    setFormat(format);

    // 启用手势
    grabGesture(Qt::PinchGesture);
    setMouseTracking(true);

    // 连接上下文销毁信号
    connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, &OpenGLVideoWidget::onAboutToBeDestroyed);

    if (logger) {
        logger->info("OpenGLVideoWidget created");
    }
}

OpenGLVideoWidget::~OpenGLVideoWidget()
{
    // 清理渲染器
    if (renderer_) {
        renderer_->close();
        renderer_.reset();
    }

    if (logger) {
        logger->info("OpenGLVideoWidget destroyed");
    }
}

void OpenGLVideoWidget::setRenderer(VideoRendererPtr renderer)
{
    // 关闭旧渲染器
    if (renderer_) {
        renderer_->close();
    }

    renderer_ = renderer;

    if (renderer_) {
        // 设置渲染目标
        renderer_->setTarget(this);

        // 打开渲染器
        if (!renderer_->isOpen()) {
            renderer_->open();
        }

        // 更新渲染器参数
        updateRendererParams();
        applyStereoParams();

        if (logger) {
            logger->info("OpenGLVideoWidget::setRenderer: Renderer set to {}", renderer_->name().toStdString());
        }
    }
}

VideoRendererPtr OpenGLVideoWidget::renderer() const
{
    return renderer_;
}

void OpenGLVideoWidget::setStereoFormat(StereoFormat format)
{
    stereoFormat_ = format;

    // 更新渲染器
    if (renderer_) {
        auto *stereoRenderer = dynamic_cast<StereoOpenGLRenderer *>(renderer_.get());
        if (stereoRenderer) {
            stereoRenderer->setStereoFormat(format);
        }
    }

    emit stereoFormatChanged(format);

    if (logger) {
        logger->info("OpenGLVideoWidget::setStereoFormat: Format set to {}", static_cast<int>(format));
    }
}

void OpenGLVideoWidget::setStereoInputFormat(StereoInputFormat inputFormat)
{
    stereoInputFormat_ = inputFormat;

    if (renderer_) {
        auto *stereoRenderer = dynamic_cast<StereoOpenGLRenderer *>(renderer_.get());
        if (stereoRenderer) {
            stereoRenderer->setStereoInputFormat(inputFormat);
        }
    }

    if (logger) {
        logger->info("OpenGLVideoWidget::setStereoInputFormat: Input format set to {}", static_cast<int>(inputFormat));
    }
}

void OpenGLVideoWidget::setStereoOutputFormat(StereoOutputFormat outputFormat)
{
    stereoOutputFormat_ = outputFormat;

    if (renderer_) {
        auto *stereoRenderer = dynamic_cast<StereoOpenGLRenderer *>(renderer_.get());
        if (stereoRenderer) {
            stereoRenderer->setStereoOutputFormat(outputFormat);
        }
    }

    if (logger) {
        logger->info("OpenGLVideoWidget::setStereoOutputFormat: Output format set to {}", static_cast<int>(outputFormat));
    }
}

void OpenGLVideoWidget::setParallaxShift(int shift)
{
    parallaxShift_ = shift;

    if (renderer_) {
        auto *stereoRenderer = dynamic_cast<StereoOpenGLRenderer *>(renderer_.get());
        if (stereoRenderer) {
            stereoRenderer->setParallaxShift(shift);
        }
    }

    if (logger) {
        logger->info("OpenGLVideoWidget::setParallaxShift: Parallax shift set to {}", shift);
    }
}

QImage OpenGLVideoWidget::grabFrame()
{
    // 使用 OpenGL 帧缓冲对象抓取当前画面
    makeCurrent();

    QImage image;
    if (renderer_ && renderer_->isOpen()) {
        // 抓取当前帧
        // 注意：这里需要确保在正确的 OpenGL 上下文中
        // 实际实现可能需要根据 OpenGLRenderer 的具体实现调整
        image = grabFramebuffer();
    }

    doneCurrent();
    return image;
}

void OpenGLVideoWidget::clear()
{
    if (renderer_) {
        renderer_->clear();
    }
    update();
}

bool OpenGLVideoWidget::isReady() const
{
    return initialized_ && renderer_ && renderer_->isReady();
}

void OpenGLVideoWidget::initializeGL()
{
    initializeOpenGLFunctions();

    initialized_ = true;

    if (logger) {
        logger->info("OpenGLVideoWidget::initializeGL: OpenGL initialized");
        logger->info("  OpenGL Version: {}", reinterpret_cast<const char *>(glGetString(GL_VERSION)));
        logger->info("  GLSL Version: {}", reinterpret_cast<const char *>(glGetString(GL_SHADING_LANGUAGE_VERSION)));
        logger->info("  Vendor: {}", reinterpret_cast<const char *>(glGetString(GL_VENDOR)));
        logger->info("  Renderer: {}", reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
    }

    // 如果已有渲染器，重新打开
    if (renderer_) {
        renderer_->open();
    }
}

void OpenGLVideoWidget::paintGL()
{
    // 清空背景
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // 渲染器会在 VideoThread 中调用 render() 方法
    // 这里不需要额外操作

    emit frameRendered();
}

void OpenGLVideoWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);

    // 更新渲染器参数
    updateRendererParams();

    if (logger) {
        logger->debug("OpenGLVideoWidget::resizeGL: {}x{}", w, h);
    }
}

void OpenGLVideoWidget::mousePressEvent(QMouseEvent *event)
{
    mousePressed_ = true;
    lastMousePos_ = event->pos();

    // 可以在这里添加拖动逻辑
    // 例如：调整视差、移动画面等

    QOpenGLWidget::mousePressEvent(event);
}

void OpenGLVideoWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (mousePressed_) {
        QPoint delta = event->pos() - lastMousePos_;
        lastMousePos_ = event->pos();

        // 可以在这里添加拖动逻辑
        // 例如：调整视差、移动画面等

        // 示例：拖动调整视差
        if (stereoFormat_ != STEREO_FORMAT_NORMAL_2D) {
            int newShift = parallaxShift_ + delta.x() / 10;
            setParallaxShift(newShift);
        }
    }

    QOpenGLWidget::mouseMoveEvent(event);
}

void OpenGLVideoWidget::mouseReleaseEvent(QMouseEvent *event)
{
    mousePressed_ = false;
    QOpenGLWidget::mouseReleaseEvent(event);
}

void OpenGLVideoWidget::wheelEvent(QWheelEvent *event)
{
    // 缩放功能
    int delta = event->angleDelta().y();
    double zoomDelta = delta > 0 ? 0.1 : -0.1;

    if (renderer_) {
        // 获取当前缩放
        double currentZoom = 1.0; // 默认缩放
        // 这里需要添加获取当前缩放的方法
        // renderer_->setZoom(currentZoom + zoomDelta);
    }

    QOpenGLWidget::wheelEvent(event);
}

bool OpenGLVideoWidget::event(QEvent *event)
{
    if (event->type() == QEvent::Gesture) {
        QGestureEvent *gestureEvent = static_cast<QGestureEvent *>(event);
        if (QGesture *gesture = gestureEvent->gesture(Qt::PinchGesture)) {
            QPinchGesture *pinch = static_cast<QPinchGesture *>(gesture);
            // 处理缩放手势
            if (pinch->changeFlags() & QPinchGesture::ScaleFactorChanged) {
                // 可以在这里处理缩放
            }
        }
    }

    return QOpenGLWidget::event(event);
}

void OpenGLVideoWidget::onAboutToBeDestroyed()
{
    // OpenGL 上下文即将销毁
    if (renderer_) {
        renderer_->close();
    }

    initialized_ = false;

    if (logger) {
        logger->info("OpenGLVideoWidget::onAboutToBeDestroyed: OpenGL context about to be destroyed");
    }
}

void OpenGLVideoWidget::updateRendererParams()
{
    if (!renderer_) {
        return;
    }

    // 更新渲染器参数
    int w = width();
    int h = height();

    if (w > 0 && h > 0) {
        renderer_->setVideoSize(QSize(w, h));
        renderer_->setRenderRegion(QRect(0, 0, w, h));

        // 设置渲染参数
        auto *oglRenderer = dynamic_cast<OpenGLRenderer *>(renderer_.get());
        if (oglRenderer) {
            double aspectRatio = static_cast<double>(w) / h;
            oglRenderer->setRenderParams(w, h, aspectRatio, 1.0);
        }
    }
}

void OpenGLVideoWidget::applyStereoParams()
{
    if (!renderer_) {
        return;
    }

    // 应用 3D 参数
    auto *stereoRenderer = dynamic_cast<StereoOpenGLRenderer *>(renderer_.get());
    if (stereoRenderer) {
        stereoRenderer->setStereoFormat(stereoFormat_);
        stereoRenderer->setStereoInputFormat(stereoInputFormat_);
        stereoRenderer->setStereoOutputFormat(stereoOutputFormat_);
        stereoRenderer->setParallaxShift(parallaxShift_);
    }
}
