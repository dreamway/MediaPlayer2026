#include "hardware_decoder.h"
#include "../GlobalDef.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <cstring>
#include <vector>

extern "C" {
#include <libavutil/pixdesc.h>  // 需要AVPixFmtDescriptor和AV_PIX_FMT_FLAG_HWACCEL
}

// 支持的硬件解码器列表（按优先级排序，按平台区分）
const char *HardwareDecoder::SUPPORTED_HW_DECODERS[] = {
#ifdef _WIN32
    "d3d11va", // Windows DirectX 11
#endif
#ifdef __linux__
    "vaapi",   // Linux VA-API (Intel/AMD)
    "vdpau",   // Linux VDPAU (NVIDIA legacy)
#endif
#ifdef __APPLE__
    "videotoolbox", // macOS VideoToolbox
#endif
    "cuda",    // NVIDIA CUDA (cross-platform)
};

const int HardwareDecoder::NUM_SUPPORTED_DECODERS = sizeof(SUPPORTED_HW_DECODERS) / sizeof(SUPPORTED_HW_DECODERS[0]);

HardwareDecoder::HardwareDecoder()
    : hw_device_ctx_(nullptr)
    , needs_transfer_(false)
    , device_type_name_("")
    , hw_pix_fmt_(AV_PIX_FMT_NONE)
    , hw_width_(0)
    , hw_height_(0)
    , hw_profile_(0)
{}

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

    const char *codec_name = avcodec_get_name(codec_id);
    logger->info("Trying to create hardware device context for codec: {}", codec_name ? codec_name : "unknown");

    for (int i = 0; i < NUM_SUPPORTED_DECODERS; ++i) {
        const char *device_type = SUPPORTED_HW_DECODERS[i];

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

        const AVCodec *test_codec = avcodec_find_decoder_by_name(test_name.c_str());

        if (!test_codec) {
            logger->debug("Hardware decoder '{}' not found, skipping device context creation for {}", test_name, device_type);
            continue;
        }

        logger->info("Hardware decoder '{}' exists, attempting to create device context for {}", test_name, device_type);

        // 找到对应的解码器，尝试创建设备上下文
        int ret = av_hwdevice_ctx_create(&hw_device_ctx_, av_hwdevice_find_type_by_name(device_type), nullptr, nullptr, 0);

        if (ret == 0 && hw_device_ctx_) {
            logger->info("Hardware device context created successfully: {}", device_type);
            return std::string(device_type);
        } else {
            if (ret < 0) {
                char errbuf[256] = {0};
                av_strerror(ret, errbuf, sizeof(errbuf));
                logger->warn("Failed to create hardware device context: {} (ret: {}), error: {}", device_type, ret, errbuf);
            } else {
                logger->warn("Failed to create hardware device context: {} (ret: {})", device_type, ret);
            }
        }
    }

    logger->info("No suitable hardware device context could be created for codec: {}", codec_name ? codec_name : "unknown");
    return "";
}

const AVCodec *HardwareDecoder::findHardwareDecoder(AVCodecID codec_id, const std::string &device_type)
{
    // 根据编解码器ID和设备类型查找硬件解码器
    // FFmpeg 硬件解码器命名规则可能有多种：
    // 1. codec_name + "_" + device_type (例如：h264_d3d11va, hevc_cuda, h264_qsv)
    // 2. 某些设备可能有不同的命名（例如：h264_nvdec 而不是 h264_cuda）

    const char *codec_name = avcodec_get_name(codec_id);
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
        possible_names.push_back(std::string(codec_name) + "_cuda"); // 标准格式（虽然可能不存在）
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
    for (const auto &hw_decoder_name : possible_names) {
        logger->debug("Trying to find hardware decoder: {}", hw_decoder_name);
        const AVCodec *hw_codec = avcodec_find_decoder_by_name(hw_decoder_name.c_str());
        if (hw_codec) {
            logger->info("Found hardware decoder: {}", hw_decoder_name);
            return hw_codec;
        }
    }

    // 如果都找不到，尝试枚举所有硬件解码器（用于调试）
    logger->debug("Hardware decoder not found for codec: {}, device: {}. Tried names:", codec_name, device_type);
    for (const auto &name : possible_names) {
        logger->debug("  - {}", name);
    }

    // 列出所有可用的硬件解码器（用于调试）
    logger->debug("Available hardware decoders (for debugging):");
    void *opaque = nullptr;
    const AVCodec *codec = nullptr;
    while ((codec = av_codec_iterate(&opaque)) != nullptr) {
        if (codec->type == AVMEDIA_TYPE_VIDEO && (codec->capabilities & AV_CODEC_CAP_HARDWARE)) {
            // 检查是否与当前编解码器相关
            if (codec->id == codec_id || (codec_name && strstr(codec->name, codec_name) != nullptr)) {
                logger->debug("  - {} (id: {})", codec->name, static_cast<int>(codec->id));
            }
        }
    }

    return nullptr;
}

const AVCodec *HardwareDecoder::tryInitHardwareDecoder(AVCodecID codec_id, AVCodecContext *codec_ctx)
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
    hw_width_ = 0;
    hw_height_ = 0;
    hw_profile_ = 0;

    // 尝试创建硬件设备上下文
    device_type_name_ = tryCreateDeviceContext(codec_id);
    if (device_type_name_.empty()) {
        logger->info("No hardware device available, will use software decoder");
        return nullptr;
    }

    // 查找硬件解码器
    const AVCodec *hw_codec = findHardwareDecoder(codec_id, device_type_name_);
    if (!hw_codec) {
        logger->warn("Hardware decoder not found for codec: {}, device: {}", avcodec_get_name(codec_id), device_type_name_);
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
        const AVCodecHWConfig *config = avcodec_get_hw_config(hw_codec, i);
        if (!config) {
            break;
        }

        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX && config->device_type == hw_type) {
            // 设置硬件设备上下文
            codec_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx_);
            if (!codec_ctx->hw_device_ctx) {
                logger->error("Failed to reference hardware device context");
                av_buffer_unref(&hw_device_ctx_);
                hw_device_ctx_ = nullptr;
                device_type_name_ = "";
                return nullptr;
            }

            // [新增] 创建硬件帧池（hw_frames_ctx）
            // 必须检查 hw_device_ctx_ 的有效性，否则可能导致后续转换失败
            if (!hw_device_ctx_) {
                logger->error("Hardware decoder: hw_device_ctx_ is null, cannot allocate hardware frames context");
                return nullptr;
            }

            AVBufferRef *hw_frames_ref = av_hwframe_ctx_alloc(hw_device_ctx_);
            if (!hw_frames_ref) {
                int ret = AVERROR(ENOMEM);
                char errbuf[256] = {0};
                av_strerror(ret, errbuf, sizeof(errbuf));
                logger->error("Failed to allocate hardware frames context: {} ({})", ret, errbuf);
                return nullptr;
            }

            AVHWFramesContext *frames_ctx = (AVHWFramesContext *) hw_frames_ref->data;
            frames_ctx->format = config->pix_fmt;

            // 根据设备类型选择软件格式
            // D3D11VA 和 CUDA 通常输出 NV12
            frames_ctx->sw_format = AV_PIX_FMT_NV12;

            frames_ctx->width = codec_ctx->width;
            frames_ctx->height = codec_ctx->height;
            frames_ctx->initial_pool_size = 20; // 预分配20个帧

            // 初始化硬件帧池
            int ret = av_hwframe_ctx_init(hw_frames_ref);
            if (ret < 0) {
                char errbuf[256] = {0};
                av_strerror(ret, errbuf, sizeof(errbuf));
                logger->error("Failed to initialize hardware frames context: {} ({})", ret, errbuf);
                av_buffer_unref(&hw_frames_ref);
                av_buffer_unref(&hw_device_ctx_);
                hw_device_ctx_ = nullptr;
                device_type_name_ = "";
                return nullptr;
            }

            // 设置硬件帧池到编解码器上下文
            codec_ctx->hw_frames_ctx = hw_frames_ref;

            // 保存硬件像素格式
            hw_pix_fmt_ = config->pix_fmt;

            // [参考QtAV] 初始化硬件解码器状态
            hw_width_ = codedWidth(codec_ctx);
            hw_height_ = codedHeight(codec_ctx);
            hw_profile_ = codec_ctx->profile;

            // 设置get_format回调函数（关键！硬件解码器需要这个回调）
            codec_ctx->get_format = get_hw_format;

            // 将HardwareDecoder实例指针存储到codec_ctx的opaque中，以便回调函数访问
            codec_ctx->opaque = this;

            // 检查是否需要帧转换（根据设备类型）
            // D3D11VA 和 CUDA 都需要转换到软件帧
            needs_transfer_ = true;

            logger->info(
                "Hardware decoder initialized successfully: {}, device: {}, hw_pix_fmt: {}, needs_transfer: {}",
                hw_codec->name,
                device_type_name_,
                static_cast<int>(hw_pix_fmt_),
                needs_transfer_);
            return hw_codec;
        }
    }

    logger->warn("No suitable hardware config found for codec: {}, device: {}", hw_codec->name, device_type_name_);
    av_buffer_unref(&hw_device_ctx_);
    hw_device_ctx_ = nullptr;
    device_type_name_ = "";
    return nullptr;
}

// get_format回调函数：FFmpeg硬件解码器会调用此函数来选择像素格式
// 参考QtAV的getFormat实现，改进硬件格式选择逻辑
AVPixelFormat HardwareDecoder::get_hw_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts)
{
    // 从opaque中获取HardwareDecoder实例
    HardwareDecoder *hw_decoder = static_cast<HardwareDecoder *>(ctx->opaque);
    if (!hw_decoder) {
        logger->error("get_hw_format: HardwareDecoder instance not available");
        return AV_PIX_FMT_NONE;
    }

    // [参考QtAV] 设置允许软件回退标志
#ifdef AV_HWACCEL_FLAG_ALLOW_SOFTWARE
    ctx->hwaccel_flags |= AV_HWACCEL_FLAG_ALLOW_SOFTWARE;
#endif

    // [参考QtAV] 检查是否有硬件加速格式可用
    bool can_hwaccel = false;
    for (size_t i = 0; pix_fmts[i] != AV_PIX_FMT_NONE; i++) {
        const AVPixFmtDescriptor *dsc = av_pix_fmt_desc_get(pix_fmts[i]);
        if (dsc == nullptr)
            continue;
        bool hwaccel = (dsc->flags & AV_PIX_FMT_FLAG_HWACCEL) != 0;
        
        logger->debug("get_hw_format: available {}ware decoder output format {} ({})",
                     hwaccel ? "hard" : "soft", static_cast<int>(pix_fmts[i]), dsc->name);
        if (hwaccel)
            can_hwaccel = true;
    }

    if (!can_hwaccel) {
        logger->warn("get_hw_format: No hardware acceleration format available, falling back to software");
        return avcodec_default_get_format(ctx, pix_fmts);
    }

    // [参考QtAV] 查找匹配的硬件像素格式
    for (size_t i = 0; pix_fmts[i] != AV_PIX_FMT_NONE; i++) {
        if (hw_decoder->hw_pix_fmt_ != pix_fmts[i])
            continue;

        // [参考QtAV] 检查尺寸和profile是否变化，如果没变化且hwaccel_context已设置，直接返回
        if (hw_decoder->hw_width_ == hw_decoder->codedWidth(ctx) &&
            hw_decoder->hw_height_ == hw_decoder->codedHeight(ctx) &&
            hw_decoder->hw_profile_ == ctx->profile &&
            ctx->hwaccel_context) {
            logger->debug("get_hw_format: Reusing existing hardware context, format: {}", static_cast<int>(pix_fmts[i]));
            return pix_fmts[i];
        }

        // [参考QtAV] 更新硬件解码器状态
        hw_decoder->hw_width_ = hw_decoder->codedWidth(ctx);
        hw_decoder->hw_height_ = hw_decoder->codedHeight(ctx);
        hw_decoder->hw_profile_ = ctx->profile;

        logger->info("get_hw_format: Selected hardware pixel format: {} ({}x{}, profile: {})",
                    static_cast<int>(pix_fmts[i]),
                    hw_decoder->hw_width_,
                    hw_decoder->hw_height_,
                    hw_decoder->hw_profile_);
        return pix_fmts[i];
    }

    // [参考QtAV] 如果找不到匹配的硬件格式，回退到默认行为
    logger->warn("get_hw_format: Hardware pixel format {} not found in supported list, falling back to default",
                static_cast<int>(hw_decoder->hw_pix_fmt_));
    return avcodec_default_get_format(ctx, pix_fmts);
}

// 辅助方法：获取编码宽度（参考QtAV）
int HardwareDecoder::codedWidth(AVCodecContext *avctx) const
{
    if (avctx->coded_width > 0)
        return avctx->coded_width;
    return avctx->width;
}

// 辅助方法：获取编码高度（参考QtAV）
int HardwareDecoder::codedHeight(AVCodecContext *avctx) const
{
    if (avctx->coded_height > 0)
        return avctx->coded_height;
    return avctx->height;
}

int HardwareDecoder::transferFrame(AVFrame *hw_frame, AVFrame *sw_frame, AVCodecContext *codec_ctx)
{
    if (!hw_frame || !sw_frame || !codec_ctx) {
        logger->error("Invalid parameters for frame transfer");
        return AVERROR(EINVAL);
    }

    if (!hw_device_ctx_) {
        logger->error("Hardware device context not initialized");
        return AVERROR(EINVAL);
    }

    // [参考QtAV和FFmpeg最佳实践] 先unref sw_frame，确保它是干净的
    av_frame_unref(sw_frame);

    // [修复] 如果 hw_frame->hw_frames_ctx 为空，尝试从 codec_ctx->hw_frames_ctx 获取
    // 这是修复 hw_frames_ctx 缺失问题的关键
    if (!hw_frame->hw_frames_ctx && codec_ctx->hw_frames_ctx) {
        logger->debug("HardwareDecoder::transferFrame: hw_frame has no hw_frames_ctx, using codec_ctx->hw_frames_ctx");
        hw_frame->hw_frames_ctx = av_buffer_ref(codec_ctx->hw_frames_ctx);
        if (!hw_frame->hw_frames_ctx) {
            logger->error("HardwareDecoder::transferFrame: Failed to reference codec_ctx->hw_frames_ctx");
            return AVERROR(ENOMEM);
        }
    }

    // [修复] 参考QtAV和FFmpeg最佳实践：在transfer之前设置sw_frame的格式和尺寸
    // 从hw_frames_ctx获取软件格式（如果可用）
    if (hw_frame->hw_frames_ctx) {
        AVHWFramesContext *frames_ctx = (AVHWFramesContext *)hw_frame->hw_frames_ctx->data;
        if (frames_ctx) {
            // 设置软件帧格式（从hw_frames_ctx获取）
            sw_frame->format = frames_ctx->sw_format;
            sw_frame->width = hw_frame->width;
            sw_frame->height = hw_frame->height;
            
            logger->debug(
                "HardwareDecoder::transferFrame: Setting sw_frame format={}, size={}x{} from hw_frames_ctx",
                static_cast<int>(sw_frame->format),
                sw_frame->width,
                sw_frame->height);
        } else {
            // 如果frames_ctx为空，使用默认NV12格式
            sw_frame->format = AV_PIX_FMT_NV12;
            sw_frame->width = hw_frame->width;
            sw_frame->height = hw_frame->height;
            logger->warn(
                "HardwareDecoder::transferFrame: frames_ctx is null, using default NV12 format, size={}x{}",
                sw_frame->width,
                sw_frame->height);
        }
    } else {
        // 如果仍然没有hw_frames_ctx，使用默认NV12格式，但记录警告
        sw_frame->format = AV_PIX_FMT_NV12;
        sw_frame->width = hw_frame->width;
        sw_frame->height = hw_frame->height;
        logger->warn(
            "HardwareDecoder::transferFrame: hw_frame has no hw_frames_ctx and codec_ctx->hw_frames_ctx is also null, using default NV12 format, size={}x{}",
            sw_frame->width,
            sw_frame->height);
    }

    // [参考QtAV和FFmpeg最佳实践] 在transfer之前，确保sw_frame有正确的格式和尺寸
    // av_hwframe_transfer_data 会自动分配缓冲区，但需要先设置format和size
    // 参考 videodecode.cpp L407 和 QMPlayer2 Frame.cpp L537
    // av_hwframe_transfer_data 的参数顺序是：dst, src, flags
    int ret = av_hwframe_transfer_data(sw_frame, hw_frame, 0);
    if (ret < 0) {
        char errbuf[256] = {0};
        av_strerror(ret, errbuf, sizeof(errbuf));
        logger->error("Failed to transfer hardware frame to software frame: {} ({})", ret, errbuf);
        logger->error(
            "Hardware frame: format={}, width={}, height={}, hw_frames_ctx={}, data[0]={}",
            static_cast<int>(hw_frame->format),
            hw_frame->width,
            hw_frame->height,
            hw_frame->hw_frames_ctx != nullptr ? "valid" : "null",
            hw_frame->data[0] != nullptr ? "valid" : "null");
        return ret;
    }

    // [参考QtAV和FFmpeg最佳实践] 在transfer后复制属性
    // 注意：av_hwframe_transfer_data已经设置了format、width、height，但还需要复制其他属性
    ret = av_frame_copy_props(sw_frame, hw_frame);
    if (ret < 0) {
        char errbuf[256] = {0};
        av_strerror(ret, errbuf, sizeof(errbuf));
        logger->error("Failed to copy frame properties: {} ({})", ret, errbuf);
        logger->error(
            "Software frame before copy_props: format={}, width={}, height={}, data[0]={}",
            static_cast<int>(sw_frame->format),
            sw_frame->width,
            sw_frame->height,
            sw_frame->data[0] != nullptr ? "valid" : "null");
        return ret;
    }

    // [验证] 检查转换后的帧数据（参考QtAV的验证逻辑）
    if (!sw_frame->data[0] || sw_frame->width <= 0 || sw_frame->height <= 0) {
        logger->error(
            "Software frame is invalid after transfer: data[0]={}, width={}, height={}, format={}",
            sw_frame->data[0] != nullptr,
            sw_frame->width,
            sw_frame->height,
            static_cast<int>(sw_frame->format));
        return AVERROR(EINVAL);
    }

    logger->debug(
        "Successfully transferred hardware frame to software frame: format={}, size={}x{}, pts={}",
        static_cast<int>(sw_frame->format),
        sw_frame->width,
        sw_frame->height,
        sw_frame->pts);

    return 0;
}

// if (!hw_device_ctx_) {
//     logger->error("Hardware device context not initialized");
//     return AVERROR(EINVAL);
// }

// // [修复] 使用 av_hwframe_map 替代 av_hwframe_transfer_data（参考 videodecode.cpp）
// // 这种方法更简单且更可靠
// int ret = av_hwframe_map(hw_frame, sw_frame, 0);
// if (ret < 0) {
//     char errbuf[256] = {0};
//     av_strerror(ret, errbuf, sizeof(errbuf));
//     logger->error("Failed to map hardware frame: {} ({})", ret, errbuf);
//     logger->error(
//         "Hardware frame: format={}, width={}, height={}, hw_frames_ctx={}",
//         static_cast<int>(hw_frame->format),
//         hw_frame->width,
//         hw_frame->height,
//         hw_frame->hw_frames_ctx != nullptr ? "valid" : "null");
//     return ret;
// }

// // [修复] 手动设置软件帧的宽高（参考 videodecode.cpp L403-404）
// sw_frame->width = hw_frame->width;
// sw_frame->height = hw_frame->height;

// logger->debug(
//     "After av_hwframe_map - sw_frame: format={}, width={}, height={}, data[0]={}",
//     static_cast<int>(sw_frame->format),
//     sw_frame->width,
//     sw_frame->height,
//     sw_frame->data[0] != nullptr ? "valid" : "null");

// // [验证] 检查转换后的帧数据
// if (!sw_frame->data[0] || sw_frame->width <= 0 || sw_frame->height <= 0) {
//     logger->error(
//         "Software frame is invalid after map: data[0]={}, width={}, height={}, format={}",
//         sw_frame->data[0] != nullptr,
//         sw_frame->width,
//         sw_frame->height,
//         static_cast<int>(sw_frame->format));
//     return AVERROR(EINVAL);
// }

// logger->debug(
//     "Successfully mapped hardware frame to software frame: format={}, size={}x{}", static_cast<int>(sw_frame->format), sw_frame->width, sw_frame->height);

// return 0;
// }
