#pragma once

#include "../VideoRenderer.h"

#include <QCoreApplication>
#include <QSet>
#include <QString>

class QWidget;
class OpenGLCommon;

/**
 * OpenGLRenderer: OpenGL 视频渲染器实现（继承 VideoRenderer）
 *
 * 职责：
 * - 使用 OpenGL 渲染视频帧
 * - 支持 YUV 到 RGB 的硬件转换
 * - 支持视频调节（亮度、对比度等）
 *
 * 设计原则：
 * - 继承 VideoRenderer 接口，实现标准渲染接口
 * - 与 VideoWidgetBase 配合，实现渲染与显示的解耦
 * - 支持 2D 和 3D 视频渲染（通过子类 StereoOpenGLRenderer）
 */
class OpenGLRenderer : public VideoRenderer
{
public:
    OpenGLRenderer();
    ~OpenGLRenderer() override;

    // VideoRenderer 接口实现
    QString name() const override;
    bool open() override;
    void close() override;
    bool isOpen() const override;
    bool render(const Frame &frame) override;
    bool renderLastFrame() override;
    bool hasLastFrame() const override;
    void setTarget(QWidget *target) override;
    QWidget *target() const override;
    std::vector<AVPixelFormat> supportedPixelFormats() const override;
    void setPaused(bool paused) override;
    void clear() override;
    bool isReady() const override;

    /**
     * 设置渲染参数
     * @param width 输出宽度
     * @param height 输出高度
     * @param aspectRatio 宽高比
     * @param zoom 缩放比例
     */
    void setRenderParams(int width, int height, double aspectRatio = 0.0, double zoom = 1.0);

    /**
     * 设置视频调节参数
     * @param brightness 亮度 (-100 ~ 100)
     * @param contrast 对比度 (-100 ~ 100)
     * @param saturation 饱和度 (-100 ~ 100)
     * @param hue 色相 (-100 ~ 100)
     */
    void setVideoAdjustment(int brightness, int contrast, int saturation, int hue);

    /**
     * 设置翻转和旋转
     * @param flip 翻转 (0: 无, 1: 水平, 2: 垂直, 3: 两者)
     * @param rotate90 是否旋转90度
     */
    void setTransform(int flip, bool rotate90);

    /**
     * 重置偏移和变换
     */
    void resetTransform();

    /**
     * 获取内部 OpenGLCommon 对象（供子类使用）
     */
    OpenGLCommon *drawable() const { return drawable_; }

    /**
     * 获取渲染Widget（供外部访问）
     */
    QWidget *widget() const;

protected:
    /**
     * 派生类使用的构造函数（跳过 drawable_ 创建）
     * @param skipDrawableCreation 如果为 true，不创建默认的 drawable_
     */
    OpenGLRenderer(bool skipDrawableCreation);

    /**
     * 初始化 OpenGL 渲染环境
     */
    virtual bool initializeGL();

    /**
     * 检查帧数据有效性
     */
    virtual bool validateFrame(const Frame &frame) const;

    /**
     * 更新颜色空间参数
     */
    virtual void updateColorSpace(const Frame &frame);

    /**
     * 初始化 drawable 的通用设置（供派生类调用）
     */
    void initDrawableSettings();

    OpenGLCommon *drawable_ = nullptr; // OpenGL 渲染核心
    QWidget *target_ = nullptr;        // 渲染目标窗口
    bool useRtt_ = false;              // 是否使用 render-to-texture
    bool bypassCompositor_ = false;    // 是否绕过合成器

    // 上一帧缓存（用于避免黑屏闪烁）
    Frame lastFrame_;           // 缓存的上一帧
    bool hasLastFrame_ = false; // 是否有缓存的帧

    // 渲染参数
    int outWidth_ = 0;
    int outHeight_ = 0;
    double aspectRatio_ = 0.0;
    double zoom_ = 1.0;
    int flip_ = 0;
    bool rotate90_ = false;

    // 视频调节参数
    int brightness_ = 0;
    int contrast_ = 0;
    int saturation_ = 0;
    int hue_ = 0;
    int sharpness_ = 0;
    bool negative_ = false;
};

using OpenGLRendererPtr = std::shared_ptr<OpenGLRenderer>;
