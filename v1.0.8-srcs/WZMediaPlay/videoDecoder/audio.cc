#include "audio.h"
#include "../movie.h"

#include "spdlog/spdlog.h"
#include <algorithm>
#include <iostream>
#include <vector>
#include <QDebug>

// 定义AV_ERROR_MAX_STRING_SIZE（如果未定义）
#ifndef AV_ERROR_MAX_STRING_SIZE
#define AV_ERROR_MAX_STRING_SIZE 64
#endif

using std::chrono::duration_cast;

Audio::Audio(Movie &movie)
    : movie_(movie)
{
    static std::once_flag once;
    std::call_once(once, []() { initAL(); });
    m_error = new char[ERROR_LEN];
}

Audio::~Audio()
{
    if (source_) {
        alDeleteSources(1, &source_);
    }
    if (buffers_[0]) {
        alDeleteBuffers(buffers_.size(), buffers_.data());
    }
    if (samples_) {
        av_freep(&samples_);
    }
}

int Audio::initAL()
{
    ALCdevice *device = alcOpenDevice(nullptr);
    if (!device) {
        logger->error("Fail to alcOpenDevice");
        return -1;
    }

    ALCcontext *ctx = alcCreateContext(device, nullptr);
    if (!ctx || alcMakeContextCurrent(ctx) == ALC_FALSE) {
        if (ctx) {
            alcDestroyContext(ctx);
        }
        alcCloseDevice(device);
        logger->critical("Fail to set alc context");
        return -1;
    }

    return 0;
}

int Audio::start()
{
    isPause.store(false);
    //isQuit.store(false);
    currentPts_ = nanoseconds{0};

    std::unique_lock<std::mutex> srclck(srcMutex_, std::defer_lock);
    milliseconds sleep_time{AudioBufferTime / 3};

    ALenum FormatStereo8{AL_FORMAT_STEREO8};
    ALenum FormatStereo16{AL_FORMAT_STEREO16};

    if (codecctx_->sample_fmt == AV_SAMPLE_FMT_U8 || codecctx_->sample_fmt == AV_SAMPLE_FMT_U8P) {
        dstSampleFmt_ = AV_SAMPLE_FMT_U8;
        frameSize_ = 1;
        if (codecctx_->channel_layout == AV_CH_LAYOUT_MONO) {
            dstChanLayout_ = codecctx_->channel_layout;
            frameSize_ *= 1;
            format_ = AL_FORMAT_MONO8;
        }
        if (!format_ || format_ == -1) {
            dstChanLayout_ = AV_CH_LAYOUT_STEREO;
            frameSize_ *= 2;
            format_ = FormatStereo8;
        }
    }

    if (!format_ || format_ == -1) {
        dstSampleFmt_ = AV_SAMPLE_FMT_S16;
        frameSize_ = 2;
        if (codecctx_->channel_layout == AV_CH_LAYOUT_MONO) {
            dstChanLayout_ = codecctx_->channel_layout;
            frameSize_ *= 1;
            format_ = AL_FORMAT_MONO16;
        }
        if (!format_ || format_ == -1) {
            dstChanLayout_ = AV_CH_LAYOUT_STEREO;
            frameSize_ *= 2;
            format_ = FormatStereo16;
        }
    }

    void *samples{nullptr};
    ALsizei buffer_len{0};

    samples_ = nullptr;
    samplesMax_ = 0;
    samplesPos_ = 0;
    samplesLen_ = 0;

    decodedFrame_.reset(av_frame_alloc());
    if (!decodedFrame_) {
        logger->error("Fail to av_frame_alloc, decodeFrame failed");
        goto finish;
    }

    if (dstChanLayout_) {
        swrctx_.reset(swr_alloc_set_opts(
            nullptr,
            dstChanLayout_,
            dstSampleFmt_,
            codecctx_->sample_rate,
            codecctx_->channel_layout ? codecctx_->channel_layout : av_get_default_channel_layout(codecctx_->channels),
            codecctx_->sample_fmt,
            codecctx_->sample_rate,
            0,
            nullptr));
        if (!swrctx_ || swr_init(swrctx_.get()) != 0) {
            logger->error("Fail to initialize swr");
            goto finish;
        }
    } else {
        assert(0);
        goto finish;
    }

    alGenBuffers(buffers_.size(), buffers_.data());
    alGenSources(1, &source_);

    if (alGetError() != AL_NO_ERROR) {
        goto finish;
    }

    buffer_len = duration_cast<seconds>(codecctx_->sample_rate * AudioBufferTime).count() * frameSize_;
    samples = av_malloc(buffer_len);

    // Prefill the codec buffer.
    do {
        const int ret = packets_.sendTo(codecctx_.get());
        if (ret == AVERROR(EAGAIN) || ret == AVErrorEOF) {
            break;
        }
        if (ret != 0) {
            logger->error("Fail to send packet audio, ret:{} ", ret);
        }
    } while (1);

    srclck.lock();

    while (!movie_.quit_.load(std::memory_order_relaxed)) {
        ALenum state;
        ALint processed, queued;

        if (isPause) {
            //qDebug() << "audio.decodeFrame, isPause, sleep 5ms";
            std::this_thread::sleep_for(milliseconds(5));
            continue;
        }

        // First remove any processed buffers.
        alGetSourcei(source_, AL_BUFFERS_PROCESSED, &processed);
        if (processed > 0) {
            std::vector<ALuint> bufids(processed);
            alSourceUnqueueBuffers(source_, processed, bufids.data());
        }

        // Refill the buffer queue.
        alGetSourcei(source_, AL_BUFFERS_QUEUED, &queued);
        while (queued < buffers_.size()) {
            if (!readAudio(static_cast<uint8_t *>(samples), buffer_len)) {
                logger->debug("audio.start, readAudio failed, break");
                break;
            }
            ALuint bufid = buffers_[bufferIdx_];
            bufferIdx_ = (bufferIdx_ + 1) % buffers_.size();
            alBufferData(bufid, format_, samples, buffer_len, codecctx_->sample_rate);
            alSourceQueueBuffers(source_, 1, &bufid);
            ++queued;
        }

        alGetSourcei(source_, AL_SOURCE_STATE, &state);
        if (state == AL_PAUSED) {
            alSourceRewind(source_);
            alSourcei(source_, AL_BUFFER, 0);
            continue;
        }

        if (state != AL_PLAYING && state != AL_PAUSED) {
            if (!play()) {
                break;
            }
        }

        if (alGetError() != AL_NO_ERROR) {
            logger->warn("alGetError != AL_NO_ERROR");
            return -1;
        }

        srcCond_.wait_for(srclck, sleep_time);
    }

    alSourceRewind(source_);
    alSourcei(source_, AL_BUFFER, 0);

    srclck.unlock();

finish:
    av_freep(&samples);

    // 标记音频已结束
    audioFinished_.store(true, std::memory_order_release);
    logger->info("audio finished.");
    return 0;
}

void Audio::showError(int err)
{
#if PRINT_LOG
    memset(m_error, 0, ERROR_LEN); // 将数组置零
    av_strerror(err, m_error, ERROR_LEN);
    logger->error("DecodeAudio Error：{}", m_error);
#else
    Q_UNUSED(err)
#endif
}

int Audio::decodeFrame()
{
    while (!movie_.quit_.load(std::memory_order_relaxed)) {
        logger->debug("Audio.decodeFrame entered.");
        int ret, pret;
        while ((ret = avcodec_receive_frame(codecctx_.get(), decodedFrame_.get())) == AVERROR(EAGAIN)
               && (pret = packets_.sendTo(codecctx_.get())) != AVErrorEOF) {
            logger->debug("Audio.decodeFrame, avcodec_receive_frmae==AVERROR, pret!=AVErrorEOF, packets.Size:{}", packets_.Size());
            //showError(ret);
        }

        if (ret != 0) {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
            logger->error("avcodec_receive_frame ret!=0, ret:{}, error:{}", ret, errbuf);
            showError(ret);
            
            if (ret == AVErrorEOF || pret == AVErrorEOF) {
                logger->debug("!!!!Audio.decodeFrame, avcodec_receive_frame, ret==AVErrorEOF || prev == AVErrorEOF, but packets.size={}", packets_.Size());
                
                // 如果遇到EOF但队列中还有数据，尝试刷新解码器并继续解码
                // 这可以处理某些视频在播放过程中音频流提前结束的情况
                if (packets_.Size() > 0) {
                    logger->info("Audio decoder EOF but {} packets remaining in queue, flushing decoder and continuing", packets_.Size());
                    // 刷新解码器缓冲区
                    avcodec_flush_buffers(codecctx_.get());
                    // 继续尝试解码队列中的数据
                    continue;
                }
                
                // 队列为空，真正的EOF
                break;
            }
            
            // 改进错误恢复机制
            if (ret == AVERROR(EAGAIN)) {
                // 暂时没有数据，短暂等待后继续
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            } else if (ret == AVERROR_INVALIDDATA) {
                // 数据损坏，跳过当前包
                logger->warn("Invalid audio data detected, skipping packet");
                // 尝试发送下一个包
                int sendRet = packets_.sendTo(codecctx_.get());
                if (sendRet < 0 && sendRet != AVERROR(EAGAIN) && sendRet != AVErrorEOF) {
                    logger->warn("Failed to send packet after invalid data error: {}", sendRet);
                }
                continue;
            } else {
                // 严重错误，记录详细信息但不崩溃
                logger->error("Serious audio decode error: {} ({}), codec context may be in invalid state", ret, errbuf);
                logger->error("Attempting to recover by flushing decoder and continuing");
                
                // 尝试刷新解码器
                avcodec_flush_buffers(codecctx_.get());
                
                // 短暂等待后继续，避免快速循环
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                
                // 检查是否应该退出
                if (movie_.quit_.load(std::memory_order_relaxed) ) {
                    break;
                }
                
                // 继续尝试解码，而不是直接退出
                continue;
            }
        } else {
            // 更新音频线程健康检查时间戳
            movie_.lastAudioFrameTime_ = std::chrono::steady_clock::now();
        }

        if (decodedFrame_->nb_samples <= 0) {
            logger->debug("decodedFrame_->nb_samples<=0, continue");
            continue;
        }
        if (decodedFrame_->best_effort_timestamp != AVNoPtsValue) {
            currentPts_ = duration_cast<nanoseconds>(seconds_d64{av_q2d(stream_->time_base) * static_cast<double>(decodedFrame_->best_effort_timestamp)});
        }
        if (decodedFrame_->nb_samples > samplesMax_) {
            av_freep(&samples_);
            av_samples_alloc(&samples_, nullptr, codecctx_->channels, decodedFrame_->nb_samples, dstSampleFmt_, 0);
            samplesMax_ = decodedFrame_->nb_samples;
        }
        int nsamples = swr_convert(swrctx_.get(), &samples_, decodedFrame_->nb_samples, (const uint8_t **) decodedFrame_->data, decodedFrame_->nb_samples);
        av_frame_unref(decodedFrame_.get());
        logger->debug("Audio.decodeFrame leaved return nsamples: {}", nsamples);
        return nsamples;
    }

    logger->info("Audio.decodeFrame loop exists, return 0");
    return 0;
}

int Audio::readAudio(uint8_t *samples, unsigned int length)
{
    unsigned int audio_size = 0;
    length /= frameSize_;
    while (audio_size < length) {
        if (samplesPos_ == samplesLen_) {
            int frame_len = decodeFrame();
            if (frame_len <= 0) {
                break;
            }
            samplesLen_ = frame_len;
            samplesPos_ = 0;
        }

        const unsigned int len = samplesLen_ - samplesPos_;
        unsigned int rem = length - audio_size;
        if (rem > len) {
            rem = len;
        }
        std::copy_n(samples_ + samplesPos_ * frameSize_, rem * frameSize_, samples);
        samplesPos_ += rem;
        currentPts_ += nanoseconds{seconds{rem}} / codecctx_->sample_rate;
        samples += rem * frameSize_;
        audio_size += rem;
    }

    if (audio_size <= 0) {
        return false;
    }

    if (audio_size < length) {
        const unsigned int rem = length - audio_size;
        std::fill_n(samples, rem * frameSize_, (dstSampleFmt_ == AV_SAMPLE_FMT_U8 ? 0x80 : 0x00));
        currentPts_ += nanoseconds{seconds{rem}} / codecctx_->sample_rate;
        audio_size += rem;
    }

    return true;
}

bool Audio::play()
{
    ALint queued{};
    alGetSourcei(source_, AL_BUFFERS_QUEUED, &queued);
    if (queued == 0) {
        return false;
    }

    alSourcePlay(source_);
    return true;
}

bool Audio::pause(bool isPause)
{
    this->isPause = isPause;

    if (isPause) {
        alSourcePause(source_);
    } else {
        alSourcePlay(source_);
    }
    return true;
}

bool Audio::stop()
{
    logger->info("Audio.stop entered.");
    std::lock_guard<std::mutex> guard(srcMutex_);
    clear();

    alSourceStop(source_);
    if (codecctx_) {
        avcodec_close(codecctx_.get());
        codecctx_.release();
    }
    currentPts_ = nanoseconds{0};
    bufferIdx_ = 0;
    source_ = 0;
    swrctx_.release();
    decodedFrame_.release();
    deviceStartTime_ = nanoseconds::min();
    packets_.setFinished();
    //packets_.Clear();
    stream_ = nullptr;

    logger->debug(" Audio.stop leaved.");

    return true;
}

void Audio::clear()
{
    logger->info("entered Audio::clear.");
    //std::lock_guard<std::mutex> guard{srcMutex_};
    //srcMutex_.lock();
    packets_.Reset();
    // 重置音频结束标志
    audioFinished_.store(false, std::memory_order_release);
    if (codecctx_) {
        avcodec_flush_buffers(codecctx_.get());
    }
    //TODO: audio buffer?
    //srcMutex_.unlock();
    logger->info("Leaving Audio::clear...");
}

nanoseconds Audio::getClock()
{
    //qDebug() << "srcMutex_before";
    ////std::lock_guard<std::mutex> lck(srcMutex_); //TODO: original, comment to see when finished play is ok?
    //qDebug() << " srcMutex entered.";
    return getClockNoLock();
}

nanoseconds Audio::getClockNoLock()
{
    nanoseconds pts = currentPts_;

    // 检查codecctx_是否有效，避免在音频停止后访问已释放的codecctx_导致崩溃
    if (source_ && codecctx_) {
        ALint offset;
        alGetSourcei(source_, AL_SAMPLE_OFFSET, &offset);
        ALint queued, status;
        alGetSourcei(source_, AL_BUFFERS_QUEUED, &queued);
        alGetSourcei(source_, AL_SOURCE_STATE, &status);

        if (status != AL_STOPPED) {
            pts -= AudioBufferTime * queued;
            pts += nanoseconds{seconds{offset}} / codecctx_->sample_rate;
        }
    }

    return std::max(pts, nanoseconds::zero());
}

void Audio::setVolume(float v)
{
    if (v < 0 or v > 1) {
        return;
    }
    storeVolumeWhenMute = v;
    if (isMute_) {
        alListenerf(AL_GAIN, 0.0);
    } else {
        alListenerf(AL_GAIN, v);
    }
}

float Audio::getVolume()
{
    //ALfloat v;
    //alGetListenerf(AL_GAIN, &v);
    //return v;

    return storeVolumeWhenMute;
}

void Audio::ToggleMute()
{
    isMute_ = !isMute_;
    if (isMute_) {
        storeVolumeWhenMute = getVolume();
        setVolume(0.0);
    } else {
        setVolume(storeVolumeWhenMute);
    }
}
