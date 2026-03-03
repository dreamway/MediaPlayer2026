#pragma once

#include "Frame.h"
#include "ffmpeg.h"
#include "chronons.h"

#include <memory>
#include <vector>
#include <string>

class VideoFilter;
// class HWDecContext;  // 已移除，不再使用
class AVCodecContext;
class AVPacket;

/**
 * Decoder: 解码器接口（参考 QMPlayer2 的 Decoder 类）
 * 
 * 职责：
 * - 统一管理视频/音频解码逻辑
 * - 分离硬件解码和软件解码
 * - 提供统一的解码接口
 * 
 * 设计特点：
 * - 纯虚接口，由 FFDecSW 和 FFDecHW 实现
 * - 支持硬件解码上下文查询
 * - 支持像素格式设置
 */
class Decoder
{
public:
    virtual ~Decoder() = default;

    /**
     * 获取解码器名称
     */
    virtual std::string name() const = 0;

    /**
     * 检查是否有硬件解码上下文（已废弃，保留用于接口兼容性）
     */
    virtual bool hasHWDecContext() const { return false; }

    // 已移除 getHWDecContext，不再使用 HWDecContext
    // virtual std::shared_ptr<HWDecContext> getHWDecContext() const { return nullptr; }

    /**
     * 获取硬件加速过滤器（用于硬件帧转换）
     */
    virtual std::shared_ptr<VideoFilter> hwAccelFilter() const { return nullptr; }

    /**
     * 设置支持的像素格式列表（由 VideoWriter 调用）
     */
    virtual void setSupportedPixelFormats(const std::vector<AVPixelFormat> &pixelFormats) {}

    /**
     * 解码视频帧
     * @param encodedPacket 编码的数据包
     * @param decoded 解码后的帧（输出）
     * @param newPixFmt 新的像素格式（输出，如果需要转换）
     * @param flush 是否刷新解码器缓冲区
     * @param hurry_up 加速模式：0=正常，>=1=快速解码低质量，~0=最快解码不复制帧
     * @return 成功返回0，失败返回负数，AVErrorEOF表示结束
     */
    virtual int decodeVideo(
        const AVPacket *encodedPacket,
        Frame &decoded,
        AVPixelFormat &newPixFmt,
        bool flush,
        unsigned hurry_up
    ) = 0;

    /**
     * 解码音频帧
     * @param encodedPacket 编码的数据包
     * @param decoded 解码后的音频数据（输出）
     * @param ts 时间戳（输出）
     * @param channels 声道数（输出）
     * @param sampleRate 采样率（输出）
     * @param flush 是否刷新解码器缓冲区
     * @return 成功返回0，失败返回负数，AVErrorEOF表示结束
     */
    virtual int decodeAudio(
        const AVPacket *encodedPacket,
        std::vector<uint8_t> &decoded,
        double &ts,
        uint8_t &channels,
        uint32_t &sampleRate,
        bool flush = false
    ) = 0;

    /**
     * 获取待处理的帧数（解码器内部缓冲的帧数）
     */
    virtual int pendingFrames() const { return 0; }

    /**
     * 清理解码器内部状态（参考 QMPlayer2 的 clearFrames）
     * 用于 seeking 或视频切换时彻底重置解码器状态
     * 默认实现为空，子类可以覆盖
     */
    virtual void clearFrames() {}

    /**
     * 刷新解码器缓冲区（直接调用 avcodec_flush_buffers）
     * 用于重置解码器的 EOF 状态，比发送 flush packet 更彻底
     * 默认实现为空，子类应该实现
     */
    virtual void flushBuffers() {}

    /**
     * 检查是否有严重错误
     */
    virtual bool hasCriticalError() const { return false; }

    /**
     * 初始化解码器（由子类实现）
     * @param codec_ctx 编解码器上下文（会被使用，但不拥有所有权）
     * @param stream_time_base 流的时间基准
     * @return 成功返回true，失败返回false
     */
    virtual bool init(AVCodecContext *codec_ctx, AVRational stream_time_base) = 0;

protected:
    /**
     * 打开解码器（由子类实现）
     * @param codec_ctx 编解码器上下文
     * @return 成功返回true，失败返回false
     */
    virtual bool open(AVCodecContext *codec_ctx) = 0;
};
