#include "VideoFilter.h"
#include "spdlog/spdlog.h"

extern spdlog::logger *logger;

VideoFilter::VideoFilter(bool fillDefaultSupportedPixelFormats)
    : m_deintFlags(0)
{
    if (fillDefaultSupportedPixelFormats) {
        // 默认支持的像素格式
        m_supportedPixelFormats = {
            AV_PIX_FMT_YUV420P,
            AV_PIX_FMT_YUVJ420P,
            AV_PIX_FMT_NV12,
            AV_PIX_FMT_RGB24,
            AV_PIX_FMT_RGBA
        };
    }
}

VideoFilter::~VideoFilter()
{
    clearBuffer();
}

void VideoFilter::clearBuffer()
{
    while (!m_internalQueue.empty()) {
        m_internalQueue.pop();
    }
}

bool VideoFilter::removeLastFromInternalBuffer()
{
    if (m_internalQueue.empty()) {
        return false;
    }
    
    // 移除最后一帧（需要先清空队列再重新添加）
    std::queue<Frame> temp;
    while (m_internalQueue.size() > 1) {
        temp.push(std::move(m_internalQueue.front()));
        m_internalQueue.pop();
    }
    m_internalQueue.pop();  // 移除最后一帧
    
    // 恢复队列
    while (!temp.empty()) {
        m_internalQueue.push(std::move(temp.front()));
        temp.pop();
    }
    
    return true;
}

void VideoFilter::addFramesToInternalQueue(std::queue<Frame> &framesQueue)
{
    while (!framesQueue.empty()) {
        m_internalQueue.push(std::move(framesQueue.front()));
        framesQueue.pop();
    }
}

Frame VideoFilter::getNewFrame(const Frame &other)
{
    if (other.isEmpty()) {
        return Frame();
    }
    
    // 创建新帧（复制数据）
    Frame newFrame = Frame::createEmpty(
        other.avFrame(),
        true,
        other.pixelFormat()
    );
    
    if (!newFrame.isEmpty()) {
        // 复制帧数据
        // TODO: 实现帧数据复制
        newFrame.setTS(other.ts());
        newFrame.setTimeBase(other.timeBase());
    }
    
    return newFrame;
}
