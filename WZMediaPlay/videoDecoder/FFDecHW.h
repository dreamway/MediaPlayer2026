#pragma once

#include "Decoder.h"
#include "FFDecSW.h"
#include "hardware_decoder.h"
#include <memory>

class AVCodecContext;
class VideoFilter;
//class HWDecContext;  // 前向声明，仅用于接口兼容性

/**
 * FFDecHW: FFmpeg 硬件解码器实现
 * 
 * 职责：
 * - 使用 HardwareDecoder 类进行硬件解码
 * - 硬件解码器直接将硬件帧转换为 NV12 格式的软件帧
 * - 简化设计：不使用 OpenGLHWInterop 等复杂架构，直接渲染
 * 
 * 设计原则：
 * - 专注于 hardware_decoder 类的优化
 * - 直接输出 NV12 格式，降低复杂度
 * - 自动回退到软件解码（FFDecSW）当硬件解码失败时
 */
class FFDecHW : public Decoder
{
public:
    FFDecHW();
    virtual ~FFDecHW();

    // Decoder 接口实现
    std::string name() const override { return "FFmpeg-HW"; }

    bool hasHWDecContext() const override { return false; } // 简化设计：不再使用 HWDecContext

    // std::shared_ptr<HWDecContext> getHWDecContext() const override;  // 已移除

    std::shared_ptr<VideoFilter> hwAccelFilter() const override { return nullptr; } // 简化设计：不再使用

    int decodeVideo(const AVPacket *encodedPacket, Frame &decoded, AVPixelFormat &newPixFmt, bool flush, unsigned hurry_up) override;

    int decodeAudio(
        const AVPacket *encodedPacket, std::vector<uint8_t> &decoded, double &ts, uint8_t &channels, uint32_t &sampleRate, bool flush = false) override;

    int pendingFrames() const override;
    bool hasCriticalError() const override;
    void clearFrames() override;  // 清理解码器内部状态
    void flushBuffers() override; // 刷新解码器缓冲区

    /**
     * 初始化硬件解码器
     * @param codec_ctx 编解码器上下文（会被使用，但不拥有所有权）
     * @param stream_time_base 流的时间基准
     * @return 成功返回true，失败返回false
     */
    bool init(AVCodecContext *codec_ctx, AVRational stream_time_base);

    /**
     * 设置预先初始化的硬件解码器
     * 用于在 avcodec_open2 之前初始化硬件解码器的情况
     * @param hw_decoder 预先初始化的硬件解码器（会转移所有权）
     */
    void setHardwareDecoder(std::unique_ptr<HardwareDecoder> hw_decoder) {
        hw_decoder_ = std::move(hw_decoder);
        if (hw_decoder_ && hw_decoder_->isInitialized()) {
            use_hw_decoder_ = true;
        }
    }

    /**
     * 清理解码器
     */
    void clear();

    /**
     * 获取硬件解码器实例
     */
    HardwareDecoder *getHardwareDecoder() const { return hw_decoder_.get(); }

    void setSupportedPixelFormats(const std::vector<AVPixelFormat> &pixelFormats) override;

private:
    bool open(AVCodecContext *codec_ctx) override;

    /**
     * 处理硬件解码错误并决定是否回退到软件解码
     * @param errorCount 当前错误计数（会被修改）
     * @param consecutiveSuccessCount 连续成功计数（会被修改）
     * @param isFallbackInProgress 回退进行中标志（会被修改）
     * @param errorReason 错误原因描述（用于日志）
     * @return true 表示应该回退到软件解码，false 表示继续尝试硬件解码
     */
    bool handleHardwareError(int &errorCount, int &consecutiveSuccessCount, bool &isFallbackInProgress, const std::string &errorReason);

    std::unique_ptr<HardwareDecoder> hw_decoder_; // 硬件解码器
    std::unique_ptr<FFDecSW> sw_fallback_;        // 软件解码器（作为回退）

    AVCodecContext *codec_ctx_;   // 编解码器上下文（不拥有所有权）
    AVRational stream_time_base_; // 流的时间基准

    bool use_hw_decoder_; // 是否使用硬件解码器

    // EAGAIN监控相关（2026-01-19）
    static const int EAGAIN_THRESHOLD = 10; // 连续10次EAGAIN后回退到软件解码
    int consecutiveEAGAINCount_ = 0;        // EAGAIN连续计数

    // 支持的像素格式列表（用于像素格式转换）
    std::vector<AVPixelFormat> supported_pixel_formats_;

    // 性能监控相关成员变量
    struct PerformanceStats
    {
        uint64_t totalFramesDecoded;    // 总解码帧数
        uint64_t hardwareFramesDecoded; // 硬件解码成功帧数
        uint64_t softwareFramesDecoded; // 软件解码帧数
        uint64_t hardwareErrors;        // 硬件解码错误次数
        uint64_t frameTransferErrors;   // 帧转换错误次数
        double totalDecodeTime;         // 总解码时间（毫秒）
        double totalFrameTransferTime;  // 总帧转换时间（毫秒）

        // 重置统计数据
        void reset()
        {
            totalFramesDecoded = 0;
            hardwareFramesDecoded = 0;
            softwareFramesDecoded = 0;
            hardwareErrors = 0;
            frameTransferErrors = 0;
            totalDecodeTime = 0.0;
            totalFrameTransferTime = 0.0;
        }
    } perfStats_; // 性能统计结构
};
