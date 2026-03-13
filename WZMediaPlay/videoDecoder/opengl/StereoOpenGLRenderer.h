#pragma once

#include "OpenGLRenderer.h"
#include "../../GlobalDef.h"

class StereoOpenGLCommon;

/**
 * StereoOpenGLRenderer: 支持 3D 立体渲染的 OpenGL 渲染器
 *
 * 继承自 OpenGLRenderer，添加 3D 立体渲染功能
 * - 2D 模式：复用 OpenGLRenderer 的 2D 渲染功能
 * - 3D 模式：使用 StereoOpenGLCommon 进行 3D 渲染
 */
class StereoOpenGLRenderer : public OpenGLRenderer
{
public:
    StereoOpenGLRenderer();
    ~StereoOpenGLRenderer() override;

    // VideoRenderer 接口重写
    QString name() const override;
    bool open() override;
    bool render(const Frame &frame) override;

    // 3D 参数设置
    void setStereoFormat(StereoFormat format);
    void setStereoInputFormat(StereoInputFormat inputFormat);
    void setStereoOutputFormat(StereoOutputFormat outputFormat);
    void setParallaxShift(int shift);
    void setStereoEnableRegion(bool enable, float topLeftX, float topLeftY, float bottomRightX, float bottomRightY);

    // BUG-037 修复：全屏模式设置
    void setFullscreenPlusStretch(bool stretch);

    // 获取当前 3D 参数
    StereoFormat getStereoFormat() const { return stereoFormat_; }
    StereoInputFormat getStereoInputFormat() const { return stereoInputFormat_; }
    StereoOutputFormat getStereoOutputFormat() const { return stereoOutputFormat_; }
    int getParallaxShift() const { return parallaxShift_; }

protected:
    /**
     * 初始化 3D OpenGL 环境
     */
    bool initializeStereoGL();

    /**
     * 渲染 2D 视频
     */
    bool render2D(const Frame &frame);

    /**
     * 渲染 3D 视频
     */
    bool render3D(const Frame &frame);

    /**
     * 获取 StereoOpenGLCommon 对象
     */
    StereoOpenGLCommon *getStereoDrawable() const;

private:
    // 3D 渲染参数
    StereoFormat stereoFormat_ = STEREO_FORMAT_NORMAL_2D;
    StereoInputFormat stereoInputFormat_ = STEREO_INPUT_FORMAT_LR;
    StereoOutputFormat stereoOutputFormat_ = STEREO_OUTPUT_FORMAT_HORIZONTAL;
    int parallaxShift_ = 0;
    bool enableStereoRegion_ = false;
    float stereoRegion_[4] = {0.0f, 0.0f, 1.0f, 1.0f};  // topLeftX, topLeftY, bottomRightX, bottomRightY
};

using StereoOpenGLRendererPtr = std::shared_ptr<StereoOpenGLRenderer>;
