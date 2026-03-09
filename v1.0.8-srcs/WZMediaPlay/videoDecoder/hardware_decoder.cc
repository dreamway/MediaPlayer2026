#include "hardware_decoder.h"
#include "spdlog/spdlog.h"
#include "../GlobalDef.h"
#include <algorithm>
#include <vector>
#include <cstring>

// 支持的硬件解码器列表（按优先级排序）
const char* HardwareDecoder::SUPPORTED_HW_DECODERS[] = {
    "d3d11va",  // Windows DirectX 11 (优先)
    "cuda",     // NVIDIA CUDA
    "qsv",      // Intel Quick Sync Video
    "dxva2",    // DirectX Video Acceleration 2 (较老)
};

const int HardwareDecoder::NUM_SUPPORTED_DECODERS = sizeof(SUPPORTED_HW_DECODERS) / sizeof(SUPPORTED_HW_DECODERS[0]);

HardwareDecoder::HardwareDecoder()
    : hw_device_ctx_(nullptr)
    , needs_transfer_(false)
    , device_type_name_("")
    , hw_pix_fmt_(AV_PIX_FMT_NONE)
{
}

HardwareDecoder::~HardwareDecoder()
{
    if (hw_device_ctx_) {
        av_buffer_unref(&hw_device_ctx_);
        hw_device_ctx_ = nullptr;
    }
}

std::string HardwareDecoder::tryCreateDeviceContext(AVCodecID codec_id)
{
    // 改进：先检查硬件解码器是否存在，再创建设备上下文
    // 这样可以避免创建了设备上下文但找不到解码器的情况
    
    const char* codec_name = avcodec_get_name(codec_id);
    logger->info("Trying to create hardware device context for codec: {}", 
                codec_name ? codec_name : "unknown");
    
    for (int i = 0; i < NUM_SUPPORTED_DECODERS; ++i) {
        const char* device_type = SUPPORTED_HW_DECODERS[i];
        
        // 先检查是否有对应的硬件解码器（不创建设备上下文）
        // 这样可以提前知道是否支持，避免不必要的设备创建
        // 注意：对于 CUDA 设备，解码器名称是 *_cuvid，而不是 *_cuda
        std::string test_name;
        if (strcmp(device_type, "cuda") == 0) {
            // CUDA 设备使用 cuvid 解码器名称
            test_name = std::string(codec_name ? codec_name : "") + "_cuvid";
        } else {
            // 其他设备使用标准格式：codec_name + "_" + device_type
            test_name = std::string(codec_name ? codec_name : "") + "_" + device_type;
        }
        
        const AVCodec* test_codec = avcodec_find_decoder_by_name(test_name.c_str());
        
        if (!test_codec) {
            logger->debug("Hardware decoder '{}' not found, skipping device context creation for {}", 
                         test_name, device_type);
            continue;
        }
        
        logger->info("Hardware decoder '{}' exists, attempting to create device context for {}", 
                    test_name, device_type);
        
        // 找到对应的解码器，尝试创建设备上下文
        int ret = av_hwdevice_ctx_create(&hw_device_ctx_, 
                                        av_hwdevice_find_type_by_name(device_type),
                                        nullptr, nullptr, 0);
        
        if (ret == 0 && hw_device_ctx_) {
            logger->info("Hardware device context created successfully: {}", device_type);
            return std::string(device_type);
        } else {
            if (ret < 0) {
                char errbuf[256] = {0};
                av_strerror(ret, errbuf, sizeof(errbuf));
                logger->warn("Failed to create hardware device context: {} (ret: {}), error: {}", 
                            device_type, ret, errbuf);
            } else {
                logger->warn("Failed to create hardware device context: {} (ret: {})", 
                            device_type, ret);
            }
        }
    }
    
    logger->info("No suitable hardware device context could be created for codec: {}", 
                codec_name ? codec_name : "unknown");
    return "";
}

const AVCodec* HardwareDecoder::findHardwareDecoder(AVCodecID codec_id, const std::string& device_type)
{
    // 根据编解码器ID和设备类型查找硬件解码器
    // FFmpeg 硬件解码器命名规则可能有多种：
    // 1. codec_name + "_" + device_type (例如：h264_d3d11va, hevc_cuda, h264_qsv)
    // 2. 某些设备可能有不同的命名（例如：h264_nvdec 而不是 h264_cuda）
    
    const char* codec_name = avcodec_get_name(codec_id);
    if (!codec_name) {
        logger->warn("Unknown codec ID: {}", static_cast<int>(codec_id));
        return nullptr;
    }
    
    // 尝试多种可能的硬件解码器名称
    std::vector<std::string> possible_names;
    
    // 对于 CUDA 设备，解码器名称是 *_cuvid（根据 FFmpeg 实际命名）
    if (device_type == "cuda") {
        // CUDA 设备使用 cuvid 解码器名称（如 h264_cuvid, hevc_cuvid）
        possible_names.push_back(std::string(codec_name) + "_cuvid");
        // 也尝试其他可能的名称（作为备选）
        possible_names.push_back(std::string(codec_name) + "_nvdec");
        possible_names.push_back(std::string(codec_name) + "_cuda");  // 标准格式（虽然可能不存在）
    } else if (device_type == "d3d11va") {
        // D3D11VA 使用标准格式
        possible_names.push_back(std::string(codec_name) + "_" + device_type);
        // 也尝试 DXVA2 作为备选
        possible_names.push_back(std::string(codec_name) + "_dxva2");
    } else {
        // 其他设备使用标准格式：codec_name + "_" + device_type
        possible_names.push_back(std::string(codec_name) + "_" + device_type);
    }
    
    // 尝试查找硬件解码器
    for (const auto& hw_decoder_name : possible_names) {
        logger->debug("Trying to find hardware decoder: {}", hw_decoder_name);
        const AVCodec* hw_codec = avcodec_find_decoder_by_name(hw_decoder_name.c_str());
        if (hw_codec) {
            logger->info("Found hardware decoder: {}", hw_decoder_name);
            return hw_codec;
        }
    }
    
    // 如果都找不到，尝试枚举所有硬件解码器（用于调试）
    logger->debug("Hardware decoder not found for codec: {}, device: {}. Tried names:", 
                 codec_name, device_type);
    for (const auto& name : possible_names) {
        logger->debug("  - {}", name);
    }
    
    // 列出所有可用的硬件解码器（用于调试）
    logger->debug("Available hardware decoders (for debugging):");
    void* opaque = nullptr;
    const AVCodec* codec = nullptr;
    while ((codec = av_codec_iterate(&opaque)) != nullptr) {
        if (codec->type == AVMEDIA_TYPE_VIDEO && 
            (codec->capabilities & AV_CODEC_CAP_HARDWARE)) {
            // 检查是否与当前编解码器相关
            if (codec->id == codec_id || 
                (codec_name && strstr(codec->name, codec_name) != nullptr)) {
                logger->debug("  - {} (id: {})", codec->name, static_cast<int>(codec->id));
            }
        }
    }
    
    return nullptr;
}

const AVCodec* HardwareDecoder::tryInitHardwareDecoder(AVCodecID codec_id, AVCodecContext* codec_ctx)
{
    if (!codec_ctx) {
        logger->error("Codec context is null");
        return nullptr;
    }
    
    // 清理之前的状态
    if (hw_device_ctx_) {
        av_buffer_unref(&hw_device_ctx_);
        hw_device_ctx_ = nullptr;
    }
    needs_transfer_ = false;
    device_type_name_ = "";
    
    // 尝试创建硬件设备上下文
    device_type_name_ = tryCreateDeviceContext(codec_id);
    if (device_type_name_.empty()) {
        logger->info("No hardware device available, will use software decoder");
        return nullptr;
    }
    
    // 查找硬件解码器
    const AVCodec* hw_codec = findHardwareDecoder(codec_id, device_type_name_);
    if (!hw_codec) {
        logger->warn("Hardware decoder not found for codec: {}, device: {}", 
                    avcodec_get_name(codec_id), device_type_name_);
        av_buffer_unref(&hw_device_ctx_);
        hw_device_ctx_ = nullptr;
        device_type_name_ = "";
        return nullptr;
    }
    
    // 获取硬件像素格式
    AVHWDeviceType hw_type = av_hwdevice_find_type_by_name(device_type_name_.c_str());
    if (hw_type == AV_HWDEVICE_TYPE_NONE) {
        logger->error("Invalid hardware device type: {}", device_type_name_);
        av_buffer_unref(&hw_device_ctx_);
        hw_device_ctx_ = nullptr;
        device_type_name_ = "";
        return nullptr;
    }
    
    // 查找支持的硬件像素格式
    for (int i = 0;; ++i) {
        const AVCodecHWConfig* config = avcodec_get_hw_config(hw_codec, i);
        if (!config) {
            break;
        }
        
        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
            config->device_type == hw_type) {
            // 设置硬件设备上下文
            codec_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx_);
            if (!codec_ctx->hw_device_ctx) {
                logger->error("Failed to reference hardware device context");
                av_buffer_unref(&hw_device_ctx_);
                hw_device_ctx_ = nullptr;
                device_type_name_ = "";
                return nullptr;
            }
            
            // 保存硬件像素格式
            hw_pix_fmt_ = config->pix_fmt;
            
            // 设置get_format回调函数（关键！硬件解码器需要这个回调）
            codec_ctx->get_format = get_hw_format;
            
            // 将HardwareDecoder实例指针存储到codec_ctx的opaque中，以便回调函数访问
            codec_ctx->opaque = this;
            
            // 对于CUDA硬件解码器，可能需要特殊处理
            // 某些参数需要在设置硬件上下文后重新设置
            if (device_type_name_ == "cuda") {
                if (codec_ctx->extradata && codec_ctx->extradata_size > 0) {
                    logger->debug("CUDA decoder: extradata_size={}, width={}, height={}", 
                                 codec_ctx->extradata_size, codec_ctx->width, codec_ctx->height);
                }
                
                // CUDA解码器可能需要确保某些参数正确设置
                // 清除可能不兼容的标志（在movie.cc中也会清除，但这里确保清除）
                codec_ctx->flags2 &= ~AV_CODEC_FLAG2_FAST;
                
                // 确保thread_count为0（硬件解码器不支持多线程）
                codec_ctx->thread_count = 0;
                
                // 某些CUDA解码器可能需要确保width和height正确设置
                if (codec_ctx->width <= 0 || codec_ctx->height <= 0) {
                    logger->warn("CUDA decoder: Invalid width/height: {}x{}", 
                                codec_ctx->width, codec_ctx->height);
                }
            }
            
            // 检查是否需要帧转换
            // 如果硬件像素格式与软件格式不同，需要转换
            needs_transfer_ = (config->pix_fmt != AV_PIX_FMT_NONE);
            
            logger->info("Hardware decoder initialized successfully: {}, device: {}, hw_pix_fmt: {}, needs_transfer: {}", 
                        hw_codec->name, device_type_name_, static_cast<int>(hw_pix_fmt_), needs_transfer_);
            return hw_codec;
        }
    }
    
    logger->warn("No suitable hardware config found for codec: {}, device: {}", 
                hw_codec->name, device_type_name_);
    av_buffer_unref(&hw_device_ctx_);
    hw_device_ctx_ = nullptr;
    device_type_name_ = "";
    return nullptr;
}

// get_format回调函数：FFmpeg硬件解码器会调用此函数来选择像素格式
AVPixelFormat HardwareDecoder::get_hw_format(AVCodecContext* ctx, const enum AVPixelFormat* pix_fmts)
{
    // 从opaque中获取HardwareDecoder实例
    HardwareDecoder* hw_decoder = static_cast<HardwareDecoder*>(ctx->opaque);
    if (!hw_decoder || hw_decoder->hw_pix_fmt_ == AV_PIX_FMT_NONE) {
        logger->error("get_hw_format: HardwareDecoder instance or hw_pix_fmt not available");
        return AV_PIX_FMT_NONE;
    }
    
    // 遍历支持的像素格式列表，找到硬件像素格式
    const enum AVPixelFormat* p;
    for (p = pix_fmts; *p != AV_PIX_FMT_NONE; p++) {
        if (*p == hw_decoder->hw_pix_fmt_) {
            logger->debug("get_hw_format: Selected hardware pixel format: {}", static_cast<int>(*p));
            return *p;
        }
    }
    
    logger->warn("get_hw_format: Hardware pixel format {} not found in supported list, falling back to first format", 
                 static_cast<int>(hw_decoder->hw_pix_fmt_));
    return pix_fmts[0];  // 如果找不到，返回第一个支持的格式
}

int HardwareDecoder::transferFrame(AVFrame* hw_frame, AVFrame* sw_frame, AVCodecContext* codec_ctx)
{
    if (!hw_frame || !sw_frame || !codec_ctx) {
        logger->error("Invalid parameters for frame transfer");
        return AVERROR(EINVAL);
    }
    
    if (!hw_device_ctx_) {
        logger->error("Hardware device context not initialized");
        return AVERROR(EINVAL);
    }
    
    // 检查是否是硬件格式
    AVHWDeviceType hw_type = av_hwdevice_find_type_by_name(device_type_name_.c_str());
    bool is_hw_format = false;
    
    // 根据设备类型检查对应的硬件格式
    if (hw_type == AV_HWDEVICE_TYPE_D3D11VA) {
        is_hw_format = (hw_frame->format == AV_PIX_FMT_D3D11);
    } else if (hw_type == AV_HWDEVICE_TYPE_CUDA) {
        is_hw_format = (hw_frame->format == AV_PIX_FMT_CUDA);
    } else if (hw_type == AV_HWDEVICE_TYPE_QSV) {
        is_hw_format = (hw_frame->format == AV_PIX_FMT_QSV);
    } else if (hw_type == AV_HWDEVICE_TYPE_DXVA2) {
        is_hw_format = (hw_frame->format == AV_PIX_FMT_DXVA2_VLD);
    }
    
    // 如果不是硬件格式，已经是软件格式，不需要转换
    if (!is_hw_format) {
        return 0;
    }
    
    // 重要：先复制硬件帧的属性到软件帧（包括宽度、高度、像素格式等）
    // 这会设置软件帧的基本信息，但不会复制数据
    int ret = av_frame_copy_props(sw_frame, hw_frame);
    if (ret < 0) {
        logger->error("Failed to copy frame properties: {}", ret);
        return ret;
    }
    
    // 从硬件帧转换数据到软件帧
    // av_hwframe_transfer_data 会根据 sw_frame 的属性自动分配缓冲区
    ret = av_hwframe_transfer_data(sw_frame, hw_frame, 0);
    if (ret < 0) {
        char errbuf[256] = {0};
        av_strerror(ret, errbuf, sizeof(errbuf));
        logger->error("Failed to transfer frame from hardware to software: {} ({})", ret, errbuf);
        logger->error("Hardware frame format: {}, width: {}, height: {}", 
                     static_cast<int>(hw_frame->format),
                     hw_frame->width, hw_frame->height);
        logger->error("Software frame format: {}, width: {}, height: {}", 
                     static_cast<int>(sw_frame->format),
                     sw_frame->width, sw_frame->height);
        return ret;
    }
    
    logger->debug("Successfully transferred hardware frame to software frame, format: {}, size: {}x{}", 
                 static_cast<int>(sw_frame->format),
                 sw_frame->width, sw_frame->height);
    
    return 0;
}

