#pragma once

#include "ffmpeg.h"
#include "packet_queue.h"

#include <AL/al.h>
#include <AL/alc.h>

#include <array>
#include <QThread>

// Per-buffer size, in time
constexpr milliseconds AudioBufferTime{20};
// Buffer total size, in time (should be divisible by the buffer time)
constexpr milliseconds AudioBufferTotalTime{180};
constexpr auto AudioBufferCount = AudioBufferTotalTime / AudioBufferTime;

class Movie;
class Audio
{
public:
    Audio(Movie &movie);
    ~Audio();

    static int initAL();

    int start();
    int decodeFrame();
    int readAudio(uint8_t *samples, unsigned int length);
    bool play();
    bool pause(bool isPause);
    bool stop();
    void clear();
    void setVolume(float v);
    float getVolume();
    void ToggleMute();

    nanoseconds getClock();
    nanoseconds getClockNoLock();

private:
    friend class Movie;
    Movie &movie_;

    AVStream *stream_{nullptr};
    AVCodecContextPtr codecctx_;

    PacketQueue<1024 * 1024> packets_;

    nanoseconds currentPts_{0};
    nanoseconds deviceStartTime_{nanoseconds::min()};
   

    AVFramePtr decodedFrame_;
    SwrContextPtr swrctx_;
    std::atomic<bool> isPause{false};

    bool isMute_ = false;
    float storeVolumeWhenMute;

    uint64_t dstChanLayout_{0};
    AVSampleFormat dstSampleFmt_{AV_SAMPLE_FMT_NONE};

    uint8_t *samples_{nullptr};
    int samplesLen_{0};
    int samplesPos_{0};
    int samplesMax_{0};

    ALenum format_{AL_NONE};
    ALuint frameSize_{0};

    std::mutex srcMutex_;
    std::condition_variable srcCond_;
    ALuint source_{0};
    std::array<ALuint, AudioBufferCount> buffers_{};
    ALuint bufferIdx_{0};

    void showError(int err);
    char *m_error = nullptr;

public:
    std::atomic<bool> audioFinished_{false}; // 音频是否已结束
};
