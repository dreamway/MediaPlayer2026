#include "movie.h"
#include "videoDecoder/audio.h"
#include "videoDecoder/chronons.h"
#include "videoDecoder/video.h"

#include <cassert>
#include <iostream>
#include <mutex>
#include <cstring>  // for memcpy, memcmp
#include <QDateTime>
#include <QDebug>
#include <QtLogging>
#include <QSettings>
#include <QCoreApplication>
using namespace std;
#include "GlobalDef.h"
#include "spdlog/spdlog.h"
#include <QTimer>

/**
 * Convenience macro, the return value should be used only directly in
 * function arguments but never stand-alone.
 */
#define av_err2str(errnum) av_make_error_string((char[AV_ERROR_MAX_STRING_SIZE]){0}, AV_ERROR_MAX_STRING_SIZE, errnum)

static double fraction_3Double(AVRational r)
{
    return r.den == 0 ? 0 : (double) r.num / r.den;
}

Movie::Movie()
    : audio_(*this)
    , video_(*this)
    , enableHardwareDecoding_(true)  // 默认启用硬件解码
{
    playState = PlayState::NONE;
    m_error = new char[ERROR_LEN];
    
    // 读取配置决定是否启用硬件解码
    QString cfgPath = QCoreApplication::applicationDirPath() + "/config/SystemConfig.ini";
    QSettings setting(cfgPath, QSettings::IniFormat);
    QVariant variant = setting.value("/System/EnableHardwareDecoding");
    enableHardwareDecoding_ = variant.isNull() ? true : variant.toBool();
    
    logger->info("Hardware decoding enabled: {}", enableHardwareDecoding_);
    
    // 初始化线程健康检查时间点
    auto now = std::chrono::steady_clock::now();
    lastVideoFrameTime_ = now;
    lastAudioFrameTime_ = now;
    lastDemuxTime_ = now;
}

Movie::~Movie()
{
    // 确保监控已停止
    if (systemMonitor_) {
        systemMonitor_->stop();
    }
    
    if (demuxThread_.joinable()) {
        demuxThread_.join();
    }
    if (m_error) {
        delete m_error;
        m_error = nullptr;
    }
}

int Movie::Open(QString filename)
{
    logger->debug("Movie.Open, filename is:{}", filename.toStdString());

    playState = PlayState::NONE;

    int result = open(filename);
    if (result != 0) {
        logger->error("Open video file:{} failed, result:{}", filename.toStdString(), result);
        showError(result);
        return -1;
    }

    setPlayState(PlayState::PLAYING);
    quit_.store(false);

    demuxThread_ = std::thread(&Movie::startDemux, this);
    logger->info("Movie.open, demuxThread is started.");

    // 启动系统资源监控
    if (!systemMonitor_) {
        systemMonitor_ = std::make_unique<SystemMonitor>();
    }
    systemMonitor_->start(this, &video_, &audio_);
    logger->info("SystemMonitor started");

    return 0;
}

std::pair<AVFrame *, int64_t> Movie::currentFrame()
{
    return video_.currentFrame();
}

int Movie::streamComponentOpen(unsigned int stream_index)
{
    avCodecContext_ = avcodec_alloc_context3(nullptr);
    if (!avCodecContext_) {
        logger->critical("Fail to avcodec_alloc_context3");
        return -1;
    }

    int ret;

    ret = avcodec_parameters_to_context(avCodecContext_, fmtctx_->streams[stream_index]->codecpar);
    if (ret < 0) {
        logger->critical("Fail to avcodec_parameters_to_context");
        return -1;
    }

    avCodecContext_->flags2 |= AV_CODEC_FLAG2_FAST; // 允许不符合规范的加速技巧。
    // 注意：硬件解码器可能不支持多线程，在使用硬件解码时会禁用
    avCodecContext_->thread_count = 8;              // 使用8线程解码（软件解码时使用）
    avCodecContext_->pkt_timebase = fmtctx_->streams[stream_index]->time_base;
    logger->info("avCodecContext->pkt_timebase:{}", fraction_3Double(avCodecContext_->pkt_timebase));
    logger->info("!!!! avCodec. find_decoder, codec_id:{}", int(avCodecContext_->codec_id));

    const AVCodec *codec = nullptr;
    
    // 对于视频流，尝试硬件解码
    if (avCodecContext_->codec_type == AVMEDIA_TYPE_VIDEO && enableHardwareDecoding_) {
        if (!hardwareDecoder_) {
            hardwareDecoder_ = std::make_unique<HardwareDecoder>();
        }
        
        // 保存原始参数，以便在硬件解码失败时恢复
        int original_width = avCodecContext_->width;
        int original_height = avCodecContext_->height;
        uint8_t* original_extradata = nullptr;
        int original_extradata_size = 0;
        if (avCodecContext_->extradata && avCodecContext_->extradata_size > 0) {
            original_extradata_size = avCodecContext_->extradata_size;
            original_extradata = (uint8_t*)av_malloc(original_extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
            if (original_extradata) {
                memcpy(original_extradata, avCodecContext_->extradata, original_extradata_size);
                memset(original_extradata + original_extradata_size, 0, AV_INPUT_BUFFER_PADDING_SIZE);
            }
        }
        
        codec = hardwareDecoder_->tryInitHardwareDecoder(avCodecContext_->codec_id, avCodecContext_);
        if (codec) {
            logger->info("Using hardware decoder: {}, device: {}", 
                       codec->name, hardwareDecoder_->getDeviceTypeName());
            // 硬件解码器通常不支持多线程，禁用线程以提高兼容性
            avCodecContext_->thread_count = 0;  // 0表示自动选择（硬件解码器通常使用单线程）
            // 硬件解码器可能不支持某些标志，清除可能不兼容的标志
            avCodecContext_->flags2 &= ~AV_CODEC_FLAG2_FAST;  // 移除FAST标志，硬件解码器可能不支持
            
            // 对于CUDA解码器，确保width、height和extradata在设置硬件上下文后仍然正确
            // 某些情况下，设置硬件上下文可能会影响这些参数
            if (hardwareDecoder_->getDeviceTypeName() && 
                strcmp(hardwareDecoder_->getDeviceTypeName(), "cuda") == 0) {
                // 确保width和height正确
                if (avCodecContext_->width != original_width || avCodecContext_->height != original_height) {
                    logger->warn("CUDA decoder: width/height changed after hw context setup, restoring: {}x{} -> {}x{}",
                                avCodecContext_->width, avCodecContext_->height, original_width, original_height);
                    avCodecContext_->width = original_width;
                    avCodecContext_->height = original_height;
                }
                
                // 确保extradata正确（某些情况下可能需要重新设置）
                if (original_extradata && original_extradata_size > 0) {
                    if (!avCodecContext_->extradata || 
                        avCodecContext_->extradata_size != original_extradata_size ||
                        memcmp(avCodecContext_->extradata, original_extradata, original_extradata_size) != 0) {
                        logger->debug("CUDA decoder: extradata may have changed, ensuring it's correct");
                        // extradata通常由avcodec_parameters_to_context设置，不应该改变
                        // 但如果改变了，可能需要重新设置（这里只记录，不修改，因为可能影响解码）
                    }
                }
            }
            
            logger->debug("Disabled multi-threading and cleared incompatible flags for hardware decoder");
            
            // 释放保存的extradata副本
            if (original_extradata) {
                av_freep(&original_extradata);
            }
        } else {
            logger->info("Hardware decoder not available, falling back to software decoder");
            // 清理硬件解码器状态
            hardwareDecoder_.reset();
            // 释放保存的extradata副本
            if (original_extradata) {
                av_freep(&original_extradata);
            }
        }
    }
    
    // 如果硬件解码失败或未启用，使用软件解码
    if (!codec) {
        codec = avcodec_find_decoder(avCodecContext_->codec_id);
        if (!codec) {
            logger->critical("Unsupported codec: {}", avcodec_get_name(avCodecContext_->codec_id));
            return -1;
        }
        logger->info("Using software decoder: {}", codec->name);
    }
    
    int open_ret = avcodec_open2(avCodecContext_, codec, nullptr);
    if (open_ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(open_ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
        logger->critical("Failed to open codec: {} (error: {})", codec->name, errbuf);
        // 如果硬件解码失败，尝试回退到软件解码
        if (hardwareDecoder_ && hardwareDecoder_->isInitialized()) {
            logger->warn("Hardware decoder open failed, trying software decoder fallback");
            
            // 关键修复：avcodec_open2失败后，codec context可能处于不一致状态
            // 需要先关闭它（如果已经部分打开），然后重新创建或重置
            // 最安全的方法是创建一个新的codec context
            AVCodecContext* old_ctx = avCodecContext_;
            avCodecContext_ = avcodec_alloc_context3(nullptr);
            if (!avCodecContext_) {
                logger->critical("Failed to allocate new codec context for fallback");
                avcodec_free_context(&old_ctx);
                return -1;
            }
            
            // 重新从codecpar复制参数
            int ret = avcodec_parameters_to_context(avCodecContext_, 
                                                   fmtctx_->streams[stream_index]->codecpar);
            if (ret < 0) {
                logger->critical("Failed to copy codec parameters for fallback: {}", ret);
                avcodec_free_context(&avCodecContext_);
                avCodecContext_ = old_ctx;
                return -1;
            }
            
            // 设置软件解码器的参数
            avCodecContext_->flags2 |= AV_CODEC_FLAG2_FAST;
            avCodecContext_->thread_count = 8;
            avCodecContext_->pkt_timebase = fmtctx_->streams[stream_index]->time_base;
            
            // 清理硬件解码器状态
            hardwareDecoder_.reset();
            
            // 释放旧的codec context
            avcodec_free_context(&old_ctx);
            
            // 查找软件解码器
            codec = avcodec_find_decoder(avCodecContext_->codec_id);
            if (!codec) {
                logger->critical("Failed to find software decoder for fallback");
                avcodec_free_context(&avCodecContext_);
                avCodecContext_ = nullptr;
                return -1;
            }
            
            // 打开软件解码器
            open_ret = avcodec_open2(avCodecContext_, codec, nullptr);
            if (open_ret < 0) {
                av_strerror(open_ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
                logger->critical("Software decoder fallback also failed: {}", errbuf);
                avcodec_free_context(&avCodecContext_);
                avCodecContext_ = nullptr;
                return -1;
            }
            logger->info("Successfully opened software decoder as fallback");
        } else {
            return -1;
        }
    }

    switch (avCodecContext_->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        audio_.stream_ = fmtctx_->streams[stream_index];
        audio_.codecctx_ = std::move(AVCodecContextPtr(avCodecContext_));
        break;
    case AVMEDIA_TYPE_VIDEO:
        video_.stream_ = fmtctx_->streams[stream_index];
        video_.codecctx_ = std::move(AVCodecContextPtr(avCodecContext_));
        break;
    default:
        return -1;
    }

    return static_cast<int>(stream_index);
}

//视频文件demux总入口
int Movie::open(QString filename)
{
    if (demuxThread_.joinable()) {
        logger->warn("Movie.open, filename:{}, demuxThread_->joinable, prev demuxThread is not ended. SHOULD NOT ENTER HERE", filename.toStdString());
        //FIX: 当正常播放完成， 实际demuxThread未销毁，在开下一个视频的时候，需要把上一个视频的demuxThread_停掉，重置状态后再开新视频
        quit_.store(true);
        try {
            demuxThread_.join();
        } catch (std::system_error ex) {
            logger->error("demuxThread.join exception:{}", ex.what());
        }

    } else {
        logger->info("demuxThread.joinable() is false, THIS IS NORMAL");
    }
    reset();

    logger->info("Movie.open, filename:{}", filename.toStdString());
    int result;

    AVFormatContext *fmtctx = avformat_alloc_context();
    AVDictionary *options = nullptr;
    const AVDictionaryEntry *tag = NULL;

    AVIOInterruptCB intrcb{nullptr, this};
    fmtctx->interrupt_callback = intrcb;
    lock.lock();
    //Open input firle, and allocate format context
    result = avformat_open_input(&fmtctx, filename.toUtf8().constData(), nullptr, &options);
    if (result != 0) {
        lock.unlock();
        logger->error("Movie.start, avformat_open_input failed:{}", result);
        showError(result);
        return -1;
    }
    fmtctx_.reset(fmtctx);
    logger->info("fmtctx_ is reset as new created fmtctx.");

    //获取流信息, retrieve stream information
    result = avformat_find_stream_info(fmtctx_.get(), nullptr);
    if (result < 0) {
        lock.unlock();
        logger->error("avformat_find_stream_info failed:{}", result);
        showError(result);
        return -1;
    }
    //获取总时长（毫秒)
    totalMilliseconds = fmtctx->duration / (AV_TIME_BASE / 1000);
    //获取总时长（秒)
    int totalSeconds = fmtctx->duration / AV_TIME_BASE;
    logger->info("获取媒体总时长（秒）:{}", totalSeconds);
    //打印视频流信息
    av_dump_format(fmtctx, 0, filename.toUtf8().constData(), 0);
    //打印音视频流信息
    mVideoStreamIndex = av_find_best_stream(fmtctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (mVideoStreamIndex < 0) {
        lock.unlock();
        logger->error("av_find_best_stream failed:{}", mVideoStreamIndex);
        showError(mVideoStreamIndex);
        return -1;
    }
    mVideoStream = fmtctx->streams[mVideoStreamIndex];

    videoWidth_ = mVideoStream->codecpar->width;
    videoHeight_ = mVideoStream->codecpar->height;
    m_frameRate = fraction_3Double(mVideoStream->avg_frame_rate);
    mTotalFrameNum = mVideoStream->nb_frames;

    logger->info("------------------------------------------------------------");
    logger->info("VideoStreamIdx（videoStream）{}", mVideoStreamIndex);
    logger->info("VidoeSize {}x{} ", mVideoStream->codecpar->width, mVideoStream->codecpar->height);
    logger->info("Video FPS: {} ", fraction_3Double(mVideoStream->avg_frame_rate));
    logger->info("Video Format {} ", mVideoStream->codecpar->format);
    logger->info("Nb_frames:{}", mTotalFrameNum);
    logger->info("------------------------------------------------------------");

    mAudioStreamIndex = av_find_best_stream(fmtctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (mAudioStreamIndex > 0) {
        mAudioStream = fmtctx->streams[mAudioStreamIndex];
        logger->info("音频信息（audioStream）{} ", mAudioStreamIndex);
        logger->info("SampleRate {} ", mAudioStream->codecpar->sample_rate);
        logger->info("SampleFormat {} ", mAudioStream->codecpar->format);
        logger->info("ChannelNo {} ", mAudioStream->codecpar->channels);
        logger->info("Codec {}", int(mAudioStream->codecpar->codec_id));
        logger->info("Audio FPS: {} ", fraction_3Double(mAudioStream->avg_frame_rate));
        logger->info("Audio FrameSize {} ", mAudioStream->codecpar->frame_size);
        logger->info("------------------------------------------------------------");
    }

    mSubtitleStreamIndex = av_find_best_stream(fmtctx, AVMEDIA_TYPE_SUBTITLE, -1, -1, nullptr, 0);
    if (mSubtitleStreamIndex > 0) {
        mSubtitleStream = fmtctx->streams[mSubtitleStreamIndex];
        logger->info("Subtitle信息（subtitleStream）{} ", mSubtitleStreamIndex);
        logger->info("样本率 {} ", mSubtitleStream->codecpar->sample_rate);
        logger->info("Subtitle采样格式 {}", mSubtitleStream->codecpar->format);
        logger->info("声道数 {} ", mSubtitleStream->codecpar->channels);
        logger->info("编码格式 {} ", int(mSubtitleStream->codecpar->codec_id));
        logger->info("Subtitle FPS: {} ", fraction_3Double(mSubtitleStream->avg_frame_rate));
        logger->info("一帧Subtitle的大小 {} ", mSubtitleStream->codecpar->frame_size);
        logger->info("------------------------------------------------------------");
    }

    lock.unlock();

    //Fetch AVDictionaryInfo
    while ((tag = av_dict_iterate(fmtctx->metadata, tag))) {
        logger->info("{}={}", tag->key, tag->value);
    }

    for (unsigned int i = 0; i < fmtctx_->nb_streams; i++) {
        auto *codecpar = fmtctx_->streams[i]->codecpar;
        switch (codecpar->codec_type) {
        case AVMEDIA_TYPE_VIDEO:
            mVideoStreamIndex = streamComponentOpen(i);
            break;
        case AVMEDIA_TYPE_AUDIO:
            mAudioStreamIndex = streamComponentOpen(i);
            break;
        case AVMEDIA_TYPE_SUBTITLE:
            mSubtitleStreamIndex = streamComponentOpen(i);
            break;
        }
    }

    if (mVideoStreamIndex < 0 && mAudioStreamIndex < 0) {
        logger->critical("Fail to open codecs");
        return -1;
    }

    clockbase_ = get_avtime() + milliseconds(750);
    sync_ = (mAudioStreamIndex > 0) ? SyncMaster::Audio : SyncMaster::Video;

    return 0;
}

bool Movie::IsSeeking()
{
    return mSeekFlag.load();
}

int Movie::startDemux()
{
    if (mAudioStreamIndex >= 0) {
        audioThread_ = std::thread(&Audio::start, &audio_);
        logger->info("Movie.startDemux, audioThread start...");
    }
    if (mVideoStreamIndex >= 0) {
        videoThread_ = std::thread(&Video::start, &video_);
        logger->info("Movie.startDemux, videoThread start...");
    }
    auto &audio_queue = audio_.packets_;
    auto &video_queue = video_.packets_;

    static int64_t outputCounter = 0;
    int ret;
    int64_t seek_target;

    while (!quit_.load(std::memory_order_relaxed)) {
        // 更新demux线程健康检查时间戳
        lastDemuxTime_ = std::chrono::steady_clock::now();
        
        if (mSeekFlag.load(std::memory_order_relaxed)) {
            // 使用更细粒度的锁，避免长时间持有
            {
                std::lock_guard<std::mutex> guard(mSeekInfoMutex);
                seek_target = mSeekInfo.seek_pos;
                logger->info("mSeekFlag is true, inside demux,  SEEKING!!!!!!!!!!!!!, seekPos:{}", seek_target);
            }
            
            // Seek操作不持有锁，避免阻塞其他线程
            ret = avformat_seek_file(fmtctx_.get(), -1, 0, seek_target, totalMilliseconds * 1000, AVSEEK_FLAG_FRAME);
            if (ret < 0) {
                logger->error("Seek failed. ret:{}", ret);
                showError(ret);
            } else {
                logger->info("Seek ok, clear buffered data");
                // 清理缓冲区时使用更短的锁
                // 重要：在Seeking时更新健康检查时间戳，避免误报线程不健康
                // Seeking后解码线程需要时间重新解码到目标位置，这是正常情况
                auto now = std::chrono::steady_clock::now();
                lastVideoFrameTime_ = now;
                lastAudioFrameTime_ = now;
                lastDemuxTime_ = now;
                
                video_.clear();
                audio_.clear();
            }

            if (ret < 0) {
                logger->error("!!!!! av_seek_frame  failed. ret:{}", ret);
                showError(ret);
                mSeekFlag.store(false);
                continue;
            }

            mSeekFlag.store(false);
            logger->info("mSeekFlag set to false, emit seekingFinished with target:{}", seek_target);

            emit seekingFinished(seek_target);
            logger->info("LEAVING SEEK !!!!!!!!   av_seek_frame for:{}", seek_target);
        }

        if (playState == PlayState::PAUSE) {
            logger->debug("PlayState::PAUSE, sleep for 5 ms");
            std::this_thread::sleep_for(milliseconds(5));
            continue;
        }

        AVPacket packet;
        int ret = av_read_frame(fmtctx_.get(), &packet);
        if (ret < 0) {
            //FIX: read Frame failed (EOF), but video_queue is not empty yet, should continue playing
            if (ret == AVERROR_EOF) {
                if (!video_queue.IsEmpty() || !audio_queue.IsEmpty() || false == video_.PictureRingBufferIsEmpty()) {
                    if (outputCounter % 500000 == 0) {
                        logger->debug(
                            "read EOF, but video_queue/audio_queue is not empty. continnue, "
                            "video_queue.Size:{},audio_queue.Size:{},pictureRingBufferIsEmpty:{}",
                            video_queue.Size(),
                            audio_queue.Size(),
                            video_.PictureRingBufferIsEmpty());
                    }
                    outputCounter += 1;
                    continue;
                } else {
                    logger->debug(
                        "read EOF, but video_queue/audio_queue is not empty. continnue, "
                        "video_queue.Size:{},audio_queue.Size:{},pictureRingBufferIsEmpty:{}, SHOULD BREAK NOW (in else)",
                        video_queue.Size(),
                        audio_queue.Size(),
                        video_.PictureRingBufferIsEmpty());
                    showError(ret);
                    quit_.store(true);
                    break;
                }
            }
        }

        //将解码出来的数据放至video_queue进一步消费
        if (packet.stream_index == mVideoStreamIndex) {
            while (!quit_.load(std::memory_order_acquire) && !video_queue.put(&packet)) {
                logger->debug("movie.start, put video queue false, sleep 100ms");
                std::this_thread::sleep_for(milliseconds(100));
            }
        } else if (packet.stream_index == mAudioStreamIndex) {
            while (!quit_.load(std::memory_order_acquire) && !audio_queue.put(&packet)) {
                logger->debug("movie.start,put audio queue failed, sleep 100ms");
                std::this_thread::sleep_for(milliseconds(100));
            }
        } else if (packet.stream_index == mSubtitleStreamIndex) {
            logger->debug("Movie.start, packet.stream_index==SubtitleStreamIndex.");
        }

        av_packet_unref(&packet);
    }

    logger->debug("Movie.mainloop, start leaving..., quit SHOULD BE True, is:{}", quit_.load());

    if (video_.codecctx_) {
        video_queue.setFinished();
        logger->debug("video_queue.setFinished ok.");
    }
    if (audio_.codecctx_) {
        audio_queue.setFinished();
        logger->debug("audio_queue.setFinished ok.");
    }

    if (audioThread_.joinable()) {
        audioThread_.join();
        logger->debug("audioThread joined.");
    }

    if (videoThread_.joinable()) {
        video_.stop();
        videoThread_.join();
        logger->debug("videoThead joined");
    }

    logger->info("startDemux is ending, emit playStateChanged with stop state");

    setPlayState(PlayState::STOP);

    logger->info("Movie.mainloop, return.");

    return 0;
}

int Movie::pause(bool isPause)
{
    if (isPause) {
        playState = PlayState::PAUSE;
    } else {
        playState = PlayState::PLAYING;
    }

    audio_.pause(isPause);
    video_.pause(isPause);
    return 0;
}

int Movie::stop()
{
    logger->info(" actual stop, set quit = true, demuxThread_.joinable?:{}", demuxThread_.joinable());
    
    // 提前设置 playState = STOP，让 IsStopped() 能立即返回 true
    // 这样可以避免 MainWindow 中的轮询等待
    if (playState != PlayState::STOP) {
        setPlayState(PlayState::STOP);
        logger->info("PlayState set to STOP early to allow fast stop detection");
    }
    
    // 停止系统资源监控
    if (systemMonitor_) {
        systemMonitor_->stop();
        logger->info("SystemMonitor stopped");
    }
    
    //使用原子操作修改quit_的值
    quit_.store(true);
    
    // 立即清空队列，避免解码线程等待消费者导致死锁
    // 这对于大码率视频切换时特别重要
    if (mVideoStreamIndex >= 0) {
        video_.clear();
        logger->info("Video queue cleared immediately to prevent deadlock");
    }
    if (mAudioStreamIndex >= 0) {
        audio_.clear();
        logger->info("Audio queue cleared immediately to prevent deadlock");
    }

    if (demuxThread_.joinable()) {
        logger->info("demuxThread.joinable.... start join");
        demuxThread_.join();
    } else {
        logger->warn("demuxThread_.joinable is False. NOT JOIN. MayBe ERROR");
    }

    totalMilliseconds = 0;

    return 0;
}

void Movie::reset()
{
    lock.lock();
    audio_.clear();
    video_.clear();
    if (fmtctx_) {
        int ret = avformat_flush(fmtctx_.get());
        if (ret < 0) {
            logger->warn("Movie.clear, avformat_flush failed:{}", ret);
            showError(ret);
        }
    }
    quit_.store(false);

    lock.unlock();
}

nanoseconds Movie::getMasterClock()
{
    switch (sync_) {
    case SyncMaster::Audio:
        // 如果音频已结束，切换到视频时钟，避免音频时钟卡住导致音视频不同步
        if (audio_.audioFinished_.load(std::memory_order_acquire)) {
            return video_.getClock();
        }
        return audio_.getClock();
    case SyncMaster::Video:
        return video_.getClock();
    default:
        assert(0);
        return getClock();
    }
}

nanoseconds Movie::getClock()
{
    assert(0);
    return nanoseconds::min();
}

void Movie::PlayPause(bool isPause)
{
    if (IsStopped()) {
        logger->info("Movie.PlayPause, IsStopped, Skip");
        return;
    }

    if (IsPlaying()) {
        logger->debug("Movie.PlayPause, IsPlaying, pause.");
        audio_.pause(true);
        video_.pause(true);
        setPlayState(PlayState::PAUSE);
    } else if (IsPaused()) {
        logger->debug("Movie.PlayPause, IsPaused, play");
        audio_.pause(false);
        video_.pause(false);

        setPlayState(PlayState::PLAYING);
    }
}

bool Movie::IsPlaying() const
{
    return playState == PlayState::PLAYING;
}

void Movie::showError(int err)
{
#if PRINT_LOG
    memset(m_error, 0, ERROR_LEN); // 将数组置零
    av_strerror(err, m_error, ERROR_LEN);
    logger->error("DecodeVideo Error：{}", m_error);
#else
    Q_UNUSED(err)
#endif
}

void Movie::Stop()
{
    if (IsStopped()) {
        logger->info("Already stopped");
        return;
    }

    stop();

    playState = PlayState::STOP;
}

bool Movie::IsStopped()
{
    logger->debug("before checking , current state:{}", int(playState));

    //如果当前是Quit状态，也需要修正其状态
    if (quit_.load(std::memory_order_relaxed) || playState == PlayState::STOP) {
        return true;
    }
    return false;
}

bool Movie::IsPaused() const
{
    return playState == PlayState::PAUSE;
}

bool Movie::IsOpened() const
{
    return fmtctx_ != nullptr;
}

void Movie::SetSeekFlag(bool flag)
{
    mSeekFlag.store(flag);
}

bool Movie::SetSeek(int64_t position)
{
    if (position < 0 || position > totalMilliseconds / 1000) {
        logger->info("position:{},  smaller than 0 or larger than:{}", position, totalMilliseconds / 1000);
        mSeekFlag.store(false);
        return false;
    }

    lock.lock();
    if (!fmtctx_.get()) {
        logger->warn(" not decoded yet, could not seek");
        mSeekFlag.store(false);
        lock.unlock();
        return false;
    }

    if (!mSeekFlag.load()) {
        int64_t timestamp = position * AV_TIME_BASE;

        logger->info(
            "position:{}, should reseek to timstamp:{}, position*AV_TIME_BASE:{}, fmtctx_->start_time:{} , AV_TIME_BASE:{} ",
            position,
            (int64_t) timestamp,
            position * AV_TIME_BASE,
            fmtctx_->start_time,
            AV_TIME_BASE);

        {
            std::lock_guard<std::mutex> guard(mSeekInfoMutex);
            logger->info(" entering guard seekInfoMutex. timestamp:{}", timestamp);
            mSeekInfo.seek_pos = timestamp;
            mSeekInfo.seek_flags = AVSEEK_FLAG_FRAME; //AVSEEK_FLAG_ANY; //AVSEEK_FLAG_BACKWARD;
            logger->info(" leaving guard seekInfoMutex.");
        }

        logger->warn(" mSeekFlag set to true.");
        mSeekFlag.store(true);
        lock.unlock();
        return true;
    } else {
        logger->warn("mseekFlag is alreay true. skip");
        mSeekFlag.store(false);
        lock.unlock();
        return false;
    }
}

// 返回视频时长(毫秒)
int64_t Movie::GetDurationInMs() const
{
    return totalMilliseconds;
}

void Movie::SetVolume(float volume)
{
    if (volume <= 0.0) {
        volume = 0;
    }
    if (volume >= 1.0) {
        volume = 1.0;
    }
    return audio_.setVolume(volume);
}

float Movie::GetVolume()
{
    return audio_.getVolume();
}

void Movie::ToggleMute()
{
    audio_.ToggleMute();
}

bool Movie::SetPlaybackRate(double rate)
{
    return false;
}

double Movie::GetPlaybackRate() const
{
    return 1.0f;
}

void Movie::setPlayState(PlayState state)
{
    playState = state;
    logger->info(" emit playStateChanged with state:{}", int(state));
    emit playStateChanged(state);
}

// 获取视频参数，使用avcodec_parameters_free()清理空间
AVCodecParameters *Movie::getVideoParameters()
{
    lock.lock();
    if (!fmtctx_.get()) {
        lock.unlock();
        return nullptr;
    }
    AVCodecParameters *videoParameters = avcodec_parameters_alloc();
    avcodec_parameters_copy(videoParameters, fmtctx_.get()->streams[mVideoStreamIndex]->codecpar);
    lock.unlock();
    return videoParameters;
}

// 获取音频参数，使用avcodec_parameters_free()清理空间
AVCodecParameters *Movie::getAudioParameters()
{
    lock.lock();
    if (!fmtctx_.get()) {
        lock.unlock();
        return nullptr;
    }
    AVCodecParameters *audioParameters = avcodec_parameters_alloc();
    avcodec_parameters_copy(audioParameters, fmtctx_.get()->streams[mAudioStreamIndex]->codecpar);
    lock.unlock();
    return audioParameters;
}

float Movie::GetVideoFrameRate()
{
    return m_frameRate;
}

PlayState Movie::GetPlayState()
{
    logger->info(" quit_ (if playState is STOP, quit should be true), playState:{}, quit:{} ", int(playState), quit_.load());
    return playState;
}

bool Movie::isThreadHealthy()
{
    // 如果正在Seeking，暂时放宽健康检查，避免误报
    // Seeking时清空队列后需要时间重新解码，这是正常情况
    if (mSeekFlag.load(std::memory_order_relaxed)) {
        return true;  // Seeking期间认为线程健康
    }
    
    auto now = std::chrono::steady_clock::now();
    auto videoTimeout = now - lastVideoFrameTime_;
    auto audioTimeout = now - lastAudioFrameTime_;
    auto demuxTimeout = now - lastDemuxTime_;
    
    // 如果超过5秒没有新帧，认为线程可能有问题
    bool videoHealthy = videoTimeout < std::chrono::seconds(5);
    bool audioHealthy = audioTimeout < std::chrono::seconds(5);
    bool demuxHealthy = demuxTimeout < std::chrono::seconds(5);
    
    if (!videoHealthy || !audioHealthy || !demuxHealthy) {
        logger->warn("Thread health check failed - Video: {}ms, Audio: {}ms, Demux: {}ms", 
                    std::chrono::duration_cast<std::chrono::milliseconds>(videoTimeout).count(),
                    std::chrono::duration_cast<std::chrono::milliseconds>(audioTimeout).count(),
                    std::chrono::duration_cast<std::chrono::milliseconds>(demuxTimeout).count());
    }
    
    return videoHealthy && audioHealthy && demuxHealthy;
}

void Movie::waitForVideoPreload(int timeoutMs)
{
    if (mVideoStreamIndex >= 0) {
        video_.waitForPreload(timeoutMs);
    }
}

bool Movie::isPlaybackFinished(int64_t currentElapsedSeconds)
{
    // 注意：当onPlayStateChanged被调用时，playState可能已经是STOP了
    // 所以我们不能简单地因为playState==STOP就返回false
    // 需要综合其他条件来判断是否是播放完成导致的STOP
    
    // 如果quit_为true，说明已经停止，但我们仍然可以检查是否是播放完成
    
    // 检查1：队列是否都为空（或基本为空）
    bool videoQueueEmpty = video_.packets_.IsEmpty();
    bool audioQueueEmpty = audio_.packets_.IsEmpty();
    bool pictureQueueEmpty = video_.PictureRingBufferIsEmpty();
    
    // 检查图片队列是否基本为空（可能还有少量未消费的帧，但只要不再写入就认为完成）
    size_t readIdx = video_.getQueueReadIndex();
    size_t writeIdx = video_.getQueueWriteIndex();
    size_t queueSize = video_.getQueueSize();
    size_t usedSlots = (writeIdx >= readIdx) ? (writeIdx - readIdx) : (queueSize - readIdx + writeIdx);
    // 如果队列使用量少于总大小的20%，认为基本为空（可能还有最后几帧在渲染）
    bool pictureQueueMostlyEmpty = (usedSlots * 5) <= queueSize;
    
    // 检查2：线程健康状态
    // 对于播放完成，Video和Audio线程停止是正常的，但Demux应该也停止了（因为EOF）
    auto now = std::chrono::steady_clock::now();
    auto videoTimeout = now - lastVideoFrameTime_;
    auto audioTimeout = now - lastAudioFrameTime_;
    auto demuxTimeout = now - lastDemuxTime_;
    
    // Video和Audio超过3秒没有新帧（可能是播放完成）
    // 对于播放完成的情况，Video和Audio线程停止是正常的
    bool videoFinished = videoTimeout >= std::chrono::seconds(3);
    bool audioFinished = audioTimeout >= std::chrono::seconds(3);
    // Demux线程在检测到EOF后会快速退出，所以demuxTimeout可能很短或很长（如果已经退出）
    // 我们主要依赖quit_标志和队列状态来判断
    
    // 检查3：播放时间是否接近总时长（如果总时长有效）
    bool timeNearEnd = false;
    int64_t totalSeconds = totalMilliseconds / 1000;
    if (totalSeconds > 3) {
        // 如果当前时间接近总时长（误差在10秒内），或者已经超过总时长
        // 放宽误差范围，因为某些视频可能在接近结束时提前停止
        int64_t timeDiff = totalSeconds - currentElapsedSeconds;
        timeNearEnd = (timeDiff >= -2 && timeDiff <= 10);  // 允许10秒误差（可能是计时不准确或提前停止）
        if (timeDiff < -2) {
            // 如果已经超过总时长，肯定是播放完成了
            timeNearEnd = true;
        }
    }
    
    // 检查4：如果队列为空且线程已停止，即使时间未到，也可能是播放完成
    // 这可以处理某些视频在播放过程中提前结束的情况
    bool earlyFinish = false;
    
    // 情况1：所有队列都为空，且线程已停止
    if (videoQueueEmpty && audioQueueEmpty && (pictureQueueEmpty || pictureQueueMostlyEmpty)) {
        // 如果所有队列都为空，且线程已停止超过1秒，可能是播放完成
        if (videoFinished && audioFinished) {
            int64_t videoTimeoutSeconds = std::chrono::duration_cast<std::chrono::seconds>(videoTimeout).count();
            int64_t audioTimeoutSeconds = std::chrono::duration_cast<std::chrono::seconds>(audioTimeout).count();
            // 如果线程停止超过1秒，且当前播放时间超过总时长的70%，可能是播放完成
            if (videoTimeoutSeconds >= 1 && audioTimeoutSeconds >= 1) {
                if (totalSeconds > 3 && currentElapsedSeconds >= totalSeconds * 0.7) {
                    earlyFinish = true;
                    logger->info("Early finish detected: queues empty, threads stopped, currentTime:{}s/{}, progress:{}%",
                                currentElapsedSeconds, totalSeconds, (currentElapsedSeconds * 100) / totalSeconds);
                } else if (totalSeconds <= 3 && currentElapsedSeconds >= 1) {
                    // 对于短视频（<=3秒），如果播放超过1秒且队列为空，可能是播放完成
                    earlyFinish = true;
                    logger->info("Early finish detected for short video: queues empty, threads stopped, currentTime:{}s",
                                currentElapsedSeconds);
                }
            }
        }
    }
    
    // 情况2：视频队列为空、图片队列为空，但音频队列还有数据
    // 如果音频解码线程已经停止（EOF），即使队列中还有数据，也应该认为播放完成
    // 因为这些数据可能是无法解码的（EOF后的残留数据）
    if (videoQueueEmpty && (pictureQueueEmpty || pictureQueueMostlyEmpty) && !audioQueueEmpty) {
        if (videoFinished && audioFinished) {
            int64_t videoTimeoutSeconds = std::chrono::duration_cast<std::chrono::seconds>(videoTimeout).count();
            int64_t audioTimeoutSeconds = std::chrono::duration_cast<std::chrono::seconds>(audioTimeout).count();
            // 如果视频和音频线程都已停止超过3秒，且视频队列和图片队列都为空
            // 即使音频队列还有数据，也应该认为播放完成（音频解码线程可能因为EOF而停止）
            if (videoTimeoutSeconds >= 3 && audioTimeoutSeconds >= 3) {
                // 检查是否接近视频结束（当前时间超过总时长的80%）
                if (totalSeconds > 3 && currentElapsedSeconds >= totalSeconds * 0.8) {
                    earlyFinish = true;
                    logger->info("Early finish detected: video/audio threads stopped, video/picture queues empty, "
                                "but audio queue has {} packets (likely EOF residue). currentTime:{}s/{}, progress:{}%",
                                audio_.packets_.Size(), currentElapsedSeconds, totalSeconds, 
                                (currentElapsedSeconds * 100) / totalSeconds);
                } else if (totalSeconds <= 3 && videoTimeoutSeconds >= 3 && audioTimeoutSeconds >= 3) {
                    // 对于短视频，如果线程停止超过3秒，也认为是播放完成
                    earlyFinish = true;
                    logger->info("Early finish detected for short video: threads stopped >3s, audio queue has {} packets (likely EOF residue)",
                                audio_.packets_.Size());
                }
            }
        }
    }
    
    // 综合判断：
    // 1. 所有队列都为空（说明数据已经处理完）
    // 2. Video和Audio线程都已停止（超过3秒没有新帧）
    // 3. 要么quit_为true（说明已经停止），要么时间接近结束，要么线程停止时间超过总时长（肯定播放完了）
    // 注意：如果总时长无效（<=3秒），主要依赖队列和线程状态来判断
    
    // 新增条件：如果Video和Audio线程停止时间超过总时长+5秒，肯定是播放完成了
    bool timeoutExceeded = false;
    if (totalSeconds > 3) {
        int64_t videoTimeoutSeconds = std::chrono::duration_cast<std::chrono::seconds>(videoTimeout).count();
        int64_t audioTimeoutSeconds = std::chrono::duration_cast<std::chrono::seconds>(audioTimeout).count();
        // 如果线程停止时间超过总时长+5秒，肯定是播放完成了
        timeoutExceeded = (videoTimeoutSeconds > totalSeconds + 5) && (audioTimeoutSeconds > totalSeconds + 5);
    }
    
    // 综合判断：
    // 1. 视频队列为空（说明已经没有新视频数据）
    // 2. 图片队列为空或基本为空（可能还有最后几帧在渲染）
    // 3. Video和Audio线程都已停止（超过3秒没有新帧）
    // 4. 满足停止条件之一：quit_为true、时间接近结束、总时长无效、超时超过总时长、提前完成、或音频队列为空
    // 注意：如果earlyFinish为true，即使音频队列还有数据（EOF后的残留数据），也应该认为播放完成
    bool isFinished = videoQueueEmpty && (pictureQueueEmpty || pictureQueueMostlyEmpty) && 
                      videoFinished && audioFinished && 
                      (quit_.load() || timeNearEnd || totalSeconds <= 3 || timeoutExceeded || earlyFinish || audioQueueEmpty);
    
    if (isFinished) {
        logger->info("Playback finished detected - VideoQueue:{}, AudioQueue:{}, PictureQueue:{}, PictureQueueMostlyEmpty:{}, "
                    "VideoTimeout:{}ms, AudioTimeout:{}ms, DemuxTimeout:{}ms, "
                    "CurrentTime:{}s, TotalTime:{}s, TimeNearEnd:{}, TimeoutExceeded:{}, UsedSlots:{}/{}",
                    videoQueueEmpty, audioQueueEmpty, pictureQueueEmpty, pictureQueueMostlyEmpty,
                    std::chrono::duration_cast<std::chrono::milliseconds>(videoTimeout).count(),
                    std::chrono::duration_cast<std::chrono::milliseconds>(audioTimeout).count(),
                    std::chrono::duration_cast<std::chrono::milliseconds>(demuxTimeout).count(),
                    currentElapsedSeconds, totalSeconds, timeNearEnd, timeoutExceeded, usedSlots, queueSize);
    }
    
    return isFinished;
}

bool Movie::checkAndStopIfFinished(int64_t currentElapsedSeconds)
{
    // 如果已经是STOP状态，不需要检查
    if (playState == PlayState::STOP || quit_.load()) {
        return false;
    }
    
    // 检查是否播放完成
    if (isPlaybackFinished(currentElapsedSeconds)) {
        logger->info("checkAndStopIfFinished: Detected playback finished, triggering stop. currentElapsed:{}s", currentElapsedSeconds);
        
        // 主动触发停止
        // 注意：这里设置quit_为true，会让startDemux退出并最终设置playState为STOP
        quit_.store(true);
        
        // 也可以直接设置playState并发送信号，但为了保持一致性，让startDemux正常退出更好
        // 不过为了更快响应，我们可以提前发送信号
        // 但要注意：如果startDemux还在运行，可能会重复发送STOP信号
        
        // 先检查一下，如果quit_已经设置，startDemux应该很快会退出
        logger->info("checkAndStopIfFinished: quit_ set to true, waiting for startDemux to exit naturally");
        
        return true;
    }
    
    return false;
}