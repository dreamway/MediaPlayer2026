#pragma once

#include "Decoder.h"
#include "ffmpeg.h"

#include <memory>
#include <vector>

class AVCodecContext;
class SwsContext;

/**
 * FFDecSW: FFmpeg 软件解码器实现（参考 QMPlayer2 的 FFDecSW 类）
 * 
 * 职责：
 * - 实现软件解码逻辑
 * - 处理像素格式转换
 * - 管理解码器上下文
 */
class FFDecSW : public Decoder
{
public:
    FFDecSW();
    virtual ~FFDecSW();

    // Decoder 接口实现
    std::string name() const override { return "FFmpeg-SW"; }
    
    int decodeVideo(
        const AVPacket *encodedPacket,
        Frame &decoded,
        AVPixelFormat &newPixFmt,
        bool flush,
        unsigned hurry_up
    ) override;

    int decodeAudio(
        const AVPacket *encodedPacket,
        std::vector<uint8_t> &decoded,
        double &ts,
        uint8_t &channels,
        uint32_t &sampleRate,
        bool flush = false
    ) override;

    int pendingFrames() const override;
    
    // 清理解码器内部状态（参考 QMPlayer2 的 clearFrames）
    // 用于 seeking 或视频切换时彻底重置解码器状态
    void clearFrames();
    
    // 刷新解码器缓冲区（直接调用 avcodec_flush_buffers）
    void flushBuffers() override;
    bool hasCriticalError() const override { return m_hasCriticalError; }

    /**
     * 初始化解码器
     * @param codec_ctx 编解码器上下文（会被使用，但不拥有所有权）
     * @param stream_time_base 流的时间基准
     * @return 成功返回true，失败返回false
     */
    bool init(AVCodecContext *codec_ctx, AVRational stream_time_base);

    /**
     * 清理解码器
     */
    void clear();

    /**
     * 检查解码器是否已初始化
     */
    bool isInitialized() const { return codec_ctx_ != nullptr && temp_frame_ != nullptr; }

    /**
     * 设置支持的像素格式列表
     */
    void setSupportedPixelFormats(const std::vector<AVPixelFormat> &pixelFormats) override;

     // 像素格式转换
    Frame convertPixelFormat(const Frame &frame, AVPixelFormat targetFormat);


private:
    bool open(AVCodecContext *codec_ctx) override;

   

    AVCodecContext *codec_ctx_;  // 编解码器上下文（不拥有所有权）
    AVRational stream_time_base_; // 流的时间基准
    
    AVFramePtr temp_frame_;  // 临时帧（用于解码）
    
    std::vector<AVPixelFormat> supported_pixel_formats_;  // 支持的像素格式列表
    SwsContext *sws_ctx_;  // 格式转换上下文
    
    int last_width_;   // 上次转换的宽度
    int last_height_;  // 上次转换的高度
    AVPixelFormat last_format_;  // 上次转换的格式
    
    bool m_hasCriticalError;  // 是否有严重错误
};
