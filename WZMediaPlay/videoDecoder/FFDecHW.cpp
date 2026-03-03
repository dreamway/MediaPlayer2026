#include "FFDecHW.h"
#include "Frame.h"
#include "VideoFilter.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <libavcodec/avcodec.h>

extern spdlog::logger *logger;

FFDecHW::FFDecHW()
    : codec_ctx_(nullptr)
    , stream_time_base_({1, 1000000})
    , use_hw_decoder_(false)
    , perfStats_()
{
    hw_decoder_ = std::make_unique<HardwareDecoder>();
    sw_fallback_ = std::make_unique<FFDecSW>();

    // 初始化性能统计
    perfStats_.reset();
    logger->info("FFDecHW::FFDecHW: Performance monitoring initialized");
}

FFDecHW::~FFDecHW()
{
    clear();
}

bool FFDecHW::init(AVCodecContext *codec_ctx, AVRational stream_time_base)
{
    if (!codec_ctx) {
        logger->error("FFDecHW::init: codec_ctx is null");
        return false;
    }

    // 先清理之前的状态
    clear();

    codec_ctx_ = codec_ctx;
    stream_time_base_ = stream_time_base;
    use_hw_decoder_ = false;

    logger->info("FFDecHW::init: Attempting to initialize hardware decoder for codec: {}", avcodec_get_name(codec_ctx->codec_id));

    // 尝试初始化硬件解码器
    const AVCodec *hw_codec = hw_decoder_->tryInitHardwareDecoder(codec_ctx->codec_id, codec_ctx);

    // 启用硬件解码（如果初始化成功）
    if (hw_codec && hw_decoder_->isInitialized()) {
        use_hw_decoder_ = true;
        logger->info("FFDecHW::init: Hardware decoding ENABLED - using hardware decoder: {}", hw_codec->name);
    } else {
        use_hw_decoder_ = false;
        logger->warn("FFDecHW::init: Hardware decoding DISABLED - hardware decoder initialization failed, using software decoder instead");
    }

    // 预初始化软件解码器作为备选，但不立即使用
    logger->info("FFDecHW::init: Pre-initializing software fallback decoder");
    if (!sw_fallback_->init(codec_ctx, stream_time_base)) {
        logger->warn("FFDecHW::init: Failed to pre-initialize software fallback, will try again if needed");
        // 预初始化失败不影响硬件解码的使用，后续需要时会重新尝试
    } else {
        logger->info("FFDecHW::init: Software fallback decoder pre-initialized successfully");
    }

    return true;
}


void FFDecHW::clear()
{
    logger->info("FFDecHW::clear: Clearing decoder resources");

    // 输出当前性能统计
    if (perfStats_.totalFramesDecoded > 0) {
        logger->info(
            "FFDecHW::clear: Performance Statistics - Total frames: {}, HW: {}, SW: {}, HW errors: {}, Transfer errors: {}",
            perfStats_.totalFramesDecoded,
            perfStats_.hardwareFramesDecoded,
            perfStats_.softwareFramesDecoded,
            perfStats_.hardwareErrors,
            perfStats_.frameTransferErrors);
        if (perfStats_.totalDecodeTime > 0) {
            double avgDecodeTime = perfStats_.totalDecodeTime / perfStats_.totalFramesDecoded;
            logger->info("FFDecHW::clear: Avg decode time: {:.2f}ms, Total decode time: {:.2f}ms", avgDecodeTime, perfStats_.totalDecodeTime);
        }
        if (perfStats_.frameTransferErrors > 0) {
            logger->info("FFDecHW::clear: Frame transfer errors: {}", perfStats_.frameTransferErrors);
        }
    }

    // 重置性能统计
    perfStats_.reset();

    // 重置状态标志
    use_hw_decoder_ = false;

    // 清理软件解码器资源
    if (sw_fallback_) {
        logger->debug("FFDecHW::clear: Clearing software fallback decoder");
        sw_fallback_->clear();
    }

    // 硬件解码器资源由hw_decoder_自己管理，不需要在这里清理
    // 但可以重置相关状态

    logger->info("FFDecHW::clear: Decoder resources cleared successfully");
}

bool FFDecHW::open(AVCodecContext *codec_ctx)
{
    // 硬件解码器不需要额外的打开操作
    // codec_ctx 应该已经在外部打开
    logger->debug("FFDecHW::open: Called with codec_ctx: {}", codec_ctx != nullptr ? "valid" : "null");
    return codec_ctx != nullptr;
}

// std::shared_ptr<HWDecContext> FFDecHW::getHWDecContext() const
// {
//     // 简化设计：只使用 hardware_decoder 类进行硬件解码
//     // hardware_decoder 直接将硬件帧转换为 NV12 格式的软件帧
//     // 不需要 OpenGLHWInterop 等复杂架构，直接渲染即可
//     // 因此返回 nullptr，表示不使用 HWDecContext
//     return nullptr;
// }

// std::shared_ptr<VideoFilter> FFDecHW::hwAccelFilter() const
// {
//     // TODO: 实现硬件加速过滤器
//     // 这将在 VideoFilter 创建后实现
//     return nullptr;
// }

void FFDecHW::setSupportedPixelFormats(const std::vector<AVPixelFormat> &pixelFormats)
{
    supported_pixel_formats_ = pixelFormats;

    // 同时设置给软件回退解码器
    if (sw_fallback_) {
        sw_fallback_->setSupportedPixelFormats(pixelFormats);
    }

    logger->info("FFDecHW::setSupportedPixelFormats: Set {} supported pixel formats", pixelFormats.size());
    for (size_t i = 0; i < pixelFormats.size(); i++) {
        logger->debug("  [{}]: {}", i, av_get_pix_fmt_name(pixelFormats[i]));
    }
}

int FFDecHW::decodeVideo(const AVPacket *encodedPacket, Frame &decoded, AVPixelFormat &newPixFmt, bool flush, unsigned hurry_up)
{
    static int decodeCounter = 0;
    static int hardwareErrorCount = 0;
    static int consecutiveSuccessCount = 0;
    static bool isFallbackInProgress = false;
    decodeCounter++;

    // 性能计时开始
    auto decodeStartTime = std::chrono::high_resolution_clock::now();
    auto frameTransferStartTime = std::chrono::high_resolution_clock::now();
    auto frameTransferEndTime = frameTransferStartTime;
    bool isFrameTransferPerformed = false;

    if (!codec_ctx_) {
        logger->error("FFDecHW::decodeVideo: decoder not initialized");
        return AVERROR(EINVAL);
    }

    // 每100帧输出一次性能统计（仅在debug模式下）
    if (logger && decodeCounter % 100 == 0) {
        logger->debug(
            "FFDecHW::decodeVideo: Frame #{} - HW: {}, SW: {}, Errors: {}, Transfer errors: {}",
            decodeCounter,
            perfStats_.hardwareFramesDecoded,
            perfStats_.softwareFramesDecoded,
            perfStats_.hardwareErrors,
            perfStats_.frameTransferErrors);
        if (perfStats_.totalDecodeTime > 0 && perfStats_.totalFramesDecoded > 0) {
            double avgDecodeTime = perfStats_.totalDecodeTime / perfStats_.totalFramesDecoded;
            logger->debug("FFDecHW::decodeVideo: Avg decode time: {:.2f}ms", avgDecodeTime);
        }
    }

    // 1. 检查是否应该使用软件回退
    if (!use_hw_decoder_ || !hw_decoder_->isInitialized()) {
        // 使用软件解码器回退
        logger->debug("FFDecHW::decodeVideo: Using software fallback decoder");

        // 确保软件解码器已初始化
        if (!sw_fallback_->isInitialized()) {
            logger->info("FFDecHW::decodeVideo: Initializing software fallback decoder");
            if (!sw_fallback_->init(codec_ctx_, stream_time_base_)) {
                logger->error("FFDecHW::decodeVideo: Failed to initialize software fallback");
                return AVERROR(EINVAL);
            }
            logger->info("FFDecHW::decodeVideo: Software fallback decoder initialized successfully");
        }

        int ret = sw_fallback_->decodeVideo(encodedPacket, decoded, newPixFmt, flush, hurry_up);

        // 更新软件解码统计
        if (ret == 0) {
            perfStats_.softwareFramesDecoded++;
            perfStats_.totalFramesDecoded++;
        }

        // 计算解码时间
        auto decodeEndTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> decodeDuration = decodeEndTime - decodeStartTime;
        perfStats_.totalDecodeTime += decodeDuration.count();

        return ret;
    }

    // 2. 使用硬件解码器路径
    logger->debug("FFDecHW::decodeVideo: Using hardware decoder path");

    // EAGAIN处理修复（2026-01-19）：跟踪packet状态
    bool packetSent = false;        // packet是否被解码器消费
    bool packetNotConsumed = false; // packet未消费但已发送

    // 2.1 发送数据包到硬件解码器
    int ret = 0;
    if (encodedPacket) {
        ret = avcodec_send_packet(codec_ctx_, encodedPacket);
        if (ret == 0) {
            packetSent = true;
        } else if (ret == AVERROR(EAGAIN)) {
            // 解码器内部缓冲区满，需要先 receive_frame（参考 FFDecSW）
            packetNotConsumed = true;
            logger->debug("FFDecHW::decodeVideo: avcodec_send_packet EAGAIN, decoder buffer full, packet not consumed");
            // 不要在这里返回，继续执行 receive_frame
        } else {
            if (ret != AVErrorEOF) {
                logger->warn("FFDecHW::decodeVideo: avcodec_send_packet failed: {}", ret);
            }

            // 分析错误类型，有些错误可能不需要立即回退
            bool shouldIncrementError = true;
            if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
                // 这些错误是正常的，不需要计算为硬件错误
                shouldIncrementError = false;
            }

            if (shouldIncrementError) {
                // 使用统一的错误处理函数
                handleHardwareError(hardwareErrorCount, consecutiveSuccessCount, isFallbackInProgress, "avcodec_send_packet failed");
            }

            return ret;
        }
    } else if (flush) {
        // 刷新解码器缓冲区
        logger->debug("FFDecHW::decodeVideo: Flushing decoder");
        ret = avcodec_send_packet(codec_ctx_, nullptr);
        if (ret < 0 && ret != AVErrorEOF) {
            logger->warn("FFDecHW::decodeVideo: flush failed: {}", ret);
            return ret;
        }
        packetSent = true; // 标记 flush 操作已发送
    }

    // 2.2 接收解码后的帧（可能是硬件帧）
    AVFrame *hw_frame = av_frame_alloc();
    if (!hw_frame) {
        logger->error("FFDecHW::decodeVideo: Failed to allocate hardware frame");
        return AVERROR(ENOMEM);
    }

    ret = avcodec_receive_frame(codec_ctx_, hw_frame);
    if (ret < 0) {
        av_frame_free(&hw_frame);
        if (ret == AVERROR(EAGAIN)) {
            // EAGAIN修复（参考 FFDecSW）：需要更多数据或packet未消费，正常情况
            // 根据packet状态返回约定值
            if (packetSent && !packetNotConsumed) {
                // packet已消费，但没有输出帧（正常情况，参考 FFDecSW）
                logger->debug("FFDecHW::decodeVideo: EAGAIN with packet consumed, no output frame - returning 1");
                return 1; // 特殊值1：packet已消费，无输出帧
            } else if (packetNotConsumed) {
                // packet未消费，需要先receive_frame（正常情况，参考 FFDecSW）
                logger->debug("FFDecHW::decodeVideo: EAGAIN with packet not consumed - returning EAGAIN to retry");
                return AVERROR(EAGAIN); // 返回EAGAIN，需要重试
            } else {
                // 其他情况（如flush）
                logger->debug("FFDecHW::decodeVideo: EAGAIN without packet - returning EAGAIN");
                return AVERROR(EAGAIN); // 返回EAGAIN
            }
        }
        if (ret == AVErrorEOF) {
            // 播放结束，正常情况
            return ret;
        }

        logger->warn("FFDecHW::decodeVideo: avcodec_receive_frame failed: {}", ret);

        // 使用统一的错误处理函数
        handleHardwareError(hardwareErrorCount, consecutiveSuccessCount, isFallbackInProgress, "avcodec_receive_frame failed");

        return ret;
    }

    // 2.3 硬件解码成功，重置错误计数
    hardwareErrorCount = 0;
    consecutiveSuccessCount++;

    // 2.4 处理硬件帧转换（如果需要）
    AVFrame *sw_frame = nullptr;
    if (hw_decoder_->needsTransfer()) {
        // 需要将硬件帧转换为软件帧
        sw_frame = av_frame_alloc();
        if (!sw_frame) {
            logger->error("FFDecHW::decodeVideo: Failed to allocate software frame");
            av_frame_free(&hw_frame);

            // 更新错误统计
            perfStats_.hardwareErrors++;
            return AVERROR(ENOMEM);
        }

        // 记录帧转换开始时间
        frameTransferStartTime = std::chrono::high_resolution_clock::now();
        isFrameTransferPerformed = true;

        // [调试] 检查 hw_frame 的状态
        logger->debug(
            "FFDecHW::decodeVideo: Before transfer - hw_frame: format={}, width={}, height={}, data[0]={}",
            static_cast<int>(hw_frame->format),
            hw_frame->width,
            hw_frame->height,
            hw_frame->data[0] != nullptr ? "valid" : "null");

        ret = hw_decoder_->transferFrame(hw_frame, sw_frame, codec_ctx_);

        // 记录帧转换结束时间
        frameTransferEndTime = std::chrono::high_resolution_clock::now();

        if (ret < 0) {
            // 硬件帧转换失败，记录错误并尝试回退
            logger->error("FFDecHW::decodeVideo: transferFrame failed: {}", ret);
            av_frame_free(&hw_frame);
            av_frame_free(&sw_frame);

            // 更新错误统计
            perfStats_.hardwareErrors++;
            perfStats_.frameTransferErrors++;

            // 使用统一的错误处理函数
            handleHardwareError(hardwareErrorCount, consecutiveSuccessCount, isFallbackInProgress, "frame transfer failed");

            return ret;
        }

        // 计算并更新帧转换时间
        std::chrono::duration<double, std::milli> frameTransferDuration = frameTransferEndTime - frameTransferStartTime;
        perfStats_.totalFrameTransferTime += frameTransferDuration.count();

        // 检查转换后的帧是否有效（在创建 Frame 之前）
        if (!sw_frame->data[0] || sw_frame->width <= 0 || sw_frame->height <= 0) {
            logger->error(
                "FFDecHW::decodeVideo: sw_frame is invalid after transfer - data[0]: {}, width: {}, height: {}, format: {}",
                sw_frame->data[0] != nullptr,
                sw_frame->width,
                sw_frame->height,
                static_cast<int>(sw_frame->format));
            av_frame_free(&hw_frame);
            av_frame_free(&sw_frame);

            // 硬件帧无效，使用统一的错误处理函数
            handleHardwareError(hardwareErrorCount, consecutiveSuccessCount, isFallbackInProgress, "invalid frame after transfer");

            return AVERROR(EINVAL);
        }

        // 使用转换后的软件帧（Frame 构造函数会克隆 AVFrame）
        decoded = Frame(sw_frame);
        av_frame_free(&sw_frame);
    } else {
        // 不需要转换，直接使用硬件帧（这种情况很少见，因为通常需要转换）
        decoded = Frame(hw_frame);
    }

    av_frame_free(&hw_frame);
    decoded.setTimeBase(stream_time_base_);

    // 检查是否需要像素格式转换
    // hardware_decoder 输出 NV12 格式，Shader 已经支持 NV12，所以大多数情况下不需要转换
    AVPixelFormat currentFormat = decoded.pixelFormat();
    newPixFmt = AV_PIX_FMT_NONE;

    // 检查当前格式是否被渲染器支持
    bool needsConversion = false;
    
    // 如果支持列表为空，表示渲染器支持所有格式，不需要转换
    if (supported_pixel_formats_.empty()) {
        logger->debug("FFDecHW::decodeVideo: No supported pixel formats specified, assuming all formats are supported, skipping conversion");
        needsConversion = false;
    } else {
        // 检查当前格式是否在支持列表中
        bool isSupported = std::find(supported_pixel_formats_.begin(), supported_pixel_formats_.end(), currentFormat) != supported_pixel_formats_.end();
        
        if (isSupported) {
            // 格式已被支持，不需要转换
            logger->debug("FFDecHW::decodeVideo: Current format {} is supported by renderer, skipping conversion", av_get_pix_fmt_name(currentFormat));
            needsConversion = false;
        } else {
            // 格式不被支持，需要转换
            logger->debug("FFDecHW::decodeVideo: Current format {} is not supported by renderer, conversion needed", av_get_pix_fmt_name(currentFormat));
            needsConversion = true;
        }
    }

    if (needsConversion) {
        // 确定目标格式：优先使用 NV12，如果不支持则使用 YUV420P
        AVPixelFormat targetFormat = AV_PIX_FMT_NV12;
        bool supportsNV12 = std::find(supported_pixel_formats_.begin(), supported_pixel_formats_.end(), AV_PIX_FMT_NV12) != supported_pixel_formats_.end();

        if (!supportsNV12) {
            targetFormat = AV_PIX_FMT_YUV420P;
        }

        logger->debug("FFDecHW::decodeVideo: Converting pixel format from {} to {}", av_get_pix_fmt_name(currentFormat), av_get_pix_fmt_name(targetFormat));

        newPixFmt = targetFormat;

        // 确保软件解码器已初始化（用于格式转换）
        if (!sw_fallback_->isInitialized()) {
            if (!sw_fallback_->init(codec_ctx_, stream_time_base_)) {
                logger->error("FFDecHW::decodeVideo: Failed to initialize software fallback for conversion");
                newPixFmt = AV_PIX_FMT_NONE;
                return AVERROR(EINVAL);
            }
        }

        // 执行格式转换
        Frame converted = sw_fallback_->convertPixelFormat(decoded, targetFormat);
        if (!converted.isEmpty()) {
            decoded = std::move(converted);
            logger->debug("FFDecHW::decodeVideo: Successfully converted pixel format from {} to {}", av_get_pix_fmt_name(currentFormat), av_get_pix_fmt_name(targetFormat));
        } else {
            logger->warn("FFDecHW::decodeVideo: Pixel format conversion failed");
            newPixFmt = AV_PIX_FMT_NONE;
        }
    } else {
        // 不需要转换，直接使用NV12格式（Shader已经支持）
        if (currentFormat == AV_PIX_FMT_NV12) {
            logger->debug("FFDecHW::decodeVideo: Using NV12 format directly, no conversion needed (Shader supports NV12)");
        }
    }

    // 更新硬件解码成功统计
    perfStats_.hardwareFramesDecoded++;
    perfStats_.totalFramesDecoded++;

    // 计算总解码时间
    auto decodeEndTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> decodeDuration = decodeEndTime - decodeStartTime;
    perfStats_.totalDecodeTime += decodeDuration.count();

    // 记录帧转换时间（如果执行了转换）
    if (isFrameTransferPerformed) {
        std::chrono::duration<double, std::milli> frameTransferDuration = frameTransferEndTime - frameTransferStartTime;
        perfStats_.totalFrameTransferTime += frameTransferDuration.count();
    }

    return 0;
}

int FFDecHW::decodeAudio(const AVPacket *encodedPacket, std::vector<uint8_t> &decoded, double &ts, uint8_t &channels, uint32_t &sampleRate, bool flush)
{
    // 音频通常不使用硬件解码，使用软件解码器
    if (sw_fallback_) {
        return sw_fallback_->decodeAudio(encodedPacket, decoded, ts, channels, sampleRate, flush);
    }
    return AVERROR(EINVAL);
}

int FFDecHW::pendingFrames() const
{
    if (sw_fallback_) {
        return sw_fallback_->pendingFrames();
    }
    return 0;
}

bool FFDecHW::hasCriticalError() const
{
    if (sw_fallback_) {
        return sw_fallback_->hasCriticalError();
    }
    return false;
}

void FFDecHW::clearFrames()
{
    logger->debug("FFDecHW::clearFrames: Clearing decoder frames");

    // 清理软件解码器的内部帧队列
    if (sw_fallback_) {
        logger->debug("FFDecHW::clearFrames: Clearing software fallback decoder frames");
        sw_fallback_->clearFrames();
    }

    // 硬件解码器的帧通常由FFmpeg管理，但我们可以确保状态正确
    if (use_hw_decoder_ && codec_ctx_ && hw_decoder_->isInitialized()) {
        logger->debug("FFDecHW::clearFrames: Hardware decoder active, ensuring proper state");
        // 硬件解码器的刷新会在flushBuffers中处理
    }

    logger->debug("FFDecHW::clearFrames: Decoder frames cleared successfully");
}

void FFDecHW::flushBuffers()
{
    logger->debug("FFDecHW::flushBuffers: Flushing decoder buffers");

    // 优先刷新硬件解码器缓冲区
    if (use_hw_decoder_ && codec_ctx_ && hw_decoder_->isInitialized()) {
        logger->debug("FFDecHW::flushBuffers: Flushing hardware decoder buffers");
        avcodec_flush_buffers(codec_ctx_);
        logger->debug("FFDecHW::flushBuffers: Hardware decoder buffers flushed");

        // 重置 EAGAIN 计数（flush 后重新开始计数）
        consecutiveEAGAINCount_ = 0;
        logger->debug("FFDecHW::flushBuffers: EAGAIN count reset");
    }

    // 同时刷新软件解码器缓冲区（如果已初始化）
    if (sw_fallback_ && sw_fallback_->isInitialized()) {
        logger->debug("FFDecHW::flushBuffers: Flushing software fallback decoder buffers");
        sw_fallback_->flushBuffers();
        logger->debug("FFDecHW::flushBuffers: Software fallback decoder buffers flushed");
    }

    logger->debug("FFDecHW::flushBuffers: All decoder buffers flushed successfully");
}

bool FFDecHW::handleHardwareError(int &errorCount, int &consecutiveSuccessCount, bool &isFallbackInProgress, const std::string &errorReason)
{
    // 增加错误计数，重置成功计数
    errorCount++;
    consecutiveSuccessCount = 0;

    logger->debug("FFDecHW::handleHardwareError: {} (error count: {}, consecutive success: {})", errorReason, errorCount, consecutiveSuccessCount);

    // 如果错误次数超过阈值且没有正在回退，则回退到软件解码
    const int ERROR_THRESHOLD = 3;
    if (errorCount > ERROR_THRESHOLD && !isFallbackInProgress) {
        logger->warn(
            "FFDecHW::handleHardwareError: Hardware decode failed {} times (reason: {}), "
            "preparing to fallback to software decoder",
            errorCount,
            errorReason);

        // 立即刷新解码器缓冲区，确保回退时不会有残留的硬件帧
        avcodec_flush_buffers(codec_ctx_);

        // 标记回退过程开始
        isFallbackInProgress = true;

        // 初始化软件解码器，确保在下次调用时可以立即使用
        if (!sw_fallback_->isInitialized()) {
            logger->info("FFDecHW::handleHardwareError: Initializing software fallback decoder");
            if (sw_fallback_->init(codec_ctx_, stream_time_base_)) {
                logger->info("FFDecHW::handleHardwareError: Software fallback decoder initialized successfully");
            } else {
                logger->error("FFDecHW::handleHardwareError: Failed to initialize software fallback");
            }
        }

        // 切换到软件解码器
        use_hw_decoder_ = false;
        errorCount = 0;
        isFallbackInProgress = false;

        logger->warn("FFDecHW::handleHardwareError: Fallback to software decoder completed successfully");
        return true; // 表示应该回退
    }

    return false; // 继续尝试硬件解码
}
