#include "Demuxer.h"
#include "../GlobalDef.h"
#include <spdlog/spdlog.h>

extern spdlog::logger *logger;

static double fraction_3Double(AVRational r)
{
    return r.den == 0 ? 0 : (double) r.num / r.den;
}

Demuxer::Demuxer()
    : videoStreamIndex_(-1)
    , audioStreamIndex_(-1)
    , subtitleStreamIndex_(-1)
    , videoStream_(nullptr)
    , audioStream_(nullptr)
    , subtitleStream_(nullptr)
    , durationMs_(0)
    , videoWidth_(0)
    , videoHeight_(0)
    , videoFrameRate_(0.0)
    , totalFrameNum_(0)
{
    // 原子变量使用默认初始化
}

Demuxer::~Demuxer()
{
    close();
}

bool Demuxer::open(const QString& filename)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 如果已经打开，先关闭
    if (fmtctx_) {
        close();
    }
    
    logger->info("Demuxer::open, filename:{}", filename.toStdString());
    
    AVFormatContext *fmtctx = avformat_alloc_context();
    if (!fmtctx) {
        logger->error("Demuxer::open, avformat_alloc_context failed");
        return false;
    }
    
    // 设置中断回调（用于取消操作）
    // 注意：这里暂时不设置 interrupt_callback，因为需要 Movie 的实例
    // 后续可以考虑通过回调函数或其他方式传递
    
    AVDictionary *options = nullptr;
    
    // 打开输入文件
    int result = avformat_open_input(&fmtctx, filename.toUtf8().constData(), nullptr, &options);
    if (result != 0) {
        logger->error("Demuxer::open, avformat_open_input failed:{}", result);
        avformat_free_context(fmtctx);
        return false;
    }
    
    fmtctx_.reset(fmtctx);
    
    // 获取流信息
    result = avformat_find_stream_info(fmtctx_.get(), nullptr);
    if (result < 0) {
        logger->error("Demuxer::open, avformat_find_stream_info failed:{}", result);
        fmtctx_.reset();
        return false;
    }
    
    // 获取总时长（毫秒）
    durationMs_ = fmtctx->duration / (AV_TIME_BASE / 1000);
    int totalSeconds = fmtctx->duration / AV_TIME_BASE;
    logger->info("Demuxer::open, 获取媒体总时长（秒）:{}", totalSeconds);
    
    // 打印视频流信息
    av_dump_format(fmtctx, 0, filename.toUtf8().constData(), 0);
    
    // 查找视频流
    videoStreamIndex_ = av_find_best_stream(fmtctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (videoStreamIndex_ >= 0) {
        videoStream_ = fmtctx->streams[videoStreamIndex_];
        videoWidth_ = videoStream_->codecpar->width;
        videoHeight_ = videoStream_->codecpar->height;
        videoFrameRate_ = fraction_3Double(videoStream_->avg_frame_rate);
        totalFrameNum_ = videoStream_->nb_frames;
        
        logger->info("------------------------------------------------------------");
        logger->info("VideoStreamIdx（videoStream）{}", videoStreamIndex_);
        logger->info("VideoSize {}x{} ", videoWidth_, videoHeight_);
        logger->info("Video FPS: {} ", videoFrameRate_);
        logger->info("Video Format {} ", videoStream_->codecpar->format);
        logger->info("Nb_frames:{}", totalFrameNum_);
        logger->info("------------------------------------------------------------");
    } else {
        logger->warn("Demuxer::open, no video stream found");
    }
    
    // 查找音频流
    audioStreamIndex_ = av_find_best_stream(fmtctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audioStreamIndex_ >= 0) {
        audioStream_ = fmtctx->streams[audioStreamIndex_];
        logger->info("音频信息（audioStream）{} ", audioStreamIndex_);
        logger->info("SampleRate {} ", audioStream_->codecpar->sample_rate);
        logger->info("SampleFormat {} ", audioStream_->codecpar->format);
        logger->info("ChannelNo {} ", audioStream_->codecpar->ch_layout.nb_channels);
        logger->info("Codec {}", int(audioStream_->codecpar->codec_id));
        logger->info("Audio FPS: {} ", fraction_3Double(audioStream_->avg_frame_rate));
        logger->info("Audio FrameSize {} ", audioStream_->codecpar->frame_size);
        logger->info("------------------------------------------------------------");
    } else {
        logger->info("Demuxer::open, no audio stream found");
    }
    
    // 查找字幕流
    subtitleStreamIndex_ = av_find_best_stream(fmtctx, AVMEDIA_TYPE_SUBTITLE, -1, -1, nullptr, 0);
    if (subtitleStreamIndex_ >= 0) {
        subtitleStream_ = fmtctx->streams[subtitleStreamIndex_];
        logger->info("Subtitle信息（subtitleStream）{} ", subtitleStreamIndex_);
        logger->info("样本率 {} ", subtitleStream_->codecpar->sample_rate);
        logger->info("Subtitle采样格式 {}", subtitleStream_->codecpar->format);
        logger->info("声道数 {} ", subtitleStream_->codecpar->ch_layout.nb_channels);
        logger->info("编码格式 {} ", int(subtitleStream_->codecpar->codec_id));
        logger->info("Subtitle FPS: {} ", fraction_3Double(subtitleStream_->avg_frame_rate));
        logger->info("一帧Subtitle的大小 {} ", subtitleStream_->codecpar->frame_size);
        logger->info("------------------------------------------------------------");
    }
    
    // eof_ 和 seeking_ 已被移除，现在通过 PlayController 状态机管理
    
    logger->info("Demuxer::open succeeded");
    return true;
}

void Demuxer::close()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (fmtctx_) {
        logger->info("Demuxer::close");
        fmtctx_.reset();
    }
    
    videoStreamIndex_ = -1;
    audioStreamIndex_ = -1;
    subtitleStreamIndex_ = -1;
    videoStream_ = nullptr;
    audioStream_ = nullptr;
    subtitleStream_ = nullptr;
    durationMs_ = 0;
    videoWidth_ = 0;
    videoHeight_ = 0;
    videoFrameRate_ = 0.0;
    totalFrameNum_ = 0;
    // eof_ 和 seeking_ 已被移除，现在通过 PlayController 状态机管理
}

bool Demuxer::isOpened() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return fmtctx_ != nullptr;
}

bool Demuxer::readPacket(AVPacket& packet)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!fmtctx_) {
        logger->error("Demuxer::readPacket, not opened");
        return false;
    }

    
    int ret = av_read_frame(fmtctx_.get(), &packet);
    if (ret < 0) {
        if (ret == AVERROR_EOF) {
            eof_ = true;
            logger->trace("Demuxer::readPacket, EOF");
        } else {
            logger->error("Demuxer::readPacket, av_read_frame failed:{}", ret);
        }
        return false;
    }

    eof_ = false;
    return true;
}

bool Demuxer::seek(int64_t positionMs, bool backward)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!fmtctx_) {
        logger->error("Demuxer::seek, not opened");
        return false;
    }
    
    // seeking_ 已被移除，现在通过 PlayController 状态机管理
    
    // 将毫秒转换为 AV_TIME_BASE 单位（微秒）
    // positionMs 是毫秒，需要转换为微秒
    int64_t seekTarget = positionMs * 1000;  // 转换为微秒
    
    logger->info("Demuxer::seek, positionMs:{}, seekTarget:{} (us), backward:{}", positionMs, seekTarget, backward);
    
    // 优化：使用更智能的KeyFrame跳转策略，直接跳到目标点附近的关键帧
    // 1. 优先使用AVSEEK_FLAG_FRAME直接跳到目标点附近的关键帧（避免"先跳回原点"的问题）
    // 2. 如果失败，根据backward参数选择向后或向前搜索
    // 这样可以实现直接跳转，而不是先跳回原点再跳到目标点
    
    int ret = -1;
    bool seekOk = false;
    
    // 策略1：优先尝试直接跳到目标点附近的关键帧（使用AVSEEK_FLAG_FRAME）
    // AVSEEK_FLAG_FRAME会跳到目标时间戳附近的关键帧，而不是之前的关键帧
    // 这样可以避免"先跳回原点再跳到目标点"的问题
    ret = av_seek_frame(fmtctx_.get(), -1, seekTarget, AVSEEK_FLAG_FRAME);
    seekOk = (ret >= 0);
    
    if (seekOk) {
        logger->debug("Demuxer::seek: Direct keyframe seek succeeded (AVSEEK_FLAG_FRAME)");
    } else if (backward) {
        // 策略2：如果直接跳转失败且需要向后搜索，尝试向后搜索到关键帧
        logger->debug("Demuxer::seek: Direct seek failed, trying backward seek to keyframe");
        ret = av_seek_frame(fmtctx_.get(), -1, seekTarget, AVSEEK_FLAG_BACKWARD);
        seekOk = (ret >= 0);
        if (seekOk) {
            logger->debug("Demuxer::seek: Backward seek succeeded (AVSEEK_FLAG_BACKWARD)");
        }
    }
    
    if (!seekOk) {
        // 策略3：如果还是失败，尝试不使用任何标志（向前搜索到最近的关键帧）
        logger->debug("Demuxer::seek: Previous seeks failed, trying forward seek");
        ret = av_seek_frame(fmtctx_.get(), -1, seekTarget, 0);
        seekOk = (ret >= 0);
        if (seekOk) {
            logger->debug("Demuxer::seek: Forward seek succeeded");
        }
    }
    
    // 参考 QMPlayer2 的错误恢复机制
    if (!seekOk) {
        // 尝试读取一个数据包，检查是否是 EOF 或其他错误
        // 使用 av_packet_alloc() 替代已废弃的 av_init_packet()
        AVPacket* testPacket = av_packet_alloc();
        if (!testPacket) {
            logger->error("Demuxer::seek: Failed to allocate test packet");
            // seeking_ 已被移除，现在通过 PlayController 状态机管理
            return false;
        }
        
        int readRet = av_read_frame(fmtctx_.get(), testPacket);
        
        if (readRet == AVERROR_EOF || readRet == 0) {
            // 如果是 EOF 或成功读取，尝试反向 seek
            if (readRet == 0) {
                av_packet_unref(testPacket);
            }
            
            int64_t durationUs = durationMs_ * 1000;
            if (durationUs <= 0 || seekTarget < durationUs) {
                // 尝试反向 seek（参考 QMPlayer2）
                logger->warn("Demuxer::seek, first seek failed ({}), trying reverse seek", ret);
                ret = av_seek_frame(fmtctx_.get(), -1, seekTarget, !backward ? AVSEEK_FLAG_BACKWARD : 0);
                seekOk = (ret >= 0);
            } else if (readRet == AVERROR_EOF) {
                // 允许 seek 到文件末尾（参考 QMPlayer2）
                seekOk = true;
                logger->info("Demuxer::seek: Allowing seek to end of file");
            }
        }
        
        // 释放测试数据包
        av_packet_free(&testPacket);
        
        if (!seekOk) {
            char errbuf[64] = {0};
            av_strerror(ret, errbuf, sizeof(errbuf));
            logger->error("Demuxer::seek, av_seek_frame failed after retry: {} ({})", ret, errbuf);
            // seeking_ 已被移除，现在通过 PlayController 状态机管理
            return false;
        }
    }
    
    logger->info("Demuxer::seek succeeded");
    // seeking_ 已被移除，现在通过 PlayController 状态机管理
    // eof_ 已被移除，现在通过 PlayController 状态机管理  // Seek 后重置 EOF 标志
    
    return true;
}

AVFormatContext* Demuxer::releaseFormatContext()
{
    std::lock_guard<std::mutex> lock(mutex_);
    AVFormatContext* ptr = fmtctx_.release();
    // 重置相关指针，因为不再拥有所有权
    videoStream_ = nullptr;
    audioStream_ = nullptr;
    subtitleStream_ = nullptr;
    logger->info("Demuxer::releaseFormatContext, ownership transferred");
    return ptr;
}
