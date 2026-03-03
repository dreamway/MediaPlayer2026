#pragma once

#include "Frame.h"

#include <memory>
#include <vector>
#include <QRect>
#include <QSize>
#include <QWidget>

/**
 * VideoRenderer: 视频渲染器抽象基类（参考 QtAV 的 VideoRenderer 设计）
 * 
 * 职责：
 * - 定义视频渲染的标准接口
 * - 支持多种渲染后端（OpenGL、DirectX、Software 等）
 * - 与 VideoWidgetBase 配合，实现渲染与显示的解耦
 * 
 * 设计原则：
 * - 渲染器只负责将 Frame 渲染到目标 surface
 * - 不直接管理窗口，通过 setTarget() 设置渲染目标
 * - 支持动态切换渲染器而无需重建 VideoWidget
 */
class VideoRenderer
{
public:
    virtual ~VideoRenderer() = default;

    /**
     * 获取渲染器名称
     */
    virtual QString name() const = 0;

    /**
     * 打开渲染器（初始化资源）
     * @return 成功返回 true，失败返回 false
     */
    virtual bool open() = 0;

    /**
     * 关闭渲染器（释放资源）
     */
    virtual void close() = 0;

    /**
     * 检查渲染器是否已打开
     */
    virtual bool isOpen() const = 0;

    /**
     * 渲染视频帧
     * @param frame 要渲染的视频帧
     * @return 成功返回 true，失败返回 false
     */
    virtual bool render(const Frame &frame) = 0;

    /**
     * 设置渲染目标窗口
     * @param target 目标 QWidget，渲染器将在此窗口上绘制
     */
    virtual void setTarget(QWidget *target) = 0;

    /**
     * 获取渲染目标窗口
     */
    virtual QWidget *target() const = 0;

    /**
     * 获取支持的像素格式列表
     */
    virtual std::vector<AVPixelFormat> supportedPixelFormats() const = 0;

    /**
     * 设置视频大小（用于计算渲染区域）
     * @param size 视频原始大小
     */
    virtual void setVideoSize(const QSize &size) { videoSize_ = size; }

    /**
     * 获取视频大小
     */
    virtual QSize videoSize() const { return videoSize_; }

    /**
     * 设置渲染区域（在目标窗口中的位置）
     * @param region 渲染区域，相对于目标窗口
     */
    virtual void setRenderRegion(const QRect &region) { renderRegion_ = region; }

    /**
     * 获取渲染区域
     */
    virtual QRect renderRegion() const { return renderRegion_; }

    /**
     * 暂停/恢复渲染
     * @param paused true 表示暂停，false 表示恢复
     */
    virtual void setPaused(bool paused) { paused_ = paused; }

    /**
     * 检查是否暂停
     */
    virtual bool isPaused() const { return paused_; }

    /**
     * 清空当前画面（用于 Seeking 时清除旧画面）
     */
    virtual void clear() {}

    /**
     * 检查是否准备好渲染
     */
    virtual bool isReady() const = 0;

    /**
     * 渲染缓存的上一帧（当没有新帧时使用，避免黑屏）
     * @return 成功返回 true，失败返回 false
     */
    virtual bool renderLastFrame() { return false; }

    /**
     * 检查是否有缓存的上一帧
     */
    virtual bool hasLastFrame() const { return false; }

    /**
     * 获取统计信息（丢帧数、渲染帧数等）
     */
    struct Stats {
        int renderedFrames = 0;  // 已渲染帧数
        int droppedFrames = 0;   // 丢帧数
        double currentFps = 0.0; // 当前帧率
    };
    virtual Stats stats() const { return stats_; }
protected:
    QSize videoSize_;     // 视频原始大小
    QRect renderRegion_;  // 渲染区域
    bool paused_ = false; // 暂停状态
    Stats stats_;         // 统计信息
};

using VideoRendererPtr = std::shared_ptr<VideoRenderer>;
