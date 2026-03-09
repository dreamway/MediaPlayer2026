#pragma once

#include "chronons.h"
#include "ffmpeg.h"
#include "packet_queue.h"

#include <array>
#include <atomic>
#include <QThread>

#define VIDEO_PICTURE_QUEUE_SIZE 24
#define ERROR_LEN 1024
#define PRINT_LOG 1

class Movie;
class Video 
{
public:
    Video(Movie &movie);
    ~Video();

    std::pair<AVFrame *, int64_t> currentFrame();
    void SetPause(bool isPause);
    bool PictureRingBufferIsEmpty();
    
    // 获取队列状态（用于监控）
    size_t getQueueReadIndex() const { return pictQRead_.load(); }
    size_t getQueueWriteIndex() const { return pictQWrite_.load(); }
    size_t getQueueSize() const { return pictQ_.size(); }

    // 预加载相关
    bool isPreloadCompleted() const { return preloadCompleted_.load(std::memory_order_acquire); }
    void waitForPreload(int timeoutMs = 5000);  // 等待预加载完成

    //TODO: 动态调整packet大小，无需一次性解压过多
    /*bool ResizePacketSize(int videoWidth, int videoHeight, int videoFrameChannel);*/


private:
    int start();
    int pause(bool isPause);
    int stop();
    void clear();
    void showError(int err);
    nanoseconds getClock();

    friend class Movie;
    Movie &movie_;
    std::atomic<bool> isPause{ false };
    std::atomic<bool> isQuit{ false };

    AVStream *stream_{nullptr};
    AVCodecContextPtr codecctx_;

    //用RingBuffer的方式存储读取的AVFrame
    // 优化内存管理：减少缓冲区大小，从20MB减少到5MB，减少内存压力
    PacketQueue<20*1024 * 1024> packets_;

    nanoseconds displayPts_{0};
    microseconds displayPtsTime_{microseconds::min()};
    std::mutex dispPtsMutex_;

    struct Picture {
        AVFramePtr frame;
        AVFramePtr sw_frame;      // 软件帧（用于硬件解码时的帧转换）
        AVFramePtr yuv420p_frame; // YUV420P 帧（用于 NV12 到 YUV420P 的格式转换）
        nanoseconds pts{nanoseconds::min()};
    };
    std::array<Picture, VIDEO_PICTURE_QUEUE_SIZE> pictQ_;
    std::atomic<size_t> pictQRead_{0u}, pictQWrite_{1u};
    std::mutex pictQMutex_;
    std::condition_variable pictQCond_;
    
    // 预加载相关
    std::atomic<bool> preloadCompleted_{false};  // 预加载是否完成
    static constexpr size_t PRELOAD_TARGET_FRAMES = 12;  // 预加载目标帧数（队列的一半）

     char *m_error = nullptr;
};
