#pragma once

#include "VideoRenderer.h"
#include "../GlobalDef.h"

#include <QWidget>
#include <QImage>
#include <memory>

/**
 * VideoWidgetBase: 视频显示窗口抽象基类
 * 
 * 职责：
 * - 定义视频显示窗口的标准接口
 * - 与 VideoRenderer 配合，实现渲染与显示的解耦
 * - 支持 2D/3D 视频格式切换
 * - 支持截图功能
 * 
 * 设计原则：
 * - VideoWidget 只负责显示，不负责渲染
 * - 渲染由 VideoRenderer 完成
 * - 支持动态切换渲染器而无需重建 VideoWidget
 * - MainWindow 只操作 VideoWidgetBase 接口，不依赖具体实现
 */
class VideoWidgetBase : public QWidget
{
    Q_OBJECT

public:
    explicit VideoWidgetBase(QWidget *parent = nullptr)
        : QWidget(parent)
    {
    }

    virtual ~VideoWidgetBase() = default;

    /**
     * 设置渲染器
     * @param renderer 视频渲染器
     */
    virtual void setRenderer(VideoRendererPtr renderer) = 0;

    /**
     * 获取渲染器
     */
    virtual VideoRendererPtr renderer() const = 0;

    /**
     * 设置立体视频格式
     * @param format 立体格式（2D 或 3D）
     */
    virtual void setStereoFormat(StereoFormat format) = 0;

    /**
     * 获取当前立体视频格式
     */
    virtual StereoFormat stereoFormat() const = 0;

    /**
     * 设置 3D 输入格式（左右/右左/上下）
     * @param inputFormat 输入格式
     */
    virtual void setStereoInputFormat(StereoInputFormat inputFormat) = 0;

    /**
     * 获取当前 3D 输入格式
     */
    virtual StereoInputFormat stereoInputFormat() const = 0;

    /**
     * 设置 3D 输出格式（垂直/水平/棋盘等）
     * @param outputFormat 输出格式
     */
    virtual void setStereoOutputFormat(StereoOutputFormat outputFormat) = 0;

    /**
     * 获取当前 3D 输出格式
     */
    virtual StereoOutputFormat stereoOutputFormat() const = 0;

    /**
     * 设置视差偏移
     * @param shift 偏移值（像素）
     */
    virtual void setParallaxShift(int shift) = 0;

    /**
     * 获取当前视差偏移
     */
    virtual int parallaxShift() const = 0;

    /**
     * 截图功能
     * @return 当前显示的画面 QImage
     */
    virtual QImage grabFrame() = 0;

    /**
     * 清空画面（用于 Seeking 时清除旧画面）
     */
    virtual void clear() = 0;

    /**
     * 检查是否准备好显示
     */
    virtual bool isReady() const = 0;

    /**
     * 设置全屏模式
     * @param fullscreen true 表示全屏，false 表示窗口
     */
    virtual void setFullScreen(bool fullscreen) { fullscreen_ = fullscreen; }

    /**
     * 检查是否全屏
     */
    virtual bool isFullScreen() const { return fullscreen_; }

    /**
     * 设置保持宽高比
     * @param keep true 表示保持宽高比，false 表示拉伸填充
     */
    virtual void setKeepAspectRatio(bool keep) { keepAspectRatio_ = keep; }

    /**
     * 检查是否保持宽高比
     */
    virtual bool keepAspectRatio() const { return keepAspectRatio_; }

signals:
    /**
     * 帧渲染完成信号
     */
    void frameRendered();

    /**
     * 渲染错误信号
     * @param error 错误信息
     */
    void errorOccurred(const QString &error);

    /**
     * 立体格式改变信号
     * @param format 新的立体格式
     */
    void stereoFormatChanged(StereoFormat format);

protected:
    bool fullscreen_ = false;           // 全屏状态
    bool keepAspectRatio_ = true;       // 保持宽高比
};

using VideoWidgetBasePtr = std::shared_ptr<VideoWidgetBase>;
