/*
    StereoOpenGLRenderer: 3D stereo video renderer implementation
    Based on QMPlay2's StereoWriter with VideoRenderer interface
*/

#include "StereoOpenGLRenderer.h"
#include "StereoOpenGLCommon.hpp"
#include "StereoOpenGLWidget.hpp"
#include "StereoOpenGLWindow.hpp"
#include "../Frame.h"

#include <spdlog/spdlog.h>
#include <QSettings>

extern spdlog::logger *logger;

StereoOpenGLRenderer::StereoOpenGLRenderer()
    : OpenGLRenderer(true)  // 调用基类构造函数，跳过 drawable_ 创建
{
    // 创建 StereoOpenGLCommon 的具体实现
    // StereoOpenGLCommon 是抽象类，需要使用 StereoOpenGLWidget 或 StereoOpenGLWindow
    // 使用基类的 useRtt_ 成员来决定创建哪种类型
    if (useRtt_) {
        drawable_ = new StereoOpenGLWidget;
    } else {
        drawable_ = new StereoOpenGLWindow;
    }

    // 调用基类方法初始化通用设置
    initDrawableSettings();

    // 读取设置
    QSettings settings;
    bypassCompositor_ = settings.value("OpenGL/BypassCompositor", false).toBool();
}

StereoOpenGLRenderer::~StereoOpenGLRenderer()
{
    // 基类会处理 drawable 的清理
}

QString StereoOpenGLRenderer::name() const
{
    return "StereoOpenGL 3.3";
}

bool StereoOpenGLRenderer::open()
{
    if (!drawable_) {
        if (logger) {
            logger->error("StereoOpenGLRenderer::open: drawable is null");
        }
        return false;
    }

    // 初始化 OpenGL 环境
    if (!initializeGL()) {
        if (logger) {
            logger->error("StereoOpenGLRenderer::open: initializeGL failed");
        }
        return false;
    }

    // 初始化 3D 参数
    auto *stereoDrawable = getStereoDrawable();
    if (stereoDrawable) {
        stereoDrawable->setStereoFormat(stereoFormat_);
        stereoDrawable->setStereoInputFormat(stereoInputFormat_);
        stereoDrawable->setStereoOutputFormat(stereoOutputFormat_);
        stereoDrawable->setParallaxShift(parallaxShift_);
        if (enableStereoRegion_) {
            stereoDrawable->setStereoEnableRegion(true, stereoRegion_[0], stereoRegion_[1], stereoRegion_[2], stereoRegion_[3]);
        }
    }

    // 如果有目标窗口，设置为 drawable 的父窗口
    if (target_ && drawable_->widget()) {
        drawable_->widget()->setParent(target_);
    }

    if (logger) {
        logger->info("StereoOpenGLRenderer::open: Stereo OpenGL renderer opened successfully");
    }

    return true;
}

bool StereoOpenGLRenderer::render(const Frame &frame)
{
    if (stereoFormat_ == STEREO_FORMAT_NORMAL_2D) {
        // 2D 模式：使用基类的渲染
        return OpenGLRenderer::render(frame);
    } else {
        // 3D 模式：使用 3D 渲染
        return render3D(frame);
    }
}

void StereoOpenGLRenderer::setStereoFormat(StereoFormat format)
{
    stereoFormat_ = format;

    auto *stereoDrawable = getStereoDrawable();
    if (stereoDrawable) {
        stereoDrawable->setStereoFormat(format);
        // BUG 13：切换格式后立即触发重绘，使画面反映 2D/3D 变化
        stereoDrawable->updateGL(false);
    }

    if (logger) {
        logger->info("StereoOpenGLRenderer::setStereoFormat: Format set to {}", static_cast<int>(format));
    }
}

void StereoOpenGLRenderer::setStereoInputFormat(StereoInputFormat inputFormat)
{
    stereoInputFormat_ = inputFormat;

    auto *stereoDrawable = getStereoDrawable();
    if (stereoDrawable) {
        stereoDrawable->setStereoInputFormat(inputFormat);
        stereoDrawable->updateGL(false);
    }

    if (logger) {
        logger->info("StereoOpenGLRenderer::setStereoInputFormat: Input format set to {}", static_cast<int>(inputFormat));
    }
}

void StereoOpenGLRenderer::setStereoOutputFormat(StereoOutputFormat outputFormat)
{
    stereoOutputFormat_ = outputFormat;

    auto *stereoDrawable = getStereoDrawable();
    if (stereoDrawable) {
        stereoDrawable->setStereoOutputFormat(outputFormat);
        stereoDrawable->updateGL(false);
    }

    if (logger) {
        logger->info("StereoOpenGLRenderer::setStereoOutputFormat: Output format set to {}", static_cast<int>(outputFormat));
    }
}

void StereoOpenGLRenderer::setParallaxShift(int shift)
{
    parallaxShift_ = shift;

    auto *stereoDrawable = getStereoDrawable();
    if (stereoDrawable) {
        stereoDrawable->setParallaxShift(shift);
    }

    if (logger) {
        logger->info("StereoOpenGLRenderer::setParallaxShift: Parallax shift set to {}", shift);
    }
}

void StereoOpenGLRenderer::setStereoEnableRegion(bool enable, float topLeftX, float topLeftY, float bottomRightX, float bottomRightY)
{
    enableStereoRegion_ = enable;
    stereoRegion_[0] = topLeftX;
    stereoRegion_[1] = topLeftY;
    stereoRegion_[2] = bottomRightX;
    stereoRegion_[3] = bottomRightY;

    auto *stereoDrawable = getStereoDrawable();
    if (stereoDrawable) {
        stereoDrawable->setStereoEnableRegion(enable, topLeftX, topLeftY, bottomRightX, bottomRightY);
    }

    if (logger) {
        logger->info("StereoOpenGLRenderer::setStereoEnableRegion: Region enabled={}, ({}, {}) - ({}, {})",
                     enable, topLeftX, topLeftY, bottomRightX, bottomRightY);
    }
}

bool StereoOpenGLRenderer::initializeStereoGL()
{
    auto *stereoDrawable = getStereoDrawable();
    if (!stereoDrawable) {
        if (logger) {
            logger->error("StereoOpenGLRenderer::initializeStereoGL: stereoDrawable is null");
        }
        return false;
    }

    // StereoOpenGLCommon 的初始化在基类 initializeGL 中完成
    return stereoDrawable->isOK;
}

bool StereoOpenGLRenderer::render2D(const Frame &frame)
{
    // 2D 模式：直接使用基类渲染
    return OpenGLRenderer::render(frame);
}

bool StereoOpenGLRenderer::render3D(const Frame &frame)
{
    if (!isOpen()) {
        if (logger) {
            logger->warn("StereoOpenGLRenderer::render3D: Renderer not open");
        }
        return false;
    }

    if (!validateFrame(frame)) {
        return false;
    }

    auto *stereoDrawable = getStereoDrawable();
    if (!stereoDrawable) {
        if (logger) {
            logger->error("StereoOpenGLRenderer::render3D: stereoDrawable is null");
        }
        return false;
    }

    // 更新颜色空间
    updateColorSpace(frame);

    // 设置帧数据
    drawable_->videoFrame = frame;
    drawable_->isPaused = false;

    // 更新统计
    stats_.renderedFrames++;

    // 触发渲染
    try {
        stereoDrawable->updateGL(false); // 立即更新
    } catch (const std::exception &e) {
        if (logger) {
            logger->error("StereoOpenGLRenderer::render3D: Exception in updateGL: {}", e.what());
        }
        return false;
    } catch (...) {
        if (logger) {
            logger->error("StereoOpenGLRenderer::render3D: Unknown exception in updateGL");
        }
        return false;
    }

    return true;
}

StereoOpenGLCommon *StereoOpenGLRenderer::getStereoDrawable() const
{
    return dynamic_cast<StereoOpenGLCommon *>(drawable_);
}
