#pragma once

#include "ffmpeg.h"
#include "packet_queue.h"

#include <AL/al.h>
#include <AL/alc.h>

#include <array>
#include <queue>
#include <vector>
#include <QThread>

// Per-buffer size, in time - 减少缓冲区大小以加快响应
constexpr std::chrono::milliseconds AudioBufferTime{20}; // 20ms per buffer (更小的缓冲区)
// Buffer total size, in time - 减少总缓冲时间
constexpr std::chrono::milliseconds AudioBufferTotalTime{200};            // 200ms total (10个缓冲区)
constexpr auto AudioBufferCount = AudioBufferTotalTime / AudioBufferTime; // 自动计算为 10 个缓冲区

class PlayController; // Forward declaration

/**
 * ALResources: RAII 管理 OpenAL source + buffers 生命周期
 * 创建时分配资源，析构时自动释放，避免泄漏
 */
class ALResources
{
public:
    ALResources() = default;
    ~ALResources() { destroy(); }

    ALResources(const ALResources &) = delete;
    ALResources &operator=(const ALResources &) = delete;

    bool create(size_t bufferCount)
    {
        destroy();
        buffers_.resize(bufferCount, 0);
        alGenBuffers(static_cast<ALsizei>(bufferCount), buffers_.data());
        if (alGetError() != AL_NO_ERROR) { buffers_.clear(); return false; }
        alGenSources(1, &source_);
        if (alGetError() != AL_NO_ERROR) {
            alDeleteBuffers(static_cast<ALsizei>(buffers_.size()), buffers_.data());
            buffers_.clear();
            return false;
        }
        valid_ = true;
        return true;
    }

    void destroy()
    {
        if (!valid_) return;
        if (source_ != 0) {
            alSourceStop(source_);
            ALint queued = 0;
            alGetSourcei(source_, AL_BUFFERS_QUEUED, &queued);
            while (queued-- > 0) {
                ALuint buf;
                alSourceUnqueueBuffers(source_, 1, &buf);
            }
            alDeleteSources(1, &source_);
            source_ = 0;
        }
        if (!buffers_.empty()) {
            alDeleteBuffers(static_cast<ALsizei>(buffers_.size()), buffers_.data());
            buffers_.clear();
        }
        valid_ = false;
    }

    bool isValid() const { return valid_; }
    ALuint source() const { return source_; }
    const std::vector<ALuint> &buffers() const { return buffers_; }

private:
    ALuint source_{0};
    std::vector<ALuint> buffers_;
    bool valid_{false};
};

/**
 * Audio: 音频输出设备（重构后，类似 VideoWriter）
 * 只负责 OpenAL 音频输出，不包含解码逻辑
 * 
 * 职责：
 * - OpenAL 资源管理（source_, buffers_）
 * - 写入已解码的音频数据到 OpenAL 缓冲区
 * - 音量控制
 * - 音频时钟获取
 */
class Audio
{
public:
    Audio(PlayController &controller);
    ~Audio();

    static int initAL();

    // 初始化音频输出设备（创建 OpenAL 资源）
    // @param format OpenAL 格式（AL_FORMAT_MONO8, AL_FORMAT_STEREO16 等）
    // @param frameSize 每样本字节数
    // @param sampleRate 采样率
    bool open(ALenum format, ALuint frameSize, int sampleRate);

    // 关闭音频输出设备（清理 OpenAL 资源）
    void close();

    // 写入已解码的音频数据到 OpenAL 缓冲区（由 AudioThread 调用）
    // @param samples 已解码的音频数据
    // @param length 数据长度（字节数）
    // @return 成功返回 true，失败返回 false
    bool writeAudio(const uint8_t *samples, unsigned int length);

    // 更新音频时钟（由 AudioThread 调用）
    void updateClock(nanoseconds pts);

    bool play();
    bool pause();
    bool stop();
    void clear();
    void setVolume(float v);
    float getVolume();
    void ToggleMute();

    nanoseconds getClock();
    nanoseconds getClockNoLock(); // 供 AudioThread 调用，不需要锁

    // 注意：audioFinished_ 已移到 AudioThread

private:
    friend class PlayController;
    friend class AudioThread;
    PlayController &controller_;

    // 音频时钟
    nanoseconds currentPts_{0};  // 当前解码帧的 PTS（持续更新）
    std::chrono::steady_clock::time_point deviceStartTime_{};

    // OpenAL 相关
    ALenum format_{AL_NONE};
    ALuint frameSize_{0};
    int sampleRate_{0};
    ALuint source_{0};
    std::array<ALuint, AudioBufferCount> buffers_{};  // 所有缓冲区 ID（用于创建/销毁）
    std::queue<ALuint> availableBuffers_;             // 可用缓冲区队列（关键：正确的缓冲区生命周期管理）

    std::mutex srcMutex_;
    std::condition_variable srcCond_;
    bool br_{false}; // 停止标志，与线程类的br_对应
    // 暂停状态现在通过 PlayController 的状态机管理

    bool isMute_ = false;
    float storeVolumeWhenMute;
    bool playbackStarted_{false}; // 音频播放启动标志，避免重复启动
    
    // 辅助方法：回收已处理的缓冲区到可用队列
    void recycleProcessedBuffers();
};
