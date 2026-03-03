#pragma once

#include "Frame.h"

#include <memory>
#include <vector>
#include <queue>

/**
 * VideoFilter: 视频过滤器接口（参考 QMPlayer2 的 VideoFilter 类）
 * 
 * 职责：
 * - 处理视频帧过滤（如硬件帧转换、去隔行等）
 * - 支持过滤器链
 * 
 * 设计特点：
 * - 纯虚接口，由具体过滤器实现
 * - 支持内部缓冲队列
 * - 通过 filter() 方法处理帧队列
 */
class VideoFilter
{
public:
    /**
     * 去隔行标志
     */
    enum DeintFlags {
        AutoDeinterlace = 0x1,
        DoubleFramerate = 0x2,
        AutoParity = 0x4,
        TopFieldFirst = 0x8
    };

    VideoFilter(bool fillDefaultSupportedPixelFormats = true);
    virtual ~VideoFilter();

    /**
     * 清空内部缓冲区
     */
    virtual void clearBuffer();

    /**
     * 从内部缓冲区移除最后一帧
     */
    bool removeLastFromInternalBuffer();

    /**
     * 过滤帧队列
     * @param framesQueue 输入帧队列（会被修改）
     * @return 成功返回true，失败返回false
     */
    virtual bool filter(std::queue<Frame> &framesQueue) = 0;

protected:
    /**
     * 添加帧到内部队列
     */
    void addFramesToInternalQueue(std::queue<Frame> &framesQueue);

    /**
     * 获取新帧（基于现有帧创建）
     */
    Frame getNewFrame(const Frame &other);

    std::vector<AVPixelFormat> m_supportedPixelFormats;  // 支持的像素格式列表
    std::queue<Frame> m_internalQueue;  // 内部缓冲队列
    
    uint8_t m_deintFlags{0};  // 去隔行标志
};
