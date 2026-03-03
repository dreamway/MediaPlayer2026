#pragma once

#include "ffmpeg.h"
#include <memory>
#include <string>

/**
 * 硬件解码器辅助类
 * 封装硬件解码的初始化和帧转换逻辑
 * 注意：此类仅供 FFDecHW 使用，不应在其他地方直接使用
 */
class HardwareDecoder
{
public:
    HardwareDecoder();
    ~HardwareDecoder();

    /**
     * 尝试初始化硬件解码器
     * @param codec_id 编解码器ID
     * @param codec_ctx 编解码器上下文（会被修改）
     * @return 成功返回硬件解码器指针，失败返回nullptr（应使用软件解码）
     */
    const AVCodec* tryInitHardwareDecoder(AVCodecID codec_id, AVCodecContext* codec_ctx);

    /**
     * 将硬件帧转换为软件帧（如果需要）
     * @param hw_frame 硬件帧
     * @param sw_frame 软件帧（输出）
     * @param codec_ctx 编解码器上下文
     * @return 成功返回0，失败返回负数
     */
    int transferFrame(AVFrame* hw_frame, AVFrame* sw_frame, AVCodecContext* codec_ctx);

    /**
     * 检查是否需要帧转换
     * @return true表示需要转换，false表示不需要
     */
    bool needsTransfer() const { return needs_transfer_; }

    /**
     * 获取硬件设备类型名称（用于日志）
     */
    const char* getDeviceTypeName() const { return device_type_name_.c_str(); }

    /**
     * 检查是否成功初始化了硬件解码器
     */
    bool isInitialized() const { return hw_device_ctx_ != nullptr; }

private:
    /**
     * 尝试创建硬件设备上下文
     * @param codec_id 编解码器ID
     * @return 成功返回设备类型名称，失败返回空字符串
     */
    std::string tryCreateDeviceContext(AVCodecID codec_id);

    /**
     * 查找硬件解码器
     * @param codec_id 编解码器ID
     * @param device_type 设备类型名称
     * @return 硬件解码器指针，失败返回nullptr
     */
    const AVCodec* findHardwareDecoder(AVCodecID codec_id, const std::string& device_type);

    AVBufferRef* hw_device_ctx_;  // 硬件设备上下文
    bool needs_transfer_;          // 是否需要帧转换
    std::string device_type_name_; // 设备类型名称（用于日志）
    AVPixelFormat hw_pix_fmt_;     // 硬件像素格式（用于get_format回调）
    
    // 跟踪硬件解码器状态（参考QtAV）
    int hw_width_;                  // 硬件解码器宽度
    int hw_height_;                 // 硬件解码器高度
    int hw_profile_;                // 硬件解码器profile
    
    // get_format回调函数（静态，因为FFmpeg需要C函数指针）
    static AVPixelFormat get_hw_format(AVCodecContext* ctx, const enum AVPixelFormat* pix_fmts);
    
    // 辅助方法（参考QtAV）
    int codedWidth(AVCodecContext* avctx) const;
    int codedHeight(AVCodecContext* avctx) const;
    
    // 支持的硬件解码器列表（按优先级排序）
    static const char* SUPPORTED_HW_DECODERS[];
    static const int NUM_SUPPORTED_DECODERS;
};
