#pragma once

#include "../VideoWidgetBase.h"

#include <QOpenGLWidget>
#include <QOpenGLFunctions>

class OpenGLRenderer;
class StereoOpenGLRenderer;

/**
 * OpenGLVideoWidget: OpenGL 视频显示窗口
 *
 * 职责：
 * - 继承 VideoWidgetBase，实现标准视频显示接口
 * - 使用 OpenGLRenderer 进行视频渲染
 * - 支持 2D/3D 视频格式切换
 * - 支持截图功能
 *
 * 设计原则：
 * - 只负责显示，不负责渲染
 * - 渲染由 OpenGLRenderer 完成
 * - 支持动态切换渲染器而无需重建 VideoWidget
 */
class OpenGLVideoWidget : public VideoWidgetBase, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit OpenGLVideoWidget(QWidget *parent = nullptr);
    ~OpenGLVideoWidget() override;

    // VideoWidgetBase 接口实现
    void setRenderer(VideoRendererPtr renderer) override;
    VideoRendererPtr renderer() const override;

    void setStereoFormat(StereoFormat format) override;
    StereoFormat stereoFormat() const override { return stereoFormat_; }

    void setStereoInputFormat(StereoInputFormat inputFormat) override;
    StereoInputFormat stereoInputFormat() const override { return stereoInputFormat_; }

    void setStereoOutputFormat(StereoOutputFormat outputFormat) override;
    StereoOutputFormat stereoOutputFormat() const override { return stereoOutputFormat_; }

    void setParallaxShift(int shift) override;
    int parallaxShift() const override { return parallaxShift_; }

    QImage grabFrame() override;
    void clear() override;
    bool isReady() const override;

    // QOpenGLWidget 重写
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    bool event(QEvent *event) override;

private slots:
    void onAboutToBeDestroyed();

private:
    void updateRendererParams();
    void applyStereoParams();

    VideoRendererPtr renderer_;  // 视频渲染器

    // 3D 参数
    StereoFormat stereoFormat_ = STEREO_FORMAT_NORMAL_2D;
    StereoInputFormat stereoInputFormat_ = STEREO_INPUT_FORMAT_LR;
    StereoOutputFormat stereoOutputFormat_ = STEREO_OUTPUT_FORMAT_HORIZONTAL;
    int parallaxShift_ = 0;

    // 鼠标交互
    bool mousePressed_ = false;
    QPoint lastMousePos_;

    // OpenGL 状态
    bool initialized_ = false;
};
