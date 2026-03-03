#include "AudioThread.h"
#include "GlobalDef.h"
#include "PlayController.h"
#include "chronons.h"
#include "videoDecoder/OpenALAudio.h"
#include "videoDecoder/ffmpeg.h"
#include "videoDecoder/packet_queue.h"
#include "videoDecoder/AVClock.h"
#include "videoDecoder/Statistics.h"

#include <chrono>
#include <spdlog/spdlog.h>
#include <thread>

using std::chrono::duration_cast;

extern spdlog::logger *logger;

// 队列满等待时间（100ms，防止永久阻塞）
constexpr milliseconds QueueFullWaitTime{100};

// 最大连续队列满等待次数
constexpr int MaxQueueFullWaitCount{20};

// 队列满等待统计（用于动态调整）
static int queueFullWaitCount_{0};

// 上次队列满时间
static auto lastQueueFullTime = std::chrono::steady_clock::now();

AudioThread::AudioThread(PlayController *controller, Audio *audio, QObject *parent)
    : AVThread(controller)
    , audio_(audio)
    , aPackets_(nullptr)
    // 注意：paused_ 已统一到状态机管理
    , wasSeeking_(false)
{
    if (controller_) {
        aPackets_ = controller_->getAudioPacketQueue();
    }

    // 添加启动延迟，避免刚启动时环境未准备好
    logger->info("AudioThread created");
}

AudioThread::~AudioThread()
{
    // 断开finished()信号连接，避免Qt自动删除对象
    // 析构函数中不应该再调用deleteLater
    disconnect(this, SIGNAL(finished()), this, SLOT(deleteLater()));

    requestStop();

    // 检查线程是否还在运行（添加异常保护）
    bool wasRunning = false;
    try {
        wasRunning = isRunning();
    } catch (...) {
        logger->warn("AudioThread::~AudioThread: Exception while checking isRunning(), thread may be destroyed");
        wasRunning = false; // 假设已停止，继续清理
    }

    if (wasRunning) {
        wait(5000);

        // 再次检查（添加异常保护）
        bool stillRunning = false;
        try {
            stillRunning = isRunning();
        } catch (...) {
            logger->warn("AudioThread::~AudioThread: Exception while checking isRunning() after wait");
            stillRunning = false;
        }

        if (stillRunning) {
            logger->warn("AudioThread still running after wait, terminating");
            AVThread::terminate();
            wait(1000);
        }
    }
    logger->info("AudioThread destroyed");
}

void AudioThread::requestStop()
{
    br_ = true; // 使用 AVThread 的 br_ 标志
    if (audio_) {
        audio_->stop();
    }
    logger->info("AudioThread::requestStop called");
}

void AudioThread::handleFlushAudio()
{
    if (controller_->getFlushAudio()) {
        if (codecctx_) {
            avcodec_flush_buffers(codecctx_.get());
        }
        audio_->clear();
        samplesPos_ = 0;
        samplesLen_ = 0;
        controller_->setFlushAudio(false);
    }
}

void AudioThread::handlePausedState()
{
    if (controller_ && controller_->isPaused()) {
        for (int i = 0; i < 10 && !br_; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

bool AudioThread::handleSeekingState()
{
    bool isSeeking = controller_->isSeeking();

    if (isSeeking) {
        if (!wasSeeking_) {
            wasSeeking_ = true;
            try {
                if (codecctx_) {
                    avcodec_flush_buffers(codecctx_.get());
                }
            } catch (const std::exception &e) {
                logger->error("AudioThread::handleSeekingState: Exception while flushing decoder: {}", e.what());
            } catch (...) {
                logger->error("AudioThread::handleSeekingState: Unknown exception while flushing decoder");
            }

            try {
                audio_->clear();
            } catch (const std::exception &e) {
                logger->error("AudioThread::handleSeekingState: Exception while clearing audio buffers: {}", e.what());
            } catch (...) {
                logger->error("AudioThread::handleSeekingState: Unknown exception while clearing audio buffers");
            }

            samplesPos_ = 0;
            samplesLen_ = 0;

            // 重置音频时钟基准，确保 Seeking 后时钟计算正确
            // 关键修复：音频时钟必须与视频时钟基准一致，使用 getBasePts() 而不是 zero
            nanoseconds seekBasePts = controller_->getBasePts();
            currentPts_ = seekBasePts;
            deviceStartTime_ = {}; // 重置为默认值，标记为未开始播放
            audioFinished_.store(false, std::memory_order_release);

            // 重置 firstFrameAfterSeekLogged_，用于记录 seeking 后第一个解码的音频帧
            firstFrameAfterSeekLogged_ = false;

            // 通知 Audio 重置 basePts
            if (audio_) {
                audio_->updateClock(seekBasePts); // 设置基准为 seeking 位置
                
                // 更新 AVClock（如果使用音频时钟）
                if (controller_) {
                    AVClock *masterClock = controller_->getAVClock();
                    if (masterClock && masterClock->clockType() == AVClock::AudioClock) {
                        double ptsSeconds = static_cast<double>(seekBasePts.count()) / 1000000000.0;
                        masterClock->updateValue(ptsSeconds);
                    }
                }
            }
        }

        if (!aPackets_) {
            logger->error("AudioThread::handleSeekingState: aPackets_ is null");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return true;
        }

        size_t queueSize = aPackets_->Size();
        if (queueSize == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return true;
        }

        // 只跳过 PTS 小于 seek 目标的旧包，避免误删 seek 后新包导致无声（BUG 3）
        nanoseconds seekBasePts = controller_->getBasePts();
        const size_t maxSkipPerLoop = 20;
        size_t skippedCount = 0;
        while (skippedCount < maxSkipPerLoop) {
            const AVPacket *packet = aPackets_->peekPacket("AudioQueue");
            if (!packet) {
                break;
            }
            // 若包无有效 PTS 或 PTS >= seek 目标，视为新包，不再跳过
            bool shouldSkip = false;
            if (packet->pts != AV_NOPTS_VALUE && stream_ && stream_->time_base.den > 0) {
                int64_t ptsNs = static_cast<int64_t>(1e9 * av_q2d(stream_->time_base) * static_cast<double>(packet->pts));
                if (ptsNs < seekBasePts.count()) {
                    shouldSkip = true;
                }
            } else {
                // 无 PTS 的包在 seek 期间保守跳过（可能是 seek 前的残留）
                shouldSkip = (skippedCount < 5);
            }
            if (!shouldSkip) {
                break;
            }
            aPackets_->popPacket("AudioQueue");
            skippedCount++;
            size_t currentQueueSize = aPackets_->Size();
            if (currentQueueSize == 0) {
                if (logger) {
                    logger->debug("AudioThread::handleSeekingState: Queue empty after skip, stopping");
                }
                break;
            }
            if (logger && skippedCount <= 2) {
                logger->debug("AudioThread::handleSeekingState: Skipped old packet (pts < seekBasePts), remaining queue: {}", currentQueueSize);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        return true;
    }

    if (wasSeeking_ && !isSeeking) {
        wasSeeking_ = false;
        logger->info("AudioThread::handleSeekingState: Seeking finished, currentPts={}ms", std::chrono::duration_cast<milliseconds>(currentPts_).count());

        // 等待音频队列有足够数据（确保音频缓冲区已填充）
        if (aPackets_ && aPackets_->Size() < 10) {
            int waitCount = 0;
            const int maxWait = 30; // 最多等待30 * 10ms = 300ms
            while (aPackets_->Size() < 10 && waitCount < maxWait) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                waitCount++;
            }
            logger->info("AudioThread::handleSeekingState: Waited {}ms for queue to fill (size: {})", waitCount * 10, aPackets_->Size());
        }
        // Seek 结束后显式尝试启动播放，避免 OpenAL 有缓冲但未 alSourcePlay 导致无声（BUG 3）
        if (audio_) {
            audio_->play();
        }
    }

    return false;
}

bool AudioThread::decodeAndWriteAudio(int buffer_len)
{
    uint8_t *samples = static_cast<uint8_t *>(av_malloc(buffer_len));
    if (!samples) {
        if (logger) {
            logger->error("AudioThread::decodeAndWriteAudio: Failed to allocate samples buffer (size: {})", buffer_len);
        }
        return false;
    }

    bool hasData = false;
    try {
        hasData = readAudio(samples, buffer_len);
    } catch (const std::exception &e) {
        logger->error("AudioThread::decodeAndWriteAudio: Exception in readAudio(): {}", e.what());
        av_free(samples);
        return false;
    } catch (...) {
        logger->error("AudioThread::decodeAndWriteAudio: Unknown exception in readAudio()");
        av_free(samples);
        return false;
    }

    if (!hasData) {
        av_free(samples);
        return false;
    }

    // 关键修复：使用videoSeekPos代替audioSeekPos进行seek位置验证
    // 原因：AudioThread的seek位置判断与VideoThread不同步，导致音频被阻止播放
    double videoSeekPos = controller_->getVideoSeekPos();
    if (videoSeekPos > 0.0) {
        if (currentPts_ != nanoseconds::min()) {
            double audioPtsSeconds = currentPts_.count() / 1e9;
            if (audioPtsSeconds < videoSeekPos) {
                av_free(samples);
                return false;
            } else {
                // 同时清除videoSeekPos和audioSeekPos，确保两个线程都能正常播放
                controller_->setVideoSeekPos(-1.0);
                controller_->setAudioSeekPos(-1.0);
            }
        } else {
            av_free(samples);
            return false;
        }
    }

    int retryCount = 0;
    const int maxRetries = 10;
    bool writeSuccess = false;
    bool hasException = false;

    try {
        while (!writeSuccess && retryCount < maxRetries) {
            writeSuccess = audio_->writeAudio(samples, buffer_len);
            if (!writeSuccess) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                retryCount++;
            }
        }
    } catch (const std::exception &e) {
        logger->error("AudioThread::decodeAndWriteAudio: Exception in writeAudio(): {}", e.what());
        hasException = true;
        av_free(samples);
        return false;
    } catch (...) {
        logger->error("AudioThread::decodeAndWriteAudio: Unknown exception in writeAudio()");
        hasException = true;
        av_free(samples);
        return false;
    }

    if (!writeSuccess && !hasException) {
        ErrorInfo error(ErrorType::ResourceError, "writeAudio failed after " + std::to_string(maxRetries) + " retries", -1);
        RecoveryResult recovery = errorRecoveryManager_.handleError(error);

        if (recovery.action == RecoveryAction::StopPlayback) {
            logger->error("AudioThread::decodeAndWriteAudio: writeAudio failed repeatedly, stopping playback");
            av_free(samples);
            return false;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    if (writeSuccess) {
        errorRecoveryManager_.resetErrorCount(ErrorType::ResourceError);

        // 更新音频时钟：基于写入的数据量调整PTS
        // buffer_len 是字节数，需要转换为样本数，然后转换为时间
        if (codecctx_ && codecctx_->sample_rate > 0 && frameSize_ > 0) {
            unsigned int samples_written = buffer_len / frameSize_;
            // 直接计算nanoseconds：samples / sample_rate * 1e9
            nanoseconds write_duration{static_cast<long long>(static_cast<double>(samples_written) * 1000000000.0 / codecctx_->sample_rate)};
            // 更新 Audio 的时钟，增加写入的数据时长
            nanoseconds updated_pts = currentPts_ + write_duration;
            audio_->updateClock(updated_pts);
            
            // 更新 AVClock（如果使用音频时钟）
            if (controller_) {
                AVClock *masterClock = controller_->getAVClock();
                if (masterClock && masterClock->clockType() == AVClock::AudioClock) {
                    double ptsSeconds = static_cast<double>(updated_pts.count()) / 1000000000.0;
                    masterClock->updateValue(ptsSeconds);
                }
            }
        }
    }

    av_free(samples);
    return writeSuccess;
}

bool AudioThread::checkPlaybackComplete()
{
    if (aPackets_ && aPackets_->IsEmpty() && aPackets_->IsFinished()) {
        // 队列已空且已结束，尝试刷新解码器获取剩余帧
        if (codecctx_) {
            int buffer_len = (codecctx_->sample_rate * AudioBufferTime.count() * frameSize_) / 1000;
            uint8_t *samples = static_cast<uint8_t *>(av_malloc(buffer_len));
            if (samples) {
                // 尝试读取并写入剩余的音频数据
                int maxFlushAttempts = 10;
                for (int i = 0; i < maxFlushAttempts; ++i) {
                    bool hasRemainingData = readAudio(samples, buffer_len);
                    if (hasRemainingData) {
                        audio_->writeAudio(samples, buffer_len);
                    } else {
                        break;
                    }
                }
                av_free(samples);
            }
            avcodec_flush_buffers(codecctx_.get());
        }

        // 等待 OpenAL 缓冲区播放完毕
        // 这确保音频能完整播放完，而不是在缓冲区还有数据时就结束
        if (audio_) {
            // 等待最多 AudioBufferTotalTime（200ms）让缓冲区播放完
            int waitCount = 0;
            const int maxWait = 20; // 20 * 10ms = 200ms
            while (waitCount < maxWait && !br_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                waitCount++;
            }
        }

        return true;
    }
    return false;
}

void AudioThread::setCodecContext(AVCodecContextPtr codecctx, AVStream *stream)
{
    codecctx_ = std::move(codecctx);
    stream_ = stream;
    logger->info("AudioThread::setCodecContext: codec context set");
}

void AudioThread::setPaused(bool paused)
{
    // 设置暂停状态
    if (audio_) {
        audio_->pause();
    }
    logger->info("AudioThread::setPaused: {} (audio pause called)", paused);
}

void AudioThread::setStatistics(videoDecoder::Statistics* stats)
{
    statistics_ = stats;
    SPDLOG_LOGGER_INFO(logger,"AudioThread::setStatistics: statistics object set");
}

void AudioThread::run()
{
    try {
        logger->info("AudioThread::run started");
        br_ = false; // 使用 AVThread 的 br_ 标志

        // 注意：不在run()中解锁mutex，因为mutex是在主线程（构造函数）中锁定的
        // mutex会在stop()方法中（主线程）解锁，或者在析构函数中（主线程）解锁
        // 这样可以避免跨线程解锁导致的Qt断言失败
        wasSeeking_ = false;
        audioFinished_.store(false);
        currentPts_ = nanoseconds{0};
        deviceStartTime_ = nanoseconds::min();

        if (!audio_) {
            logger->error("AudioThread::run: Audio is null");
            emit errorOccurred(-1);
            return;
        }

        if (!aPackets_) {
            logger->error("AudioThread::run: Audio PacketQueue is null");
            emit errorOccurred(-1);
            return;
        }

        // 检查 codecctx_ 是否已设置
        if (!codecctx_) {
            logger->error("AudioThread::run: codecctx_ is null, codec context not set. Call setCodecContext() first.");
            emit errorOccurred(-1);
            return;
        }

        // 初始化 FFmpeg 解码相关资源（包括重采样器，会设置 dstSampleFmt_ 和 dstChanLayout_）
        if (!initializeDecoder()) {
            logger->error("AudioThread::run: initializeDecoder() failed");
            emit errorOccurred(-1);
            return;
        }

        // 参考 VideoThread 的实现，在 AudioThread 中实现解码循环
        int frames = 0; // 帧计数器（用于日志输出）
        while (!br_) {  // 使用 AVThread 的 br_ 标志
            try {
                // 1. 处理 flushAudio 标志
                handleFlushAudio();

                // 2. 处理暂停状态
                handlePausedState();
                if (br_) {
                    break; // 如果收到停止信号，立即退出
                }

                // 3. 处理 seeking 状态
                if (handleSeekingState()) {
                    continue; // seeking 期间继续循环
                }

                // 4. 检查停止标志
                if (controller_ && (controller_->isStopping() || controller_->isStopped())) {
                    logger->info("AudioThread::run: Stopping flag detected, exiting");
                    break;
                }

                // 5. 计算需要的缓冲区大小
                if (codecctx_ && frameSize_ > 0 && codecctx_->sample_rate > 0) {
                    // 计算缓冲区大小：采样率 * 时间(秒) * 每样本字节数
                    // 正确计算：(采样率 * 时间毫秒 * 每样本字节数) / 1000
                    int buffer_len = (codecctx_->sample_rate * AudioBufferTime.count() * frameSize_) / 1000;

                    if (buffer_len > 0 && buffer_len < 1024 * 1024) {
                        // 使用辅助方法解码并写入音频
                        if (decodeAndWriteAudio(buffer_len)) {
                            frames++;
                            errorRecoveryManager_.resetErrorCount(ErrorType::ResourceError);
                        } else {
                            // 写入失败，短暂等待
                            std::this_thread::sleep_for(std::chrono::milliseconds(5));
                        }
                    } else {
                        // 内存分配失败或写入失败
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                } else {
                    // buffer_len 无效，记录警告
                    if (logger) {
                        static int invalidParamCount = 0;
                        if (++invalidParamCount % 100 == 0) {
                            logger->warn(
                                "AudioThread::run: Invalid parameters (codecctx_: {}, frameSize_: {}, sample_rate: {})",
                                (void *) codecctx_.get(),
                                frameSize_,
                                codecctx_ ? codecctx_->sample_rate : 0);
                        }
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }

                // 6. 检查播放完成
                if (checkPlaybackComplete()) {
                    logger->info("AudioThread::run: Playback completed");
                    break;
                }

                // 7. 更新健康检查时间戳
                controller_->getLastAudioFrameTime() = std::chrono::steady_clock::now();
            } catch (const std::exception &e) {
                logger->error("AudioThread::run: Exception in main loop: {}", e.what());
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } catch (...) {
                logger->error("AudioThread::run: Unknown exception in main loop");
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    } catch (const std::exception &e) {
        logger->error("AudioThread::run: Fatal exception: {}", e.what());
        emit errorOccurred(-1);
    } catch (...) {
        logger->error("AudioThread::run: Fatal unknown exception");
        emit errorOccurred(-1);
    }

    cleanupDecoder();

    // 标记音频已结束
    audioFinished_.store(true, std::memory_order_release);

    logger->info("AudioThread::run finished");

    // 注意：不在run()中解锁mutex，因为mutex是在主线程中锁定的
    // mutex会在stop()方法中（主线程）解锁，或者在析构函数中（主线程）解锁
    // 这样可以避免跨线程解锁导致的Qt断言失败
}

bool AudioThread::initializeDecoder()
{
    if (!codecctx_ || !stream_) {
        logger->error("AudioThread::initializeDecoder: codecctx_ or stream_ is null");
        return false;
    }

    currentPts_ = nanoseconds{0};
    deviceStartTime_ = nanoseconds::min();
    audioFinished_.store(false, std::memory_order_release);
    samples_ = nullptr;
    samplesMax_ = 0;
    samplesPos_ = 0;
    samplesLen_ = 0;

    decodedFrame_.reset(av_frame_alloc());
    if (!decodedFrame_) {
        logger->error("AudioThread::initializeDecoder: Failed to allocate decoded frame");
        return false;
    }

    // 获取源音频的声道布局和声道数
    uint64_t srcChanLayout = 0;
    int srcChannels = codecctx_->ch_layout.nb_channels;
    if (codecctx_->ch_layout.u.mask) {
        srcChanLayout = codecctx_->ch_layout.u.mask;
    } else if (srcChannels > 0) {
        AVChannelLayout tempLayout;
        av_channel_layout_default(&tempLayout, srcChannels);
        srcChanLayout = tempLayout.u.mask;
        av_channel_layout_uninit(&tempLayout);
    }

    // 确定目标格式
    if (codecctx_->sample_fmt == AV_SAMPLE_FMT_U8 || codecctx_->sample_fmt == AV_SAMPLE_FMT_U8P) {
        dstSampleFmt_ = AV_SAMPLE_FMT_U8;
        if (srcChannels == 1) {
            dstChanLayout_ = AV_CH_LAYOUT_MONO;
            frameSize_ = 1; // U8 单声道 = 1字节/样本
        } else {
            dstChanLayout_ = AV_CH_LAYOUT_STEREO;
            frameSize_ = 2; // U8 立体声 = 1字节/样本 * 2声道 = 2字节/样本
        }
    } else {
        dstSampleFmt_ = AV_SAMPLE_FMT_S16;
        if (srcChannels == 1) {
            dstChanLayout_ = AV_CH_LAYOUT_MONO;
            frameSize_ = 2; // S16 单声道 = 2字节/样本
        } else {
            dstChanLayout_ = AV_CH_LAYOUT_STEREO;
            frameSize_ = 4; // S16 立体声 = 2字节/样本 * 2声道 = 4字节/样本
        }
    }

    logger->info(
        "AudioThread::initializeDecoder: Audio format determined (sample_fmt: {}, channels: {}, dstSampleFmt: {}, dstChanLayout: {}, frameSize: {})",
        int(codecctx_->sample_fmt),
        srcChannels,
        int(dstSampleFmt_),
        dstChanLayout_,
        frameSize_);

    // 初始化重采样器
    if (dstChanLayout_) {
        AVChannelLayout dstChLayout, srcChLayout;
        av_channel_layout_from_mask(&dstChLayout, dstChanLayout_);

        if (srcChanLayout) {
            av_channel_layout_from_mask(&srcChLayout, srcChanLayout);
        } else {
            av_channel_layout_default(&srcChLayout, srcChannels);
        }

        SwrContext *swrctx_temp = nullptr;
        int ret = swr_alloc_set_opts2(
            &swrctx_temp, &dstChLayout, dstSampleFmt_, codecctx_->sample_rate, &srcChLayout, codecctx_->sample_fmt, codecctx_->sample_rate, 0, nullptr);

        av_channel_layout_uninit(&dstChLayout);
        av_channel_layout_uninit(&srcChLayout);

        if (ret < 0 || !swrctx_temp || swr_init(swrctx_temp) != 0) {
            if (swrctx_temp) {
                swr_free(&swrctx_temp);
            }
            logger->error("AudioThread::initializeDecoder: Failed to initialize swr");
            return false;
        }

        swrctx_.reset(swrctx_temp);
    } else {
        logger->error("AudioThread::initializeDecoder: dstChanLayout_ is 0");
        return false;
    }

    // 打开音频输出设备（关键修复！）
    ALenum alFormat = AL_NONE;
    if (dstSampleFmt_ == AV_SAMPLE_FMT_U8) {
        if (codecctx_->ch_layout.nb_channels == 1) {
            alFormat = AL_FORMAT_MONO8;
        } else {
            alFormat = AL_FORMAT_STEREO8;
        }
    } else if (dstSampleFmt_ == AV_SAMPLE_FMT_S16) {
        if (codecctx_->ch_layout.nb_channels == 1) {
            alFormat = AL_FORMAT_MONO16;
        } else {
            alFormat = AL_FORMAT_STEREO16;
        }
    }

    logger
        ->info("AudioThread::initializeDecoder: Opening audio output (format: {}, frameSize: {}, sampleRate: {})", alFormat, frameSize_, codecctx_->sample_rate);

    if (!audio_->open(alFormat, frameSize_, codecctx_->sample_rate)) {
        logger->error("AudioThread::initializeDecoder: Failed to open audio output");
        return false;
    }

    logger->info("AudioThread::initializeDecoder: Audio output opened successfully");

    // 注意：不在这里调用 audio_->play()
    // OpenAL 要求队列中至少有一个缓冲区才能启动播放
    // writeAudio() 会在首次成功写入数据后自动调用 alSourcePlay()

    // Prefill the codec buffer: 预填充解码器缓冲区，但不要过度填充
    // 注意：只需要填充到解码器可以开始解码即可，不需要填满
    if (aPackets_) {
        int prefilled = 0;
        const int maxPrefill = 3; // 最多预填充3个数据包，避免过度填充
        while (prefilled < maxPrefill) {
            const int ret = aPackets_->sendTo(codecctx_.get(), "AudioQueue");
            if (ret == AVERROR(EAGAIN)) {
                // 解码器内部缓冲区已满，停止预填充
                if (logger) {
                    logger->debug("AudioThread::initializeDecoder: Decoder buffer full after {} packets", prefilled);
                }
                break;
            } else if (ret == AVErrorEOF) {
                // 队列已结束
                break;
            } else if (ret != 0) {
                char errbuf[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
                logger->error("AudioThread::initializeDecoder: Failed to send packet, ret: {} ({})", ret, errbuf);
                break;
            }
            prefilled++;
        }
        if (logger && prefilled > 0) {
            logger->debug("AudioThread::initializeDecoder: Prefilled {} packets to decoder", prefilled);
        }
    }

    logger->info("AudioThread::initializeDecoder: Decoder initialized successfully");
    return true;
}

void AudioThread::cleanupDecoder()
{
    if (samples_) {
        av_freep(&samples_);
    }
    samplesMax_ = 0;
    samplesPos_ = 0;
    samplesLen_ = 0;
    decodedFrame_.reset();
    swrctx_.reset();
    codecctx_.reset();
    stream_ = nullptr;
}

int AudioThread::decodeFrame()
{
    if (!codecctx_ || !decodedFrame_ || !aPackets_) {
        if (logger) {
            logger->debug(
                "AudioThread::decodeFrame: Missing required resources (codecctx_: {}, decodedFrame_: {}, aPackets_: {})",
                (void *) codecctx_.get(),
                (void *) decodedFrame_.get(),
                (void *) aPackets_);
        }
        return 0;
    }

    while (!controller_->isStopping() && !controller_->isStopped()) {
        // 先尝试接收解码后的帧
        int ret = avcodec_receive_frame(codecctx_.get(), decodedFrame_.get());

        if (ret == AVERROR(EAGAIN)) {
            // 解码器需要更多输入，尝试发送数据包
            int pret = aPackets_->sendTo(codecctx_.get(), "AudioQueue");

            if (pret == AVErrorEOF) {
                // 队列已结束，尝试flush解码器获取剩余帧
                if (aPackets_ && aPackets_->Size() > 0) {
                    logger->info("AudioThread::decodeFrame: EOF but {} packets remaining, flushing decoder", aPackets_->Size());
                    avcodec_flush_buffers(codecctx_.get());
                    continue;
                }
                // 真正的 EOF，标记音频结束
                audioFinished_.store(true, std::memory_order_release);
                break;
            } else if (pret == AVERROR(EAGAIN)) {
                // 解码器内部缓冲区已满，但receive_frame也返回EAGAIN，这是异常状态
                // 可能原因：解码器状态异常，需要flush
                if (logger) {
                    logger->warn("AudioThread::decodeFrame: Both receive_frame and send_packet return EAGAIN, flushing decoder");
                }
                avcodec_flush_buffers(codecctx_.get());
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            } else if (pret != 0 && pret != AVERROR(EAGAIN)) {
                // 发送数据包时出错
                char errbuf[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(pret, errbuf, AV_ERROR_MAX_STRING_SIZE);
                logger->error("AudioThread::decodeFrame: Failed to send packet: {} ({})", pret, errbuf);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            // 成功发送数据包，继续循环尝试接收帧
            continue;
        } else if (ret != 0) {
            // 接收帧时出错
            if (ret == AVErrorEOF) {
                // EOF，检查是否还有数据包
                if (aPackets_ && aPackets_->Size() > 0) {
                    logger->info("AudioThread::decodeFrame: EOF but {} packets remaining, flushing decoder", aPackets_->Size());
                    avcodec_flush_buffers(codecctx_.get());
                    continue;
                }
                // 真正的 EOF，标记音频结束
                audioFinished_.store(true, std::memory_order_release);
                break;
            } else if (ret == AVERROR_INVALIDDATA) {
                logger->warn("AudioThread::decodeFrame: Invalid audio data detected, skipping packet");
                // 尝试发送下一个数据包
                int pret = aPackets_->sendTo(codecctx_.get(), "AudioQueue");
                if (pret == AVErrorEOF) {
                    audioFinished_.store(true, std::memory_order_release);
                    break;
                }
                continue;
            } else {
                char errbuf[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
                logger->error("AudioThread::decodeFrame: Decode error: {} ({})", ret, errbuf);
                avcodec_flush_buffers(codecctx_.get());
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
        } else {
            // 成功接收到帧，更新音频线程健康检查时间戳
            controller_->getLastAudioFrameTime() = std::chrono::steady_clock::now();
        }

        if (decodedFrame_->nb_samples <= 0) {
            continue;
        }

        // 更新 PTS（基于解码帧的时间戳）
        // 这是基准 PTS，实际播放位置会在 readAudio 中基于已播放的样本数调整
        if (decodedFrame_->best_effort_timestamp != AVNoPtsValue && stream_) {
            nanoseconds absolutePts = duration_cast<nanoseconds>(seconds_d64{av_q2d(stream_->time_base) * static_cast<double>(decodedFrame_->best_effort_timestamp)});
            
            // 关键修复：在seeking后，使用相对PTS而不是绝对PTS
            // 如果正在seeking或刚完成seeking，需要确保PTS与basePts一致
            if (wasSeeking_ || controller_->isSeeking()) {
                nanoseconds basePts = controller_->getBasePts();
                // 如果绝对PTS小于basePts，说明这是seeking前的帧，应该跳过
                // 否则，使用绝对PTS（因为basePts已经设置为seeking位置）
                if (absolutePts >= basePts) {
                    currentPts_ = absolutePts;
                } else {
                    // 跳过这个帧，继续解码下一个
                    av_frame_unref(decodedFrame_.get());
                    continue;
                }
            } else {
                currentPts_ = absolutePts;
            }

            // 记录 seeking 后第一个解码的音频帧的 PTS（用于验证）
            if (!firstFrameAfterSeekLogged_ && wasSeeking_) {
                nanoseconds basePts = controller_->getBasePts();
                logger->info(
                    "AudioThread::decodeFrame: First audio frame after seek, absolutePTS={}ms, basePts={}ms, currentPts={}ms, diff={}ms",
                    std::chrono::duration_cast<milliseconds>(absolutePts).count(),
                    std::chrono::duration_cast<milliseconds>(basePts).count(),
                    std::chrono::duration_cast<milliseconds>(currentPts_).count(),
                    std::chrono::duration_cast<milliseconds>(currentPts_ - basePts).count());
                firstFrameAfterSeekLogged_ = true;
            }

            // 更新 Audio 的时钟基准（使用currentPts_，在seeking后应该是正确的）
            audio_->updateClock(currentPts_);
            
            // 更新 AVClock（如果使用音频时钟）
            if (controller_) {
                AVClock *masterClock = controller_->getAVClock();
                if (masterClock && masterClock->clockType() == AVClock::AudioClock) {
                    double ptsSeconds = static_cast<double>(currentPts_.count()) / 1000000000.0;
                    masterClock->updateValue(ptsSeconds);
                }
            }
        }

        // 分配重采样缓冲区
        if (decodedFrame_->nb_samples > samplesMax_) {
            av_freep(&samples_);
            int ret = av_samples_alloc(&samples_, nullptr, codecctx_->ch_layout.nb_channels, decodedFrame_->nb_samples, dstSampleFmt_, 0);
            if (ret < 0) {
                char errbuf[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
                logger->error("AudioThread::decodeFrame: Failed to allocate samples buffer: {} ({})", ret, errbuf);
                av_frame_unref(decodedFrame_.get());
                continue; // 跳过这一帧，继续处理下一帧
            }
            samplesMax_ = decodedFrame_->nb_samples;
        }

        // 验证 samples_ 是否有效
        if (!samples_) {
            logger->error("AudioThread::decodeFrame: samples_ is null after allocation");
            av_frame_unref(decodedFrame_.get());
            continue;
        }

        // 重采样
        int nsamples = swr_convert(swrctx_.get(), &samples_, decodedFrame_->nb_samples, (const uint8_t **) decodedFrame_->data, decodedFrame_->nb_samples);
        av_frame_unref(decodedFrame_.get());

        // 检查重采样结果
        if (nsamples < 0) {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(nsamples, errbuf, AV_ERROR_MAX_STRING_SIZE);
            logger->error("AudioThread::decodeFrame: swr_convert failed: {} ({})", nsamples, errbuf);
            continue; // 跳过这一帧，继续处理下一帧
        }

        if (nsamples == 0) {
            // 重采样返回0可能是正常的（延迟），继续处理
            if (logger) {
                static int zeroSampleCount = 0;
                if (++zeroSampleCount % 100 == 0) {
                    logger->debug("AudioThread::decodeFrame: swr_convert returned 0 samples (count: {})", zeroSampleCount);
                }
            }
            continue;
        }

        if (logger && nsamples > 0) {
            static int decodeCount = 0;
            if (++decodeCount % 100 == 0) {
                logger->debug("AudioThread::decodeFrame: Decoded {} samples", nsamples);
            }
        }

        // 更新音频解码统计
        if (statistics_) {
            statistics_->incrementDecodedFrames(false);
        }

        return nsamples;
    }

    return 0;
}

bool AudioThread::readAudio(uint8_t *samples, unsigned int length)
{
    if (!samples || length == 0 || frameSize_ == 0) {
        if (logger) {
            logger->debug("AudioThread::readAudio: Invalid parameters (samples: {}, length: {}, frameSize_: {})", (void *) samples, length, frameSize_);
        }
        return false;
    }

    // length 是字节数，需要转换为样本数
    unsigned int samples_needed = length / frameSize_;
    unsigned int samples_read = 0;

    if (logger && samples_needed == 0) {
        logger->warn("AudioThread::readAudio: samples_needed is 0 (length: {}, frameSize_: {})", length, frameSize_);
        return false;
    }

    while (samples_read < samples_needed) {
        // 如果内部缓冲区已用完，解码新帧
        if (samplesPos_ >= samplesLen_) {
            int frame_len = 0;
            try {
                frame_len = decodeFrame();
            } catch (const std::exception &e) {
                logger->error("AudioThread::readAudio: Exception in decodeFrame(): {}", e.what());
                break;
            } catch (...) {
                logger->error("AudioThread::readAudio: Unknown exception in decodeFrame()");
                break;
            }

            if (frame_len <= 0) {
                // 没有更多数据，跳出循环
                break;
            }

            // 验证 frame_len 的合理性
            if (frame_len > samplesMax_ || frame_len < 0) {
                logger->error("AudioThread::readAudio: Invalid frame_len: {} (samplesMax_: {})", frame_len, samplesMax_);
                break;
            }

            samplesLen_ = frame_len;
            samplesPos_ = 0;
        }

        // 验证 samples_ 是否有效
        if (!samples_ || samplesPos_ >= samplesLen_) {
            logger->error(
                "AudioThread::readAudio: Invalid samples_ state (samples_: {}, samplesPos_: {}, samplesLen_: {})", (void *) samples_, samplesPos_, samplesLen_);
            break;
        }

        // 计算可以复制的样本数
        const unsigned int available = static_cast<unsigned int>(samplesLen_ - samplesPos_);
        const unsigned int needed = samples_needed - samples_read;
        const unsigned int to_copy = (needed < available) ? needed : available;

        // 验证复制范围（将 samplesPos_ 和 samplesLen_ 转换为 unsigned 进行比较，避免有符号/无符号不匹配警告）
        if (to_copy == 0 || static_cast<unsigned int>(samplesPos_) + to_copy > static_cast<unsigned int>(samplesLen_)) {
            logger->error("AudioThread::readAudio: Invalid copy range (to_copy: {}, samplesPos_: {}, samplesLen_: {})", to_copy, samplesPos_, samplesLen_);
            break;
        }

        // 复制数据
        try {
            std::copy_n(samples_ + samplesPos_ * frameSize_, to_copy * frameSize_, samples);
        } catch (const std::exception &e) {
            logger->error("AudioThread::readAudio: Exception in std::copy_n(): {}", e.what());
            break;
        } catch (...) {
            logger->error("AudioThread::readAudio: Unknown exception in std::copy_n()");
            break;
        }

        samplesPos_ += to_copy;
        samples += to_copy * frameSize_;
        samples_read += to_copy;
    }

    // 如果没有读取到任何数据，返回 false
    if (samples_read == 0) {
        if (logger) {
            static int zeroReadCount = 0;
            if (++zeroReadCount % 100 == 0) {
                logger->debug("AudioThread::readAudio: No data read (samples_read: 0), count: {}", zeroReadCount);
            }
        }
        return false;
    }

    // 如果读取的数据不足，用静音填充
    if (samples_read < samples_needed) {
        const unsigned int rem = samples_needed - samples_read;
        std::fill_n(samples, rem * frameSize_, (dstSampleFmt_ == AV_SAMPLE_FMT_U8 ? 0x80 : 0x00));
        samples_read += rem;
    }

    // 更新时钟（基于已读取的样本数）
    // 注意：currentPts_ 已经在 decodeFrame 中设置为当前帧的 PTS
    // 这里需要减去还未播放的样本数（samplesLen_ - samplesPos_）
    if (codecctx_ && codecctx_->sample_rate > 0 && samples_read > 0) {
        // 计算已读取样本对应的时间
        nanoseconds samples_duration = nanoseconds{seconds{samples_read}} / codecctx_->sample_rate;
        // 更新 Audio 的时钟基准
        // currentPts_ 已经在 decodeFrame 中更新，这里只需要同步到 Audio
        audio_->updateClock(currentPts_);
        
        // 更新 AVClock（如果使用音频时钟）
        if (controller_) {
            AVClock *masterClock = controller_->getAVClock();
            if (masterClock && masterClock->clockType() == AVClock::AudioClock) {
                double ptsSeconds = static_cast<double>(currentPts_.count()) / 1000000000.0;
                masterClock->updateValue(ptsSeconds);
            }
        }
        
        // 更新音频播放统计
        if (statistics_) {
            statistics_->incrementPlayedFrames();
            statistics_->setAudioClock(std::chrono::duration_cast<std::chrono::milliseconds>(currentPts_).count() / 1000.0);
        }
    }

    return true;
}

nanoseconds AudioThread::getClock() const
{
    // 音频时钟基于当前 PTS（由 decodeFrame 更新）
    // 通过 Audio 获取实际的播放位置
    nanoseconds pts = currentPts_;

    // 通过 Audio 获取实际播放位置
    if (audio_) {
        pts = audio_->getClockNoLock();
    }

    return std::max(pts, nanoseconds::zero());
}
