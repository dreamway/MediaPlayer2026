#include "FFDecSW.h"
#include "spdlog/spdlog.h"

extern spdlog::logger *logger;

#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>

FFDecSW::FFDecSW()
    : codec_ctx_(nullptr)
    , stream_time_base_({1, 1000000})
    , sws_ctx_(nullptr)
    , last_width_(0)
    , last_height_(0)
    , last_format_(AV_PIX_FMT_NONE)
    , m_hasCriticalError(false)
{
}

FFDecSW::~FFDecSW()
{
    clear();
    if (sws_ctx_) {
        sws_freeContext(sws_ctx_);
        sws_ctx_ = nullptr;
    }
}

bool FFDecSW::init(AVCodecContext *codec_ctx, AVRational stream_time_base)
{
    if (!codec_ctx) {
        logger->error("FFDecSW::init: codec_ctx is null");
        return false;
    }
    
    logger->info("FFDecSW::init: Initializing decoder with codec_ctx: {}, codec_id: {}", 
                 (void*)codec_ctx, int(codec_ctx->codec_id));
    
    codec_ctx_ = codec_ctx;
    stream_time_base_ = stream_time_base;
    temp_frame_ = AVFramePtr(av_frame_alloc());
    
    if (!temp_frame_) {
        logger->error("FFDecSW::init: failed to allocate temp frame");
        return false;
    }
    
    m_hasCriticalError = false;
    logger->info("FFDecSW::init: Decoder initialized successfully (codec_ctx_: {}, temp_frame_: {})", 
                 (void*)codec_ctx_, (void*)temp_frame_.get());
    return true;
}

void FFDecSW::clear()
{
    if (sws_ctx_) {
        sws_freeContext(sws_ctx_);
        sws_ctx_ = nullptr;
    }
    temp_frame_.reset();
    last_width_ = 0;
    last_height_ = 0;
    last_format_ = AV_PIX_FMT_NONE;
}

bool FFDecSW::open(AVCodecContext *codec_ctx)
{
    // 软件解码器不需要额外的打开操作
    // codec_ctx 应该已经在外部打开
    return codec_ctx != nullptr;
}

void FFDecSW::setSupportedPixelFormats(const std::vector<AVPixelFormat> &pixelFormats)
{
    supported_pixel_formats_ = pixelFormats;
}

int FFDecSW::decodeVideo(
    const AVPacket *encodedPacket,
    Frame &decoded,
    AVPixelFormat &newPixFmt,
    bool flush,
    unsigned hurry_up)
{
    if (!codec_ctx_ || !temp_frame_) {
        logger->error("FFDecSW::decodeVideo: decoder not initialized (codec_ctx_: {}, temp_frame_: {})", 
                     (void*)codec_ctx_, (void*)temp_frame_.get());
        return AVERROR(EINVAL);
    }

    // 返回值约定：
    // 0: 成功，有输出帧，packet 已消费
    // 1: packet 已消费，但没有输出帧（需要更多数据）
    // 2: 有输出帧，但 packet 未消费（解码器缓冲区满）
    // 负数: 错误码
    
    int ret = 0;
    bool packetSent = false;
    bool packetNotConsumed = false;
    
    if (encodedPacket) {
        ret = avcodec_send_packet(codec_ctx_, encodedPacket);
        if (ret == 0) {
            packetSent = true;
        } else if (ret == AVERROR(EAGAIN)) {
            // 解码器内部缓冲区满，需要先 receive_frame
            // packet 没有被消费，先尝试取出帧
            packetNotConsumed = true;
            // 不要在这里返回，继续执行 receive_frame
        } else {
            if (ret != AVErrorEOF) {
                logger->warn("FFDecSW::decodeVideo: avcodec_send_packet failed: {}", ret);
            }
            return ret;
        }
    } else if (flush) {
        // 刷新解码器
        ret = avcodec_send_packet(codec_ctx_, nullptr);
        if (ret < 0 && ret != AVErrorEOF) {
            logger->warn("FFDecSW::decodeVideo: flush failed: {}", ret);
            return ret;
        }
        packetSent = true;
    }

    // 接收解码后的帧
    auto receiveStart = std::chrono::steady_clock::now();
    ret = avcodec_receive_frame(codec_ctx_, temp_frame_.get());
    auto receiveEnd = std::chrono::steady_clock::now();
    auto receiveDuration = std::chrono::duration_cast<std::chrono::milliseconds>(receiveEnd - receiveStart).count();
    logger->debug("FFDecSW::decodeVideo: receive_frame elapsed:{}", receiveDuration);
    
    if (ret < 0) {
        if (ret == AVERROR(EAGAIN)) {
            // 需要更多数据
            if (packetSent) {
                // packet 已被消费，但还没有输出帧
                return 1;
            } else if (packetNotConsumed) {
                // packet 未被消费，且没有输出帧
                // 这种情况不应该发生（send_packet EAGAIN 意味着解码器有数据）
                // 但如果发生了，返回 EAGAIN 让调用者重试
                logger->debug("FFDecSW::decodeVideo: send_packet EAGAIN but receive_frame also EAGAIN, will retry");
                return AVERROR(EAGAIN);
            } else {
                return AVERROR(EAGAIN);
            }
        }
        if (ret == AVErrorEOF) {
            return ret;
        }
        logger->warn("FFDecSW::decodeVideo: avcodec_receive_frame failed: {}", ret);
        return ret;
    }

    // 成功接收到帧，创建 Frame 对象
    decoded = Frame(temp_frame_.get());
    decoded.setTimeBase(stream_time_base_);

    // 检查是否需要格式转换
    AVPixelFormat currentFormat = decoded.pixelFormat();
    newPixFmt = AV_PIX_FMT_NONE;

    if (!supported_pixel_formats_.empty()) {
        bool needsConversion = true;
        for (AVPixelFormat fmt : supported_pixel_formats_) {
            if (fmt == currentFormat) {
                needsConversion = false;
                break;
            }
        }

        if (needsConversion && !supported_pixel_formats_.empty()) {
            newPixFmt = supported_pixel_formats_[0];
            Frame converted = convertPixelFormat(decoded, newPixFmt);
            if (!converted.isEmpty()) {
                decoded = std::move(converted);
            } else {
                logger->warn("FFDecSW::decodeVideo: pixel format conversion failed");
                newPixFmt = AV_PIX_FMT_NONE;
            }
        }
    }

    // 返回正确的状态码
    if (packetNotConsumed) {
        // 有输出帧，但 packet 未消费
        return 2;
    }
    return 0; // 正常情况：有输出帧，packet 已消费
}

int FFDecSW::decodeAudio(
    const AVPacket *encodedPacket,
    std::vector<uint8_t> &decoded,
    double &ts,
    uint8_t &channels,
    uint32_t &sampleRate,
    bool flush)
{
    if (!codec_ctx_ || !temp_frame_) {
        logger->error("FFDecSW::decodeAudio: decoder not initialized");
        return AVERROR(EINVAL);
    }

    // 发送数据包到解码器
    int ret = 0;
    if (encodedPacket) {
        ret = avcodec_send_packet(codec_ctx_, encodedPacket);
        if (ret < 0 && ret != AVERROR(EAGAIN)) {
            if (ret != AVErrorEOF) {
                logger->warn("FFDecSW::decodeAudio: avcodec_send_packet failed: {}", ret);
            }
            return ret;
        }
    } else if (flush) {
        ret = avcodec_send_packet(codec_ctx_, nullptr);
        if (ret < 0 && ret != AVErrorEOF) {
            logger->warn("FFDecSW::decodeAudio: flush failed: {}", ret);
            return ret;
        }
    }

    // 接收解码后的帧
    ret = avcodec_receive_frame(codec_ctx_, temp_frame_.get());
    if (ret < 0) {
        if (ret == AVERROR(EAGAIN)) {
            return ret;
        }
        if (ret == AVErrorEOF) {
            return ret;
        }
        logger->warn("FFDecSW::decodeAudio: avcodec_receive_frame failed: {}", ret);
        return ret;
    }

    // 提取音频数据
    AVFrame *frame = temp_frame_.get();
    channels = frame->ch_layout.nb_channels;
    sampleRate = frame->sample_rate;

    // 计算时间戳
    if (frame->pts != AVNoPtsValue) {
        ts = av_q2d(stream_time_base_) * frame->pts;
    } else if (frame->best_effort_timestamp != AVNoPtsValue) {
        ts = av_q2d(stream_time_base_) * frame->best_effort_timestamp;
    } else {
        ts = 0.0;
    }

    // 复制音频数据
    int data_size = av_samples_get_buffer_size(nullptr, channels, frame->nb_samples,
                                               (AVSampleFormat)frame->format, 1);
    decoded.resize(data_size);
    memcpy(decoded.data(), frame->data[0], data_size);

    return 0;
}

int FFDecSW::pendingFrames() const
{
    // 软件解码器通常没有内部缓冲
    return 0;
}

void FFDecSW::clearFrames()
{
    // 软件解码器没有内部帧队列，但可以重置 temp_frame_
    // 这确保在 seeking 或视频切换时，解码器状态是干净的
    if (temp_frame_) {
        av_frame_unref(temp_frame_.get());
    }
    logger->debug("FFDecSW::clearFrames: Decoder frames cleared");
}

void FFDecSW::flushBuffers()
{
    if (codec_ctx_) {
        avcodec_flush_buffers(codec_ctx_);
        logger->debug("FFDecSW::flushBuffers: Decoder buffers flushed");
    }
}

Frame FFDecSW::convertPixelFormat(const Frame &frame, AVPixelFormat targetFormat)
{
    if (frame.isEmpty()) {
        return Frame();
    }

    AVPixelFormat srcFormat = frame.pixelFormat();
    if (srcFormat == targetFormat) {
        return frame;
    }

    int width = frame.width();
    int height = frame.height();

    // 检查是否需要重新创建转换上下文
    if (!sws_ctx_ ||
        last_width_ != width ||
        last_height_ != height ||
        last_format_ != srcFormat) {
        
        if (sws_ctx_) {
            sws_freeContext(sws_ctx_);
        }

        sws_ctx_ = sws_getContext(
            width, height, srcFormat,
            width, height, targetFormat,
            SWS_BILINEAR, nullptr, nullptr, nullptr);

        if (!sws_ctx_) {
            logger->error("FFDecSW::convertPixelFormat: failed to create SwsContext");
            return Frame();
        }

        last_width_ = width;
        last_height_ = height;
        last_format_ = srcFormat;
    }

    // 创建目标帧
    Frame targetFrame = Frame::createEmpty(
        frame.avFrame(),
        true,
        targetFormat
    );

    if (targetFrame.isEmpty()) {
        logger->error("FFDecSW::convertPixelFormat: failed to create target frame");
        return Frame();
    }

    // 执行转换
    // Frame::dataArr() 返回 uint8_t**，linesize() 返回 int*
    uint8_t **src_data = const_cast<Frame&>(frame).dataArr();
    const int *src_linesize = frame.linesize();
    uint8_t **dst_data = targetFrame.dataArr();
    int *dst_linesize = targetFrame.linesize();
    
    int ret = sws_scale(
        sws_ctx_,
        src_data, src_linesize, 0, height,
        dst_data, dst_linesize);

    if (ret != height) {
        logger->error("FFDecSW::convertPixelFormat: sws_scale failed: {} (expected {})", ret, height);
        return Frame();
    }

    // 复制时间戳和其他属性
    targetFrame.setTS(frame.ts());
    targetFrame.setTimeBase(frame.timeBase());

    return targetFrame;
}
