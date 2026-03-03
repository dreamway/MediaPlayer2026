#include "OpenALAudio.h"
#include "PlayController.h" // 需要完整的 PlayController 定义（必须在 audio.h 之前）

#include "spdlog/spdlog.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <vector>
#include <QDebug>

// 定义AV_ERROR_MAX_STRING_SIZE（如果未定义）
#ifndef AV_ERROR_MAX_STRING_SIZE
#define AV_ERROR_MAX_STRING_SIZE 64
#endif

using std::chrono::duration_cast;

extern spdlog::logger *logger;

Audio::Audio(PlayController &controller)
    : controller_(controller)
{
    static std::once_flag once;
    std::call_once(once, []() { initAL(); });
}

Audio::~Audio()
{
    close();
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

bool Audio::open(ALenum format, ALuint frameSize, int sampleRate)
{
    std::lock_guard<std::mutex> lock(srcMutex_);

    format_ = format;
    frameSize_ = frameSize;
    sampleRate_ = sampleRate;
    currentPts_ = nanoseconds{0};
    deviceStartTime_ = {};
    playbackStarted_ = false;
    br_ = false;

    logger->info("Audio::open: format={}, frameSize={}, sampleRate={}", int(format), frameSize, sampleRate);

    // 清空可用缓冲区队列
    while (!availableBuffers_.empty()) {
        availableBuffers_.pop();
    }

    // 创建 OpenAL 资源
    alGenBuffers(static_cast<ALsizei>(buffers_.size()), buffers_.data());
    alGenSources(1, &source_);

    if (alGetError() != AL_NO_ERROR) {
        logger->error("Audio::open: Failed to create OpenAL resources");
        return false;
    }

    // 将所有缓冲区加入可用队列
    for (ALuint bufid : buffers_) {
        availableBuffers_.push(bufid);
    }

    logger
        ->info("Audio::open: OpenAL resources created (format: {}, frameSize: {}, sampleRate: {}, buffers: {})", format, frameSize, sampleRate, buffers_.size());
    return true;
}

void Audio::close()
{
    std::unique_lock<std::mutex> lock(srcMutex_);

    if (source_ == 0) {
        return; // 已经关闭
    }

    logger->info("Audio::close: Closing audio device");

    // 停止播放
    alSourceStop(source_);
    ALenum alError = alGetError();
    if (alError != AL_NO_ERROR) {
        logger->warn("Audio::close: alSourceStop failed: {}", alGetString(alError));
    }

    // 取消所有缓冲区
    alSourcei(source_, AL_BUFFER, 0);
    alError = alGetError();
    if (alError != AL_NO_ERROR) {
        logger->warn("Audio::close: alSourcei failed: {}", alGetString(alError));
    }

    // 释放所有缓冲区
    for (ALuint buffer : buffers_) {
        if (buffer != 0) {
            alDeleteBuffers(1, &buffer);
            ALenum error = alGetError();
            if (error != AL_NO_ERROR) {
                logger->warn("Audio::close: alDeleteBuffers failed: {}", alGetString(error));
            }
        }
    }

    // 释放source
    alDeleteSources(1, &source_);
    alError = alGetError();
    if (alError != AL_NO_ERROR) {
        logger->warn("Audio::close: alDeleteSources failed: {}", alGetString(alError));
    }

    // 清空缓冲区数组和可用队列
    buffers_.fill(0);
    while (!availableBuffers_.empty()) {
        availableBuffers_.pop();
    }
    source_ = 0;

    // 重置时钟和状态
    currentPts_ = nanoseconds{0};
    deviceStartTime_ = {};
    playbackStarted_ = false;

    logger->info("Audio::close: Audio device closed");
}

// 辅助方法：回收已处理的缓冲区到可用队列
void Audio::recycleProcessedBuffers()
{
    ALint processed = 0;
    alGetSourcei(source_, AL_BUFFERS_PROCESSED, &processed);
    ALenum alError = alGetError();

    if (alError != AL_NO_ERROR || processed <= 0) {
        return;
    }

    // 限制一次回收的数量，避免异常情况
    if (processed > static_cast<ALint>(buffers_.size())) {
        logger->warn("Audio::recycleProcessedBuffers: Unexpected processed count: {}, limiting to {}", processed, buffers_.size());
        processed = static_cast<ALint>(buffers_.size());
    }

    std::vector<ALuint> bufids(processed);
    alSourceUnqueueBuffers(source_, processed, bufids.data());
    alError = alGetError();

    if (alError != AL_NO_ERROR) {
        logger->error("Audio::recycleProcessedBuffers: alSourceUnqueueBuffers failed: {}", alGetString(alError));
        return;
    }

    // 将回收的缓冲区放入可用队列
    for (int i = 0; i < processed; ++i) {
        availableBuffers_.push(bufids[i]);
    }

}

bool Audio::writeAudio(const uint8_t *samples, unsigned int length)
{
    try {
        if (!samples || length == 0) {
            logger->warn("Audio::writeAudio: Invalid parameters (samples: {}, length: {})", samples != nullptr, length);
            return false;
        }

        std::unique_lock<std::mutex> lock(srcMutex_);

        // 检查source是否有效
        if (source_ == 0) {
            logger->error("Audio::writeAudio: Invalid source (source_ == 0)");
            return false;
        }

        if (br_) {
            return false;
        }

        // 步骤1：回收已处理的缓冲区
        recycleProcessedBuffers();

        // 步骤2：如果没有可用缓冲区，等待
        if (availableBuffers_.empty()) {
            // 检查播放状态，如果停止了需要重新启动（否则缓冲区永远不会被处理）
            ALint sourceState = AL_INITIAL;
            alGetSourcei(source_, AL_SOURCE_STATE, &sourceState);
            ALenum alError = alGetError();

            if (alError == AL_NO_ERROR && sourceState != AL_PLAYING && playbackStarted_) {
                logger->warn("Audio::writeAudio: Source stopped unexpectedly (state: {}), restarting", sourceState);
                alSourcePlay(source_);
                alError = alGetError();
                if (alError == AL_NO_ERROR) {
                    logger->info("Audio::writeAudio: Playback restarted");
                }
            }

            // 等待缓冲区可用
            bool hasBuffer = srcCond_.wait_for(lock, std::chrono::milliseconds(100), [this]() -> bool {
                if (br_ || source_ == 0) {
                    return true; // 停止信号
                }
                // 尝试回收缓冲区
                recycleProcessedBuffers();
                return !availableBuffers_.empty();
            });

            if (br_ || source_ == 0) {
                return false;
            }

            if (!hasBuffer || availableBuffers_.empty()) {
                return false;
            }
        }

        // 步骤3：从可用队列获取一个缓冲区
        ALuint bufid = availableBuffers_.front();
        availableBuffers_.pop();

        // 步骤4：写入数据到缓冲区
        alBufferData(bufid, format_, samples, length, sampleRate_);
        ALenum alError = alGetError();
        if (alError != AL_NO_ERROR) {
            logger->error("Audio::writeAudio: alBufferData failed: {} (bufid={})", alGetString(alError), bufid);
            // 将缓冲区放回可用队列（避免丢失）
            availableBuffers_.push(bufid);
            return false;
        }

        // 步骤5：将缓冲区加入 OpenAL 队列
        alSourceQueueBuffers(source_, 1, &bufid);
        alError = alGetError();
        if (alError != AL_NO_ERROR) {
            logger->error("Audio::writeAudio: alSourceQueueBuffers failed: {}", alGetString(alError));
            // 将缓冲区放回可用队列（避免丢失）
            availableBuffers_.push(bufid);
            return false;
        }

        // 步骤6：检查是否需要启动播放
        ALint queued = 0;
        alGetSourcei(source_, AL_BUFFERS_QUEUED, &queued);
        alError = alGetError();
        if (alError != AL_NO_ERROR) {
            queued = 1;
        }

        // 播放启动逻辑：
        // - seeking 后：只要有可用缓冲区就尝试启动（避免无声）
        // - 正常播放：至少2个缓冲区才启动播放（保证流畅）
        // 关键修复：seeking 后 playbackStarted_ 被重置，但 queued 可能不足 2 个
        if (!playbackStarted_) {
            bool shouldStart = false;

            if (queued >= 2 || !availableBuffers_.empty()) {
                shouldStart = true;
            }

            if (shouldStart) {
                ALint state;
                alGetSourcei(source_, AL_SOURCE_STATE, &state);
                alError = alGetError();

                alSourcePlay(source_);
                alError = alGetError();

                if (alError != AL_NO_ERROR) {
                    logger->error("Audio::writeAudio: alSourcePlay failed: {}", alGetString(alError));
                    return false;
                }

                // 验证播放状态
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                alGetSourcei(source_, AL_SOURCE_STATE, &state);

                // 只有在 AL_PLAYING 状态下才设置 deviceStartTime_
                // 这确保我们只在音频真正开始播放时开始计时
                if (state == AL_PLAYING) {
                    // 关键：使用当前音频帧的PTS作为基准，而不是0，确保与视频基准一致
                    // 如果basePts还未设置，使用当前音频帧的PTS作为全局基准
                    nanoseconds currentBasePts = controller_.getBasePts();
                    if (currentBasePts == nanoseconds::min()) {
                        controller_.setBasePts(currentPts_);
                    }
                    if (deviceStartTime_ == std::chrono::steady_clock::time_point{}) {
                        deviceStartTime_ = std::chrono::steady_clock::now();
                    }

                    playbackStarted_ = true;
                } else {
                    nanoseconds currentBasePts = controller_.getBasePts();
                    if (currentBasePts == nanoseconds::min()) {
                        controller_.setBasePts(currentPts_);
                    }
                    playbackStarted_ = true;
                }
            }
        } else if (playbackStarted_) {
            nanoseconds currentBasePts = controller_.getBasePts();
            if (currentBasePts == nanoseconds::min()) {
                controller_.setBasePts(currentPts_);
            }
        }

        return true;
    } catch (const std::exception &e) {
        logger->error("Audio::writeAudio: Exception: {}", e.what());
        return false;
    } catch (...) {
        logger->error("Audio::writeAudio: Unknown exception");
        return false;
    }
}

void Audio::updateClock(nanoseconds pts)
{
    std::lock_guard<std::mutex> lock(srcMutex_);
    currentPts_ = pts;
}

// 注意：decodeFrame() 和 readAudio() 已移到 AudioThread 中

bool Audio::play()
{
    std::lock_guard<std::mutex> lock(srcMutex_);

    if (!source_) {
        if (logger) {
            logger->warn("Audio::play: source_ is null, audio not opened");
        }
        return false;
    }

    ALint queued{};
    alGetSourcei(source_, AL_BUFFERS_QUEUED, &queued);
    ALenum alError = alGetError();
    if (alError != AL_NO_ERROR) {
        if (logger) {
            logger->error("Audio::play: alGetSourcei(AL_BUFFERS_QUEUED) failed with error: {}", alError);
        }
        return false;
    }

    if (queued == 0) {
        return false;
    }

    alSourcePlay(source_);
    alError = alGetError();
    if (alError != AL_NO_ERROR) {
        if (logger) {
            logger->error("Audio::play: alSourcePlay failed with error: {}", alError);
        }
        return false;
    }

    return true;
}

bool Audio::pause()
{
    std::lock_guard<std::mutex> lock(srcMutex_);

    if (!source_) {
        if (logger) {
            logger->warn("Audio::pause: source_ is null, audio not opened");
        }
        return false;
    }

    ALenum alError = AL_NO_ERROR;

    alSourcePause(source_);
    alError = alGetError();

    if (alError != AL_NO_ERROR) {
        if (logger) {
            logger->error("Audio::pause: alSourcePause failed with error: {}", alError);
        }
        return false;
    }

    return true;
}

bool Audio::stop()
{
    // 设置停止标志，唤醒等待的线程
    br_ = true;
    srcCond_.notify_all();

    std::lock_guard<std::mutex> guard(srcMutex_);

    if (source_) {
        alSourceStop(source_);
        ALenum alError = alGetError();
        if (alError != AL_NO_ERROR) {
            if (logger) {
                logger->error("Audio::stop: alSourceStop failed with error: {}", alError);
            }
            // 继续执行清理，即使停止失败
        }
    }

    // 清空缓冲区并重置可用队列（使用 alSourcei 解除关联，避免 alSourceUnqueueBuffers 失败）
    if (source_) {
        alSourcei(source_, AL_BUFFER, 0);
        ALenum alError = alGetError();
        if (alError != AL_NO_ERROR && logger) {
            logger->warn("Audio::stop: alSourcei(AL_BUFFER,0) failed: {}", alGetString(alError));
        }
    }

    // 重置可用缓冲区队列
    while (!availableBuffers_.empty()) {
        availableBuffers_.pop();
    }
    for (ALuint bufid : buffers_) {
        if (bufid != 0) {
            availableBuffers_.push(bufid);
        }
    }

    currentPts_ = nanoseconds{0};
    deviceStartTime_ = {};
    playbackStarted_ = false;

    return true;
}

void Audio::clear()
{
    std::lock_guard<std::mutex> lock(srcMutex_);

    // 清空OpenAL缓冲区（确保Seeking后音频从新位置开始播放）
    if (source_) {
        // 停止播放
        alSourceStop(source_);
        ALenum alError = alGetError();
        if (alError != AL_NO_ERROR && logger) {
            logger->warn("Audio::clear: alSourceStop failed: {}", alGetString(alError));
        }
        // 回退到初始状态，确保下次 alSourcePlay 时从新数据开始（日志分析：Seeking 后音频不播放）
        alSourceRewind(source_);
        alError = alGetError();
        if (alError != AL_NO_ERROR && logger) {
            logger->warn("Audio::clear: alSourceRewind failed: {}", alGetString(alError));
        }

        // 使用 alSourcei(AL_BUFFER, 0) 解除所有缓冲区关联，避免 alSourceUnqueueBuffers 在
        // 源状态转换期间返回 AL_INVALID_OPERATION(40963)，且防止将仍排队中的缓冲区误放入
        // availableBuffers_ 导致后续 alBufferData 报 "Modifying storage for in-use buffer"
        alSourcei(source_, AL_BUFFER, 0);
        alError = alGetError();
        if (alError != AL_NO_ERROR && logger) {
            logger->warn("Audio::clear: alSourcei(AL_BUFFER,0) failed: {}", alGetString(alError));
        }

        // 重置可用缓冲区队列（解除关联后所有缓冲区可安全复用）
        while (!availableBuffers_.empty()) {
            availableBuffers_.pop();
        }
        for (ALuint bufid : buffers_) {
            if (bufid != 0) {
                availableBuffers_.push(bufid);
            }
        }

        // 重置状态
        currentPts_ = nanoseconds{0};
        playbackStarted_ = false;
        deviceStartTime_ = {};
    }
}

nanoseconds Audio::getClock()
{
    // 注意：getClockNoLock() 内部已经有适当的同步机制
    // 不需要额外的锁保护，避免死锁风险
    return getClockNoLock();
}

nanoseconds Audio::getClockNoLock()
{
    // 参考旧版本的实现：使用 currentPts_ 减去已排队的缓冲区时间，再加上当前播放位置
    // 关键：需要对齐到 basePts_，确保与视频时钟基准一致
    
    nanoseconds pts = currentPts_;
    
    // 获取基准PTS（播放开始时的PTS）
    nanoseconds basePts = controller_.getBasePts();
    
    // 如果basePts未设置，使用currentPts_作为基准（向后兼容）
    if (basePts == nanoseconds::min()) {
        basePts = currentPts_;
    }
    
    // 对齐到基准：pts = currentPts_ - basePts（从0开始）
    if (basePts != nanoseconds::min() && currentPts_ >= basePts) {
        pts = currentPts_ - basePts;
    } else {
        // 如果currentPts_ < basePts，说明还没开始播放，返回0
        pts = nanoseconds::zero();
    }

    // 检查 OpenAL 状态并计算实际播放位置
    if (source_ && sampleRate_ > 0) {
        ALint offset = 0;
        alGetSourcei(source_, AL_SAMPLE_OFFSET, &offset);
        ALenum alError = alGetError();

        if (alError == AL_NO_ERROR && offset >= 0) {
            ALint queued = 0;
            alGetSourcei(source_, AL_BUFFERS_QUEUED, &queued);
            alError = alGetError();
            
            ALint status = AL_STOPPED;
            if (alError == AL_NO_ERROR) {
                alGetSourcei(source_, AL_SOURCE_STATE, &status);
                alError = alGetError();
            }

            if (alError == AL_NO_ERROR && status != AL_STOPPED) {
                // 关键修复：参考旧版本，减去已排队的缓冲区时间
                // pts 已经对齐到基准（从0开始），现在需要减去已排队但未播放的缓冲区时间
                pts -= AudioBufferTime * queued;
                
                // 加上当前正在播放的样本偏移
                pts += nanoseconds{static_cast<long long>(static_cast<double>(offset) * 1000000000.0 / sampleRate_)};
            }
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

    // 注意：alListenerf 是全局操作，不需要锁保护
    if (isMute_) {
        alListenerf(AL_GAIN, 0.0);
    } else {
        alListenerf(AL_GAIN, v);
    }

    ALenum alError = alGetError();
    if (alError != AL_NO_ERROR && logger) {
        logger->warn("Audio::setVolume: alListenerf failed with error: {}", alError);
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
