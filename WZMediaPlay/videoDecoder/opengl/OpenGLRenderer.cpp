/*
    OpenGLRenderer: OpenGL video renderer implementation
    Based on QMPlay2's OpenGLWriter with VideoRenderer interface
*/

#include "OpenGLRenderer.h"
#include "../Frame.h"
#include "OpenGLCommon.hpp"
#include "OpenGLWidget.hpp"
#include "OpenGLWindow.hpp"

#include <spdlog/spdlog.h>
#include <QOpenGLWidget>
#include <QSettings>
#include <QThread>

extern spdlog::logger *logger;

OpenGLRenderer::OpenGLRenderer()
    : OpenGLRenderer(false)  // 调用带参数的构造函数
{
}

OpenGLRenderer::OpenGLRenderer(bool skipDrawableCreation)
    : useRtt_(false)
    , bypassCompositor_(false)
{
    // 如果派生类会创建自己的 drawable_，则跳过创建
    if (!skipDrawableCreation) {
        // 默认不使用 render-to-texture
        // 可以从设置中读取或从外部传入
        if (useRtt_) {
            drawable_ = new OpenGLWidget;
        } else {
            drawable_ = new OpenGLWindow;
        }
        initDrawableSettings();
    }

    // 读取设置
    QSettings settings;
    bypassCompositor_ = settings.value("OpenGL/BypassCompositor", false).toBool();
}

void OpenGLRenderer::initDrawableSettings()
{
    if (drawable_ && drawable_->widget()) {
        auto w = drawable_->widget();
        w->grabGesture(Qt::PinchGesture);
        w->setMouseTracking(true);
    }

    // 读取设置
    QSettings settings;
    if (drawable_) {
        drawable_->setVSync(settings.value("OpenGL/VSync", true).toBool());
    }
}

OpenGLRenderer::~OpenGLRenderer()
{
    close();
    if (drawable_) {
        drawable_->deleteMe();
        drawable_ = nullptr;
    }
}

QString OpenGLRenderer::name() const
{
    QString glStr = "3.3"; // 默认 OpenGL 版本
    if (useRtt_)
        glStr += " (render-to-texture)";
    return "OpenGL " + glStr;
}

bool OpenGLRenderer::open()
{
    if (!drawable_) {
        if (logger) {
            logger->error("OpenGLRenderer::open: drawable is null");
        }
        return false;
    }

    // 初始化 OpenGL 环境
    if (!initializeGL()) {
        if (logger) {
            logger->error("OpenGLRenderer::open: initializeGL failed");
        }
        return false;
    }

    // 如果有目标窗口，设置为 drawable 的父窗口
    if (target_ && drawable_->widget()) {
        drawable_->widget()->setParent(target_);
    }

    if (logger) {
        logger->info("OpenGLRenderer::open: OpenGL renderer opened successfully");
    }

    return true;
}

void OpenGLRenderer::close()
{
    if (drawable_) {
        drawable_->clearImg();
    }

    if (logger) {
        logger->info("OpenGLRenderer::close: OpenGL renderer closed");
    }
}

bool OpenGLRenderer::isOpen() const
{
    return drawable_ != nullptr && drawable_->isOK;
}

bool OpenGLRenderer::render(const Frame &frame)
{
    if (!isOpen()) {
        if (logger) {
            logger->warn("OpenGLRenderer::render: Renderer not open");
        }
        return false;
    }

    if (!validateFrame(frame)) {
        return false;
    }

    // 更新颜色空间
    updateColorSpace(frame);

    // 缓存当前帧（用于没有新帧时显示上一帧，避免黑屏）
    lastFrame_ = frame;
    hasLastFrame_ = true;

    // 设置帧数据
    drawable_->videoFrame = frame;
    drawable_->isPaused = false;

    // 更新统计
    stats_.renderedFrames++;

    // 触发渲染
    try {
        drawable_->updateGL(false); // 立即更新
    } catch (const std::exception &e) {
        if (logger) {
            logger->error("OpenGLRenderer::render: Exception in updateGL: {}", e.what());
        }
        return false;
    } catch (...) {
        if (logger) {
            logger->error("OpenGLRenderer::render: Unknown exception in updateGL");
        }
        return false;
    }

    return true;
}

void OpenGLRenderer::setTarget(QWidget *target)
{
    target_ = target;

    if (drawable_ && drawable_->widget() && target) {
        drawable_->widget()->setParent(target);
    }
}

QWidget *OpenGLRenderer::target() const
{
    return target_;
}

QWidget *OpenGLRenderer::widget() const
{
    if (drawable_) {
        return drawable_->widget();
    }
    return nullptr;
}

std::vector<AVPixelFormat> OpenGLRenderer::supportedPixelFormats() const
{
    std::vector<AVPixelFormat> formats{
        AV_PIX_FMT_YUV420P,
        AV_PIX_FMT_YUVJ420P,
        AV_PIX_FMT_YUV422P,
        AV_PIX_FMT_YUVJ422P,
        AV_PIX_FMT_YUV444P,
        AV_PIX_FMT_YUVJ444P,
        AV_PIX_FMT_YUV410P,
        AV_PIX_FMT_YUV411P,
        AV_PIX_FMT_YUVJ411P,
        AV_PIX_FMT_YUV440P,
        AV_PIX_FMT_YUVJ440P,
    };

    if (drawable_ && drawable_->m_canUse16bitTexture) {
        formats.push_back(AV_PIX_FMT_YUV420P9);
        formats.push_back(AV_PIX_FMT_YUV420P10);
        formats.push_back(AV_PIX_FMT_YUV420P12);
        formats.push_back(AV_PIX_FMT_YUV420P14);
        formats.push_back(AV_PIX_FMT_YUV420P16);
        formats.push_back(AV_PIX_FMT_YUV422P9);
        formats.push_back(AV_PIX_FMT_YUV422P10);
        formats.push_back(AV_PIX_FMT_YUV422P12);
        formats.push_back(AV_PIX_FMT_YUV422P14);
        formats.push_back(AV_PIX_FMT_YUV422P16);
        formats.push_back(AV_PIX_FMT_YUV444P9);
        formats.push_back(AV_PIX_FMT_YUV444P10);
        formats.push_back(AV_PIX_FMT_YUV444P12);
        formats.push_back(AV_PIX_FMT_YUV444P14);
        formats.push_back(AV_PIX_FMT_YUV444P16);
        formats.push_back(AV_PIX_FMT_YUV440P10);
        formats.push_back(AV_PIX_FMT_YUV440P12);
    }

    return formats;
}

void OpenGLRenderer::setPaused(bool paused)
{
    paused_ = paused;
    if (drawable_) {
        drawable_->isPaused = paused;
    }
}

void OpenGLRenderer::clear()
{
    // 清除上一帧缓存
    lastFrame_.clear();
    hasLastFrame_ = false;

    if (drawable_) {
        drawable_->clearImg();
    }
}

bool OpenGLRenderer::isReady() const
{
    return drawable_ && drawable_->isOK;
}

bool OpenGLRenderer::renderLastFrame()
{
    if (!hasLastFrame_) {
        if (logger) {
            logger->debug("OpenGLRenderer::renderLastFrame: No last frame cached");
        }
        return false;
    }

    if (!isOpen()) {
        if (logger) {
            logger->warn("OpenGLRenderer::renderLastFrame: Renderer not open");
        }
        return false;
    }

    // 验证缓存的帧
    if (!validateFrame(lastFrame_)) {
        if (logger) {
            logger->warn("OpenGLRenderer::renderLastFrame: Last frame is invalid");
        }
        return false;
    }

    // 更新颜色空间
    updateColorSpace(lastFrame_);

    // 设置缓存的帧数据
    drawable_->videoFrame = lastFrame_;
    drawable_->isPaused = false;

    // 触发渲染（不增加统计，因为这是重复渲染）
    try {
        drawable_->updateGL(false);
    } catch (const std::exception &e) {
        if (logger) {
            logger->error("OpenGLRenderer::renderLastFrame: Exception in updateGL: {}", e.what());
        }
        return false;
    } catch (...) {
        if (logger) {
            logger->error("OpenGLRenderer::renderLastFrame: Unknown exception in updateGL");
        }
        return false;
    }

    if (logger) {
        logger->debug("OpenGLRenderer::renderLastFrame: Rendered last frame successfully");
    }

    return true;
}

bool OpenGLRenderer::hasLastFrame() const
{
    return hasLastFrame_;
}

void OpenGLRenderer::setRenderParams(int width, int height, double aspectRatio, double zoom)
{
    outWidth_ = width;
    outHeight_ = height;
    aspectRatio_ = aspectRatio;
    zoom_ = zoom;

    if (drawable_) {
        if (width > 0 && height > 0) {
            drawable_->outW = width;
            drawable_->outH = height;
        }

        // 更新宽高比和缩放
        if (drawable_->aRatioRef() != aspectRatio || drawable_->zoomRef() != zoom) {
            drawable_->zoomRef() = zoom;
            drawable_->aRatioRef() = aspectRatio;
            drawable_->newSize(true);
        }
    }
}

void OpenGLRenderer::setVideoAdjustment(int brightness, int contrast, int saturation, int hue)
{
    brightness_ = brightness;
    contrast_ = contrast;
    saturation_ = saturation;
    hue_ = hue;

    if (drawable_) {
        // 更新 drawable 的视频调节参数
        // 注意：这里假设 drawable 有 videoAdjustment 成员
        // 实际实现可能需要根据 OpenGLCommon 的定义调整
        drawable_->videoAdjustment.brightness = static_cast<qint16>(brightness);
        drawable_->videoAdjustment.contrast = static_cast<qint16>(contrast);
        drawable_->videoAdjustment.saturation = static_cast<qint16>(saturation);
        drawable_->videoAdjustment.hue = static_cast<qint16>(hue);
        drawable_->doReset = true;
    }
}

void OpenGLRenderer::setTransform(int flip, bool rotate90)
{
    flip_ = flip;
    rotate90_ = rotate90;

    if (drawable_) {
        drawable_->verticesIdx = rotate90 * 4 + flip;
        drawable_->doReset = true;
    }
}

void OpenGLRenderer::resetTransform()
{
    flip_ = 0;
    rotate90_ = false;

    if (drawable_) {
        drawable_->resetOffsets();
        drawable_->verticesIdx = 0;
        drawable_->doReset = true;
    }
}

bool OpenGLRenderer::initializeGL()
{
    if (!drawable_) {
        return false;
    }

    drawable_->initialize();
    return drawable_->isOK;
}

bool OpenGLRenderer::validateFrame(const Frame &frame) const
{
    if (frame.isEmpty()) {
        if (logger) {
            logger->warn("OpenGLRenderer::validateFrame: Frame is empty");
        }
        return false;
    }

    int frameWidth = frame.width(0);
    int frameHeight = frame.height(0);
    if (frameWidth <= 0 || frameHeight <= 0) {
        if (logger) {
            logger->warn("OpenGLRenderer::validateFrame: Invalid frame dimensions ({}x{})", frameWidth, frameHeight);
        }
        return false;
    }

    int numPlanes = frame.numPlanes();
    for (int p = 0; p < numPlanes && p < 3; ++p) {
        if (!frame.constData(p)) {
            if (logger) {
                logger->warn("OpenGLRenderer::validateFrame: Frame plane {} data is null", p);
            }
            return false;
        }
    }

    return true;
}

void OpenGLRenderer::updateColorSpace(const Frame &frame)
{
    if (!drawable_) {
        return;
    }

    float maxLuminance = 1000.0f;
    if (auto masteringDisplayMetadata = frame.masteringDisplayMetadata()) {
        maxLuminance = static_cast<float>(av_q2d(masteringDisplayMetadata->max_luminance));
        if (maxLuminance < 1.0f || maxLuminance > 10000.0f)
            maxLuminance = 1000.0f;
    }

    const float bitsMultiplier = (1 << frame.paddingBits());

    if (drawable_->m_colorPrimaries != frame.colorPrimaries() || drawable_->m_colorTrc != frame.colorTrc() || drawable_->m_colorSpace != frame.colorSpace()
        || drawable_->m_maxLuminance != maxLuminance || drawable_->m_bitsMultiplier != bitsMultiplier || drawable_->m_depth != frame.depth()
        || drawable_->m_limited != frame.isLimited()) {
        drawable_->m_colorPrimaries = frame.colorPrimaries();
        drawable_->m_colorTrc = frame.colorTrc();
        drawable_->m_colorSpace = frame.colorSpace();
        drawable_->m_maxLuminance = maxLuminance;
        drawable_->m_bitsMultiplier = bitsMultiplier;
        drawable_->m_depth = frame.depth();
        drawable_->m_limited = frame.isLimited();

        if (logger) {
            logger->debug("OpenGLRenderer::updateColorSpace: Color space updated");
        }
    }
}
